#ifndef SS_NODE
#define SS_NODE

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <unordered_set>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <memory>

#include "ss_data.h"
#include "../imgui/imgui.h"
#include "ss_node_types.hpp"
#include "ss_pins.hpp"
#include "ss_data.h"
#include "ga_cube_component.h"

#define NODE_TEXTURE_NULL 0xFFFFFFFF

// BASE CLASS FOR ALL GRAPH NODES, HANDLES MOST OF THE DRAWING
class Base_GraphNode {
public:
    virtual ~Base_GraphNode();
    virtual void SetBounds(float scale);
    virtual void Draw(ImDrawList* drawList, ImVec2 pos_offset, bool is_hover);
    void DrawOutputConnects(ImDrawList* drawList, ImVec2 offset);
    bool IsHovering(ImVec2 mouse_pos);
    Base_Pin* GetHoveredPin(ImVec2 mouse_pos);
    bool IsDisplayButtonHoveredOver(ImVec2 p);

    void ToggleDisplay() { is_display_up = !is_display_up; };

    virtual std::string RequestOutput(int out_index) = 0;
    virtual std::string ProcessForCode() = 0;
    void SetShaderCode(const std::string& frag_shad, const std::string& vert_shad);

    unsigned int GetMostRestrictiveGentypeInSubgraph(Base_Pin* start_pin);
    unsigned int GetMostRestrictiveGentypeInSubgraph_Rec(Base_Pin* start_pin, std::unordered_set<int>& processed_ids);

    void PropogateGentypeInSubgraph(Base_Pin* start_pin, unsigned int type);
    void PropogateGentypeInSubgraph_Rec(Base_Pin* start_pin, unsigned int type, std::unordered_set<int>& processed_ids);
    void PropogateBuildDirty();

    virtual NODE_TYPE GetNodeType() { return NODE_DEFAULT; };

    virtual bool CanConnectPins(Base_InputPin* in_pin, Base_OutputPin* out_pin);
    virtual void InformOfConnect(Base_InputPin* in_pin, Base_OutputPin* out_pin) {}

    bool GenerateIntermediateResultFrameBuffers();
    void CompileIntermediateCode(std::unique_ptr<ga_material>&& material);
    void DrawIntermediateResult(unsigned int framebuffer, const std::vector<std::unique_ptr<Parameter_Data>>& params);

    unsigned int GetImageTextureId() const { return nodes_rendered_texture; }
    virtual bool CanDrawIntermedImage() { return !output_pins[0].type.IsMatrix() && output_pins[0].type.arr_size == 1; ; };
    ImTextureID BindAndGetImageTexture();

    bool CanBeDeleted() { return GetNodeType() != NODE_TERMINAL; };

    void DisconnectAllPins();

    // GETTERS
    size_t GetInputPinCount() const { return input_pins.size(); }
    size_t GetOutputPinCount() const { return output_pins.size(); }
    const Base_InputPin& GetInputPin(int ind) const { return input_pins[ind]; }
    const Base_OutputPin& GetOutputPin(int ind) const {  return output_pins[ind]; }

    ImVec2 GetDrawPos() const { return _pos; }
    ImVec2 GetDrawOldPos() const { return _old_pos; }
    ImVec2 GetDrawRectSize() const { return rect_size; }
    ImVec2 GetDrawInputPinRelativePos(int ind) const { return in_pin_rel_pos[ind]; }
    ImVec2 GetDrawOutputPinRelativePos(int ind) const { return out_pin_rel_pos[ind]; }
    ImVec2 GetDrawInputPinSize(int ind) const { return in_pin_sizes[ind]; }
    ImVec2 GetDrawOutputPinSize(int ind) const { return out_pin_sizes[ind]; }

    bool GetHasDisplayUp() const { return is_display_up; }

    int GetID() const { return _id; }
    const std::string& GetName() const { return _name; }

    // SETTERS
    void SetName(const std::string& newName) {
        _name = newName;
    }
    void SetDrawOldPos(ImVec2 pos) {
        _old_pos = _pos = pos;
    }
    void SetDrawPos(ImVec2 pos) {
        _pos = pos;
    }

protected:
    // MEMBER VARIABLES
    int _id;
    ImVec2 _old_pos;
    ImVec2 _pos;
    std::unique_ptr<ga_cube_component> _cube = nullptr;

