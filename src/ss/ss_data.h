//
// Created by idemaj on 7/3/24.
//

#ifndef SHADER_SCUPLTOR_SS_DATA_H
#define SHADER_SCUPLTOR_SS_DATA_H

// Scalar, Vecn, Matn, Texture, sampler id

#include <vector>
#include <string>
#include "ss_node_types.h"

// LISTENER PATTERN!
class ParamDataGraphHook {
public:
    virtual void InformOfDelete(int paramID) = 0;
    virtual void UpdateParamDataContents(int paramID, GLSL_TYPE type) = 0;
    virtual void UpdateParamDataName(int paramID, const char* name) = 0;
    virtual ~ParamDataGraphHook() = default;
};

class Parameter_Data {
protected:
    GRAPH_PARAM_TYPE m_type;
    GRAPH_PARAM_GENTYPE m_gentype;
    unsigned int m_arrSize;
    int m_paramID;

    char m_paramName[64];
    // TODO: align this
    char data_container[16 * 8];

public:
    Parameter_Data(GRAPH_PARAM_TYPE type, GRAPH_PARAM_GENTYPE gentype, unsigned arrSize, int id, ParamDataGraphHook* gHook);
    void make_data(ParamDataGraphHook* graphHook, bool reset_mem = true);
    bool update_gentype(ParamDataGraphHook* graphHook, GRAPH_PARAM_GENTYPE gentype);
    bool update_type(ParamDataGraphHook* graphHook, GRAPH_PARAM_TYPE type);
    void update_name(ParamDataGraphHook* graphHook, const char* newParamName);

    __attribute__((unused)) bool is_mat() const;
    __attribute__((unused)) unsigned get_len() const;

    const char* GetData() const { return data_container; }
    const char* GetName() const { return m_paramName; }
    int GetID() const { return m_paramID; }
    GRAPH_PARAM_TYPE GetParamType() const { return m_type; };
    GRAPH_PARAM_GENTYPE GetParamGenType() const { return m_gentype; };
    GLSL_TYPE GetType() const;

    int type_to_dropbox_index() const;
    int gentype_to_dropbox_index() const;

    void draw(ParamDataGraphHook* graph);
};


/**
 * Variable data for boilerplate nodes and pins
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


#endif //SHADER_SCUPLTOR_SS_DATA_H
