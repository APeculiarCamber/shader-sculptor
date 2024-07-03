#include "ss_boilerplate.hpp"
#include "ga_material.h"

#include <iostream>

SS_Boilerplate_Manager::SS_Boilerplate_Manager() = default;

const std::string& SS_Boilerplate_Manager::get_vert_init_boilerplate_code() {
    return init_vert_code;
}
const std::string& SS_Boilerplate_Manager::get_vert_init_boilerplate_declares() {
    return init_vert_declares;
}
const std::string& SS_Boilerplate_Manager::get_frag_init_boilerplate_code() {
    return init_frag_code;
}
const std::string& SS_Boilerplate_Manager::get_frag_init_boilerplate_declares() {
    return init_frag_declares;
}
std::string SS_Boilerplate_Manager::get_output_code_for_var(std::string& var_name) const {
    auto it = var_name_to_output_code.find(var_name);
    if (it == var_name_to_output_code.end()) return {};
    return it->second;
}

void SS_Boilerplate_Manager::set_terminal_nodes(Terminal_Node* vertex_node, Terminal_Node* frag_node) {
    _vertex_node = vertex_node;
    _frag_node = frag_node;
}
const std::vector<Boilerplate_Var_Data>& SS_Boilerplate_Manager::get_usable_variables() const {
    return usable_vars;
}
const std::vector<Boilerplate_Var_Data>& SS_Boilerplate_Manager::get_vert_pin_data() const {
    return vert_pins_data;
}
const std::vector<Boilerplate_Var_Data>& SS_Boilerplate_Manager::get_frag_pin_data() const {
    return frag_pins_data;
}




Unlit_Boilerplate_Manager::Unlit_Boilerplate_Manager() : SS_Boilerplate_Manager() {
    init();
}

void Unlit_Boilerplate_Manager::init() {
    //  variables
    usable_vars.emplace_back("TEXCOORD", GLSL_TYPE(GLSL_Float | GLSL_Vec2, 1), true);
    var_name_to_output_code.insert(std::make_pair(std::string("TEXCOORD"), std::string("f_texcoord")));
    usable_vars.emplace_back("TIME", GLSL_TYPE(GLSL_Float | GLSL_Scalar, 1), true);
    var_name_to_output_code.insert(std::make_pair(std::string("TIME"), std::string("u_time")));
    usable_vars.emplace_back("WORLD NORMAL", GLSL_TYPE(GLSL_Float | GLSL_Vec3, 1), true);
    var_name_to_output_code.insert(std::make_pair(std::string("WORLD NORMAL"), std::string("f_WorldNormal")));
    usable_vars.emplace_back("WORLD POSITION", GLSL_TYPE(GLSL_Float | GLSL_Vec3, 1), true);
    var_name_to_output_code.insert(std::make_pair(std::string("WORLD POSITION"), std::string("f_WorldPos")));
    usable_vars.emplace_back("LOCAL NORMAL", GLSL_TYPE(GLSL_Float | GLSL_Vec3, 1), true);
    var_name_to_output_code.insert(std::make_pair(std::string("LOCAL NORMAL"), std::string("f_LocalNormal")));
    usable_vars.emplace_back("LOCAL POSITION", GLSL_TYPE(GLSL_Float | GLSL_Vec3, 1), true);
    var_name_to_output_code.insert(std::make_pair(std::string("LOCAL POSITION"), std::string("f_LocalPos")));
    

    // TERMINAL PINS
    vert_pins_data.emplace_back("N/A", GLSL_TYPE(GLSL_Float | GLSL_Vec3, 1), false);
    frag_pins_data.emplace_back("FRAG COLOR", GLSL_TYPE(GLSL_Float | GLSL_Vec3, 1), false);

    // CODE
    init_vert_code = "\n\
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
    
    init_frag_code = "// UNLIT";

    init_vert_declares = 
    "#version 400\n\
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

    init_frag_declares = 
    "#version 400\n\
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

    default_vert_shader = R"(#version 400
    uniform mat4 u_model_mat;
    uniform mat4 u_mvp;
    uniform vec3 u_base_color;
    uniform float u_time;
    uniform vec3 u_objectPos;

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

    void main() {
        f_color = in_color;
        f_texcoord = in_texcoord;

        f_LocalPos = in_vertex;
        f_LocalNormal = in_normal;
        f_WorldNormal = (vec4(in_vertex, 1.0) * u_model_mat).xyz;
        f_WorldPos = (vec4(in_vertex, 1.0) * u_model_mat).xyz;
        f_ViewNormal = (vec4(in_vertex, 1.0) * u_mvp).xyz;
        f_ViewPos = (vec4(in_vertex, 1.0) * u_mvp).xyz;

        gl_Position = vec4(in_vertex, 1.0) * u_mvp;
    })";
    default_frag_shader = R"(#version 400
    uniform mat4 u_model_mat;
    uniform mat4 u_mvp;
    uniform vec3 u_base_color;
    uniform float u_time;
    uniform vec3 u_objectPos;

    in vec3 f_color;
    in vec2 f_texcoord;

    in vec3 f_WorldPos;
    in vec3 f_ViewPos;
    in vec3 f_LocalPos;
    in vec3 f_WorldNormal;
    in vec3 f_LocalNormal;
    in vec3 f_ViewNormal;
    void main() {
        \t// MAKE IT HAPPEN HERE MOSTLY
        \tfloat v = dot(f_ViewNormal, vec3(1, 0, 0));
        \tgl_FragColor = vec4(v,v,v,1);
    })";
}

