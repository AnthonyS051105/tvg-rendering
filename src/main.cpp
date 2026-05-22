// OpenGL Phong-shaded dining-table viewer
// Assignment: OpenGL Group Assignment (Part 1)
// - OBJ loader with VBO/VAO
// - Phong vertex + fragment shaders (with diffuse texture support)
// - Model / View / Projection matrices
// - Multiple material setups (keyboard 1-5)
// Controls: 1-5 = material preset, 6-8 = view position, ESC = quit

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <fstream>
#include <sstream>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <cmath>

// ─── Shader loader ───────────────────────────────────────────────────────────

static std::string readFile(const std::string& path)
{
    std::ifstream f(path);
    if (!f.is_open()) { std::cerr << "Cannot open file: " << path << "\n"; return ""; }
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static GLuint compileShader(GLenum type, const std::string& src)
{
    GLuint id = glCreateShader(type);
    const char* c = src.c_str();
    glShaderSource(id, 1, &c, nullptr);
    glCompileShader(id);
    GLint ok;
    glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512]; glGetShaderInfoLog(id, 512, nullptr, log);
        std::cerr << "Shader compile error:\n" << log << "\n";
    }
    return id;
}

static GLuint createProgram(const std::string& vertPath, const std::string& fragPath)
{
    GLuint vs = compileShader(GL_VERTEX_SHADER,   readFile(vertPath));
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, readFile(fragPath));
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    GLint ok;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512]; glGetProgramInfoLog(prog, 512, nullptr, log);
        std::cerr << "Program link error:\n" << log << "\n";
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

// ─── Texture loader ──────────────────────────────────────────────────────────

static std::map<std::string, GLuint> textureCache;

static GLuint loadTexture(const std::string& path)
{
    if (path.empty()) return 0;
    auto it = textureCache.find(path);
    if (it != textureCache.end()) return it->second;

    stbi_set_flip_vertically_on_load(true);
    int w, h, ch;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &ch, 0);
    if (!data) {
        std::cerr << "Failed to load texture: " << path << "\n";
        return 0;
    }

    GLenum fmt = (ch == 4) ? GL_RGBA : (ch == 3) ? GL_RGB : GL_RED;

    GLuint id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, fmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);
    textureCache[path] = id;
    return id;
}

// ─── MTL material ────────────────────────────────────────────────────────────

struct MtlMaterial {
    glm::vec3   Ka{0.2f};
    glm::vec3   Kd{0.8f};
    glm::vec3   Ks{0.5f};
    float       Ns{32.0f};
    float       d{1.0f};
    std::string map_Kd;   // diffuse texture filename
};

static std::map<std::string, MtlMaterial> loadMTL(const std::string& path,
                                                   const std::string& dir)
{
    std::map<std::string, MtlMaterial> mats;
    std::ifstream f(path);
    if (!f.is_open()) { std::cerr << "Cannot open MTL: " << path << "\n"; return mats; }

    std::string line, current;
    while (std::getline(f, line)) {
        // strip comment
        auto hash = line.find('#');
        if (hash != std::string::npos) line = line.substr(0, hash);

        std::istringstream iss(line);
        std::string tok;
        iss >> tok;
        if (tok.empty()) continue;

        if (tok == "newmtl") {
            iss >> current;
            mats[current] = MtlMaterial{};
        } else if (tok == "Ka") {
            iss >> mats[current].Ka.r >> mats[current].Ka.g >> mats[current].Ka.b;
        } else if (tok == "Kd") {
            iss >> mats[current].Kd.r >> mats[current].Kd.g >> mats[current].Kd.b;
        } else if (tok == "Ks") {
            iss >> mats[current].Ks.r >> mats[current].Ks.g >> mats[current].Ks.b;
        } else if (tok == "Ns") {
            iss >> mats[current].Ns;
        } else if (tok == "d") {
            iss >> mats[current].d;
        } else if (tok == "map_Kd") {
            std::string fname; iss >> fname;
            mats[current].map_Kd = dir + fname;
        }
    }
    return mats;
}

// ─── OBJ mesh ────────────────────────────────────────────────────────────────

struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 uv;
};

struct MeshGroup {
    std::string         material;
    std::vector<Vertex> vertices;
    GLuint              vao{0}, vbo{0};
};

