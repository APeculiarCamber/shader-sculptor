#ifndef SS_GRAPH
#define SS_GRAPH


#include "imgui/imgui.h"
#include "ss_data.hpp"
#include "ss_node.hpp"
#include <unordered_map>

// MAIN MANAGEMENT CLASS OF THE APPLICATION
class SS_Graph : public ParamDataGraphHook {
public:
    explicit SS_Graph(SS_Boilerplate_Manager* bp);
    ~SS_Graph() override;

    // Get node
       // 0 for input change needed, 1 for no, 2 for output changed needed
   // -1 for failed
Base_GraphNode* GetNode(int id);
    // DELETE a node with id from the graph and disconnect all pins attached to it.
    bool DeleteNode(int id);
    // Disconnect all pins from a node
    bool DisconnectAllPinsByNodeId(int id);
    // Invalidate the final shaders
    void InvalidateShaders();

    // ParamDataGraphHook OVERRIDES
    void InformOfDelete(int paramID) override;
    void UpdateParamDataContents(int paramID, GLSL_TYPE type) override;
    void UpdateParamDataName(int paramID, const char* name) override;


    // IMGUI methods

    void HandleInput();
    void Draw();
    bool DrawSavingWindow();
    bool DrawCreditsWindow();
    void DrawParamPanels();
    void DrawNodeContextWindow() const;
    void DrawImageLoaderWindow();
    void DrawControlsWindow();
    void DrawMenuButtons();


    /************************************************
     * *********************CONSTRUCTION **************************/

    // Construct a topological order from the root of a DAG
    [[nodiscard]] static std::vector<Base_GraphNode*> ConstructTopologicalOrder(Base_GraphNode* root);

    // Main generation function for both intermediate code and final code, from connected graph
    void GenerateShaderTextAndPropagate();
    // Set the intermediate display code for a node
    void SetIntermediateCodeForNode(std::string intermedCode, Base_GraphNode* node);
    // Add a uniform parameter (or sampled image) to the declaration of the shaders, allowing use of a new uniform node
    void AddParameter();


    void SetFinalShaderTextByConstructOrders(const std::vector<Base_GraphNode *> &vertOrder,
                                             const std::vector<Base_GraphNode *> &fragOrder);
    void PropagateIntermediateVertexCodeToNodes(const std::vector<Base_GraphNode *> &vertOrder);
    void PropagateIntermediateFragmentCodeToNodes(const std::vector<Base_GraphNode *> &fragOrder);

protected:
    std::unordered_map<int, std::unique_ptr<Base_GraphNode>> m_nodes;
    std::unordered_map<int, std::vector<int>> m_paramIDsToNodeIDs;

    int m_currentNodeID = 0;
    unsigned int m_mainFramebuffer{};

    bool m_bIsSaving = false;
    bool m_bCreditsUp = false;
    bool m_bControlsUp = false;
    char m_saveBuffer[256]{};

    int m_paramID = 0;
    ImVec2 m_drawPosOffset = ImVec2(0, 0);
    ImVec2 m_dragPosOffset = ImVec2(0, 0);

    bool m_bScreenDraggingNow{};
    char m_searchBuffer[256]{};

    std::vector<std::pair<unsigned int, std::string> > m_images;
    char m_imgBuffer[256]{};

    std::unique_ptr<SS_Boilerplate_Manager> m_BPManager;
    std::vector<std::unique_ptr<Parameter_Data>> m_paramDatas;

    std::string m_currentFragCode;
    std::string m_currentVertCode;

    Base_GraphNode* _dragNode = nullptr;
    Base_GraphNode* _selectedNode = nullptr;
    Base_Pin* _dragPin;
};

#endif