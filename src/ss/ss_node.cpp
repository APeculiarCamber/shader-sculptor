#include <iostream>
#include <fstream>
#include "ss_graph.hpp"
#include "ss_node.hpp"
#include "ss_parser.hpp"


#include "../graphics/ga_cube_component.h"
#include "../graphics/ga_material.h"
#include "ss_pins.hpp"
#include "ss_boilerplate.hpp"

Base_GraphNode::~Base_GraphNode() {
    if (m_nodesRenderedTexture != NODE_TEXTURE_NULL) {
        glDeleteTextures(1, &m_nodesRenderedTexture);
        glDeleteTextures(1, &m_nodesDepthTexture);
    }
}


Builtin_GraphNode::Builtin_GraphNode(Builtin_Node_Data& data, int id, ImVec2 pos) {
    m_id = id;
    m_oldPos = m_pos = pos;
        _inliner = data.in_liner;
    m_name = data._name;

    m_numInput = data.in_vars.size();
    m_numOutput = data.out_vars.size();
    m_inputPins = std::vector<Base_InputPin>(m_numInput);
    m_outputPins = std::vector<Base_OutputPin>(m_numOutput);

        for (int i = 0; i < m_numInput; ++i) {
            GLSL_TYPE type = data.in_vars[i].first;
            std::string name = data.in_vars[i].second;
            m_inputPins[i].bInput = true;
            m_inputPins[i].index = i;
            m_inputPins[i].owner = this;
            m_inputPins[i].type = type;
            m_inputPins[i]._name = name;
        }
        for (int o = 0; o < m_numOutput; ++o) {
            GLSL_TYPE type = data.out_vars[o].first;
            std::string name = data.out_vars[o].second;
            m_outputPins[o].bInput = false;
            m_outputPins[o].index = o;
            m_outputPins[o].owner = this;
            m_outputPins[o].type = type;
            m_outputPins[o]._name = name;
        }
    }










// HELPER FUNCTIONS
inline bool is_gentype(unsigned int type) {
    return (type & GLSL_GenType);
}

void Base_GraphNode::PropagateGentypeInSubgraph(Base_Pin* start_pin, unsigned int type) {
    std::unordered_set<int> processed_ids;      // only prop length
    PropogateGentypeInSubgraph_Rec(start_pin, type & GLSL_LenMask, processed_ids);
}

void Base_GraphNode::PropogateGentypeInSubgraph_Rec(Base_Pin* start_pin,
                                                    unsigned int type, std::unordered_set<int>& processed_ids) {
    // if we reach non-generic parts
    if (!is_gentype(start_pin->type.type_flags)) return;

    // Stop if processed already and add to processed
    if (processed_ids.find(m_id) != processed_ids.end()) return;
    processed_ids.insert(m_id);
    // INPUT
    for (int i = 0; i < m_numInput; ++i) {
        if (!is_gentype(m_inputPins[i].type.type_flags)) continue;

        // Intersect generic part of pin with restrictive
        unsigned int gen_len_intersect = type & GLSL_LenMask;
        m_inputPins[i].type.type_flags &= (~GLSL_LenMask); // clear len mask
        m_inputPins[i].type.type_flags |= gen_len_intersect; // set len mask

        if (not m_inputPins[i].input) continue;
        m_inputPins[i].input->owner->PropogateGentypeInSubgraph_Rec(
                m_inputPins[i].input, type, processed_ids);
    } 
    // OUTPUT
    for (int o = 0; o < m_numOutput; ++o) {
        if (!is_gentype(m_outputPins[o].type.type_flags)) continue;

        // Intersect generic part of pin with restrictive
        unsigned int gen_len_intersect = type & GLSL_LenMask;
        m_outputPins[o].type.type_flags &= ~GLSL_LenMask; // clear len mask
        m_outputPins[o].type.type_flags |= gen_len_intersect; // set len mask

        for (Base_InputPin* i_pin : m_outputPins[o].output) {
            if (!i_pin) continue;
            i_pin->owner->PropogateGentypeInSubgraph_Rec(i_pin, type, processed_ids);
        }
    }
}

void Base_GraphNode::PropagateBuildDirty() {
    m_isBuildDirty = true;
    for (int o = 0; o < m_numOutput; ++o) {
        for (Base_InputPin* i_pin : m_outputPins[o].output)
            i_pin->owner->PropagateBuildDirty();
    }
}


