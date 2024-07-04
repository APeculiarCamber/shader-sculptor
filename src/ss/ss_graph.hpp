#ifndef SS_GRAPH
#define SS_GRAPH

#include <iostream>
#include <vector>
#include <unordered_map>
#include <stack>
#include <queue>

#include "../imgui/imgui.h"
#include "ss_data.h"
#include "ss_node.hpp"
#include "ss_parser.hpp"
#include "ss_boilerplate.hpp"
#include "ss_node_factory.hpp"
#include "ss_pins.h"

// MAIN MANAGEMENT CLASS OF THE APPLICATION
class SS_Graph : public ParamDataGraphHook {
public: // TODO: scope this
    explicit SS_Graph(SS_Boilerplate_Manager* bp);
    std::unordered_map<int, std::unique_ptr<Base_GraphNode>> nodes;
    std::unordered_map<int, std::vector<int>> paramIDsToNodeIDs;

    int current_id = 0;

    unsigned int _main_framebuffer{};
    
   // 0 for input change needed, 1 for no, 2 for output changed needed
   // -1 for failed
   Base_GraphNode* get_node(int id);
    bool delete_node(int id);

    static bool ArePinsConnectable(Base_InputPin* in_pin, Base_OutputPin* out_pin);
    static bool CheckForDAGViolation(Base_InputPin* in_pin, Base_OutputPin* out_pin);

    // TODO: please move these to ss_pins
    static bool ConnectPins(Base_InputPin* in_pin, Base_OutputPin* out_pin);
    static bool DisconnectPins(Base_InputPin* in_pin, Base_OutputPin* out_pin, bool reprop = true);
    // TODO: please move this to ss_node
    static bool DisconnectAllPins(Base_GraphNode* node);
    bool disconnect_all_pins_by_id(int id);

    void invalidate_shaders();

    void InformOfDelete(int paramID) override;
    /*
    for (int id : param_node_ids) {
        graphHook->disconnect_all_pins_by_id(id);
        graphHook->update_node_by_id(id, SS_Parser::constant_type_to_type(m_gentype, m_type, m_arrSize), m_paramName);
    }
    graphHook->invalidate_shaders();
     */
    void UpdateParamDataContents(int paramID, GLSL_TYPE type) override;
    /*
    for (int id : param_node_ids) {
        graphHook->update_node_by_id(id, SS_Parser::constant_type_to_type(m_gentype, m_type, m_arrSize), m_paramName);
    }
    graphHook->invalidate_shaders();
     */
    void UpdateParamDataName(int paramID, const char* name) override;


    void handle_input();
    void draw();
    bool draw_saving_window();
    bool draw_credits();
    void draw_param_panels();
    void draw_node_context_panel() const;
    void draw_image_loader();
    void draw_controls();
    void draw_menu_buttons();


    /************************************************
     * *********************CONSTRUCTION **************************/

    [[nodiscard]] static std::vector<Base_GraphNode*> ConstructTopologicalOrder(Base_GraphNode* root);
    // construct shader code from the terminal fragment node outward
    //void Construct_Text_For_Frag(Base_GraphNode* root);
    // construct shader code from the terminal vertex node outward
    //void Construct_Text_For_Vert(Base_GraphNode* root);
    void GenerateShaderTextAndPropagate();
    void SetIntermediateCodeForNode(std::string intermedCode, Base_GraphNode* node);
    
    bool _is_saving = false;
    bool _credits_up = false;
    bool _controls_open = false;
    char save_buf[256]{};

    // Parameters
    int param_id = 0;
    void add_parameter();
    void remove_parameter(int ind);

    ImVec2 _pos_offset = ImVec2(0, 0);
    ImVec2 _drag_pos_offset = ImVec2(0, 0);

    bool _screen_dragging{};
    Base_GraphNode* drag_node = nullptr;
    Base_GraphNode* selected_node = nullptr;
    Base_Pin* drag_pin;
    char search_buf[256]{};
    SS_Node_Factory node_factory;

    std::vector<std::pair<unsigned int, std::string> > images;
    char img_buf[256]{};

    SS_Boilerplate_Manager* _bp_manager;
    std::vector<std::unique_ptr<Parameter_Data>> param_datas;

    std::string _current_frag_code;
    std::string _current_vert_code;

    void SetFinalShaderTextByConstructOrders(const std::vector<Base_GraphNode *> &vertOrder,
                                             const std::vector<Base_GraphNode *> &fragOrder);

    void PropagateIntermediateVertexCodeToNodes(const std::vector<Base_GraphNode *> &vertOrder);

    void PropagateIntermediateFragmentCodeToNodes(const std::vector<Base_GraphNode *> &fragOrder);
};

#endif