#include "ss_graph.hpp"
#include "../stb_image.h"
#include <fstream>
#include <algorithm>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

/**
 * @brief Construct a new ss graph::ss graph object
 *          
 * 
 * @param bp  : EXPECTS IT TO BE HEAP/DYNAMIC MEMORY
 */
SS_Graph::SS_Graph(SS_Boilerplate_Manager* bp) {
    glGenFramebuffers(1, &_main_framebuffer);

    drag_node = nullptr;
    drag_pin = nullptr;

    _bp_manager = bp;
    Terminal_Node* vn = new Terminal_Node(_bp_manager->get_vert_pin_data(), ++current_id, ImVec2(300, 300));
    nodes.insert(std::make_pair(current_id, (Base_GraphNode*)vn));
    Terminal_Node* fn = new Terminal_Node(_bp_manager->get_frag_pin_data(), ++current_id, ImVec2(300, 500));    
    nodes.insert(std::make_pair(current_id, (Base_GraphNode*)fn));
    _bp_manager->set_terminal_nodes(vn, fn);
    std::cout << "TRY TO DRAW UP TERMINAL" << std::endl;

    vn->generate_intermed_image();
    vn->draw_to_intermed(_main_framebuffer, _bp_manager, param_datas);

    fn->generate_intermed_image();
    fn->draw_to_intermed(_main_framebuffer, _bp_manager, param_datas);

    search_buf[0] = 0;
    img_buf[0] = 0;

    node_factory.read_builtin_file("data/builtin_glsl_funcs.txt");
}

Base_GraphNode* SS_Graph::get_node(int id) {
    auto it = nodes.find(id);
    if (it == nodes.end()) return nullptr;
    return it->second;
}

/* DELETE a node with id from the graph and disconnect all pins attached to it. */
bool SS_Graph::delete_node(int id) {
    auto it = nodes.find(id);
    if (it == nodes.end()) return false;
    if (!it->second->can_be_deleted()) return false;
    if (it->second == selected_node) selected_node = nullptr;
    if (it->second == drag_node) { drag_node = nullptr; drag_pin = nullptr; }

    Base_GraphNode* n = it->second;
    disconnect_all_pins(n);
    nodes.erase(it);
    if (it->second->get_node_type() == NODE_PARAM) {
        param_datas[0]->remove_node_type(it->second->_id);
    }
    delete n; // no LEAKS!

    return true;
}


// returns TRUE if DAG violation
bool SS_Graph::check_for_dag_violation(Base_InputPin* in_pin, Base_OutputPin* out_pin) {
    std::queue<Base_GraphNode*> n_queue;
    Base_GraphNode* n;
    n_queue.push(in_pin->owner);
    while (!n_queue.empty()) {
        n = n_queue.front();
        n_queue.pop();
        if (n == out_pin->owner) return true;
        for (int p = 0; p < n->num_output; ++p)
            for (int o = 0; o < n->output_pins[p].output.size(); ++o)
                n_queue.push(n->output_pins[p].output[o]->owner);
    }
    return false;
}

// 0 for input change needed, 1 for no change needed, 2 for output changed needed
// -1 for failed
bool SS_Graph::pins_connectable(Base_InputPin* in_pin, Base_OutputPin* out_pin) {

    bool in_accepeted = in_pin->owner->can_connect_pins(in_pin, out_pin);
    bool out_accepeted = in_pin->owner->can_connect_pins(in_pin, out_pin);
    bool dag_maintained = !check_for_dag_violation(in_pin, out_pin);
    std::cout << in_accepeted << " " << out_accepeted << " " << dag_maintained << std::endl;
    return in_accepeted && out_accepeted && dag_maintained;
}

bool SS_Graph::connect_pins(Base_InputPin* in_pin, Base_OutputPin* out_pin) {
    if (!pins_connectable(in_pin, out_pin)) return false;

    if (in_pin->input)
        disconnect_pins(in_pin, in_pin->input);

    GLSL_TYPE intersect_type = in_pin->type.intersect_copy(out_pin->type);
    in_pin->owner->propogate_gentype_in_subgraph(in_pin, intersect_type.type_flags & GLSL_LenMask);
    out_pin->owner->propogate_gentype_in_subgraph(out_pin, intersect_type.type_flags & GLSL_LenMask);
    in_pin->owner->propogate_build_dirty();

    in_pin->input = out_pin;
    out_pin->output.push_back(in_pin);
    in_pin->owner->inform_of_connect(in_pin, out_pin);
    out_pin->owner->inform_of_connect(in_pin, out_pin);
    
    return true;
}