static std::vector<MeshGroup> loadOBJ(const std::string& objPath)
{
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> uvs;
    std::vector<MeshGroup> groups;
    std::string            currentMat = "__default__";

    auto getGroup = [&](const std::string& mat) -> MeshGroup& {
        for (auto& g : groups)
            if (g.material == mat) return g;
        groups.push_back({mat, {}, 0, 0});
        return groups.back();
    };

    std::ifstream f(objPath);
    if (!f.is_open()) { std::cerr << "Cannot open OBJ: " << objPath << "\n"; return groups; }

    std::string line;
    while (std::getline(f, line)) {
        std::istringstream iss(line);
        std::string tok;
        iss >> tok;

        if (tok == "v") {
            glm::vec3 p; iss >> p.x >> p.y >> p.z; positions.push_back(p);
        } else if (tok == "vn") {
            glm::vec3 n; iss >> n.x >> n.y >> n.z; normals.push_back(n);
        } else if (tok == "vt") {
            glm::vec2 t; iss >> t.x >> t.y; uvs.push_back(t);
        } else if (tok == "usemtl") {
            iss >> currentMat;
            getGroup(currentMat);
        } else if (tok == "f") {
            std::vector<Vertex> fverts;
            std::string fstr;
            while (iss >> fstr) {
                std::replace(fstr.begin(), fstr.end(), '/', ' ');
                std::istringstream fi(fstr);
                int pi = 0, ti = 0, ni = 0;
                fi >> pi;
                char c; fi.get(c);
                if (!fi.eof() && fi.peek() != ' ') fi >> ti;
                fi.get(c);
                if (!fi.eof()) fi >> ni;

                Vertex vx{};
                auto resolveIdx = [](int i, int size) { return i > 0 ? i - 1 : size + i; };

                if (pi != 0 && std::abs(pi) <= (int)positions.size())
                    vx.pos = positions[resolveIdx(pi, positions.size())];
                if (ni != 0 && std::abs(ni) <= (int)normals.size())
                    vx.normal = normals[resolveIdx(ni, normals.size())];
                if (ti != 0 && std::abs(ti) <= (int)uvs.size())
                    vx.uv = uvs[resolveIdx(ti, uvs.size())];

                fverts.push_back(vx);
            }

            MeshGroup& grp = getGroup(currentMat);
            for (size_t i = 1; i + 1 < fverts.size(); i++) {
                grp.vertices.push_back(fverts[0]);
                grp.vertices.push_back(fverts[i]);
                grp.vertices.push_back(fverts[i + 1]);
            }
        }
    }

    // Upload to GPU
    for (auto& g : groups) {
        if (g.vertices.empty()) continue;
        glGenVertexArrays(1, &g.vao);
        glGenBuffers(1, &g.vbo);
        glBindVertexArray(g.vao);
        glBindBuffer(GL_ARRAY_BUFFER, g.vbo);
        glBufferData(GL_ARRAY_BUFFER, g.vertices.size() * sizeof(Vertex),
                     g.vertices.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              (void*)offsetof(Vertex, pos));
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              (void*)offsetof(Vertex, normal));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              (void*)offsetof(Vertex, uv));
        glEnableVertexAttribArray(2);
        glBindVertexArray(0);
    }
    return groups;
}

// ─── Camera / window state ───────────────────────────────────────────────────

enum ViewState {
    VIEW_CINEMATIC_FRONT,  // 6: Perspektif dramatis dari depan atas
    VIEW_ARCHITECT_TOP,    // 7: Top-down bird view
    VIEW_CLOSEUP_SIDE,     // 8: Close-up dari sudut kanan bawah
};

static int       materialPreset = 1;
static ViewState currentView    = VIEW_CINEMATIC_FRONT;

// ─── Preset names & matID mapping ────────────────────────────────────────────
// preset: 1=Mixed PNG, 2=Worn Leather & Walnut, 3=Natural Wood, 4=Industrial Metal, 5=Jade Exotic
// matID:  0=qita(meja), 1=lvzhi(taplak), 2=zhuoyi(kursi),
//         3=material(bunga), 4=boli(mangkuk), 5=niunai(piring/vas)

