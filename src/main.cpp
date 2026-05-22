#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "Shader.h"

struct Vertex {
    glm::vec3 Position;
    glm::vec3 Normal;
};

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void loadModel(const std::string& path, unsigned int &VAO, unsigned int &VBO, unsigned int &vertexCount) {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_GenSmoothNormals);
    if(!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::cout << "ERROR::ASSIMP:: " << importer.GetErrorString() << std::endl;
        return;
    }
    std::vector<Vertex> vertices;
    for (unsigned int m = 0; m < scene->mNumMeshes; m++) {
        aiMesh* mesh = scene->mMeshes[m];
        for(unsigned int i = 0; i < mesh->mNumVertices; i++) {
            Vertex vertex;
            vertex.Position = glm::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
            if (mesh->HasNormals()) {
                vertex.Normal = glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
            } else {
                vertex.Normal = glm::vec3(0.0f, 1.0f, 0.0f);
            }
            vertices.push_back(vertex);
        }
    }
    vertexCount = vertices.size();

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Normal));

    glBindVertexArray(0);
}

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(800, 600, "Tugas OpenGL", NULL, NULL);
    if (window == NULL) {
        std::cout << "Gagal membuat GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Gagal inisialisasi GLAD" << std::endl;
        return -1;
    }

    glEnable(GL_DEPTH_TEST);

    Shader ourShader("shaders/vertex.glsl", "shaders/fragment.glsl");
    unsigned int VAO, VBO, vertexCount;
    loadModel("models/dinning-table/dining-table.obj", VAO, VBO, vertexCount);
    while (!glfwWindowShouldClose(window)) {
        if(glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        ourShader.use();

        ourShader.setVec3("lightColor",  1.0f, 1.0f, 1.0f);
        ourShader.setVec3("lightPos",    2.0f, 5.0f, 3.0f);
        ourShader.setVec3("viewPos",     0.0f, 2.0f, 5.0f);

        // Kayu
        ourShader.setVec3("material.ambient",  0.25f, 0.20f, 0.15f); 
        ourShader.setVec3("material.diffuse",  0.6f, 0.4f, 0.2f);    
        ourShader.setVec3("material.specular", 0.5f, 0.5f, 0.5f);    
        ourShader.setFloat("material.shininess", 32.0f);             
        
        // Emas
        // ourShader.setVec3("material.ambient",  0.24725f, 0.1995f, 0.0745f); 
        // ourShader.setVec3("material.diffuse",  0.75164f, 0.60648f, 0.22648f);    
        // ourShader.setVec3("material.specular", 0.628281f, 0.555802f, 0.366065f);    
        // ourShader.setFloat("material.shininess", 51.2f);

        // Plastik
        // ourShader.setVec3("material.ambient",  0.0f, 0.1f, 0.06f); 
        // ourShader.setVec3("material.diffuse",  0.0f, 0.50980392f, 0.50980392f);    
        // ourShader.setVec3("material.specular", 0.50196078f, 0.50196078f, 0.50196078f);    
        // ourShader.setFloat("material.shininess", 32.0f);

        glm::mat4 projection = glm::perspective(glm::radians(45.0f), 800.0f / 600.0f, 0.1f, 100.0f);
        glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 2.0f, 5.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 model = glm::mat4(1.0f);
        
        model = glm::translate(model, glm::vec3(0.0f, -0.5f, 0.0f)); 
        model = glm::rotate(model, glm::radians(45.0f), glm::vec3(0.0f, 1.0f, 0.0f)); 
        model = glm::scale(model, glm::vec3(0.0015f, 0.0015f, 0.0015f)); 

        ourShader.setMat4("projection", projection);
        ourShader.setMat4("view", view);
        ourShader.setMat4("model", model);

        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, vertexCount);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glfwTerminate();
    return 0;
}