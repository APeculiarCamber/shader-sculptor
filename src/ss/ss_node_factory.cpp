#include "ss_node_factory.hpp"
#include "ss_node.hpp"
#include "ss_parser.hpp"
#include "ss_graph.hpp"

#include <vector>
#include <fstream>
#include <algorithm>
#include <string>


// ALLOWS reading from files to create builtin nodes and vector nodes and type nodes?
// ALLOWS reading from custom file to create custom nodes
// ALLOWS a request to get unique, state-agnostic ID for any given node, likely a name...

std::string string_to_lower(std::string str) {
    
    std::transform(str.begin(), str.end(), str.begin(), [](char c) { return tolower(c); });
    return str;
}

bool SS_Node_Factory::read_builtin_file(const std::string& file) {
    std::ifstream iff(file);
    if (iff.bad()) return false;

    std::string token;
    while (iff >> token) {
        std::unordered_map<std::string, int> inputs_map; 
        node_datas.emplace_back();
        node_datas.back().in_liner = "";

        std::string str_type, str_name;
        if (token == "out") {
            iff >> str_type >> str_name;
            GLSL_TYPE t = SS_Parser::string_to_type(str_type);
            node_datas.back().out_vars.emplace_back(t, str_name);
            iff >> token; // token is =
            iff >> token; // token should be non-var element
        }
        // At this point, token should be non-processed, non-var element
        node_datas.back().in_liner += token;

        // handle variables
        while (iff >> token) {
            bool is_out = token == "out";
            bool is_in = token == "in";
            size_t end_t = token.find(';');
            if (is_out) {
                iff >> str_type >> str_name;
                node_datas.back().out_vars.emplace_back(SS_Parser::string_to_type(str_type), str_name);
                node_datas.back().in_liner += "out \%o";
                node_datas.back().in_liner += std::to_string(node_datas.back().out_vars.size()); // out var number
            } 
            else if (is_in) {
                iff >> str_type >> str_name;
                if (inputs_map.find(str_name) == inputs_map.end()) {
                    node_datas.back().in_vars.emplace_back(SS_Parser::string_to_type(str_type), str_name);
                    inputs_map.insert(std::make_pair(str_name, node_datas.back().in_vars.size()));
                }
                node_datas.back().in_liner += " \%i";
                node_datas.back().in_liner += std::to_string(inputs_map[str_name]); // in var number
            } 
            else if (end_t == std::string::npos) {
                // NON_VAR
                node_datas.back().in_liner += token;
            } 
            else /* CLOSE OUT FUNCTION */ {
                // CLOSE OUT FUNCTION
                node_datas.back().in_liner += token.substr(0, end_t);
                iff >> node_datas.back()._name;
                break;
            }
        }
    }
    return true;
}


const std::vector<Builtin_Node_Data>& SS_Node_Factory::get_matching_builtin_nodes(const std::string& query) {
    if (last_q == query)
        return last_data_builtin;

    // empty, give all
    if (query.empty()) {
        last_data_builtin = node_datas;
        return last_data_builtin;
    }

    // collect all with shared substring
    last_data_builtin.clear();
    std::copy_if(node_datas.begin(), node_datas.end(), 
        std::back_inserter(last_data_builtin),
        [query](Builtin_Node_Data& nd) {return string_to_lower(nd._name).find(query) != std::string::npos; } );
    return last_data_builtin;
}

const std::vector<Constant_Node_Data>& SS_Node_Factory::get_matching_constant_nodes(std::string query) {
    query = string_to_lower(query);
    bool all_valid = std::string("constant").find(query) != std::string::npos;
    std::string scalar("number float int double scalar");
    std::string vec2("vec2 vector2");
    std::string vec3("vec3 vector3");
    std::string vec4("vec4 vector4");

    std::string mat2("mat2 matrix2");
    std::string mat3("mat3 matrix3");
    std::string mat4("mat4 matrix4");
    last_data_constant.clear();
    if (all_valid || scalar.find(query) != std::string::npos)
        last_data_constant.push_back(Constant_Node_Data{"Scalar", SS_Scalar, SS_Float});
    if (all_valid || vec2.find(query) != std::string::npos)
        last_data_constant.push_back(Constant_Node_Data{"Vec2", SS_Vec2, SS_Float});
    if (all_valid || vec3.find(query) != std::string::npos)
        last_data_constant.push_back(Constant_Node_Data{"Vec3", SS_Vec3, SS_Float});
    if (all_valid || vec4.find(query) != std::string::npos)
        last_data_constant.push_back(Constant_Node_Data{"Vec4", SS_Vec4, SS_Float});
    if (all_valid || mat2.find(query) != std::string::npos)
        last_data_constant.push_back(Constant_Node_Data{"Mat2", SS_Mat2, SS_Float});
    if (all_valid || mat3.find(query) != std::string::npos)
        last_data_constant.push_back(Constant_Node_Data{"Mat3", SS_Mat3, SS_Float});
    if (all_valid || mat4.find(query) != std::string::npos)
        last_data_constant.push_back(Constant_Node_Data{"Mat4", SS_Mat4, SS_Float});
    return last_data_constant;
}

