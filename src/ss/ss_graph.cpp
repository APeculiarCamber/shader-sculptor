#include "ss_graph.hpp"
#include "stb_image.h"
#include "ss_pins.hpp"
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
    glGenFramebuffers(1, &m_mainFramebuffer);

    _dragNode = nullptr;
    _dragPin = nullptr;

    m_BPManager = std::unique_ptr<SS_Boilerplate_Manager>(bp);
    auto* vn = new Terminal_Node(m_BPManager->GetTerminalVertPinData(), ++m_currentNodeID, ImVec2(300, 300));
    m_nodes.insert(std::make_pair(m_currentNodeID, vn));
    auto* fn = new Terminal_Node(m_BPManager->GetTerminalFragPinData(), ++m_currentNodeID, ImVec2(300, 500));
    m_nodes.insert(std::make_pair(m_currentNodeID, fn));
    m_BPManager->SetTerminalNodes(vn, fn);
    this->GenerateShaderTextAndPropagate();

    m_searchBuffer[0] = '\0';
    m_imgBuffer[0] = '\0';

    // Static load of node factory data, NOTE: ASSUMES GRAPH SINGLETON!
    assert(SS_Node_Factory::InitReadBuiltinFile("data/builtin_glsl_funcs.txt"));
    assert(SS_Node_Factory::InitReadInBoilerplateParams(bp->GetUsableVariables()));
}

SS_Graph::~SS_Graph() {
    glDeleteFramebuffers(1, &m_mainFramebuffer);
}


Base_GraphNode* SS_Graph::GetNode(int id) {
    auto it = m_nodes.find(id);
    if (it == m_nodes.end()) return nullptr;
    return it->second.get();
}

/* DELETE a node with id from the graph and disconnect all pins attached to it. */
bool SS_Graph::DeleteNode(int id) {
    auto it = m_nodes.find(id);
    if (it == m_nodes.end()) return false;
    if (!it->second->CanBeDeleted()) return false;
    if (it->second.get() == _selectedNode) _selectedNode = nullptr;
    if (it->second.get() == _dragNode) { _dragNode = nullptr; _dragPin = nullptr; }

    Base_GraphNode* n = it->second.get();
    n->DisconnectAllPins();
    m_nodes.erase(it);

    // If it is a parameter node, we need to remove it from the parameter->node map
    if (it->second->GetNodeType() == NODE_PARAM) {
        auto* pn = (Param_Node*)it->second.get();
        assert(m_paramIDsToNodeIDs.find(pn->_paramID) != m_paramIDsToNodeIDs.end());
        auto& paramNodesOfID = m_paramIDsToNodeIDs[pn->_paramID];
        assert(std::find(paramNodesOfID.begin(), paramNodesOfID.end(), pn->GetID()) != paramNodesOfID.end());
        paramNodesOfID.erase(std::find(paramNodesOfID.begin(), paramNodesOfID.end(), pn->GetID()));
    }
    return true;
}

bool SS_Graph::DisconnectAllPinsByNodeId(int id) {
    auto it = m_nodes.find(id);
    if (it == m_nodes.end()) return false;
    it->second->DisconnectAllPins();
    return true;
}

void SS_Graph::InvalidateShaders() {
    m_currentFragCode.clear();
    m_currentVertCode.clear();
}

void SS_Graph::AddParameter() {
    m_paramDatas.emplace_back(new Parameter_Data(SS_Float, SS_Vec3, 1, ++m_paramID, this));
}

void SS_Graph::DrawParamPanels() {
    ImGui::Begin("Parameters", nullptr, ImGuiWindowFlags_NoScrollbar);
    ImGui::BeginChild("Red",  ImGui::GetWindowSize() - ImVec2(0, 50), true, ImGuiWindowFlags_HorizontalScrollbar);
    for (auto& p_data : m_paramDatas) {
        p_data->Draw(this);
    }
    ImGui::EndChild();

    if (ImGui::Button("Add Param")) {
        AddParameter();
    }
    ImGui::End();
}

