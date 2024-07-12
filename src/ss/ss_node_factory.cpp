#include "ss_node_factory.hpp"
#include "ss_node.hpp"
#include "ss_parser.hpp"
#include "ss_graph.hpp"

#include <vector>
#include <fstream>
#include <algorithm>
#include <string>



bool SS_Node_Factory::bBoilerplateInitialized = false;
std::vector<Boilerplate_Var_Data> SS_Node_Factory::boilerplateVarDatas{};
bool SS_Node_Factory::InitReadInBoilerplateParams(const std::vector<Boilerplate_Var_Data> &varData) {
    assert((not bBoilerplateInitialized)
        and "Error: boilerplate parameters can only be initialized once for singleton node factory.");
    boilerplateVarDatas = varData;
    bBoilerplateInitialized = true;
    return true;
}

bool SS_Node_Factory::bNodeDataInitialized = false;
std::vector<Builtin_Node_Data> SS_Node_Factory::nodeDatas{};
bool SS_Node_Factory::InitReadBuiltinFile(const std::string& file) {
    assert((not bNodeDataInitialized)
           and "Error: node data can only be initialized once for singleton node factory.");
    std::ifstream iff(file);
    if (iff.bad()) return false;
    if (not nodeDatas.empty()) return false;

    std::string token;
    while (iff >> token) {
        std::unordered_map<std::string, int> inputs_map; 
        nodeDatas.emplace_back();
        nodeDatas.back().in_liner = "";

        std::string str_type, str_name;
        if (token == "out") {
            iff >> str_type >> str_name;
            GLSL_TYPE t = SS_Parser::StringToGLSLType(str_type);
            nodeDatas.back().out_vars.emplace_back(t, str_name);
            iff >> token; // token is =
            iff >> token; // token should be non-var element
        }
        // At this point, token should be non-processed, non-var element
        nodeDatas.back().in_liner += token;

        // handle variables
        while (iff >> token) {
            bool is_out = token == "out";
            bool is_in = token == "in";
            size_t end_t = token.find(';');
            if (is_out) {
                iff >> str_type >> str_name;
                nodeDatas.back().out_vars.emplace_back(SS_Parser::StringToGLSLType(str_type), str_name);
                nodeDatas.back().in_liner += "out \%o";
                nodeDatas.back().in_liner += std::to_string(nodeDatas.back().out_vars.size()); // out var number
            } 
            else if (is_in) {
                iff >> str_type >> str_name;
                if (inputs_map.find(str_name) == inputs_map.end()) {
                    nodeDatas.back().in_vars.emplace_back(SS_Parser::StringToGLSLType(str_type), str_name);
                    inputs_map.insert(std::make_pair(str_name, nodeDatas.back().in_vars.size()));
                }
                nodeDatas.back().in_liner += " \%i";
                nodeDatas.back().in_liner += std::to_string(inputs_map[str_name]); // in var number
            } 
            else if (end_t == std::string::npos) {
                // NON_VAR
                nodeDatas.back().in_liner += token;
            } 
            else /* CLOSE OUT FUNCTION */ {
                // CLOSE OUT FUNCTION
                nodeDatas.back().in_liner += token.substr(0, end_t);
                iff >> nodeDatas.back()._name;
                break;
            }
        }
    }
    bNodeDataInitialized = true;
    return true;
}


std::vector<Builtin_Node_Data> SS_Node_Factory::GetMatchingBuiltinNodes(const std::string& query) {
    // empty, give all
    if (query.empty()) {
        return nodeDatas;
    }

    // collect all with shared substring
    std::vector<Builtin_Node_Data> builtinDatas;
    std::copy_if(nodeDatas.begin(), nodeDatas.end(),
                 std::back_inserter(builtinDatas),
                 [query](Builtin_Node_Data& nd) {return SS_Parser::StringToLower(nd._name).find(query) != std::string::npos; } );
    return builtinDatas;
}

std::vector<Constant_Node_Data> SS_Node_Factory::GetMatchingConstantNodes(const std::string& query) {
    std::string queryLower = SS_Parser::StringToLower(query);
    bool all_valid = std::string("constant").find(queryLower) != std::string::npos;
    std::string scalar("number float int double scalar");
    std::string vec2("vec2 vector2");
    std::string vec3("vec3 vector3");
    std::string vec4("vec4 vector4");

    std::string mat2("mat2 matrix2");
    std::string mat3("mat3 matrix3");
    std::string mat4("mat4 matrix4");

    std::vector<Constant_Node_Data> constantDatas;
    if (all_valid || scalar.find(queryLower) != std::string::npos)
        constantDatas.push_back(Constant_Node_Data{"Scalar", SS_Scalar, SS_Float});
    if (all_valid || vec2.find(queryLower) != std::string::npos)
        constantDatas.push_back(Constant_Node_Data{"Vec2", SS_Vec2, SS_Float});
    if (all_valid || vec3.find(queryLower) != std::string::npos)
        constantDatas.push_back(Constant_Node_Data{"Vec3", SS_Vec3, SS_Float});
    if (all_valid || vec4.find(queryLower) != std::string::npos)
        constantDatas.push_back(Constant_Node_Data{"Vec4", SS_Vec4, SS_Float});
    if (all_valid || mat2.find(queryLower) != std::string::npos)
        constantDatas.push_back(Constant_Node_Data{"Mat2", SS_Mat2, SS_Float});
    if (all_valid || mat3.find(queryLower) != std::string::npos)
        constantDatas.push_back(Constant_Node_Data{"Mat3", SS_Mat3, SS_Float});
    if (all_valid || mat4.find(queryLower) != std::string::npos)
        constantDatas.push_back(Constant_Node_Data{"Mat4", SS_Mat4, SS_Float});
    return constantDatas;
}

