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

#include "ss_data.hpp"
#include "../imgui/imgui.h"
#include "ss_node_types.hpp"
#include "ss_pins.hpp"
#include "ss_data.hpp"
#include "ga_cube_component.h"

#define NODE_TEXTURE_NULL 0xFFFFFFFF

/**
 * Base class for all graph nodes, handles drawing and display management.
 */
class Base_GraphNode {
public:
    virtual ~Base_GraphNode();
    virtual void SetBounds(float scale);
    virtual void Draw(ImDrawList* drawList, ImVec2 pos_offset, bool is_hover);
    void DrawOutputConnects(ImDrawList* drawList, ImVec2 offset);
    bool IsHovering(ImVec2 mouse_pos);
    Base_Pin* GetHoveredPin(ImVec2 mouse_pos);

    // Toggle intermediate display
    void ToggleDisplay() { m_isDisplayUp = !m_isDisplayUp; };
    bool IsDisplayButtonHoveredOver(ImVec2 p);

    virtual std::string RequestOutput(int out_index) = 0;
    virtual std::string ProcessForCode() = 0;
    void SetShaderCode(const std::string& frag_shad, const std::string& vert_shad);

    unsigned int GetMostRestrictiveGentypeInSubgraph(Base_Pin* start_pin);
    void PropagateGentypeInSubgraph(Base_Pin* start_pin, unsigned int type);
    void PropagateBuildDirty();

    virtual NODE_TYPE GetNodeType() { return NODE_DEFAULT; };

    virtual bool CanConnectPins(Base_InputPin* in_pin, Base_OutputPin* out_pin);
    virtual void InformOfConnect(Base_InputPin* in_pin, Base_OutputPin* out_pin) {}

    bool GenerateIntermediateResultFrameBuffers();
    void CompileIntermediateCode(std::unique_ptr<ga_material>&& material);
    void DrawIntermediateResult(unsigned int framebuffer, const std::vector<std::unique_ptr<Parameter_Data>>& params);

    unsigned int GetImageTextureId() const { return m_nodesRenderedTexture; }
    virtual bool CanDrawIntermedImage() { return !m_outputPins[0].type.IsMatrix() && m_outputPins[0].type.arr_size == 1; ; };
    ImTextureID BindAndGetImageTexture();

    bool CanBeDeleted() { return GetNodeType() != NODE_TERMINAL; };

    void DisconnectAllPins();

    // GETTERS
    int GetInputPinCount() const { return (int)m_inputPins.size(); }
    int GetOutputPinCount() const { return (int)m_outputPins.size(); }
    const Base_InputPin& GetInputPin(int ind) const { return m_inputPins[ind]; }
    const Base_OutputPin& GetOutputPin(int ind) const {  return m_outputPins[ind]; }

    ImVec2 GetDrawPos() const { return m_pos; }
    ImVec2 GetDrawOldPos() const { return m_oldPos; }
    ImVec2 GetDrawRectSize() const { return m_rectSize; }
    ImVec2 GetDrawInputPinRelativePos(int ind) const { return m_inPinRelPos[ind]; }
    ImVec2 GetDrawOutputPinRelativePos(int ind) const { return m_outPinRelPos[ind]; }
    ImVec2 GetDrawInputPinSize(int ind) const { return m_inPinSizes[ind]; }
    ImVec2 GetDrawOutputPinSize(int ind) const { return m_outPinSizes[ind]; }

    bool GetHasDisplayUp() const { return m_isDisplayUp; }

    int GetID() const { return m_id; }
    const std::string& GetName() const { return m_name; }

    // SETTERS
    void SetName(const std::string& newName) {
        m_name = newName;
    }
    void SetDrawOldPos(ImVec2 pos) {
        m_oldPos = m_pos = pos;
    }
    void SetDrawPos(ImVec2 pos) {
        m_pos = pos;
    }

protected:
    unsigned int GetMostRestrictiveGentypeInSubgraph_Rec(Base_Pin* start_pin, std::unordered_set<int>& processed_ids);
    void PropogateGentypeInSubgraph_Rec(Base_Pin* start_pin, unsigned int type, std::unordered_set<int>& processed_ids);

    int m_id;
    ImVec2 m_oldPos;
    ImVec2 m_pos;
    std::unique_ptr<ga_cube_component> m_cube = nullptr;

    // BOUNDS
    ImVec2 m_rectSize;
    std::vector<ImVec2> m_inPinSizes;
    std::vector<ImVec2> m_outPinSizes;

    // WARNING : FOR EASE OF USE, THESE ARE RELATIVE TO MOST UPPER LEFT POSITION OF NODE     
    std::vector<ImVec2> m_inPinRelPos;
    // WARNING : FOR EASE OF USE, THESE ARE RELATIVE TO MOST UPPER LEFT POSITION OF NODE
    std::vector<ImVec2> m_outPinRelPos;

    unsigned int m_nodesRenderedTexture {NODE_TEXTURE_NULL };
    unsigned int m_nodesDepthTexture {NODE_TEXTURE_NULL };
    std::string m_fragStr { }; // "#version 400\nvoid main() { gl_FragColor = vec4(0.5, 0.0, 1.0, 1.0); }"
    std::string m_vertStr { }; // "#version 400\nlayout(location = 0) in vec3 in_vertex; void main() { gl_Position = vec4(in_vertex, 1.0); }"

    ImVec2 m_displayPanelRelPos;
    ImVec2 m_displayPanelRelSize;

    ImVec2 m_nameRelSize;

    std::string m_name;
    
    std::vector<Base_InputPin> m_inputPins;
    std::vector<Base_OutputPin> m_outputPins;
    int m_numInput, m_numOutput;

    bool m_isDisplayUp = false;
    bool m_isBuildDirty = false;
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

    bool CanDrawIntermedImage() override { return !m_outputPins[0].type.IsMatrix() && m_outputPins[0].type.arr_size == 1; };

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

    bool CanDrawIntermedImage() override { return !m_outputPins[0].type.IsMatrix() && m_outputPins[0].type.arr_size == 1; ; };


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
    bool CanDrawIntermedImage() override { return not m_outputPins[0].type.IsMatrix() && m_outputPins[0].type.arr_size == 1; };


    bool frag_node;
    SS_Boilerplate_Manager* _bpManager;
    NODE_TYPE GetNodeType() override { return NODE_BOILER_VAR; };
    
    std::string RequestOutput(int out_index) override;
    std::string ProcessForCode() override;
};
#endif