bool SS_Graph::disconnect_pins(Base_InputPin* in_pin, Base_OutputPin* out_pin, bool reprop) {
    in_pin->input = nullptr;
    auto ptr = std::find(out_pin->output.begin(), out_pin->output.end(), in_pin);
    out_pin->output.erase(ptr);

    if (reprop) {
        // INPUT PIN PROPOGATION
        if (in_pin->type.type_flags & GLSL_GenType) {
            unsigned int new_in_type = in_pin->owner->get_most_restrictive_gentype_in_subgraph(in_pin);
            std::cout << "GOT MOST RESTRICTIVE FROM IN: " << new_in_type << " - " << SS_Parser::type_to_string(GLSL_TYPE(GLSL_Float | new_in_type, 1)) << std::endl;
            in_pin->owner->propogate_gentype_in_subgraph(in_pin, new_in_type);
        }
        // OUTPUT PIN PROPOGATION
        if (out_pin->type.type_flags & GLSL_GenType) {
            unsigned int new_out_type = out_pin->owner->get_most_restrictive_gentype_in_subgraph(out_pin);
            std::cout << "GOT MOST RESTRICTIVE FROM OUT: " << new_out_type << " - " << SS_Parser::type_to_string(GLSL_TYPE(GLSL_Float | new_out_type, 1)) << std::endl;
            out_pin->owner->propogate_gentype_in_subgraph(out_pin, new_out_type);
        }
        std::cout << std::endl;
    }
    in_pin->owner->propogate_build_dirty();
}

bool SS_Graph::disconnect_all_pins(Base_GraphNode* node) {
    for (int i = 0; i < node->num_input; ++i) {
        if (node->input_pins[i].input)
            disconnect_pins(node->input_pins + i, node->input_pins[i].input);
    }

    for (int o = 0; o < node->num_output; ++o) {
        Base_OutputPin* o_pin = node->output_pins + o;
        std::vector<Base_InputPin*> output_COPY(node->output_pins[o].output);
        for (Base_InputPin* i_pin : output_COPY) {
            std::cout << i_pin->type.type_flags << std::endl;
            disconnect_pins(i_pin, o_pin);
        }
    }
    return true;
}

bool SS_Graph::disconnect_all_pins_by_id(int id) {
    auto it = nodes.find(id);
    if (it == nodes.end()) return false;
    return disconnect_all_pins(it->second);
}

void SS_Graph::invalidate_shaders() {
    _current_frag_code.clear();
    _current_vert_code.clear();
}
 
/**
 * Scratch out:
 * 
 * How to store param nodes in list of valid
 * Implementation of deletion
 * Refactor of addition to include adding to param data list
 * Removal of param removes all nodes
 * 
 * 
 * 
 */

void SS_Graph::add_parameter() {
    param_datas.push_back(new Parameter_Data(SS_Float, SS_Vec3, this, 1, ++param_id));
}

void SS_Graph::remove_parameter(int id) {
    auto it = param_datas.begin();
    for (; it != param_datas.end(); ++it)
        if ((*it)->_id == id) break;
    if (it == param_datas.end()) return;

    (*it)->destroy_nodes(this);
    delete (*it);
    param_datas.erase(it);
}

void SS_Graph::inform_of_parameter_change(Parameter_Data* param) {
    std::vector<int>& node_ids = param->param_node_ids;
    for (int id : node_ids) {
        auto node_it = nodes.find(id);
        disconnect_all_pins(node_it->second);
    }
}


void SS_Graph::draw_param_panels() {
    ImGui::Begin("Parameters", 0, ImGuiWindowFlags_NoScrollbar);
    ImGui::BeginChild("Red",  ImGui::GetWindowSize() - ImVec2(0, 50), true, ImGuiWindowFlags_HorizontalScrollbar);
    for (Parameter_Data* p_data : param_datas) {
        p_data->draw(this);
    }
    ImGui::EndChild();

    if (ImGui::Button("Add Param")) {
        add_parameter();
    }
    ImGui::End();
}

unsigned int try_to_gen_texture(const char* img_file) {
    unsigned int texture = 0;
    int width, height, nrChannels;
    unsigned char *data = stbi_load(img_file, &width, &height, &nrChannels, STBI_rgb);
    if (data)
    {
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        // set the texture wrapping/filtering options (on the currently bound texture object)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);	
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        // load and generate the texture
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
        std::cout << "ADDING IMAGE " << texture << " OF " << width << " , " << height << " with " << nrChannels << std::endl;
    } else {
        std::cout << "Failed to load texture" << std::endl;
    }
    stbi_image_free(data);
    return texture;  
}

void SS_Graph::draw_image_loader() {
    ImGui::Begin("Image Loader", 0, ImGuiWindowFlags_NoScrollbar);
    
    ImGui::BeginChild("ImgLoads",  ImGui::GetWindowSize() - ImVec2(0, 50), true, ImGuiWindowFlags_HorizontalScrollbar);
    float x_size = fmax(ImGui::GetWindowSize().x - 50, 10);
    for (auto it = images.begin(); it != images.end(); ) {
        unsigned int tex = it->first;
        ImGui::Text("%s ||| ID=%d", it->second.c_str(), tex);
        ImGui::SameLine();
        bool queued_delete = ImGui::Button(("Delete###" + std::to_string(tex)).c_str());
        ImGui::Image((void*)(intptr_t)tex, ImVec2(x_size, x_size));
        if (queued_delete) {
            glDeleteTextures(1, &tex);
            it = images.erase(it);
        } else ++it;
    }
    ImGui::EndChild();

    ImGui::InputText("###Input Image", img_buf, 256);
    ImGui::SameLine();
    if (ImGui::Button("Add Image")) {
        unsigned int tex = try_to_gen_texture(img_buf);
        if (tex > 0) {
            images.push_back(std::make_pair(tex, std::string(img_buf)));
        }
    }
    ImGui::End();
}

