#ifndef SS_GRAPH
#define SS_GRAPH

#include <iostream>
#include <vector>
#include <unordered_map>
#include <stack>
#include <queue>

#include "ss_node.hpp"
#include "ss_parser.hpp"
#include "ss_boilerplate.hpp"
#include "ss_node_factory.hpp"
#include "../imgui/imgui.h"

// Scalar, Vecn, Matn, Texture, sampler id

// Gen are used to specify a generic size, The other GLSL_Vecn/Scalar is what is used in propogation and building...
enum GRAPH_PARAM_TYPE : unsigned int {
    SS_Float,
    SS_Double,
    SS_Int,
    SS_Texture2D,
    SS_TextureCube
};

enum GRAPH_PARAM_GENTYPE : unsigned int {
    SS_Scalar,
    SS_Vec2,
    SS_Vec3,
    SS_Vec4,
    SS_Mat2,
    SS_Mat3,
    SS_Mat4,
    SS_MAT = SS_Mat2 | SS_Mat3 | SS_Mat4,
    SS_VEC = SS_Vec2 | SS_Vec3 | SS_Vec4
};

struct Parameter_Data {
    SS_Graph* _graph;
    GRAPH_PARAM_TYPE _type;
    GRAPH_PARAM_GENTYPE _gentype;

    int _id;

    unsigned int arr_size;

    char _param_name[64];
    char data_container[16 * 8];
    std::vector<int> param_node_ids;

    Parameter_Data(GRAPH_PARAM_TYPE type, GRAPH_PARAM_GENTYPE gentype, SS_Graph* g, unsigned as, int id);
    void make_data(bool reset_mem = true);
    bool update_gentype(GRAPH_PARAM_GENTYPE gentype);
    bool update_type(GRAPH_PARAM_TYPE type);
    void update_name(const char* new_param_name);
    void add_param_node(int id);
    void remove_node_type(int id);
    void destroy_nodes(SS_Graph* g);

    __attribute__((unused)) bool is_mat() const;
    __attribute__((unused)) unsigned get_len() const;

    int type_to_dropbox_index() const;
    int gentype_to_dropbox_index() const;

    void draw(SS_Graph* graph);
};


// MAIN MANAGEMENT CLASS OF THE APPLICATION
    // CONTROLS PRACTICALLY EVERYTHING 
class SS_Graph {
public:
    explicit SS_Graph(SS_Boilerplate_Manager* bp);
    ~SS_Graph() { for (Parameter_Data* pd : param_datas) delete pd; }
    std::unordered_map<int, Base_GraphNode*> nodes;
    int current_id = 0;

    unsigned int _main_framebuffer{};
    
   // 0 for input change needed, 1 for no, 2 for output changed needed
   // -1 for failed
   Base_GraphNode* get_node(int id);
    bool delete_node(int id);

    static bool ArePinsConnectable(Base_InputPin* in_pin, Base_OutputPin* out_pin);
    static bool CheckForDAGViolation(Base_InputPin* in_pin, Base_OutputPin* out_pin);

    static bool ConnectPins(Base_InputPin* in_pin, Base_OutputPin* out_pin);
    static bool DisconnectPins(Base_InputPin* in_pin, Base_OutputPin* out_pin, bool reprop = true);

    static bool DisconnectAllPins(Base_GraphNode* node);
    bool disconnect_all_pins_by_id(int id);

    void invalidate_shaders();

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

    inline bool all_output_from_node_processed(Base_GraphNode* node, 
        const std::unordered_set<Base_GraphNode*>& processed,
        std::unordered_set<Base_GraphNode*>& down_wind);
    void get_down_wind_nodes(Base_GraphNode* root, std::unordered_set<Base_GraphNode*>& down_wind);
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
    void inform_of_parameter_change(Parameter_Data* param);

    ImVec2 _pos_offset = ImVec2(0, 0);
    ImVec2 _drag_pos_offset = ImVec2(0, 0);

    bool _screen_dragging{};
    int focus_id{};
    Base_GraphNode* drag_node = nullptr;
    Base_GraphNode* selected_node = nullptr;
    Base_Pin* drag_pin;
    char search_buf[256]{};
    SS_Node_Factory node_factory;

    std::vector<std::pair<unsigned int, std::string> > images;
    char img_buf[256]{};

    SS_Boilerplate_Manager* _bp_manager;
    std::vector<Parameter_Data*> param_datas;

    std::string _current_frag_code = "";
    std::string _current_vert_code = "";

    void SetFinalShaderTextByConstructOrders(const std::vector<Base_GraphNode *> &vertOrder,
                                             const std::vector<Base_GraphNode *> &fragOrder);

    void PropagateIntermediateVertexCodeToNodes(const std::vector<Base_GraphNode *> &vertOrder);

    void PropagateIntermediateFragmentCodeToNodes(const std::vector<Base_GraphNode *> &fragOrder);
};

#endif