unsigned int GenerateTextureFrom(const char* img_file) {
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

void SS_Graph::DrawImageLoaderWindow() {
    ImGui::Begin("Image Loader", nullptr, ImGuiWindowFlags_NoScrollbar);
    
    ImGui::BeginChild("ImgLoads",  ImGui::GetWindowSize() - ImVec2(0, 50), true, ImGuiWindowFlags_HorizontalScrollbar);
    float x_size = fmax(ImGui::GetWindowSize().x - 50.0f, 10.0f);
    for (auto it = m_images.begin(); it != m_images.end(); ) {
        unsigned int tex = it->first;
        ImGui::Text("%s ||| ID=%d", it->second.c_str(), tex);
        ImGui::SameLine();
        bool queued_delete = ImGui::Button(("Delete###" + std::to_string(tex)).c_str());
        ImGui::Image((void*)(intptr_t)tex, ImVec2(x_size, x_size));
        if (queued_delete) {
            glDeleteTextures(1, &tex);
            it = m_images.erase(it);
        } else ++it;
    }
    ImGui::EndChild();

    ImGui::InputTextWithHint("###Input Image", "Image Filepath", m_imgBuffer, 256);
    ImGui::SameLine();
    if (ImGui::Button("Add Image")) {
        unsigned int tex = GenerateTextureFrom(m_imgBuffer);
        if (tex > 0) {
            m_images.emplace_back(tex, std::string(m_imgBuffer));
        }
    }
    ImGui::End();
}

void SS_Graph::HandleInput() {
    //  MENU
    ImGui::SetNextWindowSize(ImVec2(250, 400));
    if (ImGui::BeginPopupContextWindow())
    {
        ImVec2 add_pos = ImGui::GetItemRectMin();
        if (_dragNode) {
            _dragNode->SetDrawOldPos(_dragNode->GetDrawOldPos() + ImGui::GetMouseDragDelta(0));
        }
        _dragNode = nullptr;
        _dragPin = nullptr;

        // SEARCH BAR
        ImGui::InputText("Search", m_searchBuffer, 256);
        // BUILTIN
        auto built_node_data_list = SS_Node_Factory::GetMatchingBuiltinNodes(std::string(m_searchBuffer));
        if (not built_node_data_list.empty()) ImGui::Text("Builtin:");
        for (auto nd : built_node_data_list) {
            if (ImGui::Button(nd._name.c_str())) {
                Builtin_GraphNode* n = SS_Node_Factory::BuildBuiltinNode(nd, ++m_currentNodeID,
                                                                     add_pos - (m_drawPosOffset + m_dragPosOffset));
                m_nodes.insert(std::make_pair(m_currentNodeID, (Base_GraphNode*)n));
                ImGui::CloseCurrentPopup(); ImGui::EndPopup();
                return;
            }
        }
        // CONSTANT
        auto const_node_data_list = SS_Node_Factory::GetMatchingConstantNodes(std::string(m_searchBuffer));
        if (not const_node_data_list.empty()) ImGui::Text("CONSTANT:");
        for (auto nd : const_node_data_list) {
            if (ImGui::Button(nd.m_name.c_str())) {
                Constant_Node* n = SS_Node_Factory::BuildConstantNode(nd, ++m_currentNodeID,
                                                                  add_pos - (m_drawPosOffset + m_dragPosOffset));
                m_nodes.insert(std::make_pair(m_currentNodeID, (Base_GraphNode*)n));
                ImGui::CloseCurrentPopup(); ImGui::EndPopup();
                return;
            }
        }
        // VECTOR OPS
        auto vec_node_data_list = SS_Node_Factory::GetMatchingVectorNodes(std::string(m_searchBuffer));
        if (not vec_node_data_list.empty()) ImGui::Text("VECTOR OPS:");
        for (auto nd : vec_node_data_list) {
            if (ImGui::Button(nd.m_name.c_str())) {
                Vector_Op_Node* n = SS_Node_Factory::BuildVecOpNode(nd, ++m_currentNodeID,
                                                                add_pos - (m_drawPosOffset + m_dragPosOffset));
                m_nodes.insert(std::make_pair(m_currentNodeID, (Base_GraphNode*)n));
                ImGui::CloseCurrentPopup(); ImGui::EndPopup();
                return;
            }
        }
        // PARAMS
        std::vector<Parameter_Data*> param_node_data_list =
                SS_Node_Factory::GetMatchingParamNodes(std::string(m_searchBuffer), m_paramDatas);
        if (not param_node_data_list.empty()) ImGui::Text("PARAMETERS:");

        for (Parameter_Data* nd : param_node_data_list) {
            if (ImGui::Button(nd->GetName() + 2)) {
                // Add the node
                Param_Node* n = SS_Node_Factory::BuildParamNode(nd, ++m_currentNodeID,
                                                            add_pos - (m_drawPosOffset + m_dragPosOffset));
                // Update collections
                m_nodes.insert(std::make_pair(m_currentNodeID, n));
                if (m_paramIDsToNodeIDs.find(nd->GetID()) == m_paramIDsToNodeIDs.end())
                    m_paramIDsToNodeIDs.insert({nd->GetID(), {}});
                m_paramIDsToNodeIDs[nd->GetID()].push_back(m_currentNodeID);
                ImGui::CloseCurrentPopup(); ImGui::EndPopup();
                return;
            }
        }
        // BOILERPLATE
         auto bp_node_data_list = SS_Node_Factory::GetMatchingBoilerplateNodes(std::string(m_searchBuffer));
        if (not bp_node_data_list.empty()) ImGui::Text("DEFAULTS:");
        for (auto nd : bp_node_data_list) {
            if (ImGui::Button(nd._name.c_str())) {
                Boilerplate_Var_Node* n = SS_Node_Factory::BuildBoilerplateVarNode(
                        nd, m_BPManager.get(), ++m_currentNodeID,
                        add_pos - (m_drawPosOffset + m_dragPosOffset));
                m_nodes.insert(std::make_pair(m_currentNodeID, (Base_GraphNode*)n));
                ImGui::CloseCurrentPopup(); ImGui::EndPopup();
                return;
            }
        }
        ImGui::EndPopup();
        return;
    } else {
        m_searchBuffer[0] = 0;
    }

    // DRAGS
    ImVec2 m_pos = ImGui::GetMousePos();
    int hover_id = -1;
    for (const auto& n : m_nodes) {
        if (n.second->IsHovering(m_pos - (m_drawPosOffset + m_dragPosOffset))) {
            hover_id = n.first;
            break;
        }
    }

    Base_GraphNode* hover_node = hover_id != -1 ? m_nodes[hover_id].get() : nullptr;
    Base_Pin* hover_pin = hover_node ? hover_node->GetHoveredPin(m_pos - (m_drawPosOffset + m_dragPosOffset)) : nullptr;
    bool display_button_hovered = hover_node != nullptr &&
            hover_node->IsDisplayButtonHoveredOver(m_pos - (m_drawPosOffset + m_dragPosOffset));
    if (display_button_hovered) {
        ImGui::BeginTooltip();
        ImGui::Text("DISPLAY INTERMEDIATE");
        ImGui::EndTooltip();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Delete) && hover_id != -1) {
        if (hover_pin && hover_pin->HasConnections()) {
            hover_pin->DisconnectAllFrom(true);
        } else {
            DeleteNode(hover_id);
        }
        _selectedNode =  nullptr;
        _dragNode = nullptr;
        _dragPin = nullptr;
        return;
    }

    bool b_space = ImGui::IsKeyDown(ImGuiKey_Space);
    if (ImGui::IsMouseClicked(0) && hover_id != -1 && !b_space) {
        _selectedNode = hover_node;
        if (hover_pin)
            _dragPin = hover_pin;
        else if (!display_button_hovered)
            _dragNode = hover_node;
    } else if (ImGui::IsMouseClicked(0) && b_space) {
        m_bScreenDraggingNow = true;
    }

    if (ImGui::IsMouseDragging(0)) {
        if (!m_bScreenDraggingNow) {
            if (_dragNode) {
                _dragNode->SetDrawPos(_dragNode->GetDrawOldPos() + ImGui::GetMouseDragDelta(0));
            }
        } else {
            m_dragPosOffset = ImGui::GetMouseDragDelta(0);
        }
    }

    if (ImGui::IsMouseReleased(0) && !m_bScreenDraggingNow) {
        if (hover_node) {
            if (display_button_hovered) {
                _selectedNode->ToggleDisplay();
            }
        }
        if (_dragNode)
            _dragNode->SetDrawOldPos(_dragNode->GetDrawOldPos() + ImGui::GetMouseDragDelta(0));

        if (_dragPin && hover_pin) {
            if (_dragPin->bInput && !hover_pin->bInput)
                PinOps::ConnectPins((Base_InputPin *) _dragPin, (Base_OutputPin *) hover_pin);
            else if (!_dragPin->bInput && hover_pin->bInput)
                PinOps::ConnectPins((Base_InputPin *) hover_pin, (Base_OutputPin *) _dragPin);
        }
        _dragNode = nullptr;
        _dragPin = nullptr;
    } else if (ImGui::IsMouseReleased(0) && m_bScreenDraggingNow) {
        m_drawPosOffset = m_drawPosOffset + m_dragPosOffset;
        m_dragPosOffset = ImVec2(0, 0);
        m_bScreenDraggingNow = false;
    }
}