std::string Unlit_Boilerplate_Manager::get_vert_terminal_boilerplate_code() {
    return "";
}
std::string Unlit_Boilerplate_Manager::get_frag_terminal_boilerplate_code() {
    std::string terminal_str;
    std::string n;
    if (_frag_node->input_pins->input)
        n = "vec4(" + _frag_node->input_pins->input->get_pin_output_name() + ", 1)";
    else
        n = "vec4(1, 1, 1, 1)";

    return "\tgl_FragColor = " + n + ";";
}

ga_material* Unlit_Boilerplate_Manager::make_material() {
    return new ga_material();
}








PBR_Lit_Boilerplate_Manager::PBR_Lit_Boilerplate_Manager() : SS_Boilerplate_Manager() {
    init();
}

void PBR_Lit_Boilerplate_Manager::init() {
    // VARIABLES
    usable_vars.emplace_back("TEXCOORD", GLSL_TYPE(GLSL_Float | GLSL_Vec2, 1), true);
    var_name_to_output_code.insert(std::make_pair(std::string("TEXCOORD"), std::string("f_texcoord")));
    usable_vars.emplace_back("TIME", GLSL_TYPE(GLSL_Float | GLSL_Scalar, 1), true);
    var_name_to_output_code.insert(std::make_pair(std::string("TIME"), std::string("u_time")));
    usable_vars.emplace_back("OBJECT POSITION", GLSL_TYPE(GLSL_Float | GLSL_Scalar, 1), true);
    var_name_to_output_code.insert(std::make_pair(std::string("OBJECT POSITION"), std::string("u_objectPos")));
    usable_vars.emplace_back("WORLD NORMAL", GLSL_TYPE(GLSL_Float | GLSL_Vec3, 1), true);
    var_name_to_output_code.insert(std::make_pair(std::string("WORLD NORMAL"), std::string("f_WorldNormal")));
    usable_vars.emplace_back("WORLD POSITION", GLSL_TYPE(GLSL_Float | GLSL_Vec3, 1), true);
    var_name_to_output_code.insert(std::make_pair(std::string("WORLD POSITION"), std::string("f_WorldPos")));
    usable_vars.emplace_back("LOCAL NORMAL", GLSL_TYPE(GLSL_Float | GLSL_Vec3, 1), true);
    var_name_to_output_code.insert(std::make_pair(std::string("LOCAL NORMAL"), std::string("f_LocalNormal")));
    usable_vars.emplace_back("LOCAL POSITION", GLSL_TYPE(GLSL_Float | GLSL_Vec3, 1), true);
    var_name_to_output_code.insert(std::make_pair(std::string("LOCAL POSITION"), std::string("f_LocalPos")));

    usable_vars.emplace_back("VERTEX COLOR 1", GLSL_TYPE(GLSL_Float | GLSL_Vec3, 1), true);
    var_name_to_output_code.insert(std::make_pair(std::string("VERTEX COLOR 1"), std::string("f_vertColor1")));
    usable_vars.emplace_back("VERTEX COLOR 2", GLSL_TYPE(GLSL_Float | GLSL_Vec3, 1), true);
    var_name_to_output_code.insert(std::make_pair(std::string("VERTEX COLOR 2"), std::string("f_vertColor2")));
    

    // Terminal pins
    vert_pins_data.emplace_back("WORLD DISPLACEMENT", GLSL_TYPE(GLSL_Float | GLSL_Vec3, 1), false);
    vert_pins_data.emplace_back("VERTEX COLOR 1", GLSL_TYPE(GLSL_Float | GLSL_Vec3, 1), false);
    vert_pins_data.emplace_back("VERTEX COLOR 2", GLSL_TYPE(GLSL_Float | GLSL_Vec3, 1), false);

    frag_pins_data.emplace_back("ALBEDO", GLSL_TYPE(GLSL_Float | GLSL_Vec3, 1), true);
    frag_pins_data.emplace_back("METALLIC", GLSL_TYPE(GLSL_Float | GLSL_Scalar, 1), true);
    frag_pins_data.emplace_back("ROUGHNESS", GLSL_TYPE(GLSL_Float | GLSL_Scalar, 1), true);
    frag_pins_data.emplace_back("OCCLUSION", GLSL_TYPE(GLSL_Float | GLSL_Scalar, 1), true);
    frag_pins_data.emplace_back("NORMAL", GLSL_TYPE(GLSL_Float | GLSL_Vec3, 1), true);

    // CODE
    init_vert_declares = R"(#version 400
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
    init_frag_declares = R"(
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

// ----------------------------------------------------------------------------
// Easy trick to get tangent-normals to world-space to keep PBR code simplified.
// Don't worry if you don't get what's going on; you generally want to do normal 
// mapping the usual way for performance anways; I do plan make a note of this 
// technique somewhere later in the normal mapping tutorial.
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

    init_vert_code = R"(
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

    init_frag_code = R"(// PBR-LIKE FRAGMENT CODE )";

    default_vert_shader = R"(#version 400
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
out vec3 f_vertColor2;

void main() {
    f_color = in_color;
    f_texcoord = in_texcoord;

    f_LocalNormal = in_normal;
    f_WorldNormal = (vec4(in_normal, 0.0) * u_model_mat).xyz;
    f_ViewNormal = (vec4(in_normal, 0.0) * u_mvp).xyz;
    f_LocalPos = in_vertex;
    f_WorldPos = (vec4(in_vertex, 1.0) * u_model_mat).xyz;
    f_ViewPos = (vec4(in_vertex, 1.0) * u_mvp).xyz;

    vec3 world_pos_displacement = vec3(0, 0, 0);
    f_WorldPos += world_pos_displacement; // PLACEHOLDER FOR SETTING
    f_LocalPos = (vec4(f_WorldPos, 1) * inverse(u_model_mat)).xyz; // PLACEHOLDER FOR SETTING
    f_ViewPos = (vec4(f_LocalPos, 1.0) * u_mvp).xyz; // PLACEHOLDER FOR SETTING
    f_vertColor1 = vec3(1, 1, 1); // PLACEHOLDER FOR SETTING
    f_vertColor2 = vec3(1, 1, 1); // PLACEHOLDER FOR SETTING

    gl_Position = (vec4(f_LocalPos, 1.0) * u_mvp);
})";

    // CODE ADAPTED FROM OPENGL_LEARN
    default_frag_shader = R"( 
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

