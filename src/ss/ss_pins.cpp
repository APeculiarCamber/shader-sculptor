//
// Created by idemaj on 7/3/24.
//

#include <algorithm>
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

void Base_InputPin::disconnect_all_from(bool reprop) {
    if (input) {
        auto* in_pin = this;
        auto* out_pin = input;
        in_pin->input = nullptr;
        auto& outvec = out_pin->output;
        out_pin->output.erase(std::find(outvec.begin(), outvec.end(), in_pin));

        if (reprop) {
            // INPUT PIN PROPOGATION
            if (in_pin->type.type_flags & GLSL_GenType) {
                unsigned int new_in_type = in_pin->owner->get_most_restrictive_gentype_in_subgraph(in_pin);
                in_pin->owner->propogate_gentype_in_subgraph(in_pin, new_in_type);
            }
            // OUTPUT PIN PROPOGATION
            if (out_pin->type.type_flags & GLSL_GenType) {
                unsigned int new_out_type = out_pin->owner->get_most_restrictive_gentype_in_subgraph(out_pin);
                out_pin->owner->propogate_gentype_in_subgraph(out_pin, new_out_type);
            }
        }
        in_pin->owner->propogate_build_dirty();
    }
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

void Base_OutputPin::disconnect_all_from(bool reprop) {
    std::vector<Base_InputPin*> output_COPY(output);
    for (Base_InputPin* i_pin : output_COPY)
        PinOps::DisconnectPins(i_pin, this, true);
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



// returns TRUE if DAG violation
bool PinOps::CheckForDAGViolation(Base_InputPin* in_pin, Base_OutputPin* out_pin) {
    std::queue<Base_GraphNode*> n_queue;
    Base_GraphNode* n;
    n_queue.push(in_pin->owner);
    while (!n_queue.empty()) {
        n = n_queue.front();
        n_queue.pop();
        if (n == out_pin->owner) return true;
        for (int p = 0; p < n->num_output; ++p)
            for (auto & o : n->output_pins[p].output)
                n_queue.push(o->owner);
    }
    return false;
}

// 0 for input change needed, 1 for no change needed, 2 for output changed needed
// -1 for failed
bool PinOps::ArePinsConnectable(Base_InputPin* in_pin, Base_OutputPin* out_pin) {
    bool in_accepeted = in_pin->owner->can_connect_pins(in_pin, out_pin);
    bool out_accepeted = in_pin->owner->can_connect_pins(in_pin, out_pin);
    bool dag_maintained = !CheckForDAGViolation(in_pin, out_pin);
    return in_accepeted && out_accepeted && dag_maintained;
}

bool PinOps::ConnectPins(Base_InputPin* in_pin, Base_OutputPin* out_pin) {
    if (!ArePinsConnectable(in_pin, out_pin)) return false;

    if (in_pin->input)
        DisconnectPins(in_pin, in_pin->input, true);

    GLSL_TYPE intersect_type = in_pin->type.IntersectCopy(out_pin->type);
    in_pin->owner->propogate_gentype_in_subgraph(in_pin, intersect_type.type_flags & GLSL_LenMask);
    out_pin->owner->propogate_gentype_in_subgraph(out_pin, intersect_type.type_flags & GLSL_LenMask);
    in_pin->owner->propogate_build_dirty();

    in_pin->input = out_pin;
    out_pin->output.push_back(in_pin);
    in_pin->owner->inform_of_connect(in_pin, out_pin);
    out_pin->owner->inform_of_connect(in_pin, out_pin);

    return true;
}


bool PinOps::DisconnectPins(Base_InputPin* in_pin, Base_OutputPin* out_pin, bool reprop) {
    in_pin->input = nullptr;
    auto ptr = std::find(out_pin->output.begin(), out_pin->output.end(), in_pin);
    out_pin->output.erase(ptr);

    if (reprop) {
        // INPUT PIN PROPOGATION
        if (in_pin->type.type_flags & GLSL_GenType) {
            unsigned int new_in_type = in_pin->owner->get_most_restrictive_gentype_in_subgraph(in_pin);
            in_pin->owner->propogate_gentype_in_subgraph(in_pin, new_in_type);
        }
        // OUTPUT PIN PROPOGATION
        if (out_pin->type.type_flags & GLSL_GenType) {
            unsigned int new_out_type = out_pin->owner->get_most_restrictive_gentype_in_subgraph(out_pin);
            out_pin->owner->propogate_gentype_in_subgraph(out_pin, new_out_type);
        }
    }
    in_pin->owner->propogate_build_dirty();
    return true;
}
