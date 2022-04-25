#include <iostream>
#include <fstream>
#include "ss_graph.hpp"
#include "ss_node.hpp"
#include "ss_node_factory.hpp"
#include "ss_parser.hpp"


#include "../graphics/ga_cube_component.h"
#include "../graphics/ga_material.h"

Base_GraphNode::~Base_GraphNode() {
    if (can_draw_intermed_image()) {
        glDeleteTextures(1, &nodes_rendered_texture);
        glDeleteTextures(1, &nodes_depth_texture);
    }

    if (input_pins) {
        delete[] input_pins;
    }
    if (output_pins) {
        delete[] output_pins;
    }
    handle_destruction();
}


Builtin_GraphNode::Builtin_GraphNode(Builtin_Node_Data& data, int id, ImVec2 pos) {
        _id = id;
        _old_pos = _pos = pos;
        _inliner = data.in_liner;
        _name = data._name;

        num_input = data.in_vars.size();
        num_output = data.out_vars.size();
        input_pins = new Base_InputPin[num_input];
        output_pins = new Base_OutputPin[num_output];

        for (int i = 0; i < num_input; ++i) {
            GLSL_TYPE type = data.in_vars[i].first;
            std::string name = data.in_vars[i].second;
            input_pins[i].bInput = true;
            input_pins[i].index = i;
            input_pins[i].owner = this;
            input_pins[i].type = type;
            input_pins[i]._name = name;
        }
        for (int o = 0; o < num_output; ++o) {
            GLSL_TYPE type = data.out_vars[o].first;
            std::string name = data.out_vars[o].second;
            output_pins[o].bInput = false;
            output_pins[o].index = o;
            output_pins[o].owner = this;
            output_pins[o].type = type;
            output_pins[o]._name = name;
        }
    }


inline bool Base_GraphNode::only_first_out_connected() {
    return num_output == 1 && output_pins->output.size() == 1;
}











// HELPER FUNCTIONS
inline bool is_gentype(unsigned int type) {
    return (type & GLSL_GenType);
}

void Base_GraphNode::propogate_gentype_in_subgraph(Base_Pin* start_pin, unsigned int len_type) {
    std::cout << "PROPOGATING: " << SS_Parser::type_to_string(GLSL_TYPE(GLSL_Float | (len_type & GLSL_LenMask), 1)) << std::endl;
    std::unordered_set<int> processed_ids;      // only prop length
    propogate_gentype_in_subgraph(start_pin, len_type & GLSL_LenMask, processed_ids);
}

void Base_GraphNode::propogate_gentype_in_subgraph(Base_Pin* start_pin, 
    unsigned int len_type, std::unordered_set<int>& processed_ids) {
    std::cout << "PROPPING FOR " << start_pin->owner->_name << "_" << start_pin->owner->_id << ", manager=" << _name << "_" << _id << std::endl;
    // if we reach non-generic parts
    if (!is_gentype(start_pin->type.type_flags)) return;
    std::cout << "IS VALID FOR PROP" << std::endl;

    // Stop if processed already and add to processed
    if (processed_ids.find(_id) != processed_ids.end()) return;
        std::cout << "NOT IN PROCESSED" << std::endl;
    processed_ids.insert(_id);
    // INPUT
    for (int i = 0; i < num_input; ++i) {
        if (!is_gentype(input_pins[i].type.type_flags)) continue;

        // intersect generic part of pin with restrictive
        unsigned int gen_len_intersect = len_type & GLSL_LenMask;
        input_pins[i].type.type_flags &= (~GLSL_LenMask); // clear len mask
        input_pins[i].type.type_flags |= gen_len_intersect; // set len mask
        std::cout << "Set " << input_pins[i]._name << " with " << SS_Parser::type_to_string(input_pins[i].type) << std::endl;

        if (!(input_pins + i)->input) continue;
        (input_pins + i)->input->owner->propogate_gentype_in_subgraph(
            (input_pins + i)->input, len_type, processed_ids);
    } 
    // OUTPUT
    for (int o = 0; o < num_output; ++o) {
        if (!is_gentype(output_pins[o].type.type_flags)) continue;

        // intersect generic part of pin with restrictive
        unsigned int gen_len_intersect = len_type & GLSL_LenMask;
        output_pins[o].type.type_flags &= ~GLSL_LenMask; // clear len mask
        output_pins[o].type.type_flags |= gen_len_intersect; // set len mask
        std::cout << "Set " << output_pins[o]._name << " with " << SS_Parser::type_to_string(output_pins[o].type) << std::endl;

        for (Base_InputPin* i_pin : (output_pins + o)->output) {
            if (!i_pin) continue;
            i_pin->owner->propogate_gentype_in_subgraph(i_pin, len_type, processed_ids);
        }
    }
}

