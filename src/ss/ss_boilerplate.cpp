#include "ss_boilerplate.hpp"
#include "ga_material.h"



std::string SS_Boilerplate_Manager::GetIntermediateResultCodeForVar(std::string& var_name) const {
    auto it = m_varNameToOutputCodeMap.find(var_name);
    if (it == m_varNameToOutputCodeMap.end()) return {};
    return it->second;
}

const std::vector<Boilerplate_Var_Data>& SS_Boilerplate_Manager::GetUsableVariables() const {
    return m_usableVars;
}
const std::vector<Boilerplate_Var_Data>& SS_Boilerplate_Manager::GetTerminalVertPinData() const {
    return m_vertPinData;
}
const std::vector<Boilerplate_Var_Data>& SS_Boilerplate_Manager::GetTerminalFragPinData() const {
    return m_fragPinData;
}

void SS_Boilerplate_Manager::SetTerminalNodes(Terminal_Node *vertNode, Terminal_Node *frNode) {
     vertexNode = vertNode;
     fragNode = frNode;
}


Unlit_Boilerplate_Manager::Unlit_Boilerplate_Manager() : SS_Boilerplate_Manager() {
    //  variables
    m_usableVars.push_back(Boilerplate_Var_Data{"TEXCOORD", GLSL_TYPE(GLSL_Float | GLSL_Vec2, 1), true});
    m_varNameToOutputCodeMap.insert(std::make_pair(std::string("TEXCOORD"), std::string("f_texcoord")));
    m_usableVars.push_back(Boilerplate_Var_Data{"TIME", GLSL_TYPE(GLSL_Float | GLSL_Scalar, 1), true});
    m_varNameToOutputCodeMap.insert(std::make_pair(std::string("TIME"), std::string("u_time")));
    m_usableVars.push_back(Boilerplate_Var_Data{"WORLD NORMAL", GLSL_TYPE(GLSL_Float | GLSL_Vec3, 1), true});
    m_varNameToOutputCodeMap.insert(std::make_pair(std::string("WORLD NORMAL"), std::string("f_WorldNormal")));
    m_usableVars.push_back(Boilerplate_Var_Data{"WORLD POSITION", GLSL_TYPE(GLSL_Float | GLSL_Vec3, 1), true});
    m_varNameToOutputCodeMap.insert(std::make_pair(std::string("WORLD POSITION"), std::string("f_WorldPos")));
    m_usableVars.push_back(Boilerplate_Var_Data{"LOCAL NORMAL", GLSL_TYPE(GLSL_Float | GLSL_Vec3, 1), true});
    m_varNameToOutputCodeMap.insert(std::make_pair(std::string("LOCAL NORMAL"), std::string("f_LocalNormal")));
    m_usableVars.push_back(Boilerplate_Var_Data{"LOCAL POSITION", GLSL_TYPE(GLSL_Float | GLSL_Vec3, 1), true});
    m_varNameToOutputCodeMap.insert(std::make_pair(std::string("LOCAL POSITION"), std::string("f_LocalPos")));

    // TERMINAL PINS
    m_vertPinData.push_back(Boilerplate_Var_Data{"N/A", GLSL_TYPE(GLSL_Float | GLSL_Vec3, 1), false});
    m_fragPinData.push_back(Boilerplate_Var_Data{"FRAG COLOR", GLSL_TYPE(GLSL_Float | GLSL_Vec3, 1), false});
}


std::string Unlit_Boilerplate_Manager::GetVertInitBoilerplateDeclares() {
    return "#version 400\n\
    uniform mat4 u_model_mat;\n\
    uniform mat4 u_mvp;\n\
    uniform vec3 u_base_color;\n\
    uniform float u_time;\n\
    uniform vec3 u_objectPos;\n\
    \n\
    layout(location = 0) in vec3 in_vertex;\n\
    layout(location = 1) in vec3 in_color;\n\
    layout(location = 2) in vec2 in_texcoord;\n\
    layout(location = 3) in vec3 in_normal;\n\
    \n\
    out vec3 f_color;\n\
    out vec2 f_texcoord;\n\
    \n\
    out vec3 f_WorldPos;\n\
    out vec3 f_ViewPos;\n\
    out vec3 f_LocalPos;\n\
    out vec3 f_WorldNormal;\n\
    out vec3 f_LocalNormal;\n\
    out vec3 f_ViewNormal;";
}

