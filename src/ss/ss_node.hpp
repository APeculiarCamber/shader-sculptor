#ifndef SS_NODE
#define SS_NODE

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <unordered_set>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "../imgui/imgui.h"


class SS_Graph;

typedef int arr_size;

// Gen are used to specify a generic size, The other GLSL_Vecn/Scalar is what is used in propogation and building...
enum GLSL_TYPE_ENUM {
    GLSL_Bool   = 1,
    GLSL_Int   = 2,
    GLSL_UInt   = 4,
    GLSL_Float   = 8,
    GLSL_Double   = 16,
    GLSL_AllTypes = 31,
    // Container
    GLSL_Scalar  = 32,
    GLSL_Vec2    = 64,
    GLSL_Vec3    = 128,
    GLSL_Vec4    = 256,
    GLSL_VecN    = GLSL_Vec2 | GLSL_Vec3 | GLSL_Vec4,
    GLSL_LenMask = GLSL_Scalar | GLSL_Vec2 | GLSL_Vec3 | GLSL_Vec4,
    GLSL_LenToGenPush = 4,
    GLSL_GenScalar = 512,
    GLSL_GenVec2   = 1024,
    GLSL_GenVec3   = 2048,
    GLSL_GenVec4   = 4096,
    GLSL_GenType = GLSL_GenScalar | GLSL_GenVec2 | GLSL_GenVec3 | GLSL_GenVec4,
    GLSL_GenVec = GLSL_GenVec2 | GLSL_GenVec3 | GLSL_GenVec4,

    // OTHER
    GLSL_Mat   = 8192,
    GLSL_TextureSampler2D = 8192,
    GLSL_TextureSampler3D = 16384,
    GLSL_TextureSamplerCube = 32768,

    GLSL_Out = 536870912, 
    GLSL_Array = 2147483648, // size could be embed into unsigned int, but I think it's better to have seperate
    GLSL_TypeMask = GLSL_Bool | GLSL_Int | GLSL_UInt | GLSL_Float | GLSL_Double | GLSL_TextureSampler2D | GLSL_TextureSampler3D | GLSL_TextureSamplerCube
};



struct GLSL_TYPE {
    GLSL_TYPE() {}
    GLSL_TYPE(unsigned int flags, unsigned int size) : type_flags(flags), arr_size(size) {}
    unsigned int type_flags;
    unsigned int arr_size;

    GLSL_TYPE intersect(const GLSL_TYPE& other) {
        // intersect with in mind : gentype, genvec, overrides on single, array size,y if array
        type_flags &= other.type_flags;
        arr_size = 1;
        return *this;
    }
    GLSL_TYPE intersect_copy(const GLSL_TYPE& other){
        GLSL_TYPE type;
        type.type_flags = type_flags & other.type_flags;
        type.arr_size = 1;
        return type;
    }

    inline bool is_same_value_type(const GLSL_TYPE& other) {
        return other.type_flags & type_flags & GLSL_TypeMask;
    }

    inline bool is_matrix() {
        return type_flags & GLSL_Mat;
    }
};







/*********************************************************************************
 * ********************************************************************************
 * ********************************************************************************/

struct Base_Pin;
class Base_GraphNode;
class Base_OutputPin;
class Base_InputPin;
class SS_Boilerplate_Manager;
enum VECTOR_OPS : unsigned int;

enum NODE_TYPE {
    NODE_DEFAULT,
    NODE_BUILTIN,
    NODE_CONSTANT,
    NODE_PARAM,
    NODE_VARYING, 
    NODE_VECTOR_OP,
    NODE_CUSTOM,
    NODE_TERMINAL,
    NODE_BOILER_VAR
};

// BASE CLASS FOR NODE PINS
struct Base_Pin {
    Base_GraphNode* owner;
    int index;
    GLSL_TYPE type;
    bool bInput;
    std::string _name;

    ImVec2 get_size(float circle_off, float border);
    virtual void draw(ImDrawList* drawList, ImVec2 pos, float circle_off, float border) {};
    virtual ImVec2 get_pin_pos(float circle_off, float border, float* radius) {};