const std::vector<Vector_Op_Node_Data>& SS_Node_Factory::get_matching_vector_nodes(std::string query) {
    query = string_to_lower(query);

    bool all_valid = std::string("swizzle vector swizzle").find(query) != std::string::npos;
    std::string br_vec2("vec2 break vec2 vector2 break vector2");
    std::string br_vec3("vec3 break vec3 vector3 break vector3");
    std::string br_vec4("vec4 break vec4 vector4 break vector4");
    std::string mk_vec2("vec2 make vec2 vector2 make vector2");
    std::string mk_vec3("vec3 make vec3 vector3 make vector3");
    std::string mk_vec4("vec4 make vec4 vector4 make vector4");
    
    last_data_vec_op.clear();
    if (all_valid || br_vec2.find(query) != std::string::npos)
        last_data_vec_op.emplace_back(Vector_Op_Node_Data{"break vec2", VEC_BREAK2_OP});
    if (all_valid || br_vec3.find(query) != std::string::npos)
        last_data_vec_op.push_back(Vector_Op_Node_Data{"break vec3", VEC_BREAK3_OP});
    if (all_valid || br_vec4.find(query) != std::string::npos)
        last_data_vec_op.push_back(Vector_Op_Node_Data{"break vec4", VEC_BREAK4_OP});
    
    if (all_valid || mk_vec2.find(query) != std::string::npos)
        last_data_vec_op.push_back(Vector_Op_Node_Data{"make vec2", VEC_MAKE2_OP});
    if (all_valid || mk_vec3.find(query) != std::string::npos)
        last_data_vec_op.push_back(Vector_Op_Node_Data{"make vec3", VEC_MAKE3_OP});
    if (all_valid || mk_vec4.find(query) != std::string::npos)
        last_data_vec_op.push_back(Vector_Op_Node_Data{"make vec4", VEC_MAKE4_OP});
    return last_data_vec_op;
}

std::vector<Parameter_Data*> SS_Node_Factory::get_matching_param_nodes
    (std::string query, const std::vector<Parameter_Data*>& data_in) {
    query = string_to_lower(query);
    bool all_valid =  std::string("paramsparameters").find(query) != std::string::npos;
    std::vector<Parameter_Data*> filteredParams;
    for (Parameter_Data* p_data : data_in) {
        std::string lower_param = string_to_lower(std::string(p_data->GetName()));
        if (all_valid || lower_param.find(query) != std::string::npos) {
            filteredParams.push_back(p_data);
        }
    }
    return filteredParams;
}

std::vector<Parameter_Data *> SS_Node_Factory::get_matching_param_nodes(std::string query,
                                                                        const std::vector<std::unique_ptr<Parameter_Data>> &data_in) {
    query = string_to_lower(query);
    bool all_valid =  std::string("paramsparameters").find(query) != std::string::npos;
    std::vector<Parameter_Data*> filteredParams;
    for (const auto& p_data : data_in) {
        std::string lower_param = string_to_lower(std::string(p_data->GetName()));
        if (all_valid || lower_param.find(query) != std::string::npos) {
            filteredParams.push_back(p_data.get());
        }
    }
    return filteredParams;
}

std::vector<Boilerplate_Var_Data> SS_Node_Factory::get_matching_boilerplate_nodes(
    std::string query, const SS_Boilerplate_Manager* bp_manager) {
        query = string_to_lower(query);
        std::vector<Boilerplate_Var_Data> dataBP;

        bool all_valid = std::string("boilerplatedefault").find(query) != std::string::npos;
        for (auto bp_var : bp_manager->GetUsableVariables()) {
            if (all_valid || string_to_lower(std::string(bp_var._name)).find(query) != std::string::npos)
                dataBP.push_back(bp_var);
        }
        return dataBP;
    }



void SS_Node_Factory::cache_last_query(const std::string query) {
    last_q = query;
}




Builtin_GraphNode* SS_Node_Factory::build_builtin_node(Builtin_Node_Data& node_data, int id, ImVec2 pos, unsigned int fb) {
    Builtin_GraphNode* n = new Builtin_GraphNode(node_data, id, pos);
    std::vector<Parameter_Data*> nil;
    n->GenerateIntermediateResultFrameBuffers();
    // n->CompileIntermediateCode(fb, nil);
    return n;
}

Constant_Node* SS_Node_Factory::build_constant_node(Constant_Node_Data& node_data, int id, ImVec2 pos, unsigned int fb) {
    Constant_Node* n = new Constant_Node(node_data, id, pos);
    std::vector<Parameter_Data*> nil;
    n->GenerateIntermediateResultFrameBuffers();
    // n->CompileIntermediateCode(fb, nil);
    return n;
}

Vector_Op_Node* SS_Node_Factory::build_vec_op_node(Vector_Op_Node_Data& node_data, int id, ImVec2 pos, unsigned int fb) {
    Vector_Op_Node* n = new Vector_Op_Node(node_data, id, pos);
    return n;
}

Param_Node* SS_Node_Factory::build_param_node(Parameter_Data* param_data, int id, ImVec2 pos, unsigned int fb) {
    Param_Node* n = new Param_Node(param_data, id, pos);
    std::vector<Parameter_Data*> nil;
    n->GenerateIntermediateResultFrameBuffers();
    return n;
}

Boilerplate_Var_Node* SS_Node_Factory::build_boilerplate_var_node(Boilerplate_Var_Data& data, SS_Boilerplate_Manager* bm, int id, ImVec2 pos, unsigned int fb) {
    Boilerplate_Var_Node* n = new Boilerplate_Var_Node(data, bm, id, pos);
    std::vector<Parameter_Data*> nil;
    n->GenerateIntermediateResultFrameBuffers();
    return n;
}

