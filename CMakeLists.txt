cmake_minimum_required(VERSION 3.28)
project(shader_scupltor)

set(CMAKE_CXX_STANDARD 11)

# Include directories
include_directories(glad/include/)
include_directories(glad/include/KHR)
include_directories(src/stbi)
include_directories(src/graphics)
include_directories(src/math)
include_directories(src/ss)

# Add the main source files
file(GLOB_RECURSE MAIN_SOURCES
        "./src/main.cpp"
        "./src/graphics/*.cpp"
        "./src/math/*.cpp"
        "./src/ss/*.cpp"
)

# Add the imgui source files
file(GLOB_RECURSE IMGUI_SOURCES
        "./src/imgui/*.cpp"
)

# Add the glad source files
file(GLOB_RECURSE GLAD_SOURCES
        "./glad/src/glad.c"
)

# Add the main executable
add_executable(shader_scupltor ${MAIN_SOURCES})

# Add the glad library
add_library(glad STATIC ${GLAD_SOURCES})

# Add the imgui library
add_library(imgui STATIC ${IMGUI_SOURCES})

# Find and link GLFW
find_package(glfw3 REQUIRED)
target_link_libraries(shader_scupltor glfw)

# Find and link OpenGL
find_package(OpenGL REQUIRED)
target_link_libraries(shader_scupltor OpenGL)

# Link glad and imgui to the main executable
target_link_libraries(shader_scupltor glad imgui)

# Add compile options
target_compile_options(shader_scupltor PRIVATE -Wall -Werror) # -Wextra -Wpedantic
