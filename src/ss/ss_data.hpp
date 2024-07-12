#ifndef SHADER_SCUPLTOR_SS_DATA_HPP
#define SHADER_SCUPLTOR_SS_DATA_HPP

// Scalar, Vecn, Matn, Texture, sampler id

#include <vector>
#include <string>
#include "ss_node_types.hpp"

// LISTENER PATTERN!
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
    void MakeData(ParamDataGraphHook* graphHook, bool reset_mem = true);
    bool UpdateGentype(ParamDataGraphHook* graphHook, GRAPH_PARAM_GENTYPE gentype);
    bool UpdateType(ParamDataGraphHook* graphHook, GRAPH_PARAM_TYPE type);
    void UpdateName(ParamDataGraphHook* graphHook, const char* newParamName);

    __attribute__((unused)) bool IsMatrix() const;
    __attribute__((unused)) unsigned GetLength() const;

    const char* GetData() const { return m_dataContainer; }
    const char* GetName() const { return m_paramName; }
    int GetID() const { return m_paramID; }
    GRAPH_PARAM_TYPE GetParamType() const { return m_type; };
    GRAPH_PARAM_GENTYPE GetParamGenType() const { return m_gentype; };
    GLSL_TYPE GetType() const;

    int GetTypeAsDropboxIndex() const;
    int GetGentypeAsDropboxIndex() const;

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

struct Builtin_Node_Data {
    std::string _name;
    std::string in_liner;
    std::vector<std::pair<GLSL_TYPE,std::string> > in_vars;
    std::vector<std::pair<GLSL_TYPE,std::string> > out_vars;
};

struct Constant_Node_Data {
    std::string m_name;
    GRAPH_PARAM_GENTYPE m_gentype;
    GRAPH_PARAM_TYPE m_type;
};


struct Vector_Op_Node_Data {
    std::string m_name;
    VECTOR_OPS m_op;
};


#endif //SHADER_SCUPLTOR_SS_DATA_HPP
