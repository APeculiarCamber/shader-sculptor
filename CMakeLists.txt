cmake_minimum_required(VERSION 3.28)
project(shader_sculptor)

set(CMAKE_CXX_STANDARD 11)

include_directories(glad/include/)
include_directories(glad/include/KHR)
include_directories(src/stbi)
include_directories(src)
include_directories(src/graphics)
include_directories(src/math)
include_directories(src/ss)

add_compile_definitions(CMAKE_ROOT_DIR="${CMAKE_SOURCE_DIR}/")

file(GLOB_RECURSE MAIN_SOURCES
        "./src/main.cpp"
        "./src/graphics/*.cpp"
        "./src/math/*.cpp"
        "./src/ss/*.cpp"
)

file(GLOB_RECURSE IMGUI_SOURCES
        "./src/imgui/*.cpp"
)

file(GLOB_RECURSE GLAD_SOURCES
        "./glad/src/glad.c"
)

add_executable(shader_sculptor ${MAIN_SOURCES})

add_library(glad STATIC ${GLAD_SOURCES})
add_library(imgui STATIC ${IMGUI_SOURCES})
target_link_libraries(shader_sculptor glad imgui)

find_package(glfw3 REQUIRED)
target_link_libraries(shader_sculptor glfw)
find_package(OpenGL REQUIRED)
target_link_libraries(shader_sculptor OpenGL)

target_compile_options(shader_sculptor PRIVATE -Wall -Werror) # -Wextra -Wpedantic
