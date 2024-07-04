//
// Created by idemaj on 7/3/24.
//

#include "ss_pins.h"
#include "ss_parser.hpp"
#include "ss_graph.hpp"

ImVec2 Base_Pin::get_size(float circle_off, float border) {
    ImVec2 text_size = ImGui::CalcTextSize(_name.c_str());
    float circle_rad = text_size.y / 2;
    return text_size + ImVec2(circle_rad + circle_off + border * 2, border * 2);
}

ImVec2 Base_InputPin::get_pin_pos(float circle_off, float border, float* rad) {
    assert(rad);
    ImVec2 pos = owner->_pos - ImVec2(owner->rect_size.x * .5f, owner->rect_size.y * .5f);
    ImVec2 bound_pos = owner->in_pin_rel_pos[index];
    *rad = ImGui::CalcTextSize(_name.c_str()).y;
    return pos + bound_pos + ImVec2(*rad + border, *rad + border);
}

void Base_InputPin::draw(ImDrawList* d, ImVec2 pos, float circle_off, float border) {
    ImVec2 text_size = ImGui::CalcTextSize(_name.c_str());
    ImU32 color = SS_Parser::type_to_color(type);

    float circle_rad = text_size.y / 2;

    d->AddCircleFilled(pos + ImVec2(border + circle_rad, border + circle_rad), circle_rad, color);
    d->AddText(pos + ImVec2(border + 2*circle_rad + circle_off, border), 0xffffffff, _name.c_str());
}

void Base_InputPin::disconnect_all_from(SS_Graph* g) {
    if (input)
        g->DisconnectPins(this, input);
}

ImVec2 Base_OutputPin::get_pin_pos(float circle_off, float border, float* rad) {
    assert(rad);
    ImVec2 pos = owner->_pos - ImVec2(owner->rect_size.x * .5f, owner->rect_size.y * .5f);
    ImVec2 bound_pos = owner->out_pin_rel_pos[index];
    ImVec2 text_size = ImGui::CalcTextSize(_name.c_str());
    float off_x = text_size.x;
    *rad = text_size.y;
    return pos + bound_pos + ImVec2(*rad + border + circle_off + off_x, *rad + border);
}

void Base_OutputPin::disconnect_all_from(SS_Graph* g) {
    std::vector<Base_InputPin*> output_COPY(output);

    for (Base_InputPin* i_pin : output_COPY)
        g->DisconnectPins(i_pin, this);
}

void Base_OutputPin::draw(ImDrawList* d, ImVec2 pos, float circle_off, float border) {
    ImVec2 text_size = ImGui::CalcTextSize(_name.c_str());
    ImU32 color = SS_Parser::type_to_color(type);

    float circle_rad = text_size.y / 2;

    d->AddText(pos + ImVec2(border, border), 0xffffffff, _name.c_str());
    d->AddCircleFilled(pos + ImVec2(text_size.x + border + circle_off + circle_rad, border + circle_rad), circle_rad, color);
}

std::string Base_OutputPin::get_pin_output_name() {
    return owner->request_output(index);
}