void SS_Graph::DrawControlsWindow() {
    if (!m_bControlsUp) return;
    ImGui::Begin("CONTROLS", &m_bControlsUp);
    ImGui::Text(R"(MENU BUTTONS: SEE TOOLTIPS
LEFT CLICK : SELECT AND DRAG NODE OR PIN
RIGHT CLICK: OPEN NODE ADD MENU; USE SEARCH BAR
SPACEBAR : ACTIVATE CAMERA DRAG (HOLD LEFT CLICK)
DELETE: DELETE HOVERED NODE OR PIN)");
    ImGui::End();
}

bool SS_Graph::DrawSavingWindow() {
    bool bReturn = true;
    ImGui::SetNextWindowFocus();
    ImGui::Begin("Save LOCATION");
    char* saveLocationStr = m_saveBuffer;
    char* saveFragStr = m_saveBuffer + 128;
    char* saveVertStr = m_saveBuffer + 192;
    ImGui::InputText("SAVE LOCATION", saveLocationStr, 128);
    ImGui::InputText("FRAG NAME", saveFragStr, 64);
    ImGui::InputText("VERT NAME", saveVertStr, 64);
    if (ImGui::Button("SAVE GRAPH CODE")) {
        std::ofstream frag_oss(std::string(m_saveBuffer) + "/" + std::string(saveFragStr));
        std::ofstream vert_oss(std::string(m_saveBuffer) + "/" + std::string(saveVertStr));
        if (frag_oss.good() and vert_oss.good()) {
            frag_oss << m_currentFragCode << std::endl;
            vert_oss << m_currentVertCode << std::endl;
        } else {
            std::cerr << "WARNING: Couldn't save to " << m_saveBuffer << ".\n\tThis directory might not exist." << std::endl;
        }
        bReturn = false;
    }
    if (ImGui::Button("CLOSE WITH SAVE"))
        bReturn = false;
    ImGui::End();
    return bReturn;
}

