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

struct Mesh {
    unsigned int VAO, VBO;
    unsigned int vertexCount;
    tinyobj::material_t material;
    bool hasMaterial;
};

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

bool loadModel(const std::string& objPath, const std::string& mtlSearchPath, std::vector<Mesh>& outMeshes) {
    tinyobj::ObjReaderConfig reader_config;
    reader_config.mtl_search_path = mtlSearchPath;

    tinyobj::ObjReader reader;
    if (!reader.ParseFromFile(objPath, reader_config)) {
        if (!reader.Error().empty()) {
            std::cerr << "TinyObjReader Error: " << reader.Error() << std::endl;
        }
        return false;
    }

    auto& attrib = reader.GetAttrib();
    auto& shapes = reader.GetShapes();
    auto& materials = reader.GetMaterials();

    for (size_t s = 0; s < shapes.size(); s++) {
        std::vector<Vertex> vertices;
        size_t index_offset = 0;

        for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
            size_t fv = size_t(shapes[s].mesh.num_face_vertices[f]);
            for (size_t v = 0; v < fv; v++) {
                tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];

                Vertex vertex;
                vertex.Position = glm::vec3(
                    attrib.vertices[3 * size_t(idx.vertex_index) + 0],
                    attrib.vertices[3 * size_t(idx.vertex_index) + 1],
                    attrib.vertices[3 * size_t(idx.vertex_index) + 2]
                );

                if (idx.normal_index >= 0) {
                    vertex.Normal = glm::vec3(
                        attrib.normals[3 * size_t(idx.normal_index) + 0],
                        attrib.normals[3 * size_t(idx.normal_index) + 1],
                        attrib.normals[3 * size_t(idx.normal_index) + 2]
                    );
                } else {
                    vertex.Normal = glm::vec3(0.0f, 1.0f, 0.0f);
                }

                vertices.push_back(vertex);
            }
            index_offset += fv;
        }

        Mesh currentMesh;
        currentMesh.vertexCount = vertices.size();
        currentMesh.hasMaterial = false;

        if (!shapes[s].mesh.material_ids.empty() && shapes[s].mesh.material_ids[0] >= 0) {
            int mat_id = shapes[s].mesh.material_ids[0];
            if (mat_id < materials.size()) {
                currentMesh.material = materials[mat_id];
                currentMesh.hasMaterial = true;
            }
        }

        glGenVertexArrays(1, &currentMesh.VAO);
        glGenBuffers(1, &currentMesh.VBO);

        glBindVertexArray(currentMesh.VAO);
        glBindBuffer(GL_ARRAY_BUFFER, currentMesh.VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Normal));

        glBindVertexArray(0);
        outMeshes.push_back(currentMesh);
    }
    return true;
}

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(800, 600, "Tugas OpenGL - Parsing MTL", NULL, NULL);
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
    
    std::vector<Mesh> diningTableMeshes;
    
    if (!loadModel("models/dinning-table/dining-table.obj", "models/dinning-table/", diningTableMeshes)) {
        std::cout << "Gagal memuat model meja makan!" << std::endl;
        return -1;
    }

    while (!glfwWindowShouldClose(window)) {
        if(glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        ourShader.use();

        ourShader.setVec3("lightColor",  1.0f, 1.0f, 1.0f);
        ourShader.setVec3("lightPos",    2.0f, 5.0f, 3.0f);
        ourShader.setVec3("viewPos",     0.0f, 3.0f, 8.0f); // Menaikkan posisi Y agar pandangan kamera lebih proporsional

        glm::mat4 projection = glm::perspective(glm::radians(45.0f), 800.0f / 600.0f, 0.1f, 500.0f);
        glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 3.0f, 8.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 model = glm::mat4(1.0f);
        
        model = glm::translate(model, glm::vec3(0.0f, -1.0f, 0.0f)); 
        model = glm::rotate(model, glm::radians(45.0f), glm::vec3(0.0f, 1.0f, 0.0f)); 
        model = glm::scale(model, glm::vec3(0.0015f, 0.0015f, 0.0015f)); 

        ourShader.setMat4("projection", projection);
        ourShader.setMat4("view", view);
        ourShader.setMat4("model", model);

        for (const auto& mesh : diningTableMeshes) {
            if (mesh.hasMaterial) {
                ourShader.setVec3("material.ambient",  mesh.material.ambient[0],  mesh.material.ambient[1],  mesh.material.ambient[2]);
                ourShader.setVec3("material.diffuse",  mesh.material.diffuse[0],  mesh.material.diffuse[1],  mesh.material.diffuse[2]);
                ourShader.setVec3("material.specular", mesh.material.specular[0], mesh.material.specular[1], mesh.material.specular[2]);
                ourShader.setFloat("material.shininess", mesh.material.shininess);
            } else {
                ourShader.setVec3("material.ambient",  0.1f, 0.1f, 0.1f);
                ourShader.setVec3("material.diffuse",  0.6f, 0.6f, 0.6f);
                ourShader.setVec3("material.specular", 0.4f, 0.4f, 0.4f);
                ourShader.setFloat("material.shininess", 32.0f);
            }

            glBindVertexArray(mesh.VAO);
            glDrawArrays(GL_TRIANGLES, 0, mesh.vertexCount);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    for (auto& mesh : diningTableMeshes) {
        glDeleteVertexArrays(1, &mesh.VAO);
        glDeleteBuffers(1, &mesh.VBO);
    }

    glfwTerminate();
    return 0;
}