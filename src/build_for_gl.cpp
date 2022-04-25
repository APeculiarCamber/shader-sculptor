#include <stdlib.h>
#include <iostream>

int main(int argc, char const *argv[])
{
    if (argc < 3) return EXIT_FAILURE;
    char buf[1024];
    sprintf(buf, "C:\\MinGW\\bin\\g++.exe -g -std=c++17 -I./include -L./lib ./src/%s.cpp ./src/imgui/*.cpp ./src/glad.c -lglfw3dll -o ./%s.exe", argv[1], argv[2]);
    system(buf);
    return 0;
}