bool SS_Graph::DrawCreditsWindow() {
    ImGui::Begin("CREDITS", &m_bCreditsUp);
    ImGui::Text(R"(DEVELOPER: Jay Idema

A Massive Thanks To:
Professor Eric Ameres for advising
Joey de Vries, Author of LearnOpenGL, from which a good handful of PBR-like boilerplate shader code was used
Omar Cornut for Dear IMGUI)");
    ImGui::End();
    return m_bCreditsUp;
}

void HandleMenuTooltip(const char* tip) {
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("%s", tip);
        ImGui::EndTooltip();
    }
}

void SS_Graph::DrawMenuButtons() {
    if (ImGui::BeginMenuBar()) {
        HandleMenuTooltip("Build and link the shaders, re-compiles dirty intermediate results");
        if (ImGui::Button("BUILD SHADERS")) {
            this->GenerateShaderTextAndPropagate();
        }
        HandleMenuTooltip("Build and link the fragment shader");
        if (ImGui::Button("SAVE NODES")) {
            m_bIsSaving = true;
            sprintf(m_saveBuffer, ".");
            sprintf(m_saveBuffer + 128, "frag.glsl");
            sprintf(m_saveBuffer + 192, "vert.glsl");
        }
        HandleMenuTooltip("Save out the source code");
        if (ImGui::Button("SHOW CONTROLS"))
            m_bControlsUp = !m_bControlsUp;
        if (ImGui::Button("SHOW CREDITS"))
            m_bCreditsUp = !m_bCreditsUp;
    }
    ImGui::EndMenuBar();
}

