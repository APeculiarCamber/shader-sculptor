#ifndef SS_FACTORY
#define SS_FACTORY

#include <vector>
#include <string>

#include "..\imgui\imgui.h"

// forward declares
struct GLSL_TYPE;
class Builtin_GraphNode;
class Constant_Node;
class Vector_Op_Node;
class Boilerplate_Var_Node;
class Param_Node;
class SS_Boilerplate_Manager;

enum GRAPH_PARAM_GENTYPE : unsigned int;
enum GRAPH_PARAM_TYPE : unsigned int;
struct Parameter_Data;
struct Boilerplate_Var_Data;

struct Builtin_Node_Data {
    std::string _name;
    std::string in_liner;
    std::vector<std::pair<GLSL_TYPE,std::string> > in_vars;
    std::vector<std::pair<GLSL_TYPE,std::string> > out_vars;
};

struct Constant_Node_Data {
    Constant_Node_Data(const char* str, GRAPH_PARAM_GENTYPE gt, GRAPH_PARAM_TYPE t)
         : _name(str), _gentype(gt), _type(t) {}
    std::string _name;
    GRAPH_PARAM_GENTYPE _gentype;
    GRAPH_PARAM_TYPE _type;
};

enum VECTOR_OPS : unsigned int {
    VEC_BREAK2_OP,
    VEC_BREAK3_OP,
    VEC_BREAK4_OP,
    VEC_MAKE2_OP,
    VEC_MAKE3_OP,
    VEC_MAKE4_OP
};

struct Vector_Op_Node_Data {
    Vector_Op_Node_Data(const char* str, VECTOR_OPS op) : _op(op), _name(str) {}
    std::string _name;
    VECTOR_OPS _op;
};

class SS_Node_Factory {
protected:
    std::vector<Builtin_Node_Data> node_datas;
public:
    // ALLOWS reading from files to create builtin nodes and vector nodes and type nodes?
    // ALLOWS reading from custom file to create custom nodes
    // ALLOWS a request to get unique, state-agnostic ID for any given node, likely a name...
    bool read_builtin_file(std::string file);


    // caching for queries
    std::string last_q;
    std::vector<Builtin_Node_Data> last_data_builtin;
    std::vector<Constant_Node_Data> last_data_constant;
    std::vector<Vector_Op_Node_Data> last_data_vec_op;
    std::vector<Parameter_Data*> last_data_param;
    std::vector<Boilerplate_Var_Data> last_data_BP;

    // Search functions, caches and returns search results
    const std::vector<Builtin_Node_Data>& get_matching_builtin_nodes(std::string query);
    const std::vector<Constant_Node_Data>& get_matching_constant_nodes(std::string query);
    const std::vector<Vector_Op_Node_Data>& get_matching_vector_nodes(std::string query);
    std::vector<Parameter_Data*>& get_matching_param_nodes(std::string query, std::vector<Parameter_Data*>& data_in);
    const std::vector<Boilerplate_Var_Data>& get_matching_boilerplate_nodes(std::string query, const SS_Boilerplate_Manager* bp_manager);
    void cache_last_query(std::string query);

    // Build and return dynamically allocated nodes
    Builtin_GraphNode* build_builtin_node(Builtin_Node_Data& node_data, int id, ImVec2 pos, unsigned int fb);
    Constant_Node* build_constant_node(Constant_Node_Data& node_data, int id, ImVec2 pos, unsigned int fb);
    Vector_Op_Node* build_vec_op_node(Vector_Op_Node_Data& node_data, int id, ImVec2 pos, unsigned int fb);
    Vector_Op_Node* build_vec_op_node(Boilerplate_Var_Data& node_data, int id, ImVec2 pos, unsigned int fb);
    Param_Node* build_param_node(Parameter_Data* param_data, int id, ImVec2 pos, unsigned int fb);
    Boilerplate_Var_Node* build_boilerplate_var_node(Boilerplate_Var_Data& data, SS_Boilerplate_Manager* bm, int id, ImVec2 pos, unsigned int fb);
};
#endif