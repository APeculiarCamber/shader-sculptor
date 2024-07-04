#ifndef SS_FACTORY
#define SS_FACTORY

#include <vector>
#include <string>
#include <memory>

#include "imgui/imgui.h"
#include "ss_node_types.h"
#include "ss_data.h"

// STATIC SINGLETON CLASS
class SS_Node_Factory {
protected:
    static std::vector<Builtin_Node_Data> nodeDatas;
    static std::vector<Boilerplate_Var_Data> boilerplateVarDatas;
public:
    SS_Node_Factory() = delete;
    SS_Node_Factory(const SS_Node_Factory&) = delete;
    SS_Node_Factory(SS_Node_Factory&&) = delete;

    static bool InitReadBuiltinFile(const std::string& file);
    static bool InitReadInBoilerplateParams(const std::vector<Boilerplate_Var_Data>& varData);

    // Search functions, caches and returns search results
    static std::vector<Builtin_Node_Data> GetMatchingBuiltinNodes(const std::string& query);
    static std::vector<Constant_Node_Data> GetMatchingConstantNodes(const std::string& query);
    static std::vector<Vector_Op_Node_Data> GetMatchingVectorNodes(const std::string& query);
    static std::vector<Parameter_Data*> GetMatchingParamNodes(std::string query, const std::vector<Parameter_Data*>& data_in);
    static std::vector<Parameter_Data*> GetMatchingParamNodes(std::string query, const std::vector<std::unique_ptr<Parameter_Data>>& data_in);
    static std::vector<Boilerplate_Var_Data> GetMatchingBoilerplateNodes(const std::string& query);

    // Build and return dynamically allocated nodes
    static class Builtin_GraphNode* BuildBuiltinNode(Builtin_Node_Data& node_data, int id, ImVec2 pos);
    static class Constant_Node* BuildConstantNode(Constant_Node_Data& node_data, int id, ImVec2 pos);
    static class Vector_Op_Node* BuildVecOpNode(Vector_Op_Node_Data& node_data, int id, ImVec2 pos);
    static class Param_Node* BuildParamNode(Parameter_Data* param_data, int id, ImVec2 pos);
    static class Boilerplate_Var_Node* BuildBoilerplateVarNode(Boilerplate_Var_Data& data, class SS_Boilerplate_Manager* bm, int id, ImVec2 pos);
};
#endif