void SS_Graph::Draw() {
    if (m_bIsSaving)
        m_bIsSaving = DrawSavingWindow();
    if (m_bCreditsUp)
        m_bCreditsUp = DrawCreditsWindow();

    DrawParamPanels();
    DrawNodeContextWindow();
    DrawImageLoaderWindow();
    DrawControlsWindow();
    
    // drawn m_nodes, may want to decrease view size
    for (const auto& n_it : m_nodes) {
        if (n_it.second->GetHasDisplayUp())
            n_it.second->DrawIntermediateResult(m_mainFramebuffer, m_paramDatas);
    }

    const auto& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, io.DisplaySize.y));
    ImGui::Begin("SHADER SCULPTER", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_MenuBar);
    DrawMenuButtons();
    
    if (!m_bIsSaving)
        HandleInput();
    ImDrawList* dl = ImGui::GetWindowDrawList(); 
    for (auto& p_node : m_nodes)
        p_node.second->SetBounds(1);
    for (auto& p_node : m_nodes)
        p_node.second->Draw(dl, m_drawPosOffset + m_dragPosOffset, p_node.second.get() == _selectedNode);
    for (auto& p_node : m_nodes)
        p_node.second->DrawOutputConnects(dl, m_drawPosOffset + m_dragPosOffset);
    if (_dragPin) {
        float r;
        dl->AddLine(_dragPin->GetPinPos(3, 2, &r) + m_drawPosOffset + m_dragPosOffset, ImGui::GetMousePos(), 0xffffffff);
    }

    ImGui::End();
}