// ----------------------------------------------------------------------------
// Easy trick to get tangent-normals to world-space to keep PBR code simplified.
// Don't worry if you don't get what's going on; you generally want to do normal 
// mapping the usual way for performance anways; I do plan make a note of this 
// technique somewhere later in the normal mapping tutorial.
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
}

void main()
{
    vec3 albedo = vec3(1, 1, 1);
    float roughness = 0.5;
    float metallic = 0.5;
    float ao = 1.0;
    vec3 ts_normal = vec3(0.5, 0.5, 1);
    
    vec3 N = getNormalFromMapping(ts_normal);
    N = normalize(N);
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

    FragColor = vec4(color, 1.0);
}
    )";
}

std::string PBR_Lit_Boilerplate_Manager::get_vert_terminal_boilerplate_code() {
    
    std::string world_displace;
    if (_vertex_node->input_pins[0].input)
        world_displace = _vertex_node->input_pins[0].input->get_pin_output_name();
    else  world_displace = "vec3(0, 0, 0)";
    std::string vert_color1;
    if (_vertex_node->input_pins[1].input)
        vert_color1 = _vertex_node->input_pins[1].input->get_pin_output_name();
    else  vert_color1 = "vec3(0, 0, 0)";
    std::string vert_color2;
    if (_vertex_node->input_pins[2].input)
        vert_color2 = _vertex_node->input_pins[2].input->get_pin_output_name();
    else  vert_color2 = "vec3(0, 0, 0)";

    vert_pins_data.emplace_back("WORLD DISPLACEMENT", GLSL_TYPE(GLSL_Float | GLSL_Vec3, 1), false);
    vert_pins_data.emplace_back("VERTEX COLOR 1", GLSL_TYPE(GLSL_Float | GLSL_Vec3, 1), false);
    vert_pins_data.emplace_back("VERTEX COLOR 2", GLSL_TYPE(GLSL_Float | GLSL_Vec3, 1), false);
    
    std::string post_fix = R"(
    f_WorldPos += world_pos_displacement; // PLACEHOLDER FOR SETTING
    f_LocalPos = (vec4(f_WorldPos, 1) * inverse(u_model_mat)).xyz; // PLACEHOLDER FOR SETTING
    f_ViewPos = (vec4(f_LocalPos, 1.0) * u_mvp).xyz; // PLACEHOLDER FOR SETTING

    gl_Position = (vec4(f_LocalPos, 1.0) * u_mvp);)";
    return "\nvec3 world_pos_displacement = " + world_displace + ";\n"
        + "f_vertColor1 = " + vert_color1 + ";\n"
        + "f_vertColor2 = " + vert_color2 + ";\n" + post_fix;
}

std::string PBR_Lit_Boilerplate_Manager::get_frag_terminal_boilerplate_code() {
    std::string albedo, metal, rough, ao, normal;
    if (_frag_node->input_pins[0].input)
        albedo = _frag_node->input_pins[0].input->get_pin_output_name();
    else  albedo = "vec3(1, 1, 1)";
    if (_frag_node->input_pins[1].input)
        metal = _frag_node->input_pins[1].input->get_pin_output_name();
    else  metal = "0.5";
    if (_frag_node->input_pins[2].input)
        rough = _frag_node->input_pins[2].input->get_pin_output_name();
    else  rough = "0.5";
    if (_frag_node->input_pins[3].input)
        ao = _frag_node->input_pins[3].input->get_pin_output_name();
    else  ao = "1.0";
    if (_frag_node->input_pins[4].input)
        normal = _frag_node->input_pins[4].input->get_pin_output_name();
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

ga_material* PBR_Lit_Boilerplate_Manager::make_material() {
    return (ga_material*)new ga_pbr_material();
}