void SS_Graph::handle_input() {
    //  MENU
    
    ImGui::SetNextWindowSize(ImVec2(250, 400));
    if (ImGui::BeginPopupContextWindow())
    {
        ImVec2 add_pos = ImGui::GetItemRectMin();
        if (drag_node)
            drag_node->_old_pos = drag_node->_pos = drag_node->_old_pos + ImGui::GetMouseDragDelta(0);
        drag_node = nullptr;
        drag_pin = nullptr;

        // SEARCH BAR
        ImGui::InputText("Search", search_buf, 256);
        // BUILTIN
        auto built_node_data_list = node_factory.get_matching_builtin_nodes(std::string(search_buf));
        if (built_node_data_list.size() > 0) ImGui::Text("Builtin:");
        for (auto nd : built_node_data_list) {
            if (ImGui::Button(nd._name.c_str())) {
                Builtin_GraphNode* n = node_factory.build_builtin_node(nd, ++current_id, 
                add_pos - (_pos_offset + _drag_pos_offset), _main_framebuffer);
                nodes.insert(std::make_pair(current_id, (Base_GraphNode*)n));
                ImGui::CloseCurrentPopup(); ImGui::EndPopup();
                return;
            }
        }
        // CONSTANT
        auto const_node_data_list = node_factory.get_matching_constant_nodes(std::string(search_buf));
        if (const_node_data_list.size() > 0) ImGui::Text("CONSTANT:");
        for (auto nd : const_node_data_list) {
            if (ImGui::Button(nd._name.c_str())) {
                Constant_Node* n = node_factory.build_constant_node(nd, ++current_id, 
                add_pos - (_pos_offset + _drag_pos_offset), _main_framebuffer);
                nodes.insert(std::make_pair(current_id, (Base_GraphNode*)n));
                ImGui::CloseCurrentPopup(); ImGui::EndPopup();
                return;
            }
        }
        // VECTOR OPS
        auto vec_node_data_list = node_factory.get_matching_vector_nodes(std::string(search_buf));
        if (vec_node_data_list.size() > 0) ImGui::Text("VECTOR OPS:");
        for (auto nd : vec_node_data_list) {
            if (ImGui::Button(nd._name.c_str())) {
                Vector_Op_Node* n = node_factory.build_vec_op_node(nd, ++current_id, 
                add_pos - (_pos_offset + _drag_pos_offset), _main_framebuffer);
                nodes.insert(std::make_pair(current_id, (Base_GraphNode*)n));
                ImGui::CloseCurrentPopup(); ImGui::EndPopup();
                return;
            }
        }
        // PARAMS
        std::vector<Parameter_Data*>& param_node_data_list = node_factory.get_matching_param_nodes(std::string(search_buf), param_datas); // param data managed by graph
        if (param_node_data_list.size() > 0) ImGui::Text("PARAMETERS:");

        for (Parameter_Data* nd : param_node_data_list) {
            if (ImGui::Button(nd->_param_name + 2)) {
                Param_Node* n = node_factory.build_param_node(nd, ++current_id, 
                add_pos - (_pos_offset + _drag_pos_offset), _main_framebuffer);
                nd->add_param_node(current_id);

                nodes.insert(std::make_pair(current_id, (Base_GraphNode*)n));
                nd->param_node_ids.push_back(current_id);
                ImGui::CloseCurrentPopup(); ImGui::EndPopup();
                return;
            }
        }
        // BOILERPLATE
         auto bp_node_data_list = node_factory.get_matching_boilerplate_nodes(std::string(search_buf), _bp_manager);
        if (bp_node_data_list.size() > 0) ImGui::Text("DEFAULTS:");
        for (auto nd : bp_node_data_list) {
            if (ImGui::Button(nd._name.c_str())) {
                Boilerplate_Var_Node* n = node_factory.build_boilerplate_var_node(
                    nd, _bp_manager, ++current_id, 
                    add_pos - (_pos_offset + _drag_pos_offset), _main_framebuffer);
                nodes.insert(std::make_pair(current_id, (Base_GraphNode*)n));
                ImGui::CloseCurrentPopup(); ImGui::EndPopup();
                return;
            }
        }
        ImGui::EndPopup();
        return;
    } else {
        search_buf[0] = 0;
    }

    // DRAGS
    ImVec2 m_pos = ImGui::GetMousePos();
    int hover_id = -1;
    for (auto n : nodes) {
        if (n.second->is_hovering(m_pos - (_pos_offset + _drag_pos_offset))) {
            hover_id = n.first;
            break;
        }
    }

    Base_GraphNode* hover_node = hover_id != -1 ? nodes[hover_id] : nullptr;
    Base_Pin* hover_pin = hover_node ? hover_node->get_hovered_pin(m_pos - (_pos_offset + _drag_pos_offset)) : nullptr;
    bool display_button_hovered = hover_node ? hover_node->is_display_button_hovered_over(m_pos - (_pos_offset + _drag_pos_offset)) : false;
    if (display_button_hovered) {
        ImGui::BeginTooltip();
        ImGui::Text("DISPLAY INTERMED");
        ImGui::EndTooltip();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Delete) && hover_id != -1) {
        if (hover_pin && hover_pin->has_connections()) {
            hover_pin->disconnect_all_from(this);
        } else {
            delete_node(hover_id);
        }
        selected_node =  nullptr;
        drag_node = nullptr;
        drag_pin = nullptr;
        hover_pin = nullptr;
        hover_node = nullptr;
        return;
    }

    bool b_space = ImGui::IsKeyDown(ImGuiKey_Space);
    if (ImGui::IsMouseClicked(0) && hover_id != -1 && !b_space) {
        selected_node = hover_node;
        if (hover_pin)
            drag_pin = hover_pin;
        else if (!display_button_hovered)
            drag_node = hover_node;
    } else if (ImGui::IsMouseClicked(0) && b_space) {
        _screen_dragging = true;
    }

    if (ImGui::IsMouseDragging(0)) {
        if (!_screen_dragging) {
            if (drag_node) {
                drag_node->_pos = drag_node->_old_pos + ImGui::GetMouseDragDelta(0);
            }
        } else {
            _drag_pos_offset = ImGui::GetMouseDragDelta(0);
        }
    }

    if (ImGui::IsMouseReleased(0) && !_screen_dragging) {
        if (hover_node) {
            if (display_button_hovered) {
                selected_node->toggle_display();
            }
        }
        if (drag_node)
            drag_node->_old_pos = drag_node->_pos = drag_node->_old_pos + ImGui::GetMouseDragDelta(0);

        if (drag_pin && hover_pin) {
            if (drag_pin->bInput && !hover_pin->bInput)
                connect_pins((Base_InputPin*)drag_pin, (Base_OutputPin*)hover_pin);
            else if (!drag_pin->bInput && hover_pin->bInput)
                connect_pins((Base_InputPin*)hover_pin, (Base_OutputPin*)drag_pin);
        }
        drag_node = nullptr;
        drag_pin = nullptr;
    } else if (ImGui::IsMouseReleased(0) && _screen_dragging) {
        _pos_offset = _pos_offset + _drag_pos_offset;
        _drag_pos_offset = ImVec2(0, 0);
        _screen_dragging = false;
    }
}

