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
}


/*

 OLD NOTES:
 Scalars
The basic non-vector types are:
bool: conditional type, values may be either true or false
int: a signed, two's complement, 32-bit integer
uint: an unsigned 32-bit integer
float: an IEEE-754 single-precision floating point number
double: an IEEE-754 double-precision floating-point number

Each of the scalar types, including booleans, have 2, 3, and 4-component vector equivalents. The n digit below can be 2, 3, or 4:
bvecn: a vector of booleans
ivecn: a vector of signed integers
uvecn: a vector of unsigned integers
 vecn: a vector of single-precision floating-point numbers
dvecn: a vector of double-precision floating-point numbers

Matrices
matnxm: A matrix with n columns and m rows. OpenGL uses column-major matrices, which is standard for mathematics users. Example: mat3x4.
matn: A matrix with n columns and n rows. Shorthand for matnxn

Arrays : end of var?

Samplers:, special as parameters?
gsampler1D	GL_TEXTURE_1D	1D texture
gsampler2D	GL_TEXTURE_2D	2D texture
gsampler3D	GL_TEXTURE_3D	3D texture
gsamplerCube	GL_TEXTURE_CUBE_MAP	Cubemap Texture
*/

#endif