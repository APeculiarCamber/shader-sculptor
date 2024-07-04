//
// Created by idemaj on 7/3/24.
//

#ifndef SHADER_SCUPLTOR_SS_PINS_H
#define SHADER_SCUPLTOR_SS_PINS_H


#include <string>
#include <vector>
#include "ss_node_types.h"
#include "imgui/imgui.h"

/*********************************************************************************
 * ********************************************************************************
 * ********************************************************************************/

struct Base_Pin;
class Base_GraphNode;
class SS_Graph;

// BASE CLASS FOR NODE PINS
struct Base_Pin {
    Base_GraphNode* owner;
    int index;
    GLSL_TYPE type;
    bool bInput;
    std::string _name;

    ImVec2 get_size(float circle_off, float border);
    virtual void draw(ImDrawList* drawList, ImVec2 pos, float circle_off, float border) {};
    virtual ImVec2 get_pin_pos(float circle_off, float border, float* radius) { return {0, 0}; };

    virtual bool has_connections() { return false; }
    virtual void disconnect_all_from(SS_Graph* g) {};

    virtual ~Base_Pin() = default;
};

struct Base_InputPin;
struct Base_OutputPin;

// INPUT PIN CLASS
struct Base_InputPin : Base_Pin {
    Base_OutputPin* input = nullptr;
    void draw(ImDrawList* drawList, ImVec2 pos, float circle_off, float border) override;
    ImVec2 get_pin_pos(float circle_off, float border, float* radius) override;
    bool has_connections() override { return input; }
    void disconnect_all_from(SS_Graph* g) override;
};

// OUTPUT PIN CLASS
struct Base_OutputPin : Base_Pin {
    std::vector<Base_InputPin*> output;
    std::string get_pin_output_name();
    void draw(ImDrawList* drawList, ImVec2 pos, float circle_off, float border) override;
    ImVec2 get_pin_pos(float circle_off, float border, float* radius) override;
    bool has_connections() override { return not output.empty(); }
    void disconnect_all_from(SS_Graph* g) override;
};

#endif //SHADER_SCUPLTOR_SS_PINS_H