    // BOUNDS
    ImVec2 rect_size;
    std::vector<ImVec2> in_pin_sizes;
    std::vector<ImVec2> out_pin_sizes;
    // WARNING : FOR EASE OF USE, THESE ARE RELATIVE TO MOST UPPER LEFT POSITION OF NODE     
    std::vector<ImVec2> in_pin_rel_pos;
    std::vector<ImVec2> out_pin_rel_pos;

    unsigned int nodes_rendered_texture { NODE_TEXTURE_NULL };
    unsigned int nodes_depth_texture { NODE_TEXTURE_NULL };
    std::string _frag_str { }; // "#version 400\nvoid main() { gl_FragColor = vec4(0.5, 0.0, 1.0, 1.0); }"
    std::string _vert_str { }; // "#version 400\nlayout(location = 0) in vec3 in_vertex; void main() { gl_Position = vec4(in_vertex, 1.0); }"

    ImVec2 display_panel_rel_pos;
    ImVec2 display_panel_rel_size;

    ImVec2 name_size;

    std::string _name;
    
    std::vector<Base_InputPin> input_pins;
    std::vector<Base_OutputPin> output_pins;
    int num_input, num_output;

    bool is_display_up = false;
    bool is_build_dirty = false;
};



class Builtin_GraphNode : public Base_GraphNode {
public:
    Builtin_GraphNode(Builtin_Node_Data& data, int id, ImVec2 pos);

    ~Builtin_GraphNode() override;
    NODE_TYPE GetNodeType() override { return NODE_BUILTIN; };
    
    std::string RequestOutput(int out_index) override;
    std::string ProcessForCode() override;
    bool CanDrawIntermedImage() override { return true; };

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
    NODE_TYPE GetNodeType() override { return NODE_CONSTANT; };

    bool CanDrawIntermedImage() override { return !output_pins[0].type.IsMatrix() && output_pins[0].type.arr_size == 1; };

    std::string RequestOutput(int out_index) override;
    std::string ProcessForCode() override;

};

class Vector_Op_Node : public Base_GraphNode {
public:
    VECTOR_OPS _vec_op;

    Vector_Op_Node(Vector_Op_Node_Data& data, int id, ImVec2 pos);
    void make_vec_break(int s);
    void make_vec_make(int s);

     NODE_TYPE GetNodeType() override { return NODE_VECTOR_OP; };

    bool CanDrawIntermedImage() override { return false; };

    std::string RequestOutput(int out_index) override;
    std::string ProcessForCode() override;
};

class Param_Node : public Base_GraphNode {
public:
    int _paramID;
    Param_Node(Parameter_Data* data, int id, ImVec2 pos);
    ~Param_Node() override;

    NODE_TYPE GetNodeType() override { return NODE_PARAM; };

    bool CanDrawIntermedImage() override { return !output_pins[0].type.IsMatrix() && output_pins[0].type.arr_size == 1; ; };


    std::string RequestOutput(int out_index) override;
    std::string ProcessForCode() override;

    void update_type_from_param(GLSL_TYPE type);
};

struct Boilerplate_Var_Data;
class Terminal_Node : public Base_GraphNode {
public:
    ~Terminal_Node() override;
    Terminal_Node(const std::vector<Boilerplate_Var_Data>& terminal_pins, int id, ImVec2 pos);
    bool CanDrawIntermedImage() override { return true; };

    std::string RequestOutput(int out_index) override { return {}; }
    std::string ProcessForCode() override { return {}; }

    bool frag_node;
     NODE_TYPE GetNodeType() override { return NODE_TERMINAL; };
};


class SS_Boilerplate_Manager;
class Boilerplate_Var_Node : public Base_GraphNode {
public:
    Boilerplate_Var_Node(Boilerplate_Var_Data data, SS_Boilerplate_Manager* bp, int id, ImVec2 pos);
    bool CanDrawIntermedImage() override { return not output_pins[0].type.IsMatrix() && output_pins[0].type.arr_size == 1; };


    bool frag_node;
    SS_Boilerplate_Manager* _bpManager;
    NODE_TYPE GetNodeType() override { return NODE_BOILER_VAR; };
    
    std::string RequestOutput(int out_index) override;
    std::string ProcessForCode() override;
};
#endif