    virtual bool has_connections() { return false; }
    virtual void disconnect_all_from(SS_Graph* g) {};
};

// INPUT PIN CLASS
struct Base_InputPin : Base_Pin {
    Base_OutputPin* input = nullptr;
    void draw(ImDrawList* drawList, ImVec2 pos, float circle_off, float border);
    ImVec2 get_pin_pos(float circle_off, float border, float* radius) override;
    bool has_connections() override { return input; }
    void disconnect_all_from(SS_Graph* g) override;
};

// OUTPUT PIN CLASS
struct Base_OutputPin : Base_Pin {
    std::vector<Base_InputPin*> output;
    std::string get_pin_output_name();
    void draw(ImDrawList* drawList, ImVec2 pos, float circle_off, float border);
    ImVec2 get_pin_pos(float circle_off, float border, float* radius) override;
    bool has_connections() override { return output.size(); }
    void disconnect_all_from(SS_Graph* g) override;
};


// BASE CLASS FOR ALL GRAPH NODES, HANDLES MOST OF THE DRAWING
class Base_GraphNode {
public:
    ~Base_GraphNode();
    virtual void handle_destruction() {}
    virtual void set_bounds(float scale = 1);
    virtual void draw(ImDrawList* drawList, ImVec2 pos_offset = ImVec2(0, 0), bool is_hover = false);
    void draw_output_connects(ImDrawList* drawList, ImVec2 offset);
    bool is_hovering(ImVec2 mouse_pos);
    Base_Pin* get_hovered_pin(ImVec2 mouse_pos);
    bool is_display_button_hovered_over(ImVec2 p);

    void toggle_display() { is_display_up = !is_display_up; };

    virtual std::string request_output(int out_index) {return ""; };
    virtual std::string process_for_code() { return ""; };
    void set_shader_intermed(const std::string& frag_shad, const std::string& vert_shad);

    unsigned int get_most_restrictive_gentype_in_subgraph(Base_Pin* start_pin);
    unsigned int get_most_restrictive_gentype_in_subgraph(Base_Pin* start_pin, std::unordered_set<int>& processed_ids);

    bool only_first_out_connected();
    void propogate_gentype_in_subgraph(Base_Pin* start_pin, unsigned int type);
    void propogate_gentype_in_subgraph(Base_Pin* start_pin, unsigned int type, std::unordered_set<int>& processed_ids);
    void propogate_build_dirty();

    virtual NODE_TYPE get_node_type() { return NODE_DEFAULT; };

    virtual bool can_connect_pins(Base_InputPin* in_pin, Base_OutputPin* out_pin);
    virtual void inform_of_connect(Base_InputPin* in_pin, Base_OutputPin* out_pin) {}

    bool generate_intermed_image();
    void draw_to_intermed(unsigned int framebuffer, SS_Boilerplate_Manager* bp, std::vector<struct Parameter_Data*>& params);
    void draw_to_intermed_no_recompile(unsigned int framebuffer, std::vector<struct Parameter_Data*>& params);
    void update_vertex_shader_only(std::string vert_shader_code);
    void update_frag_shader_only(std::string frag_shader_code);
    
    unsigned int get_image_texture_id() { return nodes_rendered_texture; }
    virtual bool can_draw_intermed_image() { return !output_pins->type.is_matrix() && output_pins->type.arr_size == 1; ; };
    ImTextureID bind_and_get_image_texture();

    bool can_be_deleted() { return get_node_type() != NODE_TERMINAL; };


    // MEMBER VARIABLES
    int _id;
    ImVec2 _old_pos;
    ImVec2 _pos;
    ImVec2 _size;
    class ga_cube_component* _cube = nullptr;

    // BOUNDS
    ImVec2 rect_size;
    std::vector<ImVec2> in_pin_sizes;
    std::vector<ImVec2> out_pin_sizes;
    // WARNING : FOR EASE OF USE, THESE ARE RELATIVE TO MOST UPPER LEFT POSITION OF NODE     
    std::vector<ImVec2> in_pin_rel_pos;
    std::vector<ImVec2> out_pin_rel_pos;