void SS_Graph::draw_controls() {
    if (!_controls_open) return;
    ImGui::Begin("CONTROLS", &_controls_open);
    ImGui::Text(R"(MENU BUTTONS: SEE TOOLTIPS
LEFT CLICK : SELECT AND DRAG NODE OR PIN
RIGHT CLICK: OPEN NODE ADD MENU
SPACEBAR : ACTIVATE CAMERA DRAG (HOLD LEFT CLICK)
DELETE: DELETE HOVERED NODE OR PIN)");
    ImGui::End();
}

bool SS_Graph::draw_saving_window() {
    bool ret_b = true;
    ImGui::SetNextWindowFocus();
    ImGui::Begin("Save LOCATION");
    ImGui::InputText("SAVE LOCATION", save_buf, 128);
    ImGui::InputText("FRAG NAME", save_buf + 128, 64);
    ImGui::InputText("VERT NAME", save_buf + 192, 64);
    if (ImGui::Button("SAVE GRAPH CODE")) {
        std::ofstream frag_oss(std::string(save_buf) + "/" + std::string(save_buf + 128));
        std::ofstream vert_oss(std::string(save_buf) + "/" + std::string(save_buf + 192));
        Construct_Text_For_Vert(_bp_manager->get_terminal_vert_node());
        Construct_Text_For_Frag(_bp_manager->get_terminal_frag_node());
        
        if (_current_frag_code.empty())
            frag_oss << _bp_manager->get_default_frag_shader() << std::endl;
        else
            frag_oss << _current_frag_code << std::endl; 
        if (_current_vert_code.empty())
            vert_oss << _bp_manager->get_default_vert_shader() << std::endl;
        else
            vert_oss << _current_vert_code << std::endl;
        ret_b = false;
    }
    if (ImGui::Button("CLOSE WITH SAVE"))
        ret_b = false;
    ImGui::End();
    return ret_b;
}

bool SS_Graph::draw_credits() {
    ImGui::Begin("CREDITS", &_credits_up);
    ImGui::Text(R"(LEAD PROGRAMMER: Jay Idema
A MASSIVE THANKS TO:
Professor Eric Ameres
Joey de Vries, Author of LearnOpenGL, from which a good handful of PBR-like shader code was used
Omar Cornut for Dear IMGUI)");
    ImGui::End();
    return _credits_up;
}

void handle_menu_tooltip(const char* tip) {
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text(tip);
        ImGui::EndTooltip();
    }
}

void SS_Graph::draw_menu_buttons() {
    if (ImGui::BeginMenuBar()) {
        // ImGui::Tooltip("Build the vertex shader and link it to the current fragment shader");
        if (ImGui::Button("BUILD VERTEX SHADER")) {
            if (_bp_manager->get_terminal_vert_node()) {
                std::cout << "BUILDING VERT" << std::endl;
                Construct_Text_For_Vert(_bp_manager->get_terminal_vert_node());
            }
        }
        handle_menu_tooltip("Build and link the vertex shader");
        if (ImGui::Button("BUILD FRAG SHADER")) {
            if (_bp_manager->get_terminal_frag_node()) {
                std::cout << "BUILDING FRAG" << std::endl;
                Construct_Text_For_Frag(_bp_manager->get_terminal_frag_node());
            }
        }
        handle_menu_tooltip("Build and link the fragment shader");
        if (ImGui::Button("SAVE NODES")) {
            _is_saving = true;
            sprintf(save_buf, ".");
            sprintf(save_buf + 128, "frag.glsl");
            sprintf(save_buf + 192, "vert.glsl");
        }
        handle_menu_tooltip("Save out the source code");
        if (ImGui::Button("SHOW CONTROLS"))
            _controls_open = !_controls_open;
        if (ImGui::Button("SHOW CREDITS"))
            _credits_up = !_credits_up;
    }
    ImGui::EndMenuBar();
}

void SS_Graph::draw() {
    if (_is_saving)
        _is_saving = draw_saving_window();
    if (_credits_up)
        _credits_up = draw_credits();
        
    draw_param_panels();
    draw_node_context_panel();
    draw_image_loader();
    draw_controls();
    
    // drawn nodes, may want to decrease view size
    for (auto n_it : nodes) {
        if (n_it.second->is_display_up)
            n_it.second->draw_to_intermed_no_recompile(_main_framebuffer, param_datas);
    }

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(2000, 2000));
    ImGui::Begin("SHADER SCULPTER", 0, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_MenuBar);
    draw_menu_buttons();
    
    if (!_is_saving)
        handle_input();
    ImDrawList* dl = ImGui::GetWindowDrawList(); 
    for (auto p_node : nodes)
        p_node.second->set_bounds(1);
    for (auto p_node : nodes)
        p_node.second->draw(dl, _pos_offset + _drag_pos_offset, p_node.second == selected_node);
    for (auto p_node : nodes)
        p_node.second->draw_output_connects(dl, _pos_offset + _drag_pos_offset);
    if (drag_pin) {
        float r;
        dl->AddLine(drag_pin->get_pin_pos(3, 2, &r) + _pos_offset + _drag_pos_offset, ImGui::GetMousePos(), 0xffffffff);
    }

    ImGui::End();
}


void SS_Graph::draw_node_context_panel() {
    ImGui::Begin("Node Context Panel");
    ImGui::Text("NODE:");
    if (selected_node) {
        ImGui::Text(selected_node->_name.c_str());
        if (selected_node->get_node_type() == NODE_CONSTANT) {
            Constant_Node* const_node = (Constant_Node*)selected_node;
            
            if (const_node->_data_type == SS_Float) {
                float* f_ptr = (float*)const_node->_data;
                switch (const_node->_data_gen) {
                    case SS_Scalar: ImGui::InputFloat("Input Scalar", f_ptr); break;
                    case SS_Vec2: ImGui::InputFloat2("Input Vec2", f_ptr); break;
                    case SS_Vec3: ImGui::InputFloat3("Input Vec3", f_ptr); break;
                    case SS_Vec4: ImGui::InputFloat4("Input Vec4", f_ptr); break;
                }
            }
        } else {
            ImGui::Text("NOTHING FOR NON_CONSTANT");
        }
    }
    ImGui::End();
}


/************************************************
 * *********************CONSTRUCTION **************************/


inline bool SS_Graph::all_output_from_node_processed(Base_GraphNode* node, 
    const std::unordered_set<Base_GraphNode*>& processed,
    std::unordered_set<Base_GraphNode*>& down_wind) {
    for (int p = 0; p < node->num_output; ++p) {
        for (int o = 0; o < node->output_pins[p].output.size(); ++o) {
            if (processed.find(node->output_pins[p].output[o]->owner) == processed.end()
                && down_wind.find(node->output_pins[p].output[o]->owner) != down_wind.end())
                return false; // if there is a node we need to process which is actually down wind of the root (USED BY IT)
        }
    }
    return true;
}

void SS_Graph::get_down_wind_nodes(Base_GraphNode* root, std::unordered_set<Base_GraphNode*>& down_wind) {
    std::queue<Base_GraphNode*> work_queue;
    work_queue.push(root);
    while (work_queue.size() != 0) {
        Base_GraphNode* n = work_queue.front();
        work_queue.pop();

        down_wind.insert(n);
        for (int i = 0; i < n->num_input; ++i) {
            Base_InputPin* i_pin = n->input_pins + i;
            if (i_pin->input)
                work_queue.push(i_pin->input->owner);
        }
    }
}


void SS_Graph::Construct_Priority_list_from_tree(Base_GraphNode* root, std::vector<Base_GraphNode*>& out) {

    std::stack<Base_GraphNode*> final_stack;
    std::queue<Base_GraphNode*> work_queue;
    std::unordered_set<Base_GraphNode*> processed;
    std::unordered_set<Base_GraphNode*> down_wind;
    Base_GraphNode* n;

    
    get_down_wind_nodes(root, down_wind);
    work_queue.push(root);
    while (work_queue.size() != 0) {
        n = work_queue.front();
        std::cout << "going for " << n->_name << std::endl;
        work_queue.pop();
        
        if (all_output_from_node_processed(n, processed, down_wind)) {
            for (int p = 0; p < n->num_input; ++p) {
                if (n->input_pins[p].input)
                    work_queue.push(n->input_pins[p].input->owner);
            }

            if (processed.find(n) != processed.end()) continue;
            final_stack.push(n);
            std::cout << "PUSHING " << n->_name << std::endl;
            processed.insert(n);
        }
    }

    // 
    while (final_stack.size() != 0) {
        n = final_stack.top();
        final_stack.pop();
        out.push_back(n);
    }
}

void SS_Graph::generate_frag_intermed_code(std::string frag_intermed_code, Base_GraphNode* node) {
    frag_intermed_code = "// THIS IS INTERMED\n\n" + frag_intermed_code;
    frag_intermed_code += "gl_FragColor = ";
    frag_intermed_code += SS_Parser::convert_output_to_color_str(node->request_output(0), node->output_pins->type) + ";";
    frag_intermed_code += "\n}\n";
    std::cout << "SETTING TO " << frag_intermed_code << std::endl;
    node->set_shader_intermed(
        frag_intermed_code, 
        _current_vert_code.empty() ? _bp_manager->get_default_vert_shader() : _current_vert_code
    );
}


void SS_Graph::Construct_Text_For_Vert(Base_GraphNode* root) {
std::vector<Base_GraphNode*> ordered;
    Construct_Priority_list_from_tree(root, ordered);

    std::unordered_map<Base_Pin*, std::string> pin_return_vars;
    std::unordered_set<std::string> processed_funcs;
    std::unordered_map<Base_GraphNode*, std::string> embedded; 
    std::vector<std::string> process;

    _current_vert_code.clear();

    // INTERMED FRAG ISS
    std::string intermed_iss;
    intermed_iss += _bp_manager->get_frag_init_boilerplate_declares();
    intermed_iss += '\n';
    for (Parameter_Data* p_data : param_datas) {
        intermed_iss += "uniform ";
        intermed_iss += SS_Parser::type_to_string(SS_Parser::constant_type_to_type(p_data->_gentype, p_data->_type));
        intermed_iss += " ";
        intermed_iss += std::string(p_data->_param_name) += ";\n";
    }
    intermed_iss += "\nvoid main() {\n";
    intermed_iss += _bp_manager->get_frag_init_boilerplate_code();
    intermed_iss += '\n';

    // MAIN ISS
    std::stringstream iss;
    iss << _bp_manager->get_vert_init_boilerplate_declares();

    iss << std::endl;
    for (Parameter_Data* p_data : param_datas) {
        iss << "uniform "
            << SS_Parser::type_to_string(SS_Parser::constant_type_to_type(p_data->_gentype, p_data->_type))
            << " " << (p_data->_param_name) << ";" << std::endl;
    }
    
    iss << std::endl << "void main() {" << std::endl;
    iss << _bp_manager->get_vert_init_boilerplate_code() << std::endl;

    std::cout << "Finished VERT init boilerplate" << std::endl;
    // go from least to most dependencies
    for (int n = 0; n < ordered.size() - 1; ++n) {
        Base_GraphNode* node = ordered[n];
        std::cout << "Trying with " << node->_name << "_" << node->_id << std::endl;

        std::string new_line = node->process_for_code();
        iss << '\t' << new_line;
        iss << "  // Node " << node->_name << ", id=" << node->_id << std::endl;
        intermed_iss += ("\t" + new_line);

        if (node->can_draw_intermed_image())
            generate_frag_intermed_code(intermed_iss, node);
    }

    iss << _bp_manager->get_vert_terminal_boilerplate_code() << std::endl;
    iss << "}" << std::endl;
    std::cout << "Attempting to set intermed" << std::endl;
    _bp_manager->get_terminal_vert_node()->set_shader_intermed(
        _current_frag_code.empty() ? _bp_manager->get_default_frag_shader() : _current_frag_code,
        iss.str());
    _current_vert_code = iss.str();
    std::ofstream off("VERT_TEMP.glsl");
    off << _current_vert_code << std::endl;
    off.close();

    // DRAW ALL INTERMEDIATES WITH DISPLAY UP
    for (auto n : ordered) {
        if (n->can_draw_intermed_image())
            n->draw_to_intermed(_main_framebuffer, _bp_manager, param_datas);
    }

    std::vector<Base_GraphNode*> updated_frag_nodes;
    Construct_Priority_list_from_tree(_bp_manager->get_terminal_frag_node(), updated_frag_nodes);
    for (auto n : updated_frag_nodes) {
        if (n->can_draw_intermed_image())
            n->update_vertex_shader_only(_current_vert_code);
    }
}

void SS_Graph::Construct_Text_For_Frag(Base_GraphNode* root) {
    std::vector<Base_GraphNode*> ordered;
    Construct_Priority_list_from_tree(root, ordered);

    std::unordered_map<Base_Pin*, std::string> pin_return_vars;
    std::unordered_set<std::string> processed_funcs;
    std::unordered_map<Base_GraphNode*, std::string> embedded; 
    std::vector<std::string> process;
    
    _current_frag_code.clear();
    _current_frag_code += _bp_manager->get_frag_init_boilerplate_declares();
    _current_frag_code += '\n';

    for (Parameter_Data* p_data : param_datas) {
        _current_frag_code += "uniform ";
        _current_frag_code += SS_Parser::type_to_string(SS_Parser::constant_type_to_type(p_data->_gentype, p_data->_type));
        _current_frag_code += " ";
        _current_frag_code += std::string(p_data->_param_name) += ";\n";
    }
    
    _current_frag_code += "\nvoid main() {\n";
    _current_frag_code += _bp_manager->get_frag_init_boilerplate_code();
    _current_frag_code += '\n';

    std::cout << "Finished init boilerplate" << std::endl;
    // go from least to most dependencies
    for (int n = 0; n < ordered.size() - 1; ++n) {
        Base_GraphNode* node = ordered[n];

        std::string new_line = node->process_for_code();
        _current_frag_code += '\t';
        _current_frag_code += new_line;
        _current_frag_code += ("  // Node " + node->_name + ", id=" + std::to_string(node->_id) + '\n');
        if (node->can_draw_intermed_image())
            generate_frag_intermed_code(_current_frag_code, node);
    }

    _current_frag_code += _bp_manager->get_frag_terminal_boilerplate_code();
    _current_frag_code += "\n}\n";
    _bp_manager->get_terminal_frag_node()->set_shader_intermed(
        _current_frag_code,
        _current_vert_code.empty() ? _bp_manager->get_default_vert_shader() : _current_vert_code
    );
    {
        std::ofstream off("FRAG_TEMP.glsl");
        off << _current_frag_code << std::endl;
        off.close();
    }
    _bp_manager->get_terminal_vert_node()->update_frag_shader_only(_current_frag_code);

    // DRAW ALL INTERMEDIATES WITH DISPLAY UP
    for (auto n : ordered) {
        if (n->can_draw_intermed_image()) {
            std::cout << "TRYING TO DRAW SHADERS FOR " << n->_name << "_" << n->_id << std::endl;
            n->draw_to_intermed(_main_framebuffer, _bp_manager, param_datas);
        }
    }
}


///// PARAMETER DATA

Parameter_Data::Parameter_Data(GRAPH_PARAM_TYPE type, GRAPH_PARAM_GENTYPE gentype, SS_Graph* g, unsigned as, int id) {
    _graph = g;
    
    sprintf(_param_name, "P_PARAM_%i", id);
    _type = type;
    _gentype = gentype;
    _id = id;
    arr_size = as;
    make_data();
}

void Parameter_Data::make_data(bool reset_mem) {

    if (reset_mem) {
        memset(data_container, 0, 8 * 16 * sizeof(char));
    }
    
    for (int id : param_node_ids) {
        _graph->disconnect_all_pins_by_id(id);
        Param_Node* pn = (Param_Node*)_graph->get_node(id);
        pn->update_type_from_param(SS_Parser::constant_type_to_type(_gentype, _type, arr_size));
        pn->_name = _param_name;
    }
    _graph->invalidate_shaders();
}

bool Parameter_Data::update_gentype(GRAPH_PARAM_GENTYPE gentype) {
    if (gentype == _gentype) return false;
    _gentype = gentype;
    make_data(false);
    return true;
}
bool Parameter_Data::update_type(GRAPH_PARAM_TYPE type) {
        if (type == _type) return false;
        _type = type;
        if (type == SS_Texture2D || type == SS_TextureCube) {
            std::cout << "MAKING GENTYPE TO SCALAR" << std::endl;
            _gentype = SS_Scalar;
        }
        
        make_data();
        return true;
}
bool Parameter_Data::update_array_size(int size) {
        if (size == arr_size) return false;
        arr_size = size;
        make_data();
        return true;
}

bool Parameter_Data::is_mat() {
    return SS_MAT & _gentype;
}
int Parameter_Data::get_len() {
    switch (_gentype) {
        case SS_Scalar: return 1 * arr_size;
        case SS_Vec2: return arr_size * 2;
        case SS_Vec3: return arr_size * 3;
        case SS_Vec4: return arr_size * 4;
        case SS_Mat2: return arr_size * 4;
        case SS_Mat3: return arr_size * 9;
        case SS_Mat4: return arr_size * 16;
    }
    return 0;
}

int Parameter_Data::type_to_dropbox_index() {
    return (int)_type;
}
int Parameter_Data::gentype_to_dropbox_index() {
    return (int)_gentype;
}

void Parameter_Data::update_name(const char* new_param_name) {
    if (strcmp(new_param_name, _param_name) != 0) {
        strcpy(_param_name, new_param_name);
        for (int id : param_node_ids) {
            std::cout << "changing name for " << id << std::endl;
            Param_Node* pn = (Param_Node*)_graph->get_node(id);
            pn->_name = _param_name;
            std::cout << "VICTORY" << std::endl;
        }
        _graph->invalidate_shaders();
    }
}


bool is_valid_char(char c) {
    return c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

void Parameter_Data::draw(SS_Graph* graph) {
    const char* type_strs[] { "Float", "Double", "Int", "Texture2D", "TextureCube" };
    const char* gentype_strs[] { "Scalar", "Vec2", "Vec3", "Vec4", "Mat2", "Mat3", "Mat4" };
    char name[32];
    char new_param_name[64];
    strcpy(new_param_name, _param_name);
    sprintf(name, "##INPUT_NAME%i", _id);
    ImGui::BeginGroup();
    ImGui::InputText(name, new_param_name + 2, 62);
    { // remove invalid characters
        char* fixer = new_param_name;
        do { if (!is_valid_char(*fixer)) *fixer = '_'; } while (*(++fixer) != '\0');
    }
    update_name(new_param_name);
    sprintf(name, "##TABLE%i", _id);
    ImGui::BeginTable(name, 2);
    sprintf(name, "##TYPE%i", _id);
    ImGui::TableNextColumn();
    ImGui::BeginGroup();
    ImGui::Text("TYPE");
    ImGui::PushItemWidth(-1);
    int current_type = type_to_dropbox_index();
    ImGui::ListBox(name, &current_type, type_strs, 5);
    ImGui::PopItemWidth();
    update_type((GRAPH_PARAM_TYPE)current_type);
    ImGui::EndGroup();
    
    ImGui::TableNextColumn();
    
    sprintf(name, "##GENTYPE%i", _id);
    bool disable_gentype = _type == SS_Texture2D || _type == SS_TextureCube;
    ImGui::BeginGroup();
    ImGui::BeginDisabled(disable_gentype);
    ImGui::Text("GENTYPE");
    ImGui::PushItemWidth(-1);
    int current_gentype = gentype_to_dropbox_index();
    ImGui::ListBox(name, &current_gentype, gentype_strs, 7);
    ImGui::PopItemWidth();
    update_gentype((GRAPH_PARAM_GENTYPE)current_gentype);
    ImGui::EndDisabled();
    ImGui::EndGroup();
    
    ImGui::EndTable();
    std::string param_name_str = (_param_name + 2);
    ImGuiDataType data_type = ImGuiDataType_Float;
    int data_size = sizeof(float);
    switch (_type) {
        case SS_Float: data_type = ImGuiDataType_Float; data_size = sizeof(float); break;
        case SS_Double: data_type = ImGuiDataType_Double; data_size = sizeof(double); break;
        case SS_Int: 
        case SS_Texture2D:
        case SS_TextureCube:
        data_type = ImGuiDataType_S32; data_size = sizeof(int); break;
    }
    switch (_gentype) {
        case SS_Scalar:
            ImGui::InputScalar(("Scalar-In###" + param_name_str).c_str(), data_type, data_container); break;
        case SS_Vec2:
            ImGui::InputScalarN(("Vec2-In###" + param_name_str).c_str(), data_type, data_container, 2); break;
        case SS_Vec3:
            ImGui::InputScalarN(("Vec3-In###" + param_name_str).c_str(), data_type, data_container, 3); break;
        case SS_Vec4:
            ImGui::InputScalarN(("Vec4-In###" + param_name_str).c_str(), data_type, data_container, 4); break;
        case SS_Mat2:
            ImGui::InputScalarN(("Vec2-In###" + param_name_str + "111").c_str(), data_type, data_container, 2);
            ImGui::InputScalarN(("Vec2-In###" + param_name_str + "222").c_str(), data_type, data_container + data_size * 2, 2); break;
        case SS_Mat3:
            ImGui::InputScalarN(("Mat3-In###" + param_name_str + "111").c_str(), data_type, data_container, 3);
            ImGui::InputScalarN(("Mat3-In###" + param_name_str + "222").c_str(), data_type, data_container + (data_size*3), 3);
            ImGui::InputScalarN(("Mat3-In###" + param_name_str + "333").c_str(), data_type, data_container + (data_size*6), 3); break;
        case SS_Mat4:
            ImGui::InputScalarN(("Mat4-In###" + param_name_str + "111").c_str(), data_type, data_container, 4);
            ImGui::InputScalarN(("Mat4-In###" + param_name_str + "222").c_str(), data_type, data_container + (data_size*4), 4);
            ImGui::InputScalarN(("Mat4-In###" + param_name_str + "333").c_str(), data_type, data_container + (data_size*8), 4);
            ImGui::InputScalarN(("Mat4-In###" + param_name_str + "444").c_str(), data_type, data_container + (data_size*12), 4); break;
    }

    sprintf(name, "REMOVE PARAM %i", _id);
    if (ImGui::Button(name)) {
        graph->remove_parameter(_id);
    }

    ImGui::EndGroup();
    ImVec2 min_vec = ImGui::GetItemRectMin();
    ImVec2 max_vec = ImGui::GetItemRectMax();
    ImGui::GetWindowDrawList()->AddRect(min_vec, max_vec, 0xffffffff);
}

void Parameter_Data::add_param_node(int id) {
    std::cout << "Adding " << id << " to param" << std::endl;
    param_node_ids.push_back(id);
}

void Parameter_Data::remove_node_type(int id) {
    for (auto it = param_node_ids.begin(); it != param_node_ids.end(); )
        if (*it == id) {
            it = param_node_ids.erase(it);
        } else {
            ++it;
        }
}

void Parameter_Data::destroy_nodes(SS_Graph* g) {
    auto param_node_ids_cp = param_node_ids;
    for (int n_id : param_node_ids_cp)
        g->delete_node(n_id);
}
