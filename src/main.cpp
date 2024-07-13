#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <iostream>
#include <fstream>

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include "ss/ss_graph.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "ss_boilerplate.hpp"

const unsigned int SCR_WIDTH = 1600;
const unsigned int SCR_HEIGHT = 1200;

void FramebufferSizeCallback(GLFWwindow* window, int width, int height);
void ProcessInput(GLFWwindow *window);
void MakeDefaultIMGUIIniFile(const std::string& filename);

SS_Graph* DrawGraphTypePrompt() {
    SS_Graph* ret_graph = nullptr;
    ImGui::SetNextWindowSize(ImVec2(200, 200));
    ImGui::Begin("GRAPH TYPE PROMPT", nullptr, ImGuiWindowFlags_NoResize);
    if (ImGui::Button("UNLIT GRAPH"))
        ret_graph = new SS_Graph(new Unlit_Boilerplate_Manager());
    else if (ImGui::Button("LIT-PBR GRAPH"))
        ret_graph = new SS_Graph(new PBR_Lit_Boilerplate_Manager());
    ImGui::End();
    return ret_graph;
}

int main()
{
    // glfw: initialize and configure
    // ------------------------------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // window creation
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Shader Sculptor", NULL, NULL);
    if (window == NULL)
    {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);

    // glad: load all OpenGL function pointers
    // ---------------------------------------
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        glfwTerminate();
        return -1;
    }

    MakeDefaultIMGUIIniFile("imgui.ini");
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true); 
    ImGui_ImplOpenGL3_Init("#version 400");


    // render loop
    // -----------
    char buf[2048];
    for (char & i : buf)
        i = 0;
    
    SS_Graph* graph = nullptr;

    while (!glfwWindowShouldClose(window))
    {
        // input
        ProcessInput(window);

        // render
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (graph)
            graph->Draw();
        else
            graph = DrawGraphTypePrompt();
    
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // swap buffers
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    delete graph;

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwTerminate();
    return 0;
}





// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
void ProcessInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
void FramebufferSizeCallback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

// Make a IMGUI ini file for the default positions of windows
void MakeDefaultIMGUIIniFile(const std::string& filename) {
    if (not std::ifstream(filename).good()) {
        std::cout << "Made default IMGUI ini" << std::endl;
        std::ofstream off(filename);
        off << R"(
[Window][GRAPH TYPE PROMPT]
Pos=646,425
Size=200,200
Collapsed=0

[Window][Parameters]
Pos=1228,598
Size=362,579
Collapsed=0

[Window][Node Context Panel]
Pos=902,50
Size=310,141
Collapsed=0

[Window][Image Loader]
Pos=1228,47
Size=361,534
Collapsed=0)" << std::endl;
    }
}