std::string Unlit_Boilerplate_Manager::GetVertInitBoilerplateCode() {
    return "\n\
        f_color = in_color;\n\
        f_texcoord = in_texcoord;\n\
    \n\
        f_LocalPos = in_vertex;\n\
        f_LocalNormal = in_normal;\n\
        f_WorldNormal = (vec4(in_vertex, 1.0) * u_model_mat).xyz;\n\
        f_WorldPos = (vec4(in_vertex, 1.0) * u_model_mat).xyz;\n\
        f_ViewNormal = (vec4(in_vertex, 1.0) * u_mvp).xyz;\n\
        f_ViewPos = (vec4(in_vertex, 1.0) * u_mvp).xyz;\n\
    \n\
        gl_Position = (vec4(in_vertex, 1.0) * u_mvp);";
}

std::string Unlit_Boilerplate_Manager::GetFragInitBoilerplateDeclares() {
    return "#version 400\n\
    uniform mat4 u_model_mat;\n\
    uniform mat4 u_mvp;\n\
    uniform vec3 u_base_color;\n\
    uniform float u_time;\n\
    uniform vec3 u_objectPos;\n\
    \n\
    in vec3 f_color;\n\
    in vec2 f_texcoord;\n\
    \n\
    in vec3 f_WorldPos;\n\
    in vec3 f_ViewPos;\n\
    in vec3 f_LocalPos;\n\
    in vec3 f_WorldNormal;\n\
    in vec3 f_LocalNormal;\n\
    in vec3 f_ViewNormal;";
}

std::string Unlit_Boilerplate_Manager::GetFragInitBoilerplateCode() {
    return "// UNLIT";
}

std::string Unlit_Boilerplate_Manager::GetVertTerminalBoilerplateCode() {
    return "";
}
std::string Unlit_Boilerplate_Manager::GetFragTerminalBoilerplateCode() {
    std::string terminal_str;
    std::string n;
    if (fragNode->GetInputPin(0).input)
        n = "vec4(" + fragNode->GetInputPin(0).input->get_pin_output_name() + ", 1)";
    else
        n = "vec4(1, 1, 1, 1)";

    return "\tgl_FragColor = " + n + ";";
}

std::unique_ptr<ga_material> Unlit_Boilerplate_Manager::MakeMaterial() {
    return std::unique_ptr<ga_material>(new ga_material());
}




















/**********************************************************************************************************************
 * **********************************************************************************************************************
 * ***********************************************************************************************************************/