void Base_GraphNode::propogate_build_dirty() { 
    is_build_dirty = true;
    for (int o = 0; o < num_output; ++o) {
        for (Base_InputPin* i_pin : output_pins[o].output)
            i_pin->owner->propogate_build_dirty();
    }
}


// RETURNS LEN (NON-GEN) SET FOR MOST RESTRICTIVE GENTYPES
    // If start_pin is non-generic, only LEN will be set
unsigned int Base_GraphNode::get_most_restrictive_gentype_in_subgraph(Base_Pin* start_pin) {
    if (!is_gentype(start_pin->type.type_flags)) return start_pin->type.type_flags & GLSL_LenMask;

    std::unordered_set<int> processed_ids;
    // intersect only the len-not-gen-type portion of the pin type
    unsigned int start_type = (start_pin->type.type_flags & GLSL_GenType) >> GLSL_LenToGenPush;
    std::cout << "Starting with " << SS_Parser::type_to_string(GLSL_TYPE(start_type | GLSL_Float, 1)) << std::endl;
    unsigned int restrict_type = get_most_restrictive_gentype_in_subgraph(start_pin, processed_ids);
    std::cout << "Restricting final with " << SS_Parser::type_to_string(GLSL_TYPE(restrict_type | GLSL_Float, 1)) << std::endl;
    restrict_type &= start_type;
    return restrict_type;
}


unsigned int Base_GraphNode::get_most_restrictive_gentype_in_subgraph(Base_Pin* start_pin, std::unordered_set<int>& processed_ids) {
    if (!is_gentype(start_pin->type.type_flags)) return start_pin->type.type_flags & GLSL_LenMask;
    
    // Stop if processed already and add to processed
    if (processed_ids.find(_id) != processed_ids.end()) return GLSL_LenMask;
    processed_ids.insert(_id);

    // set starting most restrictive type as gentype portion of type, shifted to moddable LenType
    unsigned int most_res_gen_type = (start_pin->type.type_flags & GLSL_GenType) >> GLSL_LenToGenPush;
    unsigned int type_type = start_pin->type.type_flags & GLSL_TypeMask;

    // INPUT
    for (int i = 0; i < num_input; ++i) {
        if (!is_gentype(input_pins[i].type.type_flags)) continue; // dont process non-generic pins in the node
        Base_OutputPin* o_pin = (input_pins + i)->input;
        if (!o_pin) continue; // output pin exists

        unsigned int res_type = get_most_restrictive_gentype_in_subgraph(o_pin, processed_ids);
        most_res_gen_type &= res_type; // intersect only for len, non-gen
    } 
    // OUTPUT
    for (int o = 0; o < num_output; ++o) {
        if (!is_gentype(output_pins[o].type.type_flags)) continue; // dont process non-generic pins in the node

        for (Base_InputPin* i_pin : (output_pins + o)->output) {
            if (!i_pin) continue; // pin exists

            unsigned int res_type = get_most_restrictive_gentype_in_subgraph(i_pin, processed_ids);
            most_res_gen_type &= res_type; // intersect only for len, non-gen
        }
    }
    return most_res_gen_type;
}

void checkCompileErrors(GLuint shader, std::string type)
{
    GLint success;
    GLchar infoLog[1024];
    if(type != "PROGRAM")  {
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if(!success)  {
            glGetShaderInfoLog(shader, 1024, NULL, infoLog);
            std::cout << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
        }
    } else {
        glGetProgramiv(shader, GL_LINK_STATUS, &success);
        if(!success) {
            glGetProgramInfoLog(shader, 1024, NULL, infoLog);
            std::cout << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
        }
    }
}

