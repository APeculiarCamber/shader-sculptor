#include "ss_graph.hpp"
#include "stb_image.h"
#include "ss_pins.h"
#include "ss_parser.hpp"
#include "ss_node_factory.hpp"
#include "ss_boilerplate.hpp"
#include <fstream>
#include <algorithm>
#include <stack>

/**
 * @brief Construct a new ss graph::ss graph object
 */
SS_Graph::SS_Graph(SS_Boilerplate_Manager* bp) {
    glGenFramebuffers(1, &_main_framebuffer);

    drag_node = nullptr;
    drag_pin = nullptr;

    _bp_manager = bp;
    auto* vn = new Terminal_Node(_bp_manager->GetTerminalVertPinData(), ++current_id, ImVec2(300, 300));
    nodes.insert(std::make_pair(current_id, vn));
    auto* fn = new Terminal_Node(_bp_manager->GetTerminalFragPinData(), ++current_id, ImVec2(300, 500));
    nodes.insert(std::make_pair(current_id, fn));
    _bp_manager->SetTerminalNodes(vn, fn);
    this->GenerateShaderTextAndPropagate();

    search_buf[0] = 0;
    img_buf[0] = 0;

    // Static load of node factory data, NOTE: ASSUMES GRAPH SINGLETON!
    assert(SS_Node_Factory::InitReadBuiltinFile("data/builtin_glsl_funcs.txt"));
    assert(SS_Node_Factory::InitReadInBoilerplateParams(bp->GetUsableVariables()));
}

Base_GraphNode* SS_Graph::get_node(int id) {
    auto it = nodes.find(id);
    if (it == nodes.end()) return nullptr;
    return it->second.get();
}

/* DELETE a node with id from the graph and disconnect all pins attached to it. */
bool SS_Graph::delete_node(int id) {
    auto it = nodes.find(id);
    if (it == nodes.end()) return false;
    if (!it->second->can_be_deleted()) return false;
    if (it->second.get() == selected_node) selected_node = nullptr;
    if (it->second.get() == drag_node) { drag_node = nullptr; drag_pin = nullptr; }

    Base_GraphNode* n = it->second.get();
    n->DisconnectAllPins();
    nodes.erase(it);

    // If it is a parameter node, we need to remove it from the parameter->node map
    if (it->second->get_node_type() == NODE_PARAM) {
        auto* pn = (Param_Node*)it->second.get();
        assert(paramIDsToNodeIDs.find(pn->_paramID) != paramIDsToNodeIDs.end());
        auto& paramNodesOfID = paramIDsToNodeIDs[pn->_paramID];
        assert(std::find(paramNodesOfID.begin(), paramNodesOfID.end(), pn->_id) != paramNodesOfID.end());
        paramNodesOfID.erase(std::find(paramNodesOfID.begin(), paramNodesOfID.end(), pn->_id));
    }
    delete n; // no LEAKS!

    return true;
}

bool SS_Graph::disconnect_all_pins_by_id(int id) {
    auto it = nodes.find(id);
    if (it == nodes.end()) return false;
    it->second->DisconnectAllPins();
    return true;
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
    param_datas.emplace_back(new Parameter_Data(SS_Float, SS_Vec3, 1, ++param_id, this));
}

void SS_Graph::draw_param_panels() {
    ImGui::Begin("Parameters", nullptr, ImGuiWindowFlags_NoScrollbar);
    ImGui::BeginChild("Red",  ImGui::GetWindowSize() - ImVec2(0, 50), true, ImGuiWindowFlags_HorizontalScrollbar);
    for (auto& p_data : param_datas) {
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
        auto built_node_data_list = SS_Node_Factory::GetMatchingBuiltinNodes(std::string(search_buf));
        if (not built_node_data_list.empty()) ImGui::Text("Builtin:");
        for (auto nd : built_node_data_list) {
            if (ImGui::Button(nd._name.c_str())) {
                Builtin_GraphNode* n = SS_Node_Factory::BuildBuiltinNode(nd, ++current_id,
                                                                     add_pos - (_pos_offset + _drag_pos_offset));
                nodes.insert(std::make_pair(current_id, (Base_GraphNode*)n));
                ImGui::CloseCurrentPopup(); ImGui::EndPopup();
                return;
            }
        }
        // CONSTANT
        auto const_node_data_list = SS_Node_Factory::GetMatchingConstantNodes(std::string(search_buf));
        if (not const_node_data_list.empty()) ImGui::Text("CONSTANT:");
        for (auto nd : const_node_data_list) {
            if (ImGui::Button(nd.m_name.c_str())) {
                Constant_Node* n = SS_Node_Factory::BuildConstantNode(nd, ++current_id,
                                                                  add_pos - (_pos_offset + _drag_pos_offset));
                nodes.insert(std::make_pair(current_id, (Base_GraphNode*)n));
                ImGui::CloseCurrentPopup(); ImGui::EndPopup();
                return;
            }
        }
        // VECTOR OPS
        auto vec_node_data_list = SS_Node_Factory::GetMatchingVectorNodes(std::string(search_buf));
        if (not vec_node_data_list.empty()) ImGui::Text("VECTOR OPS:");
        for (auto nd : vec_node_data_list) {
            if (ImGui::Button(nd.m_name.c_str())) {
                Vector_Op_Node* n = SS_Node_Factory::BuildVecOpNode(nd, ++current_id,
                                                                add_pos - (_pos_offset + _drag_pos_offset));
                nodes.insert(std::make_pair(current_id, (Base_GraphNode*)n));
                ImGui::CloseCurrentPopup(); ImGui::EndPopup();
                return;
            }
        }
        // PARAMS
        std::vector<Parameter_Data*> param_node_data_list =
                SS_Node_Factory::GetMatchingParamNodes(std::string(search_buf), param_datas);
        if (not param_node_data_list.empty()) ImGui::Text("PARAMETERS:");

        for (Parameter_Data* nd : param_node_data_list) {
            if (ImGui::Button(nd->GetName() + 2)) {
                // Add the node
                Param_Node* n = SS_Node_Factory::BuildParamNode(nd, ++current_id,
                                                            add_pos - (_pos_offset + _drag_pos_offset));
                // Update collections
                nodes.insert(std::make_pair(current_id, n));
                if (paramIDsToNodeIDs.find(nd->GetID()) == paramIDsToNodeIDs.end())
                    paramIDsToNodeIDs.insert({nd->GetID(), {}});
                paramIDsToNodeIDs[nd->GetID()].push_back(current_id);
                ImGui::CloseCurrentPopup(); ImGui::EndPopup();
                return;
            }
        }
        // BOILERPLATE
         auto bp_node_data_list = SS_Node_Factory::GetMatchingBoilerplateNodes(std::string(search_buf));
        if (not bp_node_data_list.empty()) ImGui::Text("DEFAULTS:");
        for (auto nd : bp_node_data_list) {
            if (ImGui::Button(nd._name.c_str())) {
                Boilerplate_Var_Node* n = SS_Node_Factory::BuildBoilerplateVarNode(
                        nd, _bp_manager, ++current_id,
                        add_pos - (_pos_offset + _drag_pos_offset));
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
    for (const auto& n : nodes) {
        if (n.second->is_hovering(m_pos - (_pos_offset + _drag_pos_offset))) {
            hover_id = n.first;
            break;
        }
    }

    Base_GraphNode* hover_node = hover_id != -1 ? nodes[hover_id].get() : nullptr;
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
            hover_pin->disconnect_all_from(true);
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
                PinOps::ConnectPins((Base_InputPin *) drag_pin, (Base_OutputPin *) hover_pin);
            else if (!drag_pin->bInput && hover_pin->bInput)
                PinOps::ConnectPins((Base_InputPin *) hover_pin, (Base_OutputPin *) drag_pin);
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
    for (const auto& n_it : nodes) {
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
    for (auto& p_node : nodes)
        p_node.second->set_bounds(1);
    for (auto& p_node : nodes)
        p_node.second->draw(dl, _pos_offset + _drag_pos_offset, p_node.second.get() == selected_node);
    for (auto& p_node : nodes)
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

std::unordered_map<Base_GraphNode*, int> GetInDegreesOfAllNodes(Base_GraphNode* root) {
    std::unordered_map<Base_GraphNode*, int> inDegrees;
    std::stack<Base_GraphNode*> processStack;
    processStack.push(root);
    inDegrees.insert({root, root->num_input});
    while (not processStack.empty()) {
        Base_GraphNode* node = processStack.top();
        processStack.pop();
        for (int i = 0; i < node->num_input; i++) {
            if (not node->input_pins[i].input) continue;
            Base_GraphNode* inputNode = node->input_pins[i].input->owner;
            if (inDegrees.find(inputNode) != inDegrees.end()) {
                inDegrees.insert({inputNode, inputNode->num_input});
                processStack.push(inputNode);
            }
        }
    }
    return inDegrees;
}

std::vector<Base_GraphNode*> SS_Graph::ConstructTopologicalOrder(Base_GraphNode* root) {
    std::vector<Base_GraphNode*> topOrder;
    std::stack<Base_GraphNode*> processStack;

    // Collect our starting elements
    std::unordered_map<Base_GraphNode*, int> inDegrees = GetInDegreesOfAllNodes(root);
    for (const auto& kv : inDegrees)
        if (kv.second == 0) processStack.push(kv.first);

    // Add based on processed dependencies
    while (not processStack.empty()) {
        Base_GraphNode* node = processStack.top();
        processStack.pop();
        topOrder.push_back(node);

        for (int o = 0; o < node->num_output; ++o) {
            for (const auto& conn : node->output_pins[o].output) {
                Base_GraphNode* inputNode = conn->owner;
                inDegrees[inputNode] -= 1;
                if (inDegrees[inputNode] <= 0)
                    processStack.push(inputNode);
            }
        }
    }
    return topOrder;
}

void SS_Graph::SetIntermediateCodeForNode(std::string intermedCode, Base_GraphNode* node) {
    std::string output = node->request_output(0);
    if (output.empty()) {
        node->SetShaderCode(_current_frag_code, _current_vert_code);
    } else {
        intermedCode += "gl_FragColor = ";
        intermedCode +=  SS_Parser::convert_output_to_color_str(output, node->output_pins[0].type) + ";\n}\n";
        node->SetShaderCode(intermedCode, _current_vert_code);
    }
}

void WriteParameterData(std::ostringstream& oss, const std::vector<std::unique_ptr<Parameter_Data>>& params) {
    for (const auto& p_data : params) {
        oss << "uniform ";
        oss << SS_Parser::type_to_string(p_data->GetType());
        oss << " ";
        oss << p_data->GetName() << ";\n";
    }
}

void SS_Graph::SetFinalShaderTextByConstructOrders(const std::vector<Base_GraphNode*>& vertOrder,
                                                   const std::vector<Base_GraphNode*>& fragOrder) {
    // MAXIMAL VERTEX BUILD
    {
        std::ostringstream vertIss;
        // -- header
        vertIss << _bp_manager->GetVertInitBoilerplateDeclares() << '\n';
        WriteParameterData(vertIss, param_datas);
        // -- main body
        vertIss << "\nvoid main() {\n" << _bp_manager->GetVertInitBoilerplateCode() << '\n';
        for (Base_GraphNode* node: vertOrder) {
            vertIss << "\t" << node->process_for_code() << "  // Node " << node->_name << ", id=" << node->_id << '\n';
        }
        vertIss << _bp_manager->GetVertTerminalBoilerplateCode() << "\n}\n";
        _current_vert_code = vertIss.str();
    }
    // MAXIMAL FRAG BUILD
    {
        std::ostringstream fragIss;
        // -- header
        fragIss << _bp_manager->GetFragInitBoilerplateDeclares() << '\n';
        WriteParameterData(fragIss, param_datas);
        // -- main body
        fragIss << "\nvoid main() {\n" << _bp_manager->GetFragInitBoilerplateCode() << '\n';
        for (Base_GraphNode* node: fragOrder) {
            fragIss << "\t" << node->process_for_code() << "  // Node " << node->_name << ", id=" << node->_id << '\n';
        }
        fragIss << _bp_manager->GetFragTerminalBoilerplateCode() << "\n}\n";
        _current_frag_code = fragIss.str();
    }
}

void SS_Graph::PropagateIntermediateVertexCodeToNodes(const std::vector<Base_GraphNode*>& vertOrder) {
    std::ostringstream vertIss;
    // -- header
    vertIss << _bp_manager->GetFragInitBoilerplateDeclares() << '\n';
    WriteParameterData(vertIss, param_datas);
    // -- main body
    vertIss << "\nvoid main() {\n" << _bp_manager->GetFragInitBoilerplateCode() << '\n';
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
    fragIss << _bp_manager->GetFragInitBoilerplateDeclares() << '\n';
    WriteParameterData(fragIss, param_datas);
    // -- main body
    fragIss << "\nvoid main() {\n" << _bp_manager->GetFragInitBoilerplateCode() << '\n';
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
    Terminal_Node* vn = _bp_manager->GetTerminalVertexNode();
    Terminal_Node* fn = _bp_manager->GetTerminalFragNode();
    std::vector<Base_GraphNode*> vertOrder = ConstructTopologicalOrder(vn);
    std::vector<Base_GraphNode*> fragOrder = ConstructTopologicalOrder(fn);

    SetFinalShaderTextByConstructOrders(vertOrder, fragOrder);

    PropagateIntermediateVertexCodeToNodes(vertOrder);
    PropagateIntermediateFragmentCodeToNodes(fragOrder);
}

void SS_Graph::InformOfDelete(int paramID) {
    // Need to make a copy here, delete_node modifies paramIDsToNodeIDs
    const auto nodeIDs = paramIDsToNodeIDs[paramID];
    for (int nID : nodeIDs) {
        delete_node(nID);
    }
    paramIDsToNodeIDs.erase(paramID);
}

void SS_Graph::UpdateParamDataContents(int paramID, GLSL_TYPE type) {
    const auto nodeIDs = paramIDsToNodeIDs[paramID];
    for (int nID : nodeIDs) {
        disconnect_all_pins_by_id(nID);
        auto* pn = (Param_Node*)get_node(nID);
        pn->update_type_from_param(type);
    }
    invalidate_shaders();
}

void SS_Graph::UpdateParamDataName(int paramID, const char *name) {
    const auto nodeIDs = paramIDsToNodeIDs[paramID];
    for (int nID : nodeIDs) {
        auto* pn = (Param_Node*)get_node(nID);
        pn->_name = name;
    }
    invalidate_shaders();
}
