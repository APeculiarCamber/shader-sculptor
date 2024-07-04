#ifndef SS_FACTORY
#define SS_FACTORY

#include <vector>
#include <string>
#include <memory>

#include "imgui/imgui.h"
#include "ss_node_types.h"
#include "ss_data.h"

// forward declares
// TODO: these MIGHT be able to be included, the circular wasn't real
class Builtin_GraphNode;
class Constant_Node;
class Vector_Op_Node;
class Boilerplate_Var_Node;
class Param_Node;
class SS_Boilerplate_Manager;


class SS_Node_Factory {
protected:
    std::vector<Builtin_Node_Data> node_datas;
public:
    // ALLOWS reading from files to create builtin nodes and vector nodes and type nodes?
    // ALLOWS reading from custom file to create custom nodes
    // ALLOWS a request to get unique, state-agnostic ID for any given node, likely a name...
    bool read_builtin_file(const std::string& file);


    // caching for queries
    std::string last_q;
    std::vector<Builtin_Node_Data> last_data_builtin;
    std::vector<Constant_Node_Data> last_data_constant;
    std::vector<Vector_Op_Node_Data> last_data_vec_op;

    // Search functions, caches and returns search results
    const std::vector<Builtin_Node_Data>& get_matching_builtin_nodes(const std::string& query);
    const std::vector<Constant_Node_Data>& get_matching_constant_nodes(std::string query);
    const std::vector<Vector_Op_Node_Data>& get_matching_vector_nodes(std::string query);
    static std::vector<Parameter_Data*> get_matching_param_nodes(std::string query, const std::vector<Parameter_Data*>& data_in);
    static std::vector<Parameter_Data*> get_matching_param_nodes(std::string query, const std::vector<std::unique_ptr<Parameter_Data>>& data_in);
    std::vector<Boilerplate_Var_Data> get_matching_boilerplate_nodes(std::string query, const SS_Boilerplate_Manager* bp_manager);
    void cache_last_query(std::string query);

    // Build and return dynamically allocated nodes
    Builtin_GraphNode* build_builtin_node(Builtin_Node_Data& node_data, int id, ImVec2 pos, unsigned int fb);
    Constant_Node* build_constant_node(Constant_Node_Data& node_data, int id, ImVec2 pos, unsigned int fb);
    Vector_Op_Node* build_vec_op_node(Vector_Op_Node_Data& node_data, int id, ImVec2 pos, unsigned int fb);
    Param_Node* build_param_node(Parameter_Data* param_data, int id, ImVec2 pos, unsigned int fb);
    Boilerplate_Var_Node* build_boilerplate_var_node(Boilerplate_Var_Data& data, SS_Boilerplate_Manager* bm, int id, ImVec2 pos, unsigned int fb);
};
#endif