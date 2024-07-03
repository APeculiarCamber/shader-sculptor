#include "ss_graph.hpp"
#include "stb_image.h"
#include <fstream>
#include <algorithm>
#include <glad/glad.h>

/**
 * @brief Construct a new ss graph::ss graph object
 */
SS_Graph::SS_Graph(SS_Boilerplate_Manager* bp) {
    glGenFramebuffers(1, &_main_framebuffer);

    drag_node = nullptr;
    drag_pin = nullptr;

    _bp_manager = bp;
    auto* vn = new Terminal_Node(_bp_manager->get_vert_pin_data(), ++current_id, ImVec2(300, 300));
    nodes.insert(std::make_pair(current_id, (Base_GraphNode*)vn));
    auto* fn = new Terminal_Node(_bp_manager->get_frag_pin_data(), ++current_id, ImVec2(300, 500));
    nodes.insert(std::make_pair(current_id, (Base_GraphNode*)fn));
    _bp_manager->set_terminal_nodes(vn, fn);
    this->GenerateShaderTextAndPropagate();

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
    DisconnectAllPins(n);
    nodes.erase(it);
    if (it->second->get_node_type() == NODE_PARAM) {
        param_datas[0]->remove_node_type(it->second->_id);
    }
    delete n; // no LEAKS!

    return true;
}


// returns TRUE if DAG violation
bool SS_Graph::CheckForDAGViolation(Base_InputPin* in_pin, Base_OutputPin* out_pin) {
    std::queue<Base_GraphNode*> n_queue;
    Base_GraphNode* n;
    n_queue.push(in_pin->owner);
    while (!n_queue.empty()) {
        n = n_queue.front();
        n_queue.pop();
        if (n == out_pin->owner) return true;
        for (int p = 0; p < n->num_output; ++p)
            for (auto & o : n->output_pins[p].output)
                n_queue.push(o->owner);
    }
    return false;
}

// 0 for input change needed, 1 for no change needed, 2 for output changed needed
// -1 for failed
bool SS_Graph::ArePinsConnectable(Base_InputPin* in_pin, Base_OutputPin* out_pin) {
    bool in_accepeted = in_pin->owner->can_connect_pins(in_pin, out_pin);
    bool out_accepeted = in_pin->owner->can_connect_pins(in_pin, out_pin);
    bool dag_maintained = !CheckForDAGViolation(in_pin, out_pin);
    return in_accepeted && out_accepeted && dag_maintained;
}

bool SS_Graph::ConnectPins(Base_InputPin* in_pin, Base_OutputPin* out_pin) {
    if (!ArePinsConnectable(in_pin, out_pin)) return false;

    if (in_pin->input)
        DisconnectPins(in_pin, in_pin->input);

    GLSL_TYPE intersect_type = in_pin->type.IntersectCopy(out_pin->type);
    in_pin->owner->propogate_gentype_in_subgraph(in_pin, intersect_type.type_flags & GLSL_LenMask);
    out_pin->owner->propogate_gentype_in_subgraph(out_pin, intersect_type.type_flags & GLSL_LenMask);
    in_pin->owner->propogate_build_dirty();

    in_pin->input = out_pin;
    out_pin->output.push_back(in_pin);
    in_pin->owner->inform_of_connect(in_pin, out_pin);
    out_pin->owner->inform_of_connect(in_pin, out_pin);
    
    return true;
}


bool SS_Graph::DisconnectPins(Base_InputPin* in_pin, Base_OutputPin* out_pin, bool reprop) {
    in_pin->input = nullptr;
    auto ptr = std::find(out_pin->output.begin(), out_pin->output.end(), in_pin);
    out_pin->output.erase(ptr);

    if (reprop) {
        // INPUT PIN PROPOGATION
        if (in_pin->type.type_flags & GLSL_GenType) {
            unsigned int new_in_type = in_pin->owner->get_most_restrictive_gentype_in_subgraph(in_pin);
            in_pin->owner->propogate_gentype_in_subgraph(in_pin, new_in_type);
        }
        // OUTPUT PIN PROPOGATION
        if (out_pin->type.type_flags & GLSL_GenType) {
            unsigned int new_out_type = out_pin->owner->get_most_restrictive_gentype_in_subgraph(out_pin);
            out_pin->owner->propogate_gentype_in_subgraph(out_pin, new_out_type);
        }
    }
    in_pin->owner->propogate_build_dirty();
    return true;
}