static const char* presetNames[] = {
    "Mixed PNG Textures",
    "Worn Leather & Walnut",
    "Scandinavian Modern",
    "Contemporary Luxury Metal",
    "East Asian Lacquerware",
};

// Map material name string → matID integer (sent as uniform to shader)
static const std::map<std::string, int> matIDMap = {
    {"qita",     0},
    {"lvzhi",    1},
    {"zhuoyi",   2},
    {"material", 3},
    {"boli",     4},
    {"niunai",   5},
};

// ─── Callbacks ───────────────────────────────────────────────────────────────

static void keyCallback(GLFWwindow* w, int key, int, int action, int)
{
    if (action == GLFW_PRESS) {
        if (key >= GLFW_KEY_1 && key <= GLFW_KEY_5) {
            materialPreset = key - GLFW_KEY_1 + 1;
            std::cout << "Material preset: " << presetNames[materialPreset - 1] << "\n";
        }
        if (key == GLFW_KEY_6) { currentView = VIEW_CINEMATIC_FRONT; std::cout << "View: Cinematic Front\n"; }
        if (key == GLFW_KEY_7) { currentView = VIEW_ARCHITECT_TOP;   std::cout << "View: Architect Top\n"; }
        if (key == GLFW_KEY_8) { currentView = VIEW_CLOSEUP_SIDE;    std::cout << "View: Close-up Side\n"; }
        if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(w, true);
    }
}

static void framebufferCallback(GLFWwindow*, int w, int h)
{
    glViewport(0, 0, w, h);
}

// ─── Main ────────────────────────────────────────────────────────────────────

int main()
{
    if (!glfwInit()) { std::cerr << "GLFW init failed\n"; return -1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(800, 600,
        "Dining Table – Phong Shading | 1-5:Material | 6-8:View | ESC:Quit",
        nullptr, nullptr);
    if (!window) { std::cerr << "Window creation failed\n"; glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    glfwSetKeyCallback(window, keyCallback);
    glfwSetFramebufferSizeCallback(window, framebufferCallback);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) { std::cerr << "GLEW init failed\n"; return -1; }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Paths — run from build/ directory
    const std::string shaderDir = "../shaders/";
    const std::string modelDir  = "../models/dinning-table/";

    GLuint prog = createProgram(shaderDir + "vertex.glsl", shaderDir + "fragment.glsl");

    // Preset 1: PNG textures per material group (Mixed PNG)
    // matID: 0=qita, 1=lvzhi, 2=zhuoyi, 3=material(bunga), 4=boli, 5=niunai
    std::map<std::string, GLuint> textures;
    textures["qita"]     = loadTexture(modelDir + "gltf_embedded_12.png");   // beige atlas
    textures["lvzhi"]    = loadTexture(modelDir + "gltf_embedded_3.png");    // dark grid fabric
    textures["zhuoyi"]   = loadTexture(modelDir + "gltf_embedded_15.png");   // white marble/ceramic
    textures["material"] = loadTexture(modelDir + "gltf_embedded_6.png");    // plant black-green
    textures["boli"]     = loadTexture(modelDir + "gltf_embedded_12.png");   // reuse beige for dark tint
    textures["niunai"]   = loadTexture(modelDir + "gltf_embedded_15.png");   // white porcelain
    std::cout << "Loaded preset-1 (Mixed PNG) textures.\n";

    // Load OBJ
    std::cout << "Loading OBJ model...\n";
    auto groups = loadOBJ(modelDir + "dining-table.obj");
    std::cout << "Loaded " << groups.size() << " material groups.\n";

    std::cout << "Controls: 1-5=material preset | 6=Cinematic Front | 7=Architect Top | 8=Close-up Side | ESC=quit\n";

    glm::vec3 lightPos(2.0f, 5.0f, 3.0f);

    // Uniform locations (cached)
    GLint locModel     = glGetUniformLocation(prog, "model");
    GLint locView      = glGetUniformLocation(prog, "view");
    GLint locProj      = glGetUniformLocation(prog, "projection");
    GLint locViewPos   = glGetUniformLocation(prog, "viewPos");
    GLint locLightPos  = glGetUniformLocation(prog, "lightPos");
    GLint locLightAmb  = glGetUniformLocation(prog, "lightAmb");
    GLint locLightDiff = glGetUniformLocation(prog, "lightDiff");
    GLint locLightSpec = glGetUniformLocation(prog, "lightSpec");
    GLint locDiffMap   = glGetUniformLocation(prog, "diffuseMap");
    GLint locPreset    = glGetUniformLocation(prog, "preset");
    GLint locMatID     = glGetUniformLocation(prog, "matID");
    GLint locHasTex    = glGetUniformLocation(prog, "hasTexture");

    while (!glfwWindowShouldClose(window))
    {
        glm::mat4 model      = glm::mat4(1.0f);
        glm::mat4 view       = glm::mat4(1.0f);
        glm::mat4 projection = glm::mat4(1.0f);
        glm::vec3 camPos;

        if (currentView == VIEW_CINEMATIC_FRONT) {
            camPos     = glm::vec3(0.0f, 3.0f, 8.0f);
            view       = glm::lookAt(camPos, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            model      = glm::translate(model, glm::vec3(0.0f, -1.0f, 0.0f));
            model      = glm::rotate(model, glm::radians(45.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            model      = glm::scale(model, glm::vec3(0.0015f, 0.0015f, 0.0015f));
            projection = glm::perspective(glm::radians(45.0f), 800.0f / 600.0f, 0.1f, 500.0f);
        }
        else if (currentView == VIEW_ARCHITECT_TOP) {
            camPos     = glm::vec3(0.0f, 8.0f, 0.001f);
            view       = glm::lookAt(camPos, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            model      = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f));
            model      = glm::scale(model, glm::vec3(0.0015f, 0.0015f, 0.0015f));
            projection = glm::perspective(glm::radians(60.0f), 800.0f / 600.0f, 0.1f, 500.0f);
        }
        else if (currentView == VIEW_CLOSEUP_SIDE) {
            camPos     = glm::vec3(5.0f, 0.5f, 2.0f);
            view       = glm::lookAt(camPos, glm::vec3(0.0f, -0.5f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            model      = glm::translate(model, glm::vec3(0.0f, -0.5f, 0.0f));
            model      = glm::rotate(model, glm::radians(15.0f), glm::vec3(1.0f, 0.0f, 1.0f));
            model      = glm::scale(model, glm::vec3(0.0015f, 0.0015f, 0.0015f));
            projection = glm::perspective(glm::radians(30.0f), 800.0f / 600.0f, 0.1f, 500.0f);
        }

        glClearColor(0.12f, 0.12f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(prog);
        glUniformMatrix4fv(locModel, 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(locView,  1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(locProj,  1, GL_FALSE, glm::value_ptr(projection));
        glUniform3fv(locViewPos, 1, glm::value_ptr(camPos));

        glUniform3fv(locLightPos,  1, glm::value_ptr(lightPos));
        glUniform3f(locLightAmb,  0.30f, 0.30f, 0.30f);
        glUniform3f(locLightDiff, 1.00f, 1.00f, 1.00f);
        glUniform3f(locLightSpec, 1.00f, 1.00f, 1.00f);

        glUniform1i(locDiffMap, 0); // texture unit 0
        glUniform1i(locPreset, materialPreset);

        for (auto& g : groups) {
            if (g.vao == 0 || g.vertices.empty()) continue;

            int    mid    = matIDMap.count(g.material) ? matIDMap.at(g.material) : 0;
            bool   hasTex = false;
            GLuint texID  = 0;

            // Preset 1: PNG per-material
            if (materialPreset == 1 && textures.count(g.material) && textures.at(g.material) != 0) {
                hasTex = true;
                texID  = textures.at(g.material);
            }

            glUniform1i(locMatID,  mid);
            glUniform1i(locHasTex, hasTex ? 1 : 0);

            if (hasTex) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, texID);
            }

            glBindVertexArray(g.vao);
            glDrawArrays(GL_TRIANGLES, 0, (GLsizei)g.vertices.size());
        }

        glBindVertexArray(0);
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup
    for (auto& g : groups) {
        glDeleteVertexArrays(1, &g.vao);
        glDeleteBuffers(1, &g.vbo);
    }
    for (auto& [name, id] : textureCache)
        glDeleteTextures(1, &id);
    glDeleteProgram(prog);
    glfwTerminate();
    return 0;
}
