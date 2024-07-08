#ifndef SHADER_SCUPLTOR_SS_PINS_HPP
#define SHADER_SCUPLTOR_SS_PINS_HPP


#include <string>
#include <vector>
#include "ss_node_types.hpp"
#include "imgui/imgui.h"

class Base_GraphNode;

// BASE CLASS FOR NODE PINS
struct Base_Pin {
    Base_GraphNode* owner;
    int index;
    GLSL_TYPE type;
    bool bInput;
    std::string _name;

    ImVec2 GetSize(float circle_off, float border);
    virtual void Draw(ImDrawList* drawList, ImVec2 pos, float circle_off, float border) {};
    virtual ImVec2 GetPinPos(float circle_off, float border, float* radius) { return {0, 0}; };

    virtual bool HasConnections() { return false; }
    virtual void DisconnectAllFrom(bool reprop) {};

    virtual ~Base_Pin() = default;
};

struct Base_InputPin;
struct Base_OutputPin;

// INPUT PIN CLASS
struct Base_InputPin : Base_Pin {
    Base_OutputPin* input = nullptr;
    void Draw(ImDrawList* drawList, ImVec2 pos, float circle_off, float border) override;
    ImVec2 GetPinPos(float circle_off, float border, float* radius) override;
    bool HasConnections() override { return input; }
    void DisconnectAllFrom(bool reprop) override;
};

// OUTPUT PIN CLASS
struct Base_OutputPin : Base_Pin {
    std::vector<Base_InputPin*> output;
    std::string get_pin_output_name();
    void Draw(ImDrawList* drawList, ImVec2 pos, float circle_off, float border) override;
    ImVec2 GetPinPos(float circle_off, float border, float* radius) override;
    bool HasConnections() override { return not output.empty(); }
    void DisconnectAllFrom(bool reprop) override;
};


namespace PinOps {
    /**
     * Tracks through previously constructed graph to determine if adding an edge from in_pin to out_pin will result in
     * a cycle.
     * WARNING: This explicitly assumes m_nodes are Base_GraphNode. TODO: this is bad coupling.
     * @return True iff adding the edge would NOT result a cycle (i.e. it would maintain DAG).
     */
    bool CheckForDAGViolation(Base_InputPin *in_pin, Base_OutputPin *out_pin);
    bool ArePinsConnectable(Base_InputPin* in_pin, Base_OutputPin* out_pin);
    bool ConnectPins(Base_InputPin* in_pin, Base_OutputPin* out_pin);
    bool DisconnectPins(Base_InputPin* in_pin, Base_OutputPin* out_pin, bool reprop);
}


#endif //SHADER_SCUPLTOR_SS_PINS_HPP
