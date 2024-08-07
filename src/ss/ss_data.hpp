#ifndef SHADER_SCUPLTOR_SS_DATA_HPP
#define SHADER_SCUPLTOR_SS_DATA_HPP

#include <vector>
#include <string>
#include "ss_node_types.hpp"

// Param Data Listener, not Listener Pattern, it is passed into methods like a temporary callback
class ParamDataGraphHook {
public:
    virtual void InformOfDelete(int paramID) = 0;
    virtual void UpdateParamDataContents(int paramID, GLSL_TYPE type) = 0;
    virtual void UpdateParamDataName(int paramID, const char* name) = 0;
    virtual ~ParamDataGraphHook() = default;
};

class Parameter_Data {
public:
    Parameter_Data(GRAPH_PARAM_TYPE type, GRAPH_PARAM_GENTYPE gentype, unsigned arrSize, int id, ParamDataGraphHook* gHook);
    // Update data contents to the ParamDataGraphHook, reset memory if resetMem = true
    void MakeData(ParamDataGraphHook* graphHook, bool resetMem = true);
    // Update the gentype (vec-type) of the parameter, and propagate that change to the ParamDataGraphHook
    bool UpdateGentype(ParamDataGraphHook* graphHook, GRAPH_PARAM_GENTYPE gentype);
    // Update the main type of the parameter, and propagate that change to the ParamDataGraphHook
    bool UpdateType(ParamDataGraphHook* graphHook, GRAPH_PARAM_TYPE type);
    // Update the name of the parameter, and propagate that change to the ParamDataGraphHook
    void UpdateName(ParamDataGraphHook* graphHook, const char* newParamName);

    __attribute__((unused)) bool IsMatrix() const;
    __attribute__((unused)) unsigned GetLength() const;

    // Returns a read-only pointer to the data of the parameter. WARNING: not relocatable without copy.
    const char* GetData() const { return m_dataContainer; }
    // Returns a read-only pointer to the name of the parameter. WARNING: not relocatable without copy.
    const char* GetName() const { return m_paramName; }
    // Returns the unique parameter ID of the parameter
    int GetID() const { return m_paramID; }
    // Returns the main type of the parameter
    GRAPH_PARAM_TYPE GetParamType() const { return m_type; };
    // Returns the gen-type (vec-type) of the parameter
    GRAPH_PARAM_GENTYPE GetParamGenType() const { return m_gentype; };
    // Returns the type of the parameter
    GLSL_TYPE GetType() const;

    // Return the main type as an int for indexing
    int GetTypeAsDropboxIndex() const;
    // Return the gentype as an int for indexing
    int GetGentypeAsDropboxIndex() const;

    // Draw the parameter to its IMGUI window, propagating inputs from user to the ParamDataGraphHook
    void Draw(ParamDataGraphHook* graphHook);

protected:
    GRAPH_PARAM_TYPE m_type;
    GRAPH_PARAM_GENTYPE m_gentype;
    unsigned int m_arrSize;
    int m_paramID;

    char m_paramName[64];
    char m_dataContainer[16 * 8];

} __attribute__((aligned(16)));

/**
 * Variable data for boilerplate m_nodes and pins
 */
struct Boilerplate_Var_Data {
    std::string _name;
    GLSL_TYPE type;
    bool frag_only;
};

/**
 *
 */
struct Builtin_Node_Data {
    std::string _name;
    std::string in_liner;
    std::vector<std::pair<GLSL_TYPE,std::string> > in_vars;
    std::vector<std::pair<GLSL_TYPE,std::string> > out_vars;
};

/**
 *
 */
struct Constant_Node_Data {
    std::string m_name;
    GRAPH_PARAM_GENTYPE m_gentype;
    GRAPH_PARAM_TYPE m_type;
};


/**
 * Vector Make and Break operations, for use with Nodes
 */
enum VECTOR_OPS : unsigned int {
    VEC_BREAK2_OP,
    VEC_BREAK3_OP,
    VEC_BREAK4_OP,
    VEC_MAKE2_OP,
    VEC_MAKE3_OP,
    VEC_MAKE4_OP
};
/**
 *
 */
struct Vector_Op_Node_Data {
    std::string m_name;
    VECTOR_OPS m_op;
};


#endif //SHADER_SCUPLTOR_SS_DATA_HPP