bool Base_GraphNode::generate_intermed_image() {


    glGenTextures(1, &nodes_rendered_texture);
    glBindTexture(GL_TEXTURE_2D, nodes_rendered_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 256,  256, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    // depth and useless stencil, may use later?
    glGenTextures(1, &nodes_depth_texture);
    glBindTexture(GL_TEXTURE_2D, nodes_depth_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, 256, 256, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    return true;
}

void Base_GraphNode::draw_to_intermed(unsigned int framebuffer, SS_Boilerplate_Manager* bp, std::vector<struct Parameter_Data*>& params) {
    if (_cube) delete _cube;

    std::cout << "STARTING FOR " << _name << " : " << std::endl;
    _cube = new ga_cube_component(_vert_str, _frag_str, bp);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, nodes_depth_texture, 0); 
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, nodes_rendered_texture, 0);
    
     if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << std::endl;

    glEnable(GL_DEPTH_TEST);
    glViewport(0, 0, 256, 256);
    if (is_build_dirty)
        glClearColor(0.8f, 0.1f, 0.1f, 1);
    else
        glClearColor(0.2f, 0.2f, 0.4f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
	ga_mat4f view;
	view.make_lookat_rh({2, 2, 3}, {0, 0, 0}, {0, 1, 0});
	ga_mat4f perspective;
	perspective.make_perspective_rh(ga_degrees_to_radians(45.0f), 1.0f, 0.1f, 10000.0f);
    _cube->_material->bind(view, perspective, _cube->_transform, params);
    glBindVertexArray(_cube->_vao);
    glDrawElements(GL_TRIANGLES, _cube->_index_count, GL_UNSIGNED_SHORT, 0);
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glEnable(GL_DEPTH_TEST);    

    is_build_dirty = false;

    unsigned int err = glGetError();
    //if (err != 0)
    //    std::cout << "ERROR: " << err << std::endl;
}

void Base_GraphNode::draw_to_intermed_no_recompile(unsigned int framebuffer, std::vector<struct Parameter_Data*>& params) {

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, nodes_depth_texture, 0); 
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, nodes_rendered_texture, 0);
    
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << std::endl;

    glEnable(GL_DEPTH_TEST);
    glViewport(0, 0, 256, 256);
    if (is_build_dirty)
        glClearColor(0.8f, 0.1f, 0.1f, 1);
    else
        glClearColor(0.2f, 0.2f, 0.4f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    if (_cube) {
        ga_mat4f view;
        view.make_lookat_rh({2, 2, 3}, {0, 0, 0}, {0, 1, 0});
        ga_mat4f perspective;
        perspective.make_perspective_rh(ga_degrees_to_radians(45.0f), 1.0f, 0.1f, 10000.0f);

        _cube->_material->bind(view, perspective, _cube->_transform, params);
        glBindVertexArray(_cube->_vao);
        glDrawElements(GL_TRIANGLES, _cube->_index_count, GL_UNSIGNED_SHORT, 0);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    unsigned int err = glGetError();
    //if (err != 0)
    //    std::cout << "ERROR: " << err << std::endl;
}

void Base_GraphNode::update_vertex_shader_only(std::string vert_shader_code) {
    if (_cube)
        _cube->_material->update_vertex_shader(vert_shader_code);
}
void Base_GraphNode::update_frag_shader_only(std::string frag_shader_code) {
    if (_cube)
        _cube->_material->update_frag_shader(frag_shader_code);
}

void Base_GraphNode::set_shader_intermed(const std::string& frag_shad, const std::string& vert_shad) {
    _frag_str = frag_shad;
    _vert_str = vert_shad;
/*
    std::ofstream o_vert("node_" + _name + "_" + std::to_string(_id) + "_vert.txt");
    o_vert << vert_shad << std::endl;
    o_vert.close();

    std::ofstream o_frag("node_" + _name + "_" + std::to_string(_id) + "_frag.txt");
    o_frag << frag_shad << std::endl;
    o_frag.close();
*/
}












/* AND HANDLE INPUT */
// Collect component sizes
    // size and pin position should be cached
// Draw rect
// draw name and line
// draw input pins with function
// draw output pins with function
// draw output pin bezier

float border = 5;
float mid_pin_col = 40;
float line_thickness = 2;
float line_height = 5;

float pin_circle_offset = 3;
float pin_border = 5;
float pin_y_step = 2;

unsigned rect_color = 0xff444444;
float rect_rounding = 10;

void Base_GraphNode::set_bounds(float scale) {
    // set main name size
    name_size = ImGui::CalcTextSize(_name.c_str());

    // set pin sizes and accumulate main rect size
    float in_x_max = 0, out_x_max = 0;
    float in_y = 0, out_y = 0;
    for (int i = 0; i < num_input; ++i) {
        in_pin_sizes.push_back(input_pins[i].get_size(pin_circle_offset, pin_border));
        in_x_max = std::max(in_x_max, in_pin_sizes.back().x);
        in_y += in_pin_sizes.back().y + pin_y_step;
    }
    for (int o = 0; o < num_output; ++o) {
        out_pin_sizes.push_back(output_pins[o].get_size(pin_circle_offset, pin_border));
        out_x_max = std::max(out_x_max, out_pin_sizes.back().x);
        out_y += out_pin_sizes.back().y + pin_y_step;
    }
    
    // set main rect size
    float y_max = std::max(in_y, out_y);
    float x_max = std::max(in_x_max + out_x_max + mid_pin_col, name_size.x);
    rect_size = ImVec2(x_max + border*2, y_max + name_size.y + line_height + border*2);

    // Get relative (to upper left) position of pins
    ImVec2 pin_pos = ImVec2(border, name_size.y + line_height + border);
    for (int i = 0; i < num_input; ++i) {
        ImVec2 pin_size = in_pin_sizes[i];
        Base_InputPin* pin = input_pins + i;
        in_pin_rel_pos.push_back(pin_pos);
        pin_pos.y += (pin_size.y + pin_y_step);
    }

    pin_pos = ImVec2(rect_size.x, name_size.y + line_height + border);
    for (int o = 0; o < num_output; ++o) {
        ImVec2 pin_size = out_pin_sizes[o];
        Base_OutputPin* pin = output_pins + o;
        out_pin_rel_pos.push_back(pin_pos - ImVec2(pin_size.x + border, 0));
        pin_pos.y += (pin_size.y + pin_y_step);
    }

    // intermediate
    if (can_draw_intermed_image()) {
        display_panel_rel_pos.x = 0;
        display_panel_rel_pos.y = rect_size.y;
        display_panel_rel_size.x = rect_size.x;
        display_panel_rel_size.y = 20;

        rect_size.y += display_panel_rel_size.y;
    }
}

ImTextureID Base_GraphNode::bind_and_get_image_texture() {
    glBindTexture(GL_TEXTURE0, nodes_rendered_texture);
    return (void*)(intptr_t)nodes_rendered_texture;
}


bool Base_GraphNode::can_connect_pins(Base_InputPin* in_pin, Base_OutputPin* out_pin) {
    bool size_equal = true; // in_pin->type.arr_size == out_pin->type.arr_size;
    unsigned type_intersect = in_pin->type.type_flags & out_pin->type.type_flags;

    bool valid_type = type_intersect & GLSL_TypeMask;
    std::cout << (valid_type ? "TYPE VALID" : "") << std::endl;
    bool valid_gentype = type_intersect & GLSL_LenMask;
    std::cout << (valid_gentype ? "GENTYPE VALID" : "") << std::endl;
    std::cout << size_equal << valid_type << valid_gentype << std::endl;
    return size_equal && valid_type && valid_gentype;
}


void Base_GraphNode::draw(ImDrawList* list, ImVec2 pos_offset, bool is_hover) {
    // _pos should be middle of node, but this pos is actually the upper left
    ImVec2 pos = _pos - ImVec2(rect_size.x * 0.5f, rect_size.y * 0.5f) + pos_offset;

    // Draw main rect
    unsigned int col = is_hover ? 0xffff4444 : 0xffaa4444;
    list->AddRectFilled(pos, pos + rect_size, col, rect_rounding);
    list->AddText(pos + ImVec2(border, border), 0xffffffff, _name.c_str());
    list->AddLine(pos + ImVec2(0, name_size.y + border), pos + ImVec2(rect_size.x, name_size.y + border), 0xffffffff, 2.0f);

    // For each input, ask it to be drawn at proper location
    for (int i = 0; i < num_input; ++i) {
        input_pins[i].draw(list, in_pin_rel_pos[i] + pos, pin_circle_offset, pin_border);
    }

    // For each output, ask it to be drawn at proper location
    for (int o = 0; o < num_output; ++o) {
        output_pins[o].draw(list, out_pin_rel_pos[o] + pos, pin_circle_offset, pin_border);
    }

    if (can_draw_intermed_image()) {
        list->AddRectFilled(display_panel_rel_pos + pos, display_panel_rel_pos + display_panel_rel_size + pos, 
            is_display_up ? 0xffffffff : 0xaaaaaaaa, 7);
        if (is_display_up) {
            ImVec2 display_min = display_panel_rel_pos + ImVec2(0, display_panel_rel_size.y); 
            ImVec2 display_max = display_min + ImVec2(rect_size.x, rect_size.x);

            list->AddImage(bind_and_get_image_texture(), display_min + pos, display_max + pos);
        }
    }
}

void Base_GraphNode::draw_output_connects(ImDrawList* list, ImVec2 offset) {
    for (int o = 0; o < num_output; ++o) {
        Base_OutputPin* o_pin = output_pins + o;
        ImU32 color = SS_Parser::type_to_color(o_pin->type);

        for (Base_InputPin* i_pin : o_pin->output) {
            float r;
            ImVec2 o_pos = o_pin->get_pin_pos(pin_circle_offset, pin_border, &r);
            o_pos = o_pos - ImVec2(r / 2, r / 2) + offset;
            ImVec2 i_pos = i_pin->get_pin_pos(pin_circle_offset, pin_border, &r);
            i_pos = i_pos - ImVec2(r / 2, r / 2) + offset;
            list->AddBezierCurve(o_pos, o_pos + ImVec2(50, 0), i_pos - ImVec2(50, 0), i_pos, color, 3);
        }
    }
}

bool Base_GraphNode::is_hovering(ImVec2 m) {
    ImVec2 minn = _pos - ImVec2(rect_size.x * .5f, rect_size.y * .5f);
    ImVec2 maxx = _pos + ImVec2(rect_size.x * .5f, rect_size.y * .5f);
    if (is_display_up)
        maxx.y += rect_size.x;
    return (m.x > minn.x && m.y > minn.y) && (m.x < maxx.x && m.y < maxx.y);
}

bool contains(ImVec2 minn, ImVec2 maxx, ImVec2 pt) {
    return (pt.x > minn.x && pt.y > minn.y) && (pt.x < maxx.x && pt.y < maxx.y);
}

Base_Pin* Base_GraphNode::get_hovered_pin(ImVec2 mouse_pos) {
    Base_Pin* pin = nullptr;
    ImVec2 ul = _pos - ImVec2(rect_size.x / 2, rect_size.y / 2);
    for (int i = 0; i < num_input; ++i) {
        ImVec2 pin_pos = ul + in_pin_rel_pos[i];
        if (contains(pin_pos, pin_pos + in_pin_sizes[i], mouse_pos))
            pin = input_pins + i;
    }
    for (int o = 0; o < num_output; ++o) {
        ImVec2 pin_pos = ul + out_pin_rel_pos[o];
        if (contains(pin_pos, pin_pos + out_pin_sizes[o], mouse_pos))
            pin = output_pins + o;
    }

    if (pin) {
        ImGui::BeginTooltip();
        std::string s = SS_Parser::type_to_string(pin->type);
        
        ImGui::Text(s.c_str());
        ImGui::EndTooltip();
    }
    return pin;
}

bool Base_GraphNode::is_display_button_hovered_over(ImVec2 m) {
    if (!can_draw_intermed_image()) return false;

    ImVec2 p = _pos - ImVec2(rect_size.x / 2, rect_size.y / 2);
    ImVec2 minn = p + display_panel_rel_pos;
    ImVec2 maxx = p + display_panel_rel_pos + display_panel_rel_size;
    return (m.x > minn.x && m.y > minn.y) && (m.x < maxx.x && m.y < maxx.y);
}




ImVec2 Base_Pin::get_size(float circle_off, float border) {
    ImVec2 text_size = ImGui::CalcTextSize(_name.c_str());
    float circle_rad = text_size.y / 2;
    return text_size + ImVec2(circle_rad + circle_off + border * 2, border * 2);
}

ImVec2 Base_InputPin::get_pin_pos(float circle_off, float border, float* rad) {
    assert(rad);
    ImVec2 pos = owner->_pos - ImVec2(owner->rect_size.x * .5f, owner->rect_size.y * .5f);
    ImVec2 bound_pos = owner->in_pin_rel_pos[index];
    *rad = ImGui::CalcTextSize(_name.c_str()).y;
    return pos + bound_pos + ImVec2(*rad + border, *rad + border);
}

ImVec2 Base_OutputPin::get_pin_pos(float circle_off, float border, float* rad) {
    assert(rad);
    ImVec2 pos = owner->_pos - ImVec2(owner->rect_size.x * .5f, owner->rect_size.y * .5f);
    ImVec2 bound_pos = owner->out_pin_rel_pos[index];
    ImVec2 text_size = ImGui::CalcTextSize(_name.c_str());
    float off_x = text_size.x;
    *rad = text_size.y;
    return pos + bound_pos + ImVec2(*rad + border + circle_off + off_x, *rad + border);
}



void Base_InputPin::draw(ImDrawList* d, ImVec2 pos, float circle_off, float border) {
    ImVec2 text_size = ImGui::CalcTextSize(_name.c_str());
    ImU32 color = SS_Parser::type_to_color(type);

    float circle_rad = text_size.y / 2;

    d->AddCircleFilled(pos + ImVec2(border + circle_rad, border + circle_rad), circle_rad, color);
    d->AddText(pos + ImVec2(border + 2*circle_rad + circle_off, border), 0xffffffff, _name.c_str());
}

void Base_InputPin::disconnect_all_from(SS_Graph* g) {
    if (input) 
        g->disconnect_pins(this, input);
}
void Base_OutputPin::disconnect_all_from(SS_Graph* g) {
    std::vector<Base_InputPin*> output_COPY(output);

    for (Base_InputPin* i_pin : output_COPY) 
        g->disconnect_pins(i_pin, this);
}

void Base_OutputPin::draw(ImDrawList* d, ImVec2 pos, float circle_off, float border) {
    ImVec2 text_size = ImGui::CalcTextSize(_name.c_str());
    ImU32 color = SS_Parser::type_to_color(type);

    float circle_rad = text_size.y / 2;

    d->AddText(pos + ImVec2(border, border), 0xffffffff, _name.c_str());
    d->AddCircleFilled(pos + ImVec2(text_size.x + border + circle_off + circle_rad, border + circle_rad), circle_rad, color);
}

std::string Base_OutputPin::get_pin_output_name() { 
    return owner->request_output(index); 
}






std::string Builtin_GraphNode::request_output(int out_index) {
    return pin_parse_out_names[out_index];
}

std::string Builtin_GraphNode::process_for_code() {
    pin_parse_out_names.clear();
    // OUTPUT NAMES
    for (int o = 0; o < num_output; ++o) 
        pin_parse_out_names.push_back(SS_Parser::get_unique_var_name(_id, o, output_pins[o].type));
    // INPUT NAMES
    std::vector<std::string> in_pin_names;
    for (int i = 0; i < num_input; ++i)
        if (input_pins[i].input)
            in_pin_names.push_back(input_pins[i].input->get_pin_output_name());
        else
            in_pin_names.push_back(SS_Parser::type_to_default_value(input_pins[i].type));
    
    std::stringstream sss;
    // make ouputs
    int start_out = 0;
    if (num_output > 0 && _inliner.find("\%o1") == std::string::npos) {
        start_out = 1;
    }

    // replace placeholders with output/input names
    std::string temp_inliner = _inliner;
    // OUTPUT
    for (int o = start_out; o < num_output; ++o) {
        // VARIABLES
        sss <<  SS_Parser::type_to_string(output_pins[o].type) << " " << pin_parse_out_names[o] << ";" << std::endl;
        // IN THE FUNCTION
        std::string rep = std::string("\%o") + (char)('1' + o);
        auto str_it = temp_inliner.find(rep);
        if (str_it == std::string::npos) std::cout << "Couldn't find " << rep << " in " << temp_inliner << std::endl;
        std::string fr = temp_inliner.substr(0, str_it);
        std::string ba = temp_inliner.substr(str_it + 3);
        temp_inliner = fr + pin_parse_out_names[o] + ba;
    }
    // INPUT
    for (int i = 0; i < num_input; ++i) {
        std::string rep = std::string("\%i") + (char)('1' + i);
        while (true) {
            auto str_it = temp_inliner.find(rep);
            if (str_it == std::string::npos) break;
            std::string fr = temp_inliner.substr(0, str_it);
            std::string ba = temp_inliner.substr(str_it + 3);
            temp_inliner = fr + in_pin_names[i] + ba;
        }
    }
    // if we didn't replace the 'first' output placeholder then we use assignment directly
    if (start_out == 1)
        sss  << SS_Parser::type_to_string(output_pins[0].type) << " " << pin_parse_out_names[0] << " = ";
    // close out
    sss << temp_inliner;
    sss << ";";
    return sss.str();
}







Constant_Node::Constant_Node(Constant_Node_Data& data, int id, ImVec2 pos) {
    _id = id;
    _old_pos = _pos = pos;
    _name = data._name;

    _data_gen = data._gentype;
    _data_type = data._type;

    _data = nullptr;
    if (_data_type == SS_Float) {
        switch (_data_gen) {
            case SS_Scalar: 
                _data = new float[1]; memset(_data, 0, 4);
                break;
            case SS_Vec2: 
                _data = new float[2]; memset(_data, 0, 8);
                break;
            case SS_Vec3: 
                _data = new float[3]; memset(_data, 0, 12);
                break;
            case SS_Vec4: 
                _data = new float[4]; memset(_data, 0, 16);
                break;
            case SS_Mat2:
                _data = new float[4]; memset(_data, 0, 16);
                break;
            case SS_Mat3:
                _data = new float[9]; memset(_data, 0, 36);
                break;
            case SS_Mat4:
                _data = new float[16]; memset(_data, 0, 64);
                break;
        }
    }

    num_output = 1;
    num_input = 0;
    output_pins = new Base_OutputPin[num_output];

    output_pins->type = SS_Parser::constant_type_to_type(data._gentype, data._type);
    output_pins->bInput = false;
    output_pins->index = 0;
    output_pins->owner = this;
    output_pins->_name = "CONST OUT";
}

void Vector_Op_Node::make_vec_break(int s) {
    num_input = 1;
    num_output = s;

    input_pins = new Base_InputPin[1];
    input_pins->bInput = true;
    input_pins->index = 0;
    input_pins->owner = this;
    input_pins->_name = "VECTOR";
    input_pins->type = GLSL_TYPE(GLSL_AllTypes | (GLSL_Vec2 << (s - 2)), 1);

    output_pins = new Base_OutputPin[s];
    const char* sw[] = { "x", "y", "z", "w" };
    for (int o = 0; o < s; ++o) {
        output_pins[o].bInput = false;
        output_pins[o].index = o;
        output_pins[o].owner = this;
        output_pins[o]._name = sw[o];
        output_pins[o].type = GLSL_TYPE(GLSL_AllTypes |  GLSL_Scalar, 1);
    }
}

void Vector_Op_Node::make_vec_make(int s) {
    num_input = s;
    num_output = 1;

    output_pins = new Base_OutputPin[1];
    output_pins->bInput = false;
    output_pins->index = 0;
    output_pins->owner = this;
    output_pins->_name = "VECTOR";
    output_pins->type = GLSL_TYPE(GLSL_AllTypes | (GLSL_Vec2 << (s - 2)), 1);

    input_pins = new Base_InputPin[s];
    const char* sw[] = { "x", "y", "z", "w" };
    for (int i = 0; i < s; ++i) {
        input_pins[i].bInput = true;
        input_pins[i].index = i;
        input_pins[i].owner = this;
        input_pins[i]._name = sw[i];
        input_pins[i].type = GLSL_TYPE(GLSL_AllTypes |  GLSL_Scalar, 1);
    }
}

Vector_Op_Node::Vector_Op_Node(Vector_Op_Node_Data& data, int id, ImVec2 pos) {
    _id = id;
    _old_pos = _pos = pos;
    _name = data._name;
    
    input_pins = 0;
    output_pins = 0;
    _vec_op = data._op;
    
    switch (data._op) {
        case VEC_BREAK2_OP: make_vec_break(2); break;
        case VEC_BREAK3_OP: make_vec_break(3); break;
        case VEC_BREAK4_OP: make_vec_break(4); break;
        case VEC_MAKE2_OP: make_vec_make(2); break;
        case VEC_MAKE3_OP: make_vec_make(3); break;
        case VEC_MAKE4_OP: make_vec_make(4); break;
    }
}


std::string Vector_Op_Node::request_output(int out_index) {

    if (_vec_op == VEC_BREAK2_OP || _vec_op == VEC_BREAK3_OP || _vec_op == VEC_BREAK4_OP) {
        const char* sw[] = { "x", "y", "z", "w" };
        std::string var_name = input_pins->input->owner->request_output(0);
        return var_name + "." + sw[out_index];


    } else if (_vec_op == VEC_MAKE2_OP || _vec_op == VEC_MAKE3_OP || _vec_op == VEC_MAKE4_OP) {
        std::string output = "vec";
        output += ('0' + num_input);
        output += "(";
        for (int i = 0; i < num_input; ++i) {
            if (input_pins[i].input) {
                output += input_pins[i].input->get_pin_output_name();
            } else {
                output += "0";
            }
            output += ((i + 1 == num_input) ? ")" : ",");
        }
        return output;
    }
    return "";
}
std::string Vector_Op_Node::process_for_code() {
    return "";
}





Param_Node::Param_Node(Parameter_Data* data, int id, ImVec2 pos) {
    _id = id;
    _old_pos = _pos = pos;
    _name = data->_param_name;

    num_output = 1;
    num_input = 0;
    output_pins = new Base_OutputPin[num_output];

    output_pins->type = SS_Parser::constant_type_to_type(data->_gentype, data->_type);
    output_pins->bInput = false;
    output_pins->index = 0;
    output_pins->owner = this;
    output_pins->_name = "PARAM OUT";
}

std::string Param_Node::request_output(int out_index) {
    return _name;
}
std::string Param_Node::process_for_code() {
    return "";
}

void Param_Node::update_type_from_param(GLSL_TYPE type) {
    for (int o = 0; o < num_output; ++o)
        output_pins[o].type = type;
}



Boilerplate_Var_Node::Boilerplate_Var_Node(Boilerplate_Var_Data data, SS_Boilerplate_Manager* bp, int id, ImVec2 pos) {
    _id = id;
    _old_pos = _pos = pos;
    _name = data._name;

    frag_node = data.frag_only;
    _bp_manager = bp;

    num_input = 0;
    num_output = 1;
    output_pins = new Base_OutputPin[1];
    input_pins = nullptr;

    output_pins->type = data.type;
    output_pins->bInput = false;
    output_pins->index = 0;
    output_pins->owner = this;
    output_pins->_name = data._name;
}


std::string Boilerplate_Var_Node::request_output(int out_index) {
    return _bp_manager->get_output_code_for_var(_name);
}
std::string Boilerplate_Var_Node::process_for_code() {
    return "";
}


Terminal_Node::Terminal_Node(const std::vector<Boilerplate_Var_Data>& terminal_pins, int id, ImVec2 pos) {
    _id = id;
    _old_pos = _pos = pos;
    frag_node = terminal_pins.front().frag_only;
    _name = frag_node ? "TERMINAL FRAG           " : "TERMINAL VERTEX         ";

    num_input = terminal_pins.size();
    num_output = 0;
    input_pins = nullptr;
    if (num_input > 0)
        input_pins = new Base_InputPin[num_input];
    output_pins = nullptr;

    for (int i = 0; i < terminal_pins.size(); ++i) {
        auto data = terminal_pins[i];

        input_pins[i].type = data.type;
        input_pins[i].bInput = true;
        input_pins[i].index = i;
        input_pins[i].owner = this;
        input_pins[i]._name = data._name;
    }
}





std::string Constant_Node::request_output(int out_index) {
    std::cout << "CONSTANT OUTPUT REQUEST START" << output_pins->type.is_matrix() << std::endl;
    GLSL_TYPE t = output_pins->type;
    float* f_data = (float*)_data;
    std::stringstream sss;
    
    int size = 1;
    if (t.type_flags & GLSL_Scalar) {
        sss << (*f_data);
        return sss.str();
    }

    if (t.type_flags & GLSL_Vec2)
        size = 2;
    if (t.type_flags & GLSL_Vec3)
        size = 3;
    if (t.type_flags & GLSL_Vec4)
        size = 4;
    if (t.type_flags & GLSL_Mat) {
        sss << "mat" << size << "(";
        size *= size;
    } else {
        sss << "vec" << size << "(";
    }
    std::cout << "Picked a size of " << size << std::endl;
    for (int f = 0; f < size; ++f) {
        std::cout << "Trying to get from " << f_data << std::endl;
        sss << f_data[f];
        sss << ((f + 1 != size) ? ", " : ")");
    }

    std::string s = sss.str();
    std::cout << "CONSTANT OUTPUT REQUEST END" << std::endl;
    return s;
}
std::string Constant_Node::process_for_code() {
    return "";
}