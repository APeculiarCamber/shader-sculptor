#include "ss_data.hpp"
#include "imgui/imgui.h"
#include "ss_parser.hpp"
#include <cstddef>

///// PARAMETER DATA
Parameter_Data::Parameter_Data(GRAPH_PARAM_TYPE type, GRAPH_PARAM_GENTYPE gentype, unsigned arrSize, int id, ParamDataGraphHook* gHook)
        : m_type(type), m_gentype(gentype), m_arrSize(arrSize), m_paramID(id), m_paramName {}, m_dataContainer{} {
    static_assert(offsetof(Parameter_Data, m_dataContainer) % 16 == 0, "m_dataContainer is not aligned to 16 bytes");
    sprintf(m_paramName, "P_PARAM_%i", id);
    MakeData(gHook);
}

void Parameter_Data::MakeData(ParamDataGraphHook* graphHook, bool reset_mem) {
    if (reset_mem) {
        memset(m_dataContainer, 0, 8 * 16 * sizeof(char));
    }
    graphHook->UpdateParamDataContents(m_paramID, SS_Parser::ConstantTypeToGLSLType(m_gentype, m_type, m_arrSize));
}

bool Parameter_Data::UpdateGentype(ParamDataGraphHook* graphHook, GRAPH_PARAM_GENTYPE gentype) {
    if (gentype == m_gentype) return false;
    m_gentype = gentype;
    MakeData(graphHook, false);
    return true;
}
bool Parameter_Data::UpdateType(ParamDataGraphHook* graphHook, GRAPH_PARAM_TYPE type) {
    if (type == m_type) return false;
    m_type = type;
    if (type == SS_Texture2D || type == SS_TextureCube) {
        m_gentype = SS_Scalar;
    }

    MakeData(graphHook);
    return true;
}

__attribute__((unused)) bool Parameter_Data::IsMatrix() const {
    return SS_MAT & m_gentype;
}

__attribute__((unused)) unsigned Parameter_Data::GetLength() const {
    switch (m_gentype) {
        case SS_Scalar: return 1 * m_arrSize;
        case SS_Vec2: return m_arrSize * 2;
        case SS_Vec3: return m_arrSize * 3;
        case SS_Vec4:
        case SS_Mat2: return m_arrSize * 4;
        case SS_Mat3: return m_arrSize * 9;
        case SS_Mat4: return m_arrSize * 16;
        case SS_MAT:
            break;
    }
    return 0;
}

int Parameter_Data::GetTypeAsDropboxIndex() const {
    return (int)m_type;
}
int Parameter_Data::GetGentypeAsDropboxIndex() const {
    return (int)m_gentype;
}

void Parameter_Data::UpdateName(ParamDataGraphHook* graphHook, const char* newParamName) {
    if (strcmp(newParamName, m_paramName) != 0) {
        strcpy(m_paramName, newParamName);
        graphHook->UpdateParamDataName(m_paramID, m_paramName);
    }
}


bool isValidChar(char c) {
    return c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}


void WriteParameterValueSelector(GRAPH_PARAM_GENTYPE gentype, GRAPH_PARAM_TYPE pType, bool notGentype, const std::string& paramName, char* dataContainer) {

    ImGuiDataType data_type = ImGuiDataType_Float;
    int data_size = sizeof(float);
    switch (pType) {
        case SS_Float: data_type = ImGuiDataType_Float; data_size = sizeof(float); break;
        case SS_Double: data_type = ImGuiDataType_Double; data_size = sizeof(double); break;
        case SS_Int:
        case SS_Texture2D:
        case SS_TextureCube:
            data_type = ImGuiDataType_S32; data_size = sizeof(int); break;
    }

    std::vector<std::string> labels(4);
    switch (gentype) {
        case SS_Scalar:
            labels[0] = (notGentype ? "Index-In###" : "Scalar-In###") + paramName;
            ImGui::InputScalar(labels[0].c_str(), data_type, dataContainer); break;
        case SS_Vec2:
            labels[0] = ("Vec2-In###" + paramName);
            ImGui::InputScalarN(labels[0].c_str(), data_type, dataContainer, 2); break;
        case SS_Vec3:
            labels[0] = ("Vec2-In###" + paramName);
            ImGui::InputScalarN(labels[0].c_str(), data_type, dataContainer, 3); break;
        case SS_Vec4:
            labels[0] = ("Vec4-In###" + paramName);
            ImGui::InputScalarN(labels[0].c_str(), data_type, dataContainer, 4); break;
        case SS_Mat2:
            labels = { ("Mat2-In###" + paramName + "111"), ("Mat2-In###" + paramName + "222") };
            ImGui::InputScalarN(labels[0].c_str(), data_type, dataContainer, 2);
            ImGui::InputScalarN(labels[1].c_str(), data_type, dataContainer + (data_size*2), 2); break;
        case SS_Mat3:
            labels = { ("Mat3-In###" + paramName + "111"), ("Mat3-In###" + paramName + "222"), ("Mat3-In###" + paramName + "333") };
            ImGui::InputScalarN(labels[0].c_str(), data_type, dataContainer, 3);
            ImGui::InputScalarN(labels[1].c_str(), data_type, dataContainer + (data_size*3), 3);
            ImGui::InputScalarN(labels[2].c_str(), data_type, dataContainer + (data_size*6), 3); break;
        case SS_Mat4:
            labels = { ("Mat4-In###" + paramName + "111"), ("Mat4-In###" + paramName + "222"), ("Mat4-In###" + paramName + "333"), ("Mat4-In###" + paramName + "444") };
            ImGui::InputScalarN(labels[0].c_str(), data_type, dataContainer, 4);
            ImGui::InputScalarN(labels[1].c_str(), data_type, dataContainer + (data_size*4), 4);
            ImGui::InputScalarN(labels[2].c_str(), data_type, dataContainer + (data_size*8), 4);
            ImGui::InputScalarN(labels[3].c_str(), data_type, dataContainer + (data_size*12), 4); break;
        case SS_MAT:
            break;
    }
}