std::vector<Vector_Op_Node_Data> SS_Node_Factory::GetMatchingVectorNodes(const std::string& query) {
    std::string queryLower = SS_Parser::StringToLower(query);

    bool all_valid = std::string("swizzle vector swizzle").find(queryLower) != std::string::npos;
    std::string br_vec2("vec2 break vec2 vector2 break vector2");
    std::string br_vec3("vec3 break vec3 vector3 break vector3");
    std::string br_vec4("vec4 break vec4 vector4 break vector4");
    std::string mk_vec2("vec2 make vec2 vector2 make vector2");
    std::string mk_vec3("vec3 make vec3 vector3 make vector3");
    std::string mk_vec4("vec4 make vec4 vector4 make vector4");

    std::vector<Vector_Op_Node_Data> opDatas;
    if (all_valid || br_vec2.find(queryLower) != std::string::npos)
        opDatas.push_back(Vector_Op_Node_Data{"break vec2", VEC_BREAK2_OP});
    if (all_valid || br_vec3.find(queryLower) != std::string::npos)
        opDatas.push_back(Vector_Op_Node_Data{"break vec3", VEC_BREAK3_OP});
    if (all_valid || br_vec4.find(queryLower) != std::string::npos)
        opDatas.push_back(Vector_Op_Node_Data{"break vec4", VEC_BREAK4_OP});
    
    if (all_valid || mk_vec2.find(queryLower) != std::string::npos)
        opDatas.push_back(Vector_Op_Node_Data{"make vec2", VEC_MAKE2_OP});
    if (all_valid || mk_vec3.find(queryLower) != std::string::npos)
        opDatas.push_back(Vector_Op_Node_Data{"make vec3", VEC_MAKE3_OP});
    if (all_valid || mk_vec4.find(queryLower) != std::string::npos)
        opDatas.push_back(Vector_Op_Node_Data{"make vec4", VEC_MAKE4_OP});
    return opDatas;
}

std::vector<Parameter_Data*> SS_Node_Factory::GetMatchingParamNodes
    (std::string query, const std::vector<Parameter_Data*>& data_in) {
    query = SS_Parser::StringToLower(query);
    bool all_valid =  std::string("paramsparameters").find(query) != std::string::npos;
    std::vector<Parameter_Data*> filteredParams;
    for (Parameter_Data* p_data : data_in) {
        std::string lower_param = SS_Parser::StringToLower(std::string(p_data->GetName()));
        if (all_valid || lower_param.find(query) != std::string::npos) {
            filteredParams.push_back(p_data);
        }
    }
    return filteredParams;
}

std::vector<Parameter_Data *> SS_Node_Factory::GetMatchingParamNodes(std::string query,
                                                                     const std::vector<std::unique_ptr<Parameter_Data>> &data_in) {
    query = SS_Parser::StringToLower(query);
    bool all_valid =  std::string("paramsparameters").find(query) != std::string::npos;
    std::vector<Parameter_Data*> filteredParams;
    for (const auto& p_data : data_in) {
        std::string lower_param = SS_Parser::StringToLower(std::string(p_data->GetName()));
        if (all_valid || lower_param.find(query) != std::string::npos) {
            filteredParams.push_back(p_data.get());
        }
    }
    return filteredParams;
}

std::vector<Boilerplate_Var_Data> SS_Node_Factory::GetMatchingBoilerplateNodes(const std::string& query) {
        std::string queryLower = SS_Parser::StringToLower(query);
        std::vector<Boilerplate_Var_Data> dataBP;

        bool all_valid = std::string("boilerplatedefault").find(query) != std::string::npos;
        for (const auto& bp_var : boilerplateVarDatas) {
            if (all_valid || SS_Parser::StringToLower(std::string(bp_var._name)).find(query) != std::string::npos)
                dataBP.push_back(bp_var);
        }
        return dataBP;
    }




Builtin_GraphNode* SS_Node_Factory::BuildBuiltinNode(Builtin_Node_Data& node_data, int id, ImVec2 pos) {
    auto* n = new Builtin_GraphNode(node_data, id, pos);
    n->GenerateIntermediateResultFrameBuffers();
    // std::vector<Parameter_Data*> nil;
    // n->CompileIntermediateCode(fb, nil);
    return n;
}

Constant_Node* SS_Node_Factory::BuildConstantNode(Constant_Node_Data& node_data, int id, ImVec2 pos) {
    auto* n = new Constant_Node(node_data, id, pos);
    n->GenerateIntermediateResultFrameBuffers();
    // std::vector<Parameter_Data*> nil;
    // n->CompileIntermediateCode(fb, nil);
    return n;
}

Vector_Op_Node* SS_Node_Factory::BuildVecOpNode(Vector_Op_Node_Data& node_data, int id, ImVec2 pos) {
    auto* n = new Vector_Op_Node(node_data, id, pos);
    return n;
}

Param_Node* SS_Node_Factory::BuildParamNode(Parameter_Data* param_data, int id, ImVec2 pos) {
    auto* n = new Param_Node(param_data, id, pos);
    n->GenerateIntermediateResultFrameBuffers();
    return n;
}

Boilerplate_Var_Node* SS_Node_Factory::BuildBoilerplateVarNode(Boilerplate_Var_Data& data, SS_Boilerplate_Manager* bm, int id, ImVec2 pos) {
    auto* n = new Boilerplate_Var_Node(data, bm, id, pos);
    n->GenerateIntermediateResultFrameBuffers();
    return n;
}

Terminal_Node * SS_Node_Factory::BuildTerminalNode(const std::vector<Boilerplate_Var_Data> &varData, int id, ImVec2 pos) {
    auto* tn = new Terminal_Node(varData, id, pos);
    tn->GenerateIntermediateResultFrameBuffers();
    return tn;
}