// RETURNS LEN (NON-GEN) SET FOR MOST RESTRICTIVE GENTYPES
    // If start_pin is non-generic, only LEN will be set
unsigned int Base_GraphNode::GetMostRestrictiveGentypeInSubgraph(Base_Pin* start_pin) {
    if (!is_gentype(start_pin->type.type_flags)) return start_pin->type.type_flags & GLSL_LenMask;

    std::unordered_set<int> processed_ids;
    // Intersect only the len-not-gen-type portion of the pin type
    unsigned int start_type = (start_pin->type.type_flags & GLSL_GenType) >> GLSL_LenToGenPush;
    unsigned int restrict_type = GetMostRestrictiveGentypeInSubgraph_Rec(start_pin, processed_ids);
    restrict_type &= start_type;
    return restrict_type;
}


unsigned int Base_GraphNode::GetMostRestrictiveGentypeInSubgraph_Rec(Base_Pin* start_pin, std::unordered_set<int>& processed_ids) {
    if (!is_gentype(start_pin->type.type_flags)) return start_pin->type.type_flags & GLSL_LenMask;
    
    // Stop if processed already and add to processed
    if (processed_ids.find(m_id) != processed_ids.end()) return GLSL_LenMask;
    processed_ids.insert(m_id);

    // set starting most restrictive type as gentype portion of type, shifted to moddable LenType
    unsigned int most_res_gen_type = (start_pin->type.type_flags & GLSL_GenType) >> GLSL_LenToGenPush;
    // unsigned int type_type = start_pin->type.type_flags & GLSL_TypeMask;

    // INPUT
    for (int i = 0; i < m_numInput; ++i) {
        if (!is_gentype(m_inputPins[i].type.type_flags)) continue; // dont process non-generic pins in the node
        Base_OutputPin* o_pin = m_inputPins[i].input;
        if (!o_pin) continue; // output pin exists

        unsigned int res_type = GetMostRestrictiveGentypeInSubgraph_Rec(o_pin, processed_ids);
        most_res_gen_type &= res_type; // Intersect only for len, non-gen
    } 
    // OUTPUT
    for (int o = 0; o < m_numOutput; ++o) {
        if (!is_gentype(m_outputPins[o].type.type_flags)) continue; // dont process non-generic pins in the node

        for (Base_InputPin* i_pin : m_outputPins[o].output) {
            if (!i_pin) continue; // pin exists

            unsigned int res_type = GetMostRestrictiveGentypeInSubgraph_Rec(i_pin, processed_ids);
            most_res_gen_type &= res_type; // Intersect only for len, non-gen
        }
    }
    return most_res_gen_type;
}

void checkCompileErrors(GLuint shader, const std::string& type)
{
    GLint success;
    GLchar infoLog[1024];
    if(type != "PROGRAM")  {
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if(!success)  {
            glGetShaderInfoLog(shader, 1024, NULL, infoLog);
            std::cerr << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
        }
    } else {
        glGetProgramiv(shader, GL_LINK_STATUS, &success);
        if(!success) {
            glGetProgramInfoLog(shader, 1024, NULL, infoLog);
            std::cerr << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
        }
    }
}