PBR_Lit_Boilerplate_Manager::PBR_Lit_Boilerplate_Manager() : SS_Boilerplate_Manager() {
    // VARIABLES
    m_usableVars.push_back(Boilerplate_Var_Data{"TEXCOORD", GLSL_TYPE(GLSL_Float | GLSL_Vec2, 1), true});
    m_varNameToOutputCodeMap.insert(std::make_pair(std::string("TEXCOORD"), std::string("f_texcoord")));
    m_usableVars.push_back(Boilerplate_Var_Data{"TIME", GLSL_TYPE(GLSL_Float | GLSL_Scalar, 1), true});
    m_varNameToOutputCodeMap.insert(std::make_pair(std::string("TIME"), std::string("u_time")));
    m_usableVars.push_back(Boilerplate_Var_Data{"OBJECT POSITION", GLSL_TYPE(GLSL_Float | GLSL_Scalar, 1), true});
    m_varNameToOutputCodeMap.insert(std::make_pair(std::string("OBJECT POSITION"), std::string("u_objectPos")));
    m_usableVars.push_back(Boilerplate_Var_Data{"WORLD NORMAL", GLSL_TYPE(GLSL_Float | GLSL_Vec3, 1), true});
    m_varNameToOutputCodeMap.insert(std::make_pair(std::string("WORLD NORMAL"), std::string("f_WorldNormal")));
    m_usableVars.push_back(Boilerplate_Var_Data{"WORLD POSITION", GLSL_TYPE(GLSL_Float | GLSL_Vec3, 1), true});
    m_varNameToOutputCodeMap.insert(std::make_pair(std::string("WORLD POSITION"), std::string("f_WorldPos")));
    m_usableVars.push_back(Boilerplate_Var_Data{"LOCAL NORMAL", GLSL_TYPE(GLSL_Float | GLSL_Vec3, 1), true});
    m_varNameToOutputCodeMap.insert(std::make_pair(std::string("LOCAL NORMAL"), std::string("f_LocalNormal")));
    m_usableVars.push_back(Boilerplate_Var_Data{"LOCAL POSITION", GLSL_TYPE(GLSL_Float | GLSL_Vec3, 1), true});
    m_varNameToOutputCodeMap.insert(std::make_pair(std::string("LOCAL POSITION"), std::string("f_LocalPos")));

    m_usableVars.push_back(Boilerplate_Var_Data{"VERTEX COLOR 1", GLSL_TYPE(GLSL_Float | GLSL_Vec3, 1), true});
    m_varNameToOutputCodeMap.insert(std::make_pair(std::string("VERTEX COLOR 1"), std::string("f_vertColor1")));
    m_usableVars.push_back(Boilerplate_Var_Data{"VERTEX COLOR 2", GLSL_TYPE(GLSL_Float | GLSL_Vec3, 1), true});
    m_varNameToOutputCodeMap.insert(std::make_pair(std::string("VERTEX COLOR 2"), std::string("f_vertColor2")));


    // Terminal pins
    m_vertPinData.push_back(Boilerplate_Var_Data{"WORLD DISPLACEMENT", GLSL_TYPE(GLSL_Float | GLSL_Vec3, 1), false});
    m_vertPinData.push_back(Boilerplate_Var_Data{"VERTEX COLOR 1", GLSL_TYPE(GLSL_Float | GLSL_Vec3, 1), false});
    m_vertPinData.push_back(Boilerplate_Var_Data{"VERTEX COLOR 2", GLSL_TYPE(GLSL_Float | GLSL_Vec3, 1), false});

    m_fragPinData.push_back(Boilerplate_Var_Data{"ALBEDO", GLSL_TYPE(GLSL_Float | GLSL_Vec3, 1), true});
    m_fragPinData.push_back(Boilerplate_Var_Data{"METALLIC", GLSL_TYPE(GLSL_Float | GLSL_Scalar, 1), true});
    m_fragPinData.push_back(Boilerplate_Var_Data{"ROUGHNESS", GLSL_TYPE(GLSL_Float | GLSL_Scalar, 1), true});
    m_fragPinData.push_back(Boilerplate_Var_Data{"OCCLUSION", GLSL_TYPE(GLSL_Float | GLSL_Scalar, 1), true});
    m_fragPinData.push_back(Boilerplate_Var_Data{"NORMAL", GLSL_TYPE(GLSL_Float | GLSL_Vec3, 1), true});
}

std::string PBR_Lit_Boilerplate_Manager::GetVertInitBoilerplateDeclares() {
    return R"(#version 400
uniform mat4 u_model_mat;
uniform mat4 u_mvp;
uniform vec3 u_base_color;
uniform float u_time;
uniform vec3 u_objectPos;
uniform vec3 u_eyePos;

layout(location = 0) in vec3 in_vertex;
layout(location = 1) in vec3 in_color;
layout(location = 2) in vec2 in_texcoord;
layout(location = 3) in vec3 in_normal;

out vec3 f_color;
out vec2 f_texcoord;

out vec3 f_WorldPos;
out vec3 f_ViewPos;
out vec3 f_LocalPos;
out vec3 f_WorldNormal;
out vec3 f_LocalNormal;
out vec3 f_ViewNormal;
out vec3 f_vertColor1;
out vec3 f_vertColor2;)";
}

