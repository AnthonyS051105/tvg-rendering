cmake_minimum_required(VERSION 3.16)
project(tvg-rendering)

set(CMAKE_CXX_STANDARD 17)

find_package(OpenGL REQUIRED)
find_package(glfw3  REQUIRED)
find_package(GLEW   REQUIRED)
find_package(glm    REQUIRED)

add_executable(tvg-rendering src/main.cpp)

target_link_libraries(tvg-rendering
    OpenGL::GL
    glfw
    GLEW::GLEW
    ${GLM_LIBRARIES}
)

target_include_directories(tvg-rendering PRIVATE ${GLM_INCLUDE_DIRS})
