#ifndef SS_PARSER
#define SS_PARSER

#include "imgui/imgui.h"
#include "ss_node_types.hpp"
#include <string>

namespace SS_Parser {
    // get the character of the GLSL type length
    char GetVecLenChar(GLSL_TYPE_ENUM_BITS type);
    // convert GLSL_TYPE to string
    std::string GLSLTypeToString(GLSL_TYPE type);
    // convert GLSL_TYPE to a default value
    std::string GLSLTypeToDefaultValue(GLSL_TYPE type);
    // Convert GLSL_TYPE to color, not for shader code. For node coloring.
    ImU32 GLSLTypeToColor(GLSL_TYPE type);
    // Convert string to GLSL_TYPE 
    GLSL_TYPE StringToGLSLType(std::string type_str);
    // Construct an internal name which will be unique to a node's output pin
    std::string GetUniqueVarName(int node_id, int var_id, GLSL_TYPE type);
    // Convert parameter data type to GLSL type
    GLSL_TYPE ConstantTypeToGLSLType(GRAPH_PARAM_GENTYPE gentype, GRAPH_PARAM_TYPE type, unsigned int arr_size = 1);
    // Comvert GLSL_type and pin to output color FOR CODE.
    std::string ConvertOutputToColorStr(std::string pin_name, GLSL_TYPE type);

    // Convert all uppercase letters in string to equivalent lowercase letters
    std::string StringToLower(const std::string& str);
};
#endif