std::string PBR_Lit_Boilerplate_Manager::GetVertInitBoilerplateCode() {
    return R"(
    f_color = in_color;
    f_texcoord = in_texcoord;

    f_LocalPos = in_vertex;
    f_LocalNormal = in_normal;
    f_WorldNormal = (vec4(in_normal, 0.0) * u_model_mat).xyz;
    f_ViewNormal = (vec4(in_normal, 0.0) * u_mvp).xyz;
    f_LocalPos = in_vertex;
    f_WorldPos = (vec4(in_vertex, 1.0) * u_model_mat).xyz;
    f_ViewPos = (vec4(in_vertex, 1.0) * u_mvp).xyz;
    f_vertColor1 = vec3(1, 1, 1);
    f_vertColor2 = vec3(1, 1, 1);)";
}

std::string PBR_Lit_Boilerplate_Manager::GetFragInitBoilerplateDeclares() {
    return R"(
#version 400 core
out vec4 FragColor;

in vec3 f_color;
in vec2 f_texcoord;

in vec3 f_WorldPos;
in vec3 f_ViewPos;
in vec3 f_LocalPos;
in vec3 f_WorldNormal;
in vec3 f_LocalNormal;
in vec3 f_ViewNormal;
in vec3 f_vertColor1;
in vec3 f_vertColor2;

// lights
uniform vec3 u_lightPositions[4];
uniform vec3 u_lightColors[4];

uniform mat4 u_model_mat;
uniform mat4 u_mvp;
uniform vec3 u_base_color;
uniform float u_time;
uniform vec3 u_objectPos;
uniform vec3 u_eyePos;

const float PI = 3.14159265359;
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return nom / denom;
}
// ----------------------------------------------------------------------------
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;
    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    return nom / denom;
}
// ----------------------------------------------------------------------------
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}
// ----------------------------------------------------------------------------
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
// ----------------------------------------------------------------------------
vec3 rotate_normal_vec(vec3 rot_normal, vec3 plane_normal, vec3 tangent_normal) {
    float c = dot(rot_normal, plane_normal);
    if (c < -0.999999) return -tangent_normal;
    float s = sin(acos(c));
    vec3 x = cross(rot_normal, plane_normal);
    float i_c = (1 - c);
    float i_s = (1 - s);

    mat3 rot_mat = mat3(
        c + x.x*x.x*i_c,
        x.x*x.y*i_c - x.z*s,
        x.x*x.z*i_c + x.y*s,  // 1. column

        x.y*x.x*i_c + x.z*s,
        c + x.y*x.y*i_c,
        x.y*x.z*i_c + x.x*s,  // 2. column

        x.z*x.x*i_c - x.y*s,
        x.x*x.y*i_c + x.x*s,
        c + x.z*x.z*i_c); // 3. column

    return normalize(rot_mat * tangent_normal);
}

vec3 getNormalFromMapping(vec3 tangentNormal)
{
    tangentNormal = tangentNormal * 2.0 - 1.0;

    // get tangent vector, then the other by cross product
    vec3 Q1  = dFdx(f_WorldPos);
    vec3 Q2  = dFdy(f_WorldPos);
    //vec2 st1 = dFdx(f_texcoord);
    //vec2 st2 = dFdy(f_texcoord);

    vec3 N   = normalize(f_WorldNormal);
    vec3 T  = normalize(Q1/* *st1.t*/ - Q2/* *st2.t*/);
    vec3 B  = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    return normalize(TBN * tangentNormal);
})";
}

std::string PBR_Lit_Boilerplate_Manager::GetFragInitBoilerplateCode() {
    return R"(// PBR-LIKE FRAGMENT CODE )";
}

