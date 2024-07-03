#ifndef SS_PARSER
#define SS_PARSER

#include "imgui/imgui.h"
#include <string>

struct GLSL_TYPE;
class SS_Graph;

enum GRAPH_PARAM_GENTYPE : unsigned int;
enum GRAPH_PARAM_TYPE : unsigned int;

namespace SS_Parser {
    // get the character of the GLSL type length
    char get_vec_len_char(unsigned type);
    // convert GLSL_TYPE to string
    std::string type_to_string(GLSL_TYPE type);
    // convert GLSL_TYPE to a default value
    std::string type_to_default_value(GLSL_TYPE type);
    // Convert GLSL_TYPE to color, not for shader code. For node coloring.
    ImU32 type_to_color(GLSL_TYPE type);
    // Convert string to GLSL_TYPE 
    GLSL_TYPE string_to_type(std::string type_str, bool out = false);
    std::string get_unique_var_name(int node_id, int var_id, GLSL_TYPE type);
    // Convert parameter data type to GLSL type
    GLSL_TYPE constant_type_to_type(GRAPH_PARAM_GENTYPE gentype, GRAPH_PARAM_TYPE type, unsigned int arr_size = 1);
    // Comvert GLSL_type and pin to output color FOR CODE.
    std::string convert_output_to_color_str(std::string pin_name, GLSL_TYPE type); 


    std::string serialize_graph(const SS_Graph& graph);
    void deserialize_graph(SS_Graph& graph, std::string& str);
};
#endif