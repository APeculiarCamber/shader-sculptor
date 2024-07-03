#ifndef SS_PLATE
#define SS_PLATE

#include <unordered_map>
#include "ss_node.hpp"

/**
 * @brief Variable data for boilerplate nodes and pins
 * 
 */
struct Boilerplate_Var_Data {
    Boilerplate_Var_Data(const std::string& name, GLSL_TYPE t, bool is_frag)
        : _name(name), type(t), frag_only(is_frag) {}
    std::string _name;
    GLSL_TYPE type;
    bool frag_only;
};

/**
 * @brief Base Class for boilerplate nodes and code.
 * Class is inherited from to generate different graph types.
 * 
 */
class SS_Boilerplate_Manager {
public:
    SS_Boilerplate_Manager();
    ~SS_Boilerplate_Manager() {
        if (_vertex_node) free(_vertex_node);
        if (_frag_node) free(_frag_node);
    }
    // get the terminal vertex code
    virtual std::string get_vert_terminal_boilerplate_code() = 0;
    // get the terminal fragment code
    virtual std::string get_frag_terminal_boilerplate_code() = 0;
    // make a material of type which will effectively utilize the boilerplate code
    virtual class ga_material* make_material() = 0;
    const std::string& get_vert_init_boilerplate_code();
    const std::string& get_vert_init_boilerplate_declares();
    const std::string& get_frag_init_boilerplate_code();
    const std::string& get_frag_init_boilerplate_declares();

    const std::string& get_default_vert_shader() { return default_vert_shader; };
    const std::string& get_default_frag_shader() { return default_frag_shader; };
    void set_terminal_nodes(Terminal_Node* vertex_node, Terminal_Node* frag_node);
    const std::vector<Boilerplate_Var_Data>& get_usable_variables() const;

    
    const std::vector<Boilerplate_Var_Data>& get_vert_pin_data() const;
    const std::vector<Boilerplate_Var_Data>& get_frag_pin_data() const;
    std::string get_output_code_for_var(std::string& var_name) const;

    Terminal_Node* get_terminal_frag_node() { return _frag_node; }
    Terminal_Node* get_terminal_vert_node() { return _vertex_node; }
protected:
    std::unordered_map<std::string, std::string> var_name_to_output_code;
    std::vector<Boilerplate_Var_Data> usable_vars;

    std::vector<Boilerplate_Var_Data> vert_pins_data;
    std::vector<Boilerplate_Var_Data> frag_pins_data;

    std::string init_vert_code;
    std::string init_frag_code;
    std::string init_vert_declares;
    std::string init_frag_declares;

    std::string default_vert_shader;
    std::string default_frag_shader;

    Terminal_Node* _vertex_node = nullptr;
    Terminal_Node* _frag_node = nullptr;
};

/**
 * @brief Shader with no lighting support.
 * 
 */
class Unlit_Boilerplate_Manager : public SS_Boilerplate_Manager {
public:
    Unlit_Boilerplate_Manager();
    void init();
    // Utilize pin connections to complete the shader
    std::string get_vert_terminal_boilerplate_code() override;
    // Utilize pin connections to complete the shader
    std::string get_frag_terminal_boilerplate_code() override;
    class ga_material* make_material() override;
};

/**
 * @brief Shader with PBR-like shading.
 * Allows albedo, roughness, metalness, and lights.
 * 
 */
class PBR_Lit_Boilerplate_Manager : public SS_Boilerplate_Manager {
public:
    PBR_Lit_Boilerplate_Manager();
    void init();
    // Utilize pin connections to complete the shader
    std::string get_vert_terminal_boilerplate_code() override;
    // Utilize pin connections to complete the shader
    std::string get_frag_terminal_boilerplate_code() override;
    class ga_material* make_material() override;
};
#endif