std::string PBR_Lit_Boilerplate_Manager::GetVertTerminalBoilerplateCode() {
    
    std::string world_displace;
    if (vertexNode->GetInputPin(0).input)
        world_displace = vertexNode->GetInputPin(0).input->get_pin_output_name();
    else  world_displace = "vec3(0, 0, 0)";
    std::string vert_color1;
    if (vertexNode->GetInputPin(1).input)
        vert_color1 = vertexNode->GetInputPin(1).input->get_pin_output_name();
    else  vert_color1 = "vec3(0, 0, 0)";
    std::string vert_color2;
    if (vertexNode->GetInputPin(2).input)
        vert_color2 = vertexNode->GetInputPin(2).input->get_pin_output_name();
    else  vert_color2 = "vec3(0, 0, 0)";

    m_vertPinData.push_back(Boilerplate_Var_Data{"WORLD DISPLACEMENT", GLSL_TYPE(GLSL_Float | GLSL_Vec3, 1), false});
    m_vertPinData.push_back(Boilerplate_Var_Data{"VERTEX COLOR 1", GLSL_TYPE(GLSL_Float | GLSL_Vec3, 1), false});
    m_vertPinData.push_back(Boilerplate_Var_Data{"VERTEX COLOR 2", GLSL_TYPE(GLSL_Float | GLSL_Vec3, 1), false});
    
    std::string post_fix = R"(
    f_WorldPos += world_pos_displacement; // PLACEHOLDER FOR SETTING
    f_LocalPos = (vec4(f_WorldPos, 1) * inverse(u_model_mat)).xyz; // PLACEHOLDER FOR SETTING
    f_ViewPos = (vec4(f_LocalPos, 1.0) * u_mvp).xyz; // PLACEHOLDER FOR SETTING

    gl_Position = (vec4(f_LocalPos, 1.0) * u_mvp);)";
    return "\nvec3 world_pos_displacement = " + world_displace + ";\n"
        + "f_vertColor1 = " + vert_color1 + ";\n"
        + "f_vertColor2 = " + vert_color2 + ";\n" + post_fix;
}

std::string PBR_Lit_Boilerplate_Manager::GetFragTerminalBoilerplateCode() {
    std::string albedo, metal, rough, ao, normal;
    if (fragNode->GetInputPin(0).input)
        albedo = fragNode->GetInputPin(0).input->get_pin_output_name();
    else  albedo = "vec3(1, 1, 1)";
    if (fragNode->GetInputPin(1).input)
        metal = fragNode->GetInputPin(1).input->get_pin_output_name();
    else  metal = "0.5";
    if (fragNode->GetInputPin(2).input)
        rough = fragNode->GetInputPin(2).input->get_pin_output_name();
    else  rough = "0.5";
    if (fragNode->GetInputPin(3).input)
        ao = fragNode->GetInputPin(3).input->get_pin_output_name();
    else  ao = "1.0";
    if (fragNode->GetInputPin(4).input)
        normal = fragNode->GetInputPin(4).input->get_pin_output_name();
    else  normal = "vec3(0.5, 0.5, 1.0)";

    const char* post_fix = R"(    
    vec3 N = normalize(getNormalFromMapping(ts_normal));
    vec3 V = normalize(u_eyePos - f_WorldPos);

    // calculate reflectance at normal incidence
    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, albedo, metallic);

    // reflectance equation
    vec3 Lo = vec3(0.0);
    for(int i = 0; i < 4; ++i) 
    {
        // calculate per-light radiance
        vec3 L = normalize(u_lightPositions[i] - f_WorldPos);
        vec3 H = normalize(V + L);
        float distance = length(u_lightPositions[i] - f_WorldPos);
        float attenuation = 1.0 / (distance * distance);
        vec3 radiance = u_lightColors[i] * attenuation;

        // Cook-Torrance BRDF
        float NDF = DistributionGGX(N, H, roughness);   
        float G   = GeometrySmith(N, V, L, roughness);      
        vec3 F    = fresnelSchlick(clamp(dot(H, V), 0.0, 1.0), F0);
           
        vec3 numerator    = NDF * G * F; 
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001; // + 0.0001 to prevent divide by zero
        vec3 specular = numerator / denominator;
        
        // kS is equal to Fresnel
        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;	  

        float NdotL = max(dot(N, L), 0.0);
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }

    vec3 ambient = vec3(0.03) * albedo * ao;
    vec3 color = ambient + Lo;

    // HDR tonemapping
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0/2.2)); 

    FragColor = vec4(color, 1.0);)";

    return ("\nvec3 albedo = " + albedo + ";\n"
        + "float metallic = " + metal + ";\n"
        + "float roughness = " + rough + ";\n"
        + "float ao = " + ao + ";\n"
        + "vec3 ts_normal = " + normal + ";\n") + post_fix;
}

std::unique_ptr<ga_material> PBR_Lit_Boilerplate_Manager::MakeMaterial() {
    return std::unique_ptr<ga_material>(new ga_pbr_material());
}

