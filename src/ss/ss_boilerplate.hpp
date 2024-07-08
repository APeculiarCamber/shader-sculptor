#ifndef SS_PLATE
#define SS_PLATE

#include <unordered_map>
#include <utility>
#include <memory>
#include "ss_node.hpp"

/**
 * Base Class for boilerplate m_nodes and code.
 * Manages the types of input and output variables that are available and how they are used (such as PBR-based GGX outputs used internally).
 * Class is inherited from to generate different graph types.
 */
class SS_Boilerplate_Manager {
public:
    virtual ~SS_Boilerplate_Manager() = default;
    // make a material of type which will effectively utilize the boilerplate code
    virtual std::unique_ptr<class ga_material> MakeMaterial() = 0;

    // Get the declares/header for the vertex shader
    virtual std::string GetVertInitBoilerplateDeclares() = 0;
    // Get the initial body code for the vertex shader
    virtual std::string GetVertInitBoilerplateCode() = 0;
    // Get the terminal vertex code
    virtual std::string GetVertTerminalBoilerplateCode() = 0;
    // Get the declares/header for the fragment shader
    virtual std::string GetFragInitBoilerplateDeclares() = 0;
    // Get the initial body code for the fragment shader
    virtual std::string GetFragInitBoilerplateCode() = 0;
    // Get the terminal fragment code
    virtual std::string GetFragTerminalBoilerplateCode() = 0;

    // Get descriptions of the m_nodes which shaders can use (uniforms)
    const std::vector<Boilerplate_Var_Data>& GetUsableVariables() const;

    // Get the INPUT pins required for the final vertex computations
    const std::vector<Boilerplate_Var_Data>& GetTerminalVertPinData() const;
    // Get the INPUT pins required for the final fragment computations
    const std::vector<Boilerplate_Var_Data>& GetTerminalFragPinData() const;
    // Return code to display the intermediate result of a variable as the final fragment color
    std::string GetIntermediateResultCodeForVar(std::string& var_name) const;


    // NOTE: This class does not own these m_nodes, hence the raw pointer rather than a unique pointer
    void SetTerminalNodes(Terminal_Node* vertNode, Terminal_Node* frNode);
    Terminal_Node* GetTerminalFragNode() { return fragNode; }
    Terminal_Node* GetTerminalVertexNode() { return vertexNode; }

protected:
    std::unordered_map<std::string, std::string> m_varNameToOutputCodeMap;

    std::vector<Boilerplate_Var_Data> m_usableVars;
    std::vector<Boilerplate_Var_Data> m_vertPinData;
    std::vector<Boilerplate_Var_Data> m_fragPinData;

    Terminal_Node* vertexNode = nullptr;
    Terminal_Node* fragNode = nullptr;
};

/**
 * @brief Shader with no lighting support.
 * 
 */
class Unlit_Boilerplate_Manager : public SS_Boilerplate_Manager {
public:
    Unlit_Boilerplate_Manager();
    std::string GetVertInitBoilerplateDeclares() override;
    std::string GetVertInitBoilerplateCode() override;
    std::string GetVertTerminalBoilerplateCode() override;
    std::string GetFragInitBoilerplateDeclares() override;
    std::string GetFragInitBoilerplateCode() override;
    std::string GetFragTerminalBoilerplateCode() override;
    std::unique_ptr<ga_material> MakeMaterial() override;
};

/**
 * @brief Shader with PBR-like shading.
 * Accepts albedo, roughness, metalness; and allows lights.
 * 
 */
class PBR_Lit_Boilerplate_Manager : public SS_Boilerplate_Manager {
public:
    PBR_Lit_Boilerplate_Manager();
    std::string GetVertInitBoilerplateDeclares() override;
    std::string GetVertInitBoilerplateCode() override;
    std::string GetVertTerminalBoilerplateCode() override;
    std::string GetFragInitBoilerplateDeclares() override;
    std::string GetFragInitBoilerplateCode() override;
    std::string GetFragTerminalBoilerplateCode() override;
    std::unique_ptr<ga_material> MakeMaterial() override;
};
#endif