bool Base_GraphNode::GenerateIntermediateResultFrameBuffers() {
    glGenTextures(1, &m_nodesRenderedTexture);
    glBindTexture(GL_TEXTURE_2D, m_nodesRenderedTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 256,  256, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);


    glGenTextures(1, &m_nodesDepthTexture);
    glBindTexture(GL_TEXTURE_2D, m_nodesDepthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, 256, 256, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    return true;
}

void Base_GraphNode::CompileIntermediateCode(std::unique_ptr<ga_material>&& material) {
    m_cube.reset(new ga_cube_component(m_vertStr, m_fragStr, std::move(material)));
    m_isBuildDirty = false;
    //unsigned int err = glGetError();
}

void Base_GraphNode::DrawIntermediateResult(unsigned int framebuffer, const std::vector<std::unique_ptr<Parameter_Data>>& params) {

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, m_nodesDepthTexture, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_nodesRenderedTexture, 0);
    
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cout << "Depth texture: " << m_nodesDepthTexture << ", Render Texture: " << m_nodesRenderedTexture << std::endl;
        assert(not "ERROR::FRAMEBUFFER:: Framebuffer is not complete!");
    }

    glEnable(GL_DEPTH_TEST);
    glViewport(0, 0, 256, 256);
    if (m_isBuildDirty)
        glClearColor(0.8f, 0.1f, 0.1f, 1);
    else
        glClearColor(0.2f, 0.2f, 0.4f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    if (m_cube) {
        ga_mat4f view{};
        view.make_lookat_rh(ga_vec3f{2.0f, 2.0f, 3.0f}, ga_vec3f{0, 0, 0}, ga_vec3f{0, 1, 0});
        ga_mat4f perspective{};
        perspective.make_perspective_rh(ga_degrees_to_radians(45.0f), 1.0f, 0.1f, 10000.0f);

        m_cube->_material->bind(view, perspective, m_cube->_transform, params);
        glBindVertexArray(m_cube->_vao);
        glDrawElements(GL_TRIANGLES, m_cube->_index_count, GL_UNSIGNED_SHORT, 0);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Base_GraphNode::SetShaderCode(const std::string& frag_shad, const std::string& vert_shad) {
    m_fragStr = frag_shad;
    m_vertStr = vert_shad;
}












/* AND HANDLE INPUT */
// Collect component sizes
    // size and pin position should be cached
// Draw rect
// Draw name and line
// Draw input pins with function
// Draw output pins with function
// Draw output pin bezier

const float BORDER = 5;
float mid_pin_col = 40;
float line_thickness = 2;
float line_height = 5;

float pin_circle_offset = 3;
float pin_border = 5;
float pin_y_step = 2;

unsigned rect_color = 0xff444444;
float rect_rounding = 10;

void Base_GraphNode::SetBounds(float scale) {
    // set main name size
    m_nameRelSize = ImGui::CalcTextSize(m_name.c_str());

    // set pin sizes and accumulate main rect size
    float in_x_max = 0, out_x_max = 0;
    float in_y = 0, out_y = 0;
    for (int i = 0; i < m_numInput; ++i) {
        m_inPinSizes.push_back(m_inputPins[i].GetSize(pin_circle_offset, pin_border));
        in_x_max = std::max(in_x_max, m_inPinSizes.back().x);
        in_y += m_inPinSizes.back().y + pin_y_step;
    }
    for (int o = 0; o < m_numOutput; ++o) {
        m_outPinSizes.push_back(m_outputPins[o].GetSize(pin_circle_offset, pin_border));
        out_x_max = std::max(out_x_max, m_outPinSizes.back().x);
        out_y += m_outPinSizes.back().y + pin_y_step;
    }
    
    // set main rect size
    float y_max = std::max(in_y, out_y);
    float x_max = std::max(in_x_max + out_x_max + mid_pin_col, m_nameRelSize.x);
    m_rectSize = ImVec2(x_max + BORDER * 2, y_max + m_nameRelSize.y + line_height + BORDER * 2);

    // Get relative (to upper left) position of pins
    ImVec2 pin_pos = ImVec2(BORDER, m_nameRelSize.y + line_height + BORDER);
    for (int i = 0; i < m_numInput; ++i) {
        ImVec2 pin_size = m_inPinSizes[i];
        m_inPinRelPos.push_back(pin_pos);
        pin_pos.y += (pin_size.y + pin_y_step);
    }

    pin_pos = ImVec2(m_rectSize.x, m_nameRelSize.y + line_height + BORDER);
    for (int o = 0; o < m_numOutput; ++o) {
        ImVec2 pin_size = m_outPinSizes[o];
        m_outPinRelPos.push_back(pin_pos - ImVec2(pin_size.x + BORDER, 0));
        pin_pos.y += (pin_size.y + pin_y_step);
    }

    // intermediate
    if (CanDrawIntermedImage()) {
        m_displayPanelRelPos.x = 0;
        m_displayPanelRelPos.y = m_rectSize.y;
        m_displayPanelRelSize.x = m_rectSize.x;
        m_displayPanelRelSize.y = 20;

        m_rectSize.y += m_displayPanelRelSize.y;
    }
}

ImTextureID Base_GraphNode::BindAndGetImageTexture() {
    glBindTexture(GL_TEXTURE0, m_nodesRenderedTexture);
    return (void*)(intptr_t)m_nodesRenderedTexture;
}


bool Base_GraphNode::CanConnectPins(Base_InputPin* in_pin, Base_OutputPin* out_pin) {
    bool size_equal = true; // TODO: in_pin->type.arr_size == out_pin->type.arr_size;
    unsigned type_intersect = in_pin->type.type_flags & out_pin->type.type_flags;

    bool valid_type = type_intersect & GLSL_TypeMask;
    bool valid_gentype = type_intersect & GLSL_LenMask;
    return size_equal && valid_type && valid_gentype;
}


void Base_GraphNode::Draw(ImDrawList* drawList, ImVec2 pos_offset, bool is_hover) {
    // m_pos should be middle of node, but this pos is actually the upper left
    ImVec2 pos = m_pos - ImVec2(m_rectSize.x * 0.5f, m_rectSize.y * 0.5f) + pos_offset;

    // Draw main rect
    unsigned int col = is_hover ? 0xffff4444 : 0xffaa4444;
    drawList->AddRectFilled(pos, pos + m_rectSize, col, rect_rounding);
    drawList->AddText(pos + ImVec2(BORDER, BORDER), 0xffffffff, m_name.c_str());
    drawList->AddLine(pos + ImVec2(0, m_nameRelSize.y + BORDER), pos + ImVec2(m_rectSize.x, m_nameRelSize.y + BORDER), 0xffffffff, 2.0f);

    // For each input, ask it to be drawn at proper location
    for (int i = 0; i < m_numInput; ++i) {
        m_inputPins[i].Draw(drawList, m_inPinRelPos[i] + pos, pin_circle_offset, pin_border);
    }

    // For each output, ask it to be drawn at proper location
    for (int o = 0; o < m_numOutput; ++o) {
        m_outputPins[o].Draw(drawList, m_outPinRelPos[o] + pos, pin_circle_offset, pin_border);
    }

    if (CanDrawIntermedImage()) {
        drawList->AddRectFilled(m_displayPanelRelPos + pos, m_displayPanelRelPos + m_displayPanelRelSize + pos,
                                m_isDisplayUp ? 0xffffffff : 0xaaaaaaaa, 7);
        if (m_isDisplayUp) {
            ImVec2 display_min = m_displayPanelRelPos + ImVec2(0, m_displayPanelRelSize.y);
            ImVec2 display_max = display_min + ImVec2(m_rectSize.x, m_rectSize.x);

            drawList->AddImage(BindAndGetImageTexture(), display_min + pos, display_max + pos);
        }
    }
}

void Base_GraphNode::DrawOutputConnects(ImDrawList* drawList, ImVec2 offset) {
    for (int o = 0; o < m_numOutput; ++o) {
        Base_OutputPin* o_pin = &m_outputPins[o];
        ImU32 color = SS_Parser::GLSLTypeToColor(o_pin->type);

        for (Base_InputPin* i_pin : o_pin->output) {
            float r;
            ImVec2 o_pos = o_pin->GetPinPos(pin_circle_offset, pin_border, &r);
            o_pos = o_pos - ImVec2(r / 2, r / 2) + offset;
            ImVec2 i_pos = i_pin->GetPinPos(pin_circle_offset, pin_border, &r);
            i_pos = i_pos - ImVec2(r / 2, r / 2) + offset;
            drawList->AddBezierCurve(o_pos, o_pos + ImVec2(50, 0), i_pos - ImVec2(50, 0), i_pos, color, 3);
        }
    }
}

bool Base_GraphNode::IsHovering(ImVec2 mouse_pos) {
    ImVec2 minn = m_pos - ImVec2(m_rectSize.x * .5f, m_rectSize.y * .5f);
    ImVec2 maxx = m_pos + ImVec2(m_rectSize.x * .5f, m_rectSize.y * .5f);
    if (m_isDisplayUp)
        maxx.y += m_rectSize.x;
    return (mouse_pos.x > minn.x && mouse_pos.y > minn.y) && (mouse_pos.x < maxx.x && mouse_pos.y < maxx.y);
}

bool contains(ImVec2 minn, ImVec2 maxx, ImVec2 pt) {
    return (pt.x > minn.x && pt.y > minn.y) && (pt.x < maxx.x && pt.y < maxx.y);
}

Base_Pin* Base_GraphNode::GetHoveredPin(ImVec2 mouse_pos) {
    Base_Pin* pin = nullptr;
    ImVec2 ul = m_pos - ImVec2(m_rectSize.x / 2, m_rectSize.y / 2);
    for (int i = 0; i < m_numInput; ++i) {
        ImVec2 pin_pos = ul + m_inPinRelPos[i];
        if (contains(pin_pos, pin_pos + m_inPinSizes[i], mouse_pos))
            pin = &m_inputPins[i];
    }
    for (int o = 0; o < m_numOutput; ++o) {
        ImVec2 pin_pos = ul + m_outPinRelPos[o];
        if (contains(pin_pos, pin_pos + m_outPinSizes[o], mouse_pos))
            pin = &m_outputPins[o];
    }

    if (pin) {
        ImGui::BeginTooltip();
        std::string s = SS_Parser::GLSLTypeToString(pin->type);
        
        ImGui::Text("%s", s.c_str());
        ImGui::EndTooltip();
    }
    return pin;
}

bool Base_GraphNode::IsDisplayButtonHoveredOver(ImVec2 p) {
    if (!CanDrawIntermedImage()) return false;

    ImVec2 cPos = m_pos - ImVec2(m_rectSize.x / 2, m_rectSize.y / 2);
    ImVec2 minn = cPos + m_displayPanelRelPos;
    ImVec2 maxx = cPos + m_displayPanelRelPos + m_displayPanelRelSize;
    return (p.x > minn.x && p.y > minn.y) && (p.x < maxx.x && p.y < maxx.y);
}

void Base_GraphNode::DisconnectAllPins() {
    auto* node = this;
    for (int i = 0; i < node->m_numInput; ++i) {
        if (node->m_inputPins[i].input)
            PinOps::DisconnectPins(&node->m_inputPins[i], node->m_inputPins[i].input, true);
    }

    for (int o = 0; o < node->m_numOutput; ++o) {
        Base_OutputPin* o_pin = &node->m_outputPins[o];
        std::vector<Base_InputPin*> output_COPY(node->m_outputPins[o].output);
        for (Base_InputPin* i_pin : output_COPY) {
            PinOps::DisconnectPins(i_pin, o_pin, true);
        }
    }
}



std::string Builtin_GraphNode::RequestOutput(int out_index) {
    return pin_parse_out_names[out_index];
}

std::string Builtin_GraphNode::ProcessForCode() {
    pin_parse_out_names.clear();
    // OUTPUT NAMES
    for (int o = 0; o < m_numOutput; ++o)
        pin_parse_out_names.push_back(SS_Parser::GetUniqueVarName(m_id, o, m_outputPins[o].type));
    // INPUT NAMES
    std::vector<std::string> in_pin_names;
    for (int i = 0; i < m_numInput; ++i)
        if (m_inputPins[i].input)
            in_pin_names.push_back(m_inputPins[i].input->get_pin_output_name());
        else
            in_pin_names.push_back(SS_Parser::GLSLTypeToDefaultValue(m_inputPins[i].type));
    
    std::stringstream sss;
    // make ouputs
    int start_out = 0;
    if (m_numOutput > 0 && _inliner.find("\%o1") == std::string::npos) {
        start_out = 1;
    }

    // replace placeholders with output/input names
    std::string temp_inliner = _inliner;
    // OUTPUT
    for (int o = start_out; o < m_numOutput; ++o) {
        // VARIABLES
        sss << SS_Parser::GLSLTypeToString(m_outputPins[o].type) << " " << pin_parse_out_names[o] << ";" << std::endl;
        // IN THE FUNCTION
        std::string rep = std::string("\%o") + (char)('1' + o);
        auto str_it = temp_inliner.find(rep);
        std::string fr = temp_inliner.substr(0, str_it);
        std::string ba = temp_inliner.substr(str_it + 3);
        temp_inliner = fr.append(pin_parse_out_names[o]).append(ba);
    }
    // INPUT
    for (int i = 0; i < m_numInput; ++i) {
        std::string rep = std::string("\%i") + (char)('1' + i);
        while (true) {
            auto str_it = temp_inliner.find(rep);
            if (str_it == std::string::npos) break;
            std::string fr = temp_inliner.substr(0, str_it);
            std::string ba = temp_inliner.substr(str_it + 3);
            temp_inliner = fr .append(in_pin_names[i]).append(ba);
        }
    }
    // if we didn't replace the 'first' output placeholder then we use assignment directly
    if (start_out == 1)
        sss << SS_Parser::GLSLTypeToString(m_outputPins[0].type) << " " << pin_parse_out_names[0] << " = ";
    // close out
    sss << temp_inliner;
    sss << ";";
    return sss.str();
}

Builtin_GraphNode::~Builtin_GraphNode() {
    glDeleteTextures(1, &m_nodesRenderedTexture);
    glDeleteTextures(1, &m_nodesDepthTexture);
}

Constant_Node::Constant_Node(Constant_Node_Data& data, int id, ImVec2 pos) {
    m_id = id;
    m_oldPos = m_pos = pos;
    m_name = data.m_name;

    _data_gen = data.m_gentype;
    _data_type = data.m_type;

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
            case SS_MAT:
                break;
        }
    }

    m_numOutput = 1;
    m_numInput = 0;
    m_outputPins = std::vector<Base_OutputPin>(m_numOutput);

    m_outputPins[0].type = SS_Parser::ConstantTypeToGLSLType(data.m_gentype, data.m_type);
    m_outputPins[0].bInput = false;
    m_outputPins[0].index = 0;
    m_outputPins[0].owner = this;
    m_outputPins[0]._name = "CONST OUT";
}

void Vector_Op_Node::make_vec_break(int s) {
    m_numInput = 1;
    m_numOutput = s;

    m_inputPins = std::vector<Base_InputPin>(1);
    m_inputPins[0].bInput = true;
    m_inputPins[0].index = 0;
    m_inputPins[0].owner = this;
    m_inputPins[0]._name = "VECTOR";
    m_inputPins[0].type = GLSL_TYPE(GLSL_AllTypes | (GLSL_Vec2 << (s - 2)), 1);

    m_outputPins = std::vector<Base_OutputPin>(m_numOutput);
    const char* sw[] = { "x", "y", "z", "w" };
    for (int o = 0; o < m_numOutput; ++o) {
        m_outputPins[o].bInput = false;
        m_outputPins[o].index = o;
        m_outputPins[o].owner = this;
        m_outputPins[o]._name = sw[o];
        m_outputPins[o].type = GLSL_TYPE(GLSL_AllTypes | GLSL_Scalar, 1);
    }
}

void Vector_Op_Node::make_vec_make(int s) {
    m_numInput = s;
    m_numOutput = 1;

    m_outputPins = std::vector<Base_OutputPin>(m_numInput);
    m_outputPins[0].bInput = false;
    m_outputPins[0].index = 0;
    m_outputPins[0].owner = this;
    m_outputPins[0]._name = "VECTOR";
    m_outputPins[0].type = GLSL_TYPE(GLSL_AllTypes | (GLSL_Vec2 << (s - 2)), 1);

    m_inputPins = std::vector<Base_InputPin>(m_numInput);
    const char* sw[] = { "x", "y", "z", "w" };
    for (int i = 0; i < s; ++i) {
        m_inputPins[i].bInput = true;
        m_inputPins[i].index = i;
        m_inputPins[i].owner = this;
        m_inputPins[i]._name = sw[i];
        m_inputPins[i].type = GLSL_TYPE(GLSL_AllTypes | GLSL_Scalar, 1);
    }
}

Vector_Op_Node::Vector_Op_Node(Vector_Op_Node_Data& data, int id, ImVec2 pos) {
    m_id = id;
    m_oldPos = m_pos = pos;
    m_name = data.m_name;

    m_inputPins = std::vector<Base_InputPin>(m_numInput);
    m_outputPins = std::vector<Base_OutputPin>(m_numOutput);
    _vec_op = data.m_op;
    
    switch (data.m_op) {
        case VEC_BREAK2_OP: make_vec_break(2); break;
        case VEC_BREAK3_OP: make_vec_break(3); break;
        case VEC_BREAK4_OP: make_vec_break(4); break;
        case VEC_MAKE2_OP: make_vec_make(2); break;
        case VEC_MAKE3_OP: make_vec_make(3); break;
        case VEC_MAKE4_OP: make_vec_make(4); break;
    }
}


std::string Vector_Op_Node::RequestOutput(int out_index) {

    if (_vec_op == VEC_BREAK2_OP || _vec_op == VEC_BREAK3_OP || _vec_op == VEC_BREAK4_OP) {
        const char* sw[] = { "x", "y", "z", "w" };
        std::string var_name = m_inputPins[0].input->owner->RequestOutput(0);
        return var_name + "." + sw[out_index];


    } else if (_vec_op == VEC_MAKE2_OP || _vec_op == VEC_MAKE3_OP || _vec_op == VEC_MAKE4_OP) {
        std::string output = "vec";
        output += std::to_string(m_numInput);
        output += "(";
        for (int i = 0; i < m_numInput; ++i) {
            if (m_inputPins[i].input) {
                output += m_inputPins[i].input->get_pin_output_name();
            } else {
                output += "0";
            }
            output += ((i + 1 == m_numInput) ? ")" : ",");
        }
        return output;
    }
    return "";
}
std::string Vector_Op_Node::ProcessForCode() {
    return "";
}