    unsigned int nodes_rendered_texture;
    unsigned int nodes_depth_texture;
    std::string _frag_str;
    std::string _vert_str;

    ImVec2 display_panel_rel_pos;
    ImVec2 display_panel_rel_size;

    ImVec2 name_size;

    std::string _name;
    
    Base_InputPin* input_pins = nullptr;
    Base_OutputPin* output_pins = nullptr;
    int num_input, num_output;

    bool is_display_up = false;
    bool is_build_dirty = false;
};


struct Builtin_Node_Data; // forward declare
struct Constant_Node_Data;
struct Parameter_Data;
struct Vector_Op_Node_Data;
enum GRAPH_PARAM_GENTYPE : unsigned int;
enum GRAPH_PARAM_TYPE : unsigned int;

class Builtin_GraphNode : public Base_GraphNode {
public:
    Builtin_GraphNode(Builtin_Node_Data& data, int id, ImVec2 pos);
    
    std::string make_inline_function_call(std::string& format, const std::vector<std::string>& input_strs, int off = 0);
 
    NODE_TYPE get_node_type() { return NODE_BUILTIN; };
    
    std::string request_output(int out_index) override;
    std::string process_for_code() override;
    bool can_draw_intermed_image() override { return true; };

    // Returns the string which is usable for the output specified
        // could be a variable or a function, or a swizzling/new_vec
    // ASSUMES PROCESS CODE CALLED FIRST
    std::string _inliner;
    std::vector<std::string> pin_parse_out_names;
};

class Constant_Node : public Base_GraphNode {
public:
    GRAPH_PARAM_GENTYPE _data_gen;
    GRAPH_PARAM_TYPE _data_type;
    void* _data;

    Constant_Node(Constant_Node_Data& data, int id, ImVec2 pos);
    void handle_destruction() override {if (_data) free(_data); }
    NODE_TYPE get_node_type() { return NODE_CONSTANT; };

    bool can_draw_intermed_image() override { return !output_pins->type.is_matrix() && output_pins->type.arr_size == 1; };

    std::string request_output(int out_index) override;
    std::string process_for_code() override;

};

class Vector_Op_Node : public Base_GraphNode {
public:
    VECTOR_OPS _vec_op;

    Vector_Op_Node(Vector_Op_Node_Data& data, int id, ImVec2 pos);
    void make_vec_break(int s);
    void make_vec_make(int s);

     NODE_TYPE get_node_type() { return NODE_VECTOR_OP; };

    bool can_draw_intermed_image() override { return false; };

    std::string request_output(int out_index) override;
    std::string process_for_code() override;
};

class Param_Node : public Base_GraphNode {
public:
    Param_Node(Parameter_Data* data, int id, ImVec2 pos);

    NODE_TYPE get_node_type() { return NODE_PARAM; };

    bool can_draw_intermed_image() override { return !output_pins->type.is_matrix() && output_pins->type.arr_size == 1; ; };


    std::string request_output(int out_index) override;
    std::string process_for_code() override;

    void update_type_from_param(GLSL_TYPE type);
};

struct Boilerplate_Var_Data;
class Terminal_Node : public Base_GraphNode {
public:
    Terminal_Node(const std::vector<Boilerplate_Var_Data>& terminal_pins, int id, ImVec2 pos);
    bool can_draw_intermed_image() override { return true; };


    bool frag_node;
     NODE_TYPE get_node_type() { return NODE_TERMINAL; };
};

class Boilerplate_Var_Node : public Base_GraphNode {
public:
    Boilerplate_Var_Node(Boilerplate_Var_Data data, SS_Boilerplate_Manager* bp, int id, ImVec2 pos);
    bool can_draw_intermed_image() override { return !output_pins->type.is_matrix() && output_pins->type.arr_size == 1; ; };


    bool frag_node;
    SS_Boilerplate_Manager* _bp_manager;
    NODE_TYPE get_node_type() { return NODE_BOILER_VAR; };
    
    std::string request_output(int out_index) override;
    std::string process_for_code() override;
};
#endif