void SS_Graph::DrawNodeContextWindow() const {
    ImGui::Begin("Node Context Panel");
    ImGui::Text("NODE:");
    if (_selectedNode) {
        ImGui::Text("%s", _selectedNode->GetName().c_str());
        if (_selectedNode->GetNodeType() == NODE_CONSTANT) {
            auto* const_node = (Constant_Node*)_selectedNode;
            
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
                        ImGui::Text("WARNING:\nMatrix context not yet supported.");
                        break;
                }
            }
        } else {
            ImGui::Text("Non-Constant Node");
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
    inDegrees.insert({root, root->GetInputPinCount()});
    while (not processStack.empty()) {
        Base_GraphNode* node = processStack.top();
        processStack.pop();
        for (size_t i = 0; i < node->GetInputPinCount(); i++) {
            if (not node->GetInputPin((int)i).input) continue;
            Base_GraphNode* inputNode = node->GetInputPin((int)i).input->owner;
            if (inDegrees.find(inputNode) != inDegrees.end()) {
                inDegrees.insert({inputNode, inputNode->GetInputPinCount()});
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

        for (size_t o = 0; o < node->GetOutputPinCount(); ++o) {
            for (const auto& conn : node->GetOutputPin(o).output) {
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
    std::string output = node->RequestOutput(0);
    if (output.empty()) {
        node->SetShaderCode(m_currentFragCode, m_currentVertCode);
    } else {
        intermedCode += "gl_FragColor = ";
        intermedCode += SS_Parser::ConvertOutputToColorStr(output, node->GetOutputPin(0).type) + ";\n}\n";
        node->SetShaderCode(intermedCode, m_currentVertCode);
    }
}

void WriteParameterData(std::ostringstream& oss, const std::vector<std::unique_ptr<Parameter_Data>>& params) {
    for (const auto& p_data : params) {
        oss << "uniform ";
        oss << SS_Parser::GLSLTypeToString(p_data->GetType());
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
        vertIss << m_BPManager->GetVertInitBoilerplateDeclares() << '\n';
        WriteParameterData(vertIss, m_paramDatas);
        // -- main body
        vertIss << "\nvoid main() {\n" << m_BPManager->GetVertInitBoilerplateCode() << '\n';
        for (Base_GraphNode* node: vertOrder) {
            vertIss << "\t" << node->ProcessForCode() << "  // Node " << node->GetName() << ", id=" << node->GetID() << '\n';
        }
        vertIss << m_BPManager->GetVertTerminalBoilerplateCode() << "\n}\n";
        m_currentVertCode = vertIss.str();
    }
    // MAXIMAL FRAG BUILD
    {
        std::ostringstream fragIss;
        // -- header
        fragIss << m_BPManager->GetFragInitBoilerplateDeclares() << '\n';
        WriteParameterData(fragIss, m_paramDatas);
        // -- main body
        fragIss << "\nvoid main() {\n" << m_BPManager->GetFragInitBoilerplateCode() << '\n';
        for (Base_GraphNode* node: fragOrder) {
            fragIss << "\t" << node->ProcessForCode() << "  // Node " << node->GetName() << ", id=" << node->GetID() << '\n';
        }
        fragIss << m_BPManager->GetFragTerminalBoilerplateCode() << "\n}\n";
        m_currentFragCode = fragIss.str();
    }
}

void SS_Graph::PropagateIntermediateVertexCodeToNodes(const std::vector<Base_GraphNode*>& vertOrder) {
    std::ostringstream vertIss;
    // -- header
    vertIss << m_BPManager->GetFragInitBoilerplateDeclares() << '\n';
    WriteParameterData(vertIss, m_paramDatas);
    // -- main body
    vertIss << "\nvoid main() {\n" << m_BPManager->GetFragInitBoilerplateCode() << '\n';
    for (Base_GraphNode* node: vertOrder) {
        vertIss << "\t" << node->ProcessForCode() << "  // Node " << node->GetName() << ", id=" << node->GetID() << '\n';
        SetIntermediateCodeForNode(vertIss.str(), node);
    }
    // -- compile
    for (Base_GraphNode* node: vertOrder) {
        node->CompileIntermediateCode(m_BPManager->MakeMaterial());
    }
}

void SS_Graph::PropagateIntermediateFragmentCodeToNodes(const std::vector<Base_GraphNode*>& fragOrder) {
    std::ostringstream fragIss;
    // -- header
    fragIss << m_BPManager->GetFragInitBoilerplateDeclares() << '\n';
    WriteParameterData(fragIss, m_paramDatas);
    // -- main body
    fragIss << "\nvoid main() {\n" << m_BPManager->GetFragInitBoilerplateCode() << '\n';
    for (Base_GraphNode* node: fragOrder) {
        fragIss << "\t" << node->ProcessForCode() << "  // Node " << node->GetName() << ", id=" << node->GetID() << '\n';
        SetIntermediateCodeForNode(fragIss.str(), node);
    }
    // -- compile
    for (Base_GraphNode* node: fragOrder) {
        node->CompileIntermediateCode(m_BPManager->MakeMaterial());
    }
}

void SS_Graph::GenerateShaderTextAndPropagate() {
    Terminal_Node* vn = m_BPManager->GetTerminalVertexNode();
    Terminal_Node* fn = m_BPManager->GetTerminalFragNode();
    std::vector<Base_GraphNode*> vertOrder = ConstructTopologicalOrder(vn);
    std::vector<Base_GraphNode*> fragOrder = ConstructTopologicalOrder(fn);

    SetFinalShaderTextByConstructOrders(vertOrder, fragOrder);

    PropagateIntermediateVertexCodeToNodes(vertOrder);
    PropagateIntermediateFragmentCodeToNodes(fragOrder);
}

void SS_Graph::InformOfDelete(int paramID) {
    // Need to make a copy here, DeleteNode modifies m_paramIDsToNodeIDs
    const auto nodeIDs = m_paramIDsToNodeIDs[paramID];
    for (int nID : nodeIDs) {
        DeleteNode(nID);
    }
    m_paramIDsToNodeIDs.erase(paramID);
}

void SS_Graph::UpdateParamDataContents(int paramID, GLSL_TYPE type) {
    const auto nodeIDs = m_paramIDsToNodeIDs[paramID];
    for (int nID : nodeIDs) {
        DisconnectAllPinsByNodeId(nID);
        auto* pn = (Param_Node*) GetNode(nID);
        pn->update_type_from_param(type);
    }
    InvalidateShaders();
}

void SS_Graph::UpdateParamDataName(int paramID, const char *name) {
    const auto nodeIDs = m_paramIDsToNodeIDs[paramID];
    for (int nID : nodeIDs) {
        auto* pn = (Param_Node*) GetNode(nID);
        pn->SetName(name);
    }
    InvalidateShaders();
}