Param_Node::Param_Node(Parameter_Data* data, int id, ImVec2 pos) {
    m_id = id;
    _paramID = data->GetID();
    m_oldPos = m_pos = pos;
    m_name = data->GetName();

    m_numOutput = 1;
    m_numInput = 0;
    m_outputPins = std::vector<Base_OutputPin>(m_numOutput);

    m_outputPins[0].type = data->GetType();
    m_outputPins[0].bInput = false;
    m_outputPins[0].index = 0;
    m_outputPins[0].owner = this;
    m_outputPins[0]._name = "PARAM OUT";
}

std::string Param_Node::RequestOutput(int out_index) {
    return m_name;
}
std::string Param_Node::ProcessForCode() {
    return "";
}

void Param_Node::update_type_from_param(GLSL_TYPE type) {
    for (int o = 0; o < m_numOutput; ++o)
        m_outputPins[o].type = type;
}

Param_Node::~Param_Node()  {
    glDeleteTextures(1, &m_nodesRenderedTexture);
    glDeleteTextures(1, &m_nodesDepthTexture);
}


Boilerplate_Var_Node::Boilerplate_Var_Node(Boilerplate_Var_Data data, SS_Boilerplate_Manager* bp, int id, ImVec2 pos) {
    m_id = id;
    m_oldPos = m_pos = pos;
    m_name = data._name;

    frag_node = data.frag_only;
    _bpManager = bp;

    m_numInput = 0;
    m_numOutput = 1;
    m_inputPins = std::vector<Base_InputPin>(m_numInput);
    m_outputPins = std::vector<Base_OutputPin>(m_numOutput);

    m_outputPins[0].type = data.type;
    m_outputPins[0].bInput = false;
    m_outputPins[0].index = 0;
    m_outputPins[0].owner = this;
    m_outputPins[0]._name = data._name;
}


std::string Boilerplate_Var_Node::RequestOutput(int out_index) {
    return _bpManager->GetIntermediateResultCodeForVar(m_name);
}
std::string Boilerplate_Var_Node::ProcessForCode() {
    return "";
}

Terminal_Node::Terminal_Node(const std::vector<Boilerplate_Var_Data>& terminal_pins, int id, ImVec2 pos) {
    m_id = id;
    m_oldPos = m_pos = pos;
    frag_node = terminal_pins.front().frag_only;
    m_name = frag_node ? "TERMINAL FRAG           " : "TERMINAL VERTEX         ";

    m_numInput = terminal_pins.size();
    m_numOutput = 0;
    m_inputPins = std::vector<Base_InputPin>(m_numInput);
    m_outputPins = std::vector<Base_OutputPin>(m_numOutput);

    for (std::vector<Boilerplate_Var_Data>::size_type i = 0u; i < terminal_pins.size(); ++i) {
        const auto& data = terminal_pins[i];

        m_inputPins[i].type = data.type;
        m_inputPins[i].bInput = true;
        m_inputPins[i].index = i;
        m_inputPins[i].owner = this;
        m_inputPins[i]._name = data._name;
    }
}

Terminal_Node::~Terminal_Node()  {
    glDeleteTextures(1, &m_nodesRenderedTexture);
    glDeleteTextures(1, &m_nodesDepthTexture);
}


std::string Constant_Node::RequestOutput(int out_index) {
    GLSL_TYPE t = m_outputPins[0].type;
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
    for (int f = 0; f < size; ++f) {
        sss << f_data[f];
        sss << ((f + 1 != size) ? ", " : ")");
    }

    std::string s = sss.str();
    return s;
}
std::string Constant_Node::ProcessForCode() {
    return "";
}