bool SS_Graph::DisconnectAllPins(Base_GraphNode* node) {
    for (int i = 0; i < node->num_input; ++i) {
        if (node->input_pins[i].input)
            DisconnectPins(node->input_pins + i, node->input_pins[i].input);
    }

    for (int o = 0; o < node->num_output; ++o) {
        Base_OutputPin* o_pin = node->output_pins + o;
        std::vector<Base_InputPin*> output_COPY(node->output_pins[o].output);
        for (Base_InputPin* i_pin : output_COPY) {
            DisconnectPins(i_pin, o_pin);
        }
    }
    return true;
}

bool SS_Graph::disconnect_all_pins_by_id(int id) {
    auto it = nodes.find(id);
    if (it == nodes.end()) return false;
    return DisconnectAllPins(it->second);
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
        DisconnectAllPins(node_it->second);
    }
}


void SS_Graph::draw_param_panels() {
    ImGui::Begin("Parameters", nullptr, ImGuiWindowFlags_NoScrollbar);
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
    } else {
        assert("Failed to generate texture");
    }
    stbi_image_free(data);
    return texture;  
}

void SS_Graph::draw_image_loader() {
    ImGui::Begin("Image Loader", nullptr, ImGuiWindowFlags_NoScrollbar);
    
    ImGui::BeginChild("ImgLoads",  ImGui::GetWindowSize() - ImVec2(0, 50), true, ImGuiWindowFlags_HorizontalScrollbar);
    float x_size = fmax(ImGui::GetWindowSize().x - 50.0f, 10.0f);
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

    ImGui::InputTextWithHint("###Input Image", "Image Filepath", img_buf, 256);
    ImGui::SameLine();
    if (ImGui::Button("Add Image")) {
        unsigned int tex = try_to_gen_texture(img_buf);
        if (tex > 0) {
            images.emplace_back(tex, std::string(img_buf));
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
        if (not built_node_data_list.empty()) ImGui::Text("Builtin:");
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
        if (not const_node_data_list.empty()) ImGui::Text("CONSTANT:");
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
        if (not vec_node_data_list.empty()) ImGui::Text("VECTOR OPS:");
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
        if (not param_node_data_list.empty()) ImGui::Text("PARAMETERS:");

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
        if (not bp_node_data_list.empty()) ImGui::Text("DEFAULTS:");
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
    bool display_button_hovered = hover_node != nullptr &&
                                  hover_node->is_display_button_hovered_over(m_pos - (_pos_offset + _drag_pos_offset));
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
                ConnectPins((Base_InputPin *) drag_pin, (Base_OutputPin *) hover_pin);
            else if (!drag_pin->bInput && hover_pin->bInput)
                ConnectPins((Base_InputPin *) hover_pin, (Base_OutputPin *) drag_pin);
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
RIGHT CLICK: OPEN NODE ADD MENU; USE SEARCH BAR
SPACEBAR : ACTIVATE CAMERA DRAG (HOLD LEFT CLICK)
DELETE: DELETE HOVERED NODE OR PIN)");
    ImGui::End();
}

bool SS_Graph::draw_saving_window() {
    bool bReturn = true;
    ImGui::SetNextWindowFocus();
    ImGui::Begin("Save LOCATION");
    char* saveLocationStr = save_buf;
    char* saveFragStr = save_buf + 128;
    char* saveVertStr = save_buf + 192;
    ImGui::InputText("SAVE LOCATION", saveLocationStr, 128);
    ImGui::InputText("FRAG NAME", saveFragStr, 64);
    ImGui::InputText("VERT NAME", saveVertStr, 64);
    if (ImGui::Button("SAVE GRAPH CODE")) {
        std::ofstream frag_oss(std::string(save_buf) + "/" + std::string(saveFragStr));
        std::ofstream vert_oss(std::string(save_buf) + "/" + std::string(saveVertStr));
        if (frag_oss.good() and vert_oss.good()) {
            frag_oss << _current_frag_code << std::endl;
            vert_oss << _current_vert_code << std::endl;
        } else {
            std::cerr << "WARNING: Couldn't save to " << save_buf << ".\n\tThis directory might not exist." << std::endl;
        }
        bReturn = false;
    }
    if (ImGui::Button("CLOSE WITH SAVE"))
        bReturn = false;
    ImGui::End();
    return bReturn;
}

bool SS_Graph::draw_credits() {
    ImGui::Begin("CREDITS", &_credits_up);
    ImGui::Text(R"(DEVELOPER: Jay Idema

A Massive Thanks To:
Professor Eric Ameres
Joey de Vries, Author of LearnOpenGL, from which a good handful of PBR-like shader code was used
Omar Cornut for Dear IMGUI)");
    ImGui::End();
    return _credits_up;
}

void handle_menu_tooltip(const char* tip) {
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("%s", tip);
        ImGui::EndTooltip();
    }
}

void SS_Graph::draw_menu_buttons() {
    if (ImGui::BeginMenuBar()) {
        handle_menu_tooltip("Build and link the shaders, re-compiles dirty intermediate results");
        if (ImGui::Button("BUILD SHADERS")) {
            this->GenerateShaderTextAndPropagate();
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
            n_it.second->DrawIntermediateResult(_main_framebuffer, param_datas);
    }

    const auto& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, io.DisplaySize.y));
    ImGui::Begin("SHADER SCULPTER", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_MenuBar);
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


void SS_Graph::draw_node_context_panel() const {
    ImGui::Begin("Node Context Panel");
    ImGui::Text("NODE:");
    if (selected_node) {
        ImGui::Text("%s", selected_node->_name.c_str());
        if (selected_node->get_node_type() == NODE_CONSTANT) {
            auto* const_node = (Constant_Node*)selected_node;
            
            if (const_node->_data_type == SS_Float) {
                auto* f_ptr = (float*)const_node->_data;
                switch (const_node->_data_gen) {
                    case SS_Scalar: ImGui::InputFloat("Input Scalar", f_ptr); break;
                    case SS_Vec2: ImGui::InputFloat2("Input Vec2", f_ptr); break;
                    case SS_Vec3: ImGui::InputFloat3("Input Vec3", f_ptr); break;
                    case SS_Vec4: ImGui::InputFloat4("Input Vec4", f_ptr); break;

                    case SS_Mat2:
                    case SS_Mat3:
                    case SS_Mat4:
                    case SS_MAT:
                        ImGui::Text("ERROR:\nTried to contextualize Mat.\nNot yet supported.");
                        break;
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


std::vector<Base_GraphNode*> SS_Graph::ConstructTopologicalOrder(Base_GraphNode* root) {
    std::vector<Base_GraphNode*> topOrder;
    std::stack<Base_GraphNode*> processStack;
    processStack.push(root);

    while (not processStack.empty()) {
        Base_GraphNode* n = processStack.top();
        processStack.pop();

        topOrder.push_back(n);

        for (int p = 0; p < n->num_input; ++p) {
            if (n->input_pins[p].input)
                processStack.push(n->input_pins[p].input->owner);
        }
    }
    std::reverse(topOrder.begin(), topOrder.end());
    return topOrder;
}

void SS_Graph::SetIntermediateCodeForNode(std::string intermedCode, Base_GraphNode* node) {
    std::string output = node->request_output(0);
    if (output.empty()) {
        node->SetShaderCode(_current_frag_code, _current_vert_code);
    } else {
        intermedCode += "gl_FragColor = ";
        intermedCode +=  SS_Parser::convert_output_to_color_str(output, node->output_pins->type) + ";\n}\n";
        node->SetShaderCode(intermedCode, _current_vert_code);
    }
}

void WriteParameterData(std::ostringstream& oss, const std::vector<Parameter_Data*>& params) {
    for (const Parameter_Data* p_data : params) {
        oss << "uniform ";
        oss << SS_Parser::type_to_string(SS_Parser::constant_type_to_type(p_data->_gentype, p_data->_type));
        oss << " ";
        oss << p_data->_param_name << ";\n";
    }
}

void SS_Graph::SetFinalShaderTextByConstructOrders(const std::vector<Base_GraphNode*>& vertOrder,
                                                   const std::vector<Base_GraphNode*>& fragOrder) {
    // MAXIMAL VERTEX BUILD
    {
        std::ostringstream vertIss;
        // -- header
        vertIss << _bp_manager->get_vert_init_boilerplate_declares() << '\n';
        WriteParameterData(vertIss, param_datas);
        // -- main body
        vertIss << "\nvoid main() {\n" << _bp_manager->get_vert_init_boilerplate_code() << '\n';
        for (Base_GraphNode* node: vertOrder) {
            vertIss << "\t" << node->process_for_code() << "  // Node " << node->_name << ", id=" << node->_id << '\n';
        }
        vertIss << _bp_manager->get_vert_terminal_boilerplate_code() << "\n}\n";
        _current_vert_code = vertIss.str();
    }
    // MAXIMAL FRAG BUILD
    {
        std::ostringstream fragIss;
        // -- header
        fragIss << _bp_manager->get_frag_init_boilerplate_declares() << '\n';
        WriteParameterData(fragIss, param_datas);
        // -- main body
        fragIss << "\nvoid main() {\n" << _bp_manager->get_frag_init_boilerplate_code() << '\n';
        for (Base_GraphNode* node: fragOrder) {
            fragIss << "\t" << node->process_for_code() << "  // Node " << node->_name << ", id=" << node->_id << '\n';
        }
        fragIss << _bp_manager->get_frag_terminal_boilerplate_code() << "\n}\n";
        _current_frag_code = fragIss.str();
    }
}

void SS_Graph::PropagateIntermediateVertexCodeToNodes(const std::vector<Base_GraphNode*>& vertOrder) {
    std::ostringstream vertIss;
    // -- header
    vertIss << _bp_manager->get_frag_init_boilerplate_declares() << '\n';
    WriteParameterData(vertIss, param_datas);
    // -- main body
    vertIss << "\nvoid main() {\n" << _bp_manager->get_frag_init_boilerplate_code() << '\n';
    for (Base_GraphNode* node: vertOrder) {
        vertIss << "\t" << node->process_for_code() << "  // Node " << node->_name << ", id=" << node->_id << '\n';
        SetIntermediateCodeForNode(vertIss.str(), node);
    }
    // -- compile
    for (Base_GraphNode* node: vertOrder) {
        node->CompileIntermediateCode(_bp_manager);
    }
}

void SS_Graph::PropagateIntermediateFragmentCodeToNodes(const std::vector<Base_GraphNode*>& fragOrder) {
    std::ostringstream fragIss;
    // -- header
    fragIss << _bp_manager->get_frag_init_boilerplate_declares() << '\n';
    WriteParameterData(fragIss, param_datas);
    // -- main body
    fragIss << "\nvoid main() {\n" << _bp_manager->get_frag_init_boilerplate_code() << '\n';
    for (Base_GraphNode* node: fragOrder) {
        fragIss << "\t" << node->process_for_code() << "  // Node " << node->_name << ", id=" << node->_id << '\n';
        SetIntermediateCodeForNode(fragIss.str(), node);
    }
    // -- compile
    for (Base_GraphNode* node: fragOrder) {
        node->CompileIntermediateCode(_bp_manager);
    }
}

void SS_Graph::GenerateShaderTextAndPropagate() {
    Terminal_Node* vn = _bp_manager->get_terminal_vert_node();
    Terminal_Node* fn = _bp_manager->get_terminal_frag_node();
    std::vector<Base_GraphNode*> vertOrder = ConstructTopologicalOrder(vn);
    std::vector<Base_GraphNode*> fragOrder = ConstructTopologicalOrder(fn);

    SetFinalShaderTextByConstructOrders(vertOrder, fragOrder);

    PropagateIntermediateVertexCodeToNodes(vertOrder);
    PropagateIntermediateFragmentCodeToNodes(fragOrder);
}

///// PARAMETER DATA
Parameter_Data::Parameter_Data(GRAPH_PARAM_TYPE type, GRAPH_PARAM_GENTYPE gentype, SS_Graph* g, unsigned as, int id)
    : _graph(g), _type(type), _gentype(gentype), _id(id), arr_size(as), _param_name {}, data_container{} {
    sprintf(_param_name, "P_PARAM_%i", id);
    make_data();
}

void Parameter_Data::make_data(bool reset_mem) {

    if (reset_mem) {
        memset(data_container, 0, 8 * 16 * sizeof(char));
    }
    
    for (int id : param_node_ids) {
        _graph->disconnect_all_pins_by_id(id);
        auto* pn = (Param_Node*)_graph->get_node(id);
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
            _gentype = SS_Scalar;
        }
        
        make_data();
        return true;
}

__attribute__((unused)) bool Parameter_Data::is_mat() const {
    return SS_MAT & _gentype;
}

__attribute__((unused)) unsigned Parameter_Data::get_len() const {
    switch (_gentype) {
        case SS_Scalar: return 1 * arr_size;
        case SS_Vec2: return arr_size * 2;
        case SS_Vec3: return arr_size * 3;
        case SS_Vec4:
        case SS_Mat2: return arr_size * 4;
        case SS_Mat3: return arr_size * 9;
        case SS_Mat4: return arr_size * 16;
        case SS_MAT:
            break;
    }
    return 0;
}

int Parameter_Data::type_to_dropbox_index() const {
    return (int)_type;
}
int Parameter_Data::gentype_to_dropbox_index() const {
    return (int)_gentype;
}

void Parameter_Data::update_name(const char* new_param_name) {
    if (strcmp(new_param_name, _param_name) != 0) {
        strcpy(_param_name, new_param_name);
        for (int id : param_node_ids) {
            auto* pn = (Param_Node*)_graph->get_node(id);
            pn->_name = _param_name;
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

    std::vector<std::string> labels(4);
    switch (_gentype) {
        case SS_Scalar:
            labels[0] = (disable_gentype ? "Index-In###" : "Scalar-In###") + param_name_str;
            ImGui::InputScalar(labels[0].c_str(), data_type, data_container); break;
        case SS_Vec2:
            labels[0] = ("Vec2-In###" + param_name_str);
            ImGui::InputScalarN(labels[0].c_str(), data_type, data_container, 2); break;
        case SS_Vec3:
            labels[0] = ("Vec2-In###" + param_name_str);
            ImGui::InputScalarN(labels[0].c_str(), data_type, data_container, 3); break;
        case SS_Vec4:
            labels[0] = ("Vec4-In###" + param_name_str);
            ImGui::InputScalarN(labels[0].c_str(), data_type, data_container, 4); break;
        case SS_Mat2:
            labels = { ("Mat2-In###" + param_name_str + "111"), ("Mat2-In###" + param_name_str + "222") };
            ImGui::InputScalarN(labels[0].c_str(), data_type, data_container, 2);
            ImGui::InputScalarN(labels[1].c_str(), data_type, data_container + data_size * 2, 2); break;
        case SS_Mat3:
            labels = { ("Mat3-In###" + param_name_str + "111"), ("Mat3-In###" + param_name_str + "222"), ("Mat3-In###" + param_name_str + "333") };
            ImGui::InputScalarN(labels[0].c_str(), data_type, data_container, 3);
            ImGui::InputScalarN(labels[1].c_str(), data_type, data_container + (data_size*3), 3);
            ImGui::InputScalarN(labels[2].c_str(), data_type, data_container + (data_size*6), 3); break;
        case SS_Mat4:
            labels = { ("Mat4-In###" + param_name_str + "111"), ("Mat4-In###" + param_name_str + "222"), ("Mat4-In###" + param_name_str + "333"), ("Mat4-In###" + param_name_str + "444") };
            ImGui::InputScalarN(labels[0].c_str(), data_type, data_container, 4);
            ImGui::InputScalarN(labels[1].c_str(), data_type, data_container + (data_size*4), 4);
            ImGui::InputScalarN(labels[2].c_str(), data_type, data_container + (data_size*8), 4);
            ImGui::InputScalarN(labels[3].c_str(), data_type, data_container + (data_size*12), 4); break;
        case SS_MAT:
            break;
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
