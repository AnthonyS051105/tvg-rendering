cmake_minimum_required(VERSION 3.10)
project(OpenGL_Assignment)

set(CMAKE_CXX_STANDARD 17)

include_directories(include)

add_executable(app src/main.cpp src/glad.c)

target_link_libraries(app glfw GL X11 pthread Xrandr Xi dl assimp)