void Parameter_Data::Draw(ParamDataGraphHook* graphHook) {
    const char* type_strs[] { "Float", "Double", "Int", "Texture2D", "TextureCube" };
    const char* gentype_strs[] { "Scalar", "Vec2", "Vec3", "Vec4", "Mat2", "Mat3", "Mat4" };
    char name[32];
    char new_param_name[64];
    strcpy(new_param_name, m_paramName);
    sprintf(name, "##INPUT_NAME%i", m_paramID);
    ImGui::BeginGroup();
    ImGui::InputText(name, new_param_name + 2, 62);
    { // remove invalid characters
        char* fixer = new_param_name;
        do { if (!isValidChar(*fixer)) *fixer = '_'; } while (*(++fixer) != '\0');
    }
    UpdateName(graphHook, new_param_name);
    sprintf(name, "##TABLE%i", m_paramID);
    ImGui::BeginTable(name, 2);
    sprintf(name, "##TYPE%i", m_paramID);
    ImGui::TableNextColumn();
    ImGui::BeginGroup();
    ImGui::Text("TYPE");
    ImGui::PushItemWidth(-1);
    int current_type = GetTypeAsDropboxIndex();
    ImGui::ListBox(name, &current_type, type_strs, 5);
    ImGui::PopItemWidth();
    UpdateType(graphHook, (GRAPH_PARAM_TYPE) current_type);
    ImGui::EndGroup();

    ImGui::TableNextColumn();

    sprintf(name, "##GENTYPE%i", m_paramID);
    bool disable_gentype = m_type == SS_Texture2D || m_type == SS_TextureCube;
    ImGui::BeginGroup();
    ImGui::BeginDisabled(disable_gentype);
    ImGui::Text("GENTYPE");
    ImGui::PushItemWidth(-1);
    int current_gentype = GetGentypeAsDropboxIndex();
    ImGui::ListBox(name, &current_gentype, gentype_strs, 7);
    ImGui::PopItemWidth();
    UpdateGentype(graphHook, (GRAPH_PARAM_GENTYPE) current_gentype);
    ImGui::EndDisabled();
    ImGui::EndGroup();

    ImGui::EndTable();
    std::string param_name_str = (m_paramName + 2);

    // ALLOW USER TO SELECT DEFAULT PARAMETER VALUES FOR ELIGIBLE TYPES
    WriteParameterValueSelector(m_gentype, m_type, disable_gentype, (m_paramName + 2), m_dataContainer);

    sprintf(name, "REMOVE PARAM #%i", m_paramID);
    if (ImGui::Button(name)) {
        graphHook->InformOfDelete(m_paramID);
    }

    ImGui::EndGroup();

    ImVec2 min_vec = ImGui::GetItemRectMin();
    ImVec2 max_vec = ImGui::GetItemRectMax();
    ImGui::GetWindowDrawList()->AddRect(min_vec, max_vec, 0xffffffff);
}

GLSL_TYPE Parameter_Data::GetType() const { return SS_Parser::ConstantTypeToGLSLType(m_gentype, m_type); }
