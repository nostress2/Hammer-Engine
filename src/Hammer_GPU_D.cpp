#include "Hammer_GPU_D.h"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define CGLTF_IMPLEMENTATION
//TinyGLTF
#include <../TinyGLTF/cgltf.h>

/*
MIT License

Copyright (c) [2025] [Денислав Тихомиров Цанков]

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

glm::mat4 to_glm_matrix(const float* m) {
    glm::mat4 mat;
    for (int i = 0; i < 16; ++i) {
        mat[i % 4][i / 4] = m[i];  // cgltf stores column-major but glm expects column-major, so just copy
    }
    return mat;
}

static inline bool ReadIndex(const cgltf_accessor* acc, size_t i, uint32_t& outIdx) {
    if (!acc) return false;
    outIdx = static_cast<uint32_t>(cgltf_accessor_read_index(acc, i));
    return true;
}

static inline glm::vec2 ReadVec2(const cgltf_accessor* acc, size_t i) {
    float v[4] = {0,0,0,0};
    cgltf_accessor_read_float(acc, i, v, 2);
    return glm::vec2(v[0], v[1]);
}

static inline glm::vec3 ReadVec3(const cgltf_accessor* acc, size_t i) {
    float v[4] = {0,0,0,0};
    cgltf_accessor_read_float(acc, i, v, 3);
    return glm::vec3(v[0], v[1], v[2]);
}

static inline glm::ivec4 ReadJoints4(const cgltf_accessor* acc, size_t i) {
    // joints can be u8 or u16; read as uints then clamp to int
    cgltf_uint v[4] = {0,0,0,0};
    // cgltf doesn't have a direct "read_uintN" for vectors, so read as floats OR read components:
    // easiest: read floats then cast (safe for small ints)
    float tmp[4] = {0,0,0,0};
    cgltf_accessor_read_float(acc, i, tmp, 4);
    return glm::ivec4((int)tmp[0], (int)tmp[1], (int)tmp[2], (int)tmp[3]);
}

static inline glm::vec4 ReadWeights4(const cgltf_accessor* acc, size_t i) {
    // weights may be float, unorm8, unorm16; cgltf converts to floats 0..1
    float v[4] = {0,0,0,0};
    cgltf_accessor_read_float(acc, i, v, 4);
    glm::vec4 w(v[0], v[1], v[2], v[3]);
    float s = w.x + w.y + w.z + w.w;
    if (s > 0.0f) w /= s;           // normalize to sum=1
    return w;
}

// Globals
GLuint shaderProgram, VAO, VBO, EBO, vertexShader, fragmentShader;
GLuint cubemapProgram, vertexShaderQ, fragmentShaderQ;
GLuint ParticleProgram, vertexShaderP, fragmentShaderP;
GLuint computeShader;
GLuint ComputeProgram;
GLuint ssbo;
GLuint skyboxVAO, skyboxVBO, skyboxEBO;
glm::mat4 projection = glm::mat4(1.0f);

//---|SetupMeshes|-----
void InitMeshes(int stacks, int slices){
    InitCubeMesh();
    InitPlaneMesh();
    initSphereMesh(stacks, slices);
    InitSkyboxMesh();
}

CubeMesh CMesh;
PlaneMesh PMesh;

void InitCubeMesh() {
    // Cube vertices and normals
        std::vector<GLfloat> vertices = {
        // Positions          // Normals              // Texture Coords
        // Front face
        -0.5f, -0.5f, -0.5f,  0.0f, 0.0f, -1.0f,  0.0f, 0.0f,
         0.5f, -0.5f, -0.5f,  0.0f, 0.0f, -1.0f,  1.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  0.0f, 0.0f, -1.0f,  1.0f, 1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f, 0.0f, -1.0f,  0.0f, 1.0f,

        // Back face
        -0.5f, -0.5f,  0.5f,  0.0f, 0.0f, 1.0f,  0.0f, 0.0f,
         0.5f, -0.5f,  0.5f,  0.0f, 0.0f, 1.0f,  1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  0.0f, 0.0f, 1.0f,  1.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f, 0.0f, 1.0f,  0.0f, 1.0f,

        // Left face
        -0.5f, -0.5f,  0.5f, -1.0f, 0.0f, 0.0f,  0.0f, 0.0f,
        -0.5f, -0.5f, -0.5f, -1.0f, 0.0f, 0.0f,  1.0f, 0.0f,
        -0.5f,  0.5f, -0.5f, -1.0f, 0.0f, 0.0f,  1.0f, 1.0f,
        -0.5f,  0.5f,  0.5f, -1.0f, 0.0f, 0.0f,  0.0f, 1.0f,

        // Right face
         0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 0.0f,  0.0f, 0.0f,
         0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 0.0f,  1.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  1.0f, 0.0f, 0.0f,  1.0f, 1.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 0.0f, 0.0f,  0.0f, 1.0f,

        // Bottom face
        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f, 0.0f,  0.0f, 1.0f,
         0.5f, -0.5f, -0.5f,  0.0f, -1.0f, 0.0f,  1.0f, 1.0f,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f, 0.0f,  1.0f, 0.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, -1.0f, 0.0f,  0.0f, 0.0f,

        // Top face
        -0.5f,  0.5f, -0.5f,  0.0f, 1.0f, 0.0f,  0.0f, 1.0f,
         0.5f,  0.5f, -0.5f,  0.0f, 1.0f, 0.0f,  1.0f, 1.0f,
         0.5f,  0.5f,  0.5f,  0.0f, 1.0f, 0.0f,  1.0f, 0.0f,
        -0.5f,  0.5f,  0.5f,  0.0f, 1.0f, 0.0f,  0.0f, 0.0f,
    };

    std::vector<GLuint> indices = {
        // Front face
        0, 1, 2, 2, 3, 0,
        // Back face
        4, 5, 6, 6, 7, 4,
        // Left face
        8, 9, 10, 10, 11, 8,
        // Right face
        12, 13, 14, 14, 15, 12,
        // Bottom face
        16, 17, 18, 18, 19, 16,
        // Top face
        20, 21, 22, 22, 23, 20
    };

    CMesh.indexCount = indices.size();

    glGenVertexArrays(1, &CMesh.CubeVAO);
    glGenBuffers(1, &CMesh.CubeVBO);
    glGenBuffers(1, &CMesh.CubeEBO);

    glBindVertexArray(CMesh.CubeVAO);

    glBindBuffer(GL_ARRAY_BUFFER, CMesh.CubeVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(GLfloat), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, CMesh.CubeEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STATIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (void*)0);
    glEnableVertexAttribArray(0);

    // Normal attribute
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (void*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(2);

    // Texture coordinate attribute
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (void*)(6 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void InitSkyboxMesh() {
    GLfloat vertices[] = {
        // Positions
        // Front face
        -0.5f, -0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,
         0.5f,  0.5f, -0.5f,
        -0.5f,  0.5f, -0.5f,

        // Back face
        -0.5f, -0.5f,  0.5f,
         0.5f, -0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f,

        // Left face
        -0.5f, -0.5f,  0.5f,
        -0.5f, -0.5f, -0.5f,
        -0.5f,  0.5f, -0.5f,
        -0.5f,  0.5f,  0.5f,

        // Right face
         0.5f, -0.5f,  0.5f,
         0.5f, -0.5f, -0.5f,
         0.5f,  0.5f, -0.5f,
         0.5f,  0.5f,  0.5f,

        // Bottom face
        -0.5f, -0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,
         0.5f, -0.5f,  0.5f,
        -0.5f, -0.5f,  0.5f,

        // Top face
        -0.5f,  0.5f, -0.5f,
         0.5f,  0.5f, -0.5f,
         0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f
    };

    GLuint indices[] = {
        // Front face
        0, 1, 2, 2, 3, 0,
        // Back face
        4, 5, 6, 6, 7, 4,
        // Left face
        8, 9, 10, 10, 11, 8,
        // Right face
        12, 13, 14, 14, 15, 12,
        // Bottom face
        16, 17, 18, 18, 19, 16,
        // Top face
        20, 21, 22, 22, 23, 20
    };

    // Setup VAO, VBO, EBO like before
    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glGenBuffers(1, &skyboxEBO);

    glBindVertexArray(skyboxVAO);

    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, skyboxEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

void InitPlaneMesh() {
    // Cube vertices and normals
    std::vector<GLfloat>vertices = {
        // Positions          // Normals              // Texture Coords
        // Front face
        -0.5f, -0.5f, -0.5f,  0.0f, 0.0f, -1.0f,  0.0f, 0.0f,
         0.5f, -0.5f, -0.5f,  0.0f, 0.0f, -1.0f,  1.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  0.0f, 0.0f, -1.0f,  1.0f, 1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f, 0.0f, -1.0f,  0.0f, 1.0f,
    };

    std::vector<GLuint>indices = {
        // Front face
        0, 1, 2, 2, 3, 0,
    };

    // Setup VAO, VBO, EBO like before
    glGenVertexArrays(1, &PMesh.PlaneVAO);
    glGenBuffers(1, &PMesh.PlaneVBO);
    glGenBuffers(1, &PMesh.PlaneEBO);

    PMesh.indexCount = indices.size();

    glBindVertexArray(PMesh.PlaneVAO);

    glBindBuffer(GL_ARRAY_BUFFER, PMesh.PlaneVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(GLfloat), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, PMesh.PlaneEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STATIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (void*)0);
    glEnableVertexAttribArray(0);

    // Normal attribute
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (void*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(2);

    // Texture coordinate attribute
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (void*)(6 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

SphereMesh mesh;

void initSphereMesh(int stacks, int slices) {
    std::vector<GLfloat> vertices;
    std::vector<GLuint> indices;

    float radius = 1.0f; // unit sphere, scale later!

    for (int stack = 0; stack <= stacks; ++stack) {
        float stackAngle = glm::pi<float>() / stacks * stack - glm::pi<float>() / 2.0f;
        float xy = cos(stackAngle);
        float z  = sin(stackAngle);

        for (int slice = 0; slice <= slices; ++slice) {
            float sliceAngle = 2.0f * glm::pi<float>() / slices * slice;
            float x = xy * cos(sliceAngle);
            float y = xy * sin(sliceAngle);

            // position (unit sphere)
            vertices.push_back(x * radius);
            vertices.push_back(y * radius);
            vertices.push_back(z * radius);

            // normal
            glm::vec3 normal = glm::normalize(glm::vec3(x, y, z));
            vertices.push_back(normal.x);
            vertices.push_back(normal.y);
            vertices.push_back(normal.z);

            // texcoords
            float u = (float)slice / slices;
            float v = (float)stack / stacks;
            vertices.push_back(u);
            vertices.push_back(v);
        }
    }

    for (int stack = 0; stack < stacks; ++stack) {
        for (int slice = 0; slice < slices; ++slice) {
            int first  = stack * (slices + 1) + slice;
            int second = first + slices + 1;

            indices.push_back(first);
            indices.push_back(second);
            indices.push_back(first + 1);

            indices.push_back(second);
            indices.push_back(second + 1);
            indices.push_back(first + 1);
        }
    }

    mesh.indexCount = indices.size();

    // Setup VAO/VBO/EBO
    glGenVertexArrays(1, &mesh.SphereVAO);
    glGenBuffers(1, &mesh.SphereVBO);
    glGenBuffers(1, &mesh.SphereEBO);

    glBindVertexArray(mesh.SphereVAO);

    glBindBuffer(GL_ARRAY_BUFFER, mesh.SphereVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(GLfloat), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.SphereEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STATIC_DRAW);

    // position (loc = 0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (void*)0);
    glEnableVertexAttribArray(0);

    // normal (loc = 1)
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (void*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    // texcoord (loc = 2)
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (void*)(6 * sizeof(GLfloat)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
}

bool useLight = false;  // Define the variable

void UseLightUniversaly(bool set) {
    useLight = set;  // Set the value
}
void CheckShaderCompile(GLuint shader, const std::string& name) {
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "ERROR::SHADER::COMPILATION_FAILED (" << name << ")\n"
                  << infoLog << std::endl;
    }
}
//Main shaders
void init() {
    // Vertex Shader source code
    const char* vertexShaderSource = R"(
        #version 330 core
        layout(location = 0) in vec3 aPos;
        layout(location = 1) in vec2 aTexCoord;
        layout(location = 2) in vec3 aNormal;
        layout(location = 3) in ivec4 boneIDs;
        layout(location = 4) in vec4 boneWeights;

        out vec2 TexCoord;
        out vec3 FragPos;
        out vec3 Normal;

        uniform mat4 model;
        uniform mat4 view;
        uniform mat4 projection;

        uniform bool useSkinning;
        uniform mat4 boneMatrices[200]; // You MUST declare this!

        void main() {
            vec4 pos = vec4(aPos, 1.0);
            vec3 normal = aNormal;

            if(useSkinning) {
                mat4 skinMatrix =
                    boneWeights.x * boneMatrices[boneIDs.x] +
                    boneWeights.y * boneMatrices[boneIDs.y] +
                    boneWeights.z * boneMatrices[boneIDs.z] +
                    boneWeights.w * boneMatrices[boneIDs.w];

                pos = skinMatrix * pos;
                normal = mat3(skinMatrix) * normal;
            }

            FragPos = vec3(model * pos);
            Normal = mat3(transpose(inverse(model))) * normal;
            gl_Position = projection * view * vec4(FragPos, 1.0);
            TexCoord = aTexCoord;
        }

    )";

    // Compile vertex shader
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
    glCompileShader(vertexShader);

    // Fragment Shader source code
    const char* fragmentShaderSource = R"(
        #version 330 core
        out vec4 FragColor;

        in vec2 TexCoord;
        in vec3 FragPos;
        in vec3 Normal;

        uniform sampler2D texture1;
        uniform vec3 viewPos;

        uniform int numLights;
        uniform vec3 lightPos[10];   // Max 10 lights, adjust as needed
        uniform vec3 lightColor[10];

        uniform bool isLight;
        uniform bool isCulled;
        uniform bool isShadow;

        void main() {
            if (isCulled) {
                if (dot(normalize(Normal), normalize(viewPos - FragPos)) < 0.0) {
                    discard;
                }
            }

            vec4 texColor = texture(texture1, TexCoord);
            if (texColor.a < 0.1) discard;

            if (isLight) {
                vec3 norm = normalize(Normal);
                vec3 viewDir = normalize(viewPos - FragPos);

                vec3 result = vec3(0.0);

                for (int i = 0; i < numLights; ++i) {
                    float ambientStrength = 0.2;
                    vec3 ambient = ambientStrength * lightColor[i];

                    vec3 lightDir = normalize(lightPos[i] - FragPos);
                    float diff = max(dot(norm, lightDir), 0.0);
                    vec3 diffuse = diff * lightColor[i];

                    float specularStrength = 0.5;
                    vec3 reflectDir = reflect(-lightDir, norm);
                    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
                    vec3 specular = specularStrength * spec * lightColor[i];

                    // Simple shadow: darken if facing away from light
                    if (isShadow && diff <= 0.0) {
                        // Shadows are a darker version of lightColor (or black)
                        diffuse *= 0.1;  // really dim diffuse
                        specular *= 0.0; // no specular in shadow
                    }

                    result += ambient + diffuse + specular;
                }

                result *= texColor.rgb;
                FragColor = vec4(result, texColor.a);
            } else {
                FragColor = texColor;
            }
        }
    )";

    // Compile fragment shader
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr);
    glCompileShader(fragmentShader);

    // Link shaders
    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    // Check for shader linking errors
    GLint success;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
        std::cerr << "ERROR::SHADER::LINKING_FAILED\n" << infoLog << std::endl;
    }
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW); // Counter-clockwise winding order
}
//Initializes the shaders for the cubemap
void CubemapInit() {
    // Vertex Shader source code
    const char* vertexShaderSourceQ = R"(
        #version 330 core
        layout(location = 0) in vec3 aPos;

        out vec3 TexCoords;

        uniform mat4 view;
        uniform mat4 projection;

        void main()
        {
            TexCoords = normalize(aPos);
            vec4 pos = projection * view * vec4(aPos, 1.0);
            gl_Position = pos.xyww; // Remove translation (important!)
        }
    )";

    // Compile vertex shader
    GLuint vertexShaderQ = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShaderQ, 1, &vertexShaderSourceQ, nullptr);
    glCompileShader(vertexShaderQ);

    // Fragment Shader source code
    const char* fragmentShaderSourceQ = R"(
        #version 330 core
        out vec4 FragColor;
        in vec3 TexCoords;  // TexCoords should be interpolated from vertex position
        uniform samplerCube skybox;

        void main()
        {
            FragColor = texture(skybox, TexCoords); // Sample from cubemap
        }
    )";

    // Compile fragment shader
    GLuint fragmentShaderQ = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShaderQ, 1, &fragmentShaderSourceQ, nullptr);
    glCompileShader(fragmentShaderQ);

    // Link shaders
    cubemapProgram = glCreateProgram();
    glAttachShader(cubemapProgram, vertexShaderQ);
    glAttachShader(cubemapProgram, fragmentShaderQ);
    glLinkProgram(cubemapProgram);

    // Check for shader linking errors
    GLint success;
    glGetProgramiv(cubemapProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(cubemapProgram, 512, nullptr, infoLog);
        std::cerr << "ERROR::SHADER::LINKING_FAILED\n" << infoLog << std::endl;
    }
    glFrontFace(GL_CCW); // Counter-clockwise winding order
}

// Light function for a single light source
void AddPhongLight(glm::vec3 lightPos, float intensity, glm::vec3 lightColor, glm::vec3 objectColor, glm::vec3 viewPos, const glm::mat4& model, const Camera& camera,const glm::mat4& projection){
    // Use the light shader program
    glUseProgram(shaderProgram);

    // Pass transformation matrices
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(camera.view));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    // Pass light properties
    glUniform3fv(glGetUniformLocation(shaderProgram, "lightPos"), 1, glm::value_ptr(lightPos));
    glUniform3fv(glGetUniformLocation(shaderProgram, "lightColor"), 1, glm::value_ptr(lightColor * intensity));

    // Pass view (camera) position
    glUniform3fv(glGetUniformLocation(shaderProgram, "viewPos"), 1, glm::value_ptr(viewPos));
}

// Light function for multiple light sources
void SetPhongLights(const std::vector<glm::vec3>& lightPositions, const std::vector<glm::vec3>& lightColors, float intensity, const glm::mat4& model, const Camera& camera, const glm::mat4& projection, const glm::vec3& viewPos) {
    glUseProgram(shaderProgram);

    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(camera.view));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    int numLights = (int)lightPositions.size();
    glUniform1i(glGetUniformLocation(shaderProgram, "numLights"), numLights);

    for (int i = 0; i < numLights; ++i) {
        std::string posName = "lightPos[" + std::to_string(i) + "]";
        std::string colorName = "lightColor[" + std::to_string(i) + "]";
        glUniform3fv(glGetUniformLocation(shaderProgram, posName.c_str()), 1, glm::value_ptr(lightPositions[i]));
        glUniform3fv(glGetUniformLocation(shaderProgram, colorName.c_str()), 1, glm::value_ptr(lightColors[i] * intensity));
    }

    glUniform3fv(glGetUniformLocation(shaderProgram, "viewPos"), 1, glm::value_ptr(viewPos));
}

void drawCube(const glm::vec3& position, const glm::vec3& scale, const glm::vec3& rotationAxis, const Camera& camera, const glm::mat4& projection, GLuint textureID) {

    Frustum frustum;
    frustum.extract(camera, projection);

    glm::vec3 objCenter = position;
    float objRadius = glm::length(scale) * 0.5f;

    if (!frustum.isSphereInside(objCenter, objRadius)) {
        return; // outside view, skip draw
    }

    // Calculate rotation angle and axis
    float rotationAngle = glm::length(rotationAxis); // Angle in degrees based on the magnitude of rotationAxis
    glm::vec3 normalizedAxis = glm::normalize(rotationAxis); // Normalize the rotation axis

    // Make a copy to modify
    glm::vec3 modifiableRotationAxis = rotationAxis;

    if (glm::length(rotationAxis) > 0.01f) {
        modifiableRotationAxis = glm::normalize(rotationAxis) * glm::clamp(glm::length(rotationAxis), 0.0f, 360.0f);
    }

    // Model matrix
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, position);
    model = glm::rotate(model, glm::radians(rotationAngle), normalizedAxis);
    model = glm::scale(model, scale);

    // Activate shader program and set uniforms
    glUseProgram(shaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(camera.view));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    glUniform1i(glGetUniformLocation(shaderProgram, "isLight"), useLight ? 1 : 0);
    glUniform1i(glGetUniformLocation(shaderProgram, "isCulled"), GL_TRUE);
    glUniform1i(glGetUniformLocation(shaderProgram, "isShadow"), GL_TRUE);
    glUniform1i(glGetUniformLocation(shaderProgram, "isShadow"), GL_TRUE);
    glUniform1i(glGetUniformLocation(shaderProgram, "useSkinning"), GL_FALSE);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    // Bind texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glUniform1i(glGetUniformLocation(shaderProgram, "texture1"), 0);

    // Draw cube
    glBindVertexArray(CMesh.CubeVAO);
    glDrawElements(GL_TRIANGLES, CMesh.indexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void drawPlane(const glm::vec3& position, const glm::vec3& scale, const glm::vec3& rotationAxis, const Camera& camera, const glm::mat4& projection, GLuint textureID) {

    Frustum frustum;
    frustum.extract(camera, projection);

    glm::vec3 objCenter = position;
    float objRadius = glm::length(scale) * 0.5f;

    if (!frustum.isSphereInside(objCenter, objRadius)) {
        return; // outside view, skip draw
    }

    // Calculate rotation angle and axis
    float rotationAngle = glm::length(rotationAxis); // Angle in degrees based on the magnitude of rotationAxis
    glm::vec3 normalizedAxis = glm::normalize(rotationAxis); // Normalize the rotation axis

    // Make a copy to modify
    glm::vec3 modifiableRotationAxis = rotationAxis;

    if (glm::length(rotationAxis) > 0.01f) {
        modifiableRotationAxis = glm::normalize(rotationAxis) * glm::clamp(glm::length(rotationAxis), 0.0f, 360.0f);
    }

    // Model matrix
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, position);
    model = glm::rotate(model, glm::radians(rotationAngle), normalizedAxis);
    model = glm::scale(model, scale);

    // Activate shader program and set uniforms
    glUseProgram(shaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(camera.view));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    glUniform1i(glGetUniformLocation(shaderProgram, "isLight"), useLight ? 1 : 0);
    glUniform1i(glGetUniformLocation(shaderProgram, "isCulled"), GL_TRUE);
    glUniform1i(glGetUniformLocation(shaderProgram, "isShadow"), GL_TRUE);
    glUniform1i(glGetUniformLocation(shaderProgram, "isShadow"), GL_TRUE);
    glUniform1i(glGetUniformLocation(shaderProgram, "useSkinning"), GL_FALSE);

    // Bind texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glUniform1i(glGetUniformLocation(shaderProgram, "texture1"), 0);

    // Draw cube
    glBindVertexArray(PMesh.PlaneVAO);
    glDrawElements(GL_TRIANGLES, PMesh.indexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void drawBillboardPlane(const glm::vec3& position, const glm::vec3& scale, const Camera& camera, const glm::mat4& projection, GLuint textureID) {

    Frustum frustum;
    frustum.extract(camera, projection);

    glm::vec3 objCenter = position;
    float objRadius = glm::length(scale) * 0.5f;

    if (!frustum.isSphereInside(objCenter, objRadius)) {
        return; // outside view, skip draw
    }

    // Create billboard orientation
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, position);

    // Extract camera right and up vectors
    glm::vec3 cameraRight = glm::normalize(glm::vec3(camera.view[0][0], camera.view[1][0], camera.view[2][0]));
    glm::vec3 cameraUp = glm::normalize(glm::vec3(camera.view[0][1], camera.view[1][1], camera.view[2][1]));

    // Align the plane to face the camera
    model[0] = glm::vec4(cameraRight * scale.x, 0.0f);
    model[1] = glm::vec4(cameraUp * scale.y, 0.0f);
    model[2] = glm::vec4(glm::normalize(camera.position - position), 0.0f); // Forward vector
    model[3] = glm::vec4(position, 1.0f);

    // Activate shader program and set uniforms
    glUseProgram(shaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(camera.view));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    glUniform1i(glGetUniformLocation(shaderProgram, "isLight"), useLight ? 1 : 0);
    glUniform1i(glGetUniformLocation(shaderProgram, "isCulled"), GL_FALSE);
    glUniform1i(glGetUniformLocation(shaderProgram, "isShadow"), GL_TRUE);
    glUniform1i(glGetUniformLocation(shaderProgram, "useSkinning"), GL_FALSE);

    // Bind texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glUniform1i(glGetUniformLocation(shaderProgram, "texture1"), 0);

    // Draw cube
    glBindVertexArray(PMesh.PlaneVAO);
    glDrawElements(GL_TRIANGLES, PMesh.indexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void drawAnimatedBillboard(const glm::vec3& position, const glm::vec3& scale, const Camera& camera, const glm::mat4& projection, GLuint textureID, const glm::vec3& info, float speed, float elapsedTime, int currentRow, int stopFrame) {

    Frustum frustum;
    frustum.extract(camera, projection);

    glm::vec3 objCenter = position;
    float objRadius = glm::length(scale) * 0.5f;

    if (!frustum.isSphereInside(objCenter, objRadius)) {
        return; // outside view, skip draw
    }

    // Extract animation information
    int columns = static_cast<int>(info.x);
    int rows = static_cast<int>(info.y);
    int totalFrames = static_cast<int>(info.z);

    if (currentRow < 0 || currentRow >= rows) {
        std::cerr << "Error: Invalid currentRow (" << currentRow << "). Must be between 0 and " << rows - 1 << ".\n";
        return;
    }

    int framesPerRow = totalFrames / rows;

    // Animation frame logic
    static float timeAccumulator = 0.0f;
    timeAccumulator += elapsedTime;

    int currentFrame;
    if (stopFrame >= 0 && stopFrame < framesPerRow) {
        currentFrame = stopFrame;
    } else {
        currentFrame = static_cast<int>(timeAccumulator / speed) % framesPerRow;
    }

    // Calculate UV coordinates for the current frame
    float frameWidth = 1.0f / columns;
    float frameHeight = 1.0f / rows;
    int frameColumn = currentFrame % columns;
    float uMin = frameColumn * frameWidth;
    float vMin = 1.0f - (currentRow + 1) * frameHeight;
    float uMax = uMin + frameWidth;
    float vMax = vMin + frameHeight;

    GLfloat vertices[] = {
        // Positions           // Normals               // Texture Coords
        0.5f,  0.5f, 0.0f,   0.0f, 0.0f, -1.0f,  uMax, vMax,  // Top Right
        0.5f, -0.5f, 0.0f,   0.0f, 0.0f, -1.0f,  uMax, vMin,  // Bottom Right
       -0.5f, -0.5f, 0.0f,   0.0f, 0.0f, -1.0f,  uMin, vMin,  // Bottom Left
       -0.5f,  0.5f, 0.0f,   0.0f, 0.0f, -1.0f,  uMin, vMax   // Top Left
    };

    GLuint indices[] = {
        0, 1, 3,
        1, 2, 3
    };

    // Create billboard orientation
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, position);

    // Extract camera right and up vectors
    glm::vec3 cameraRight = glm::normalize(glm::vec3(camera.view[0][0], camera.view[1][0], camera.view[2][0]));
    glm::vec3 cameraUp = glm::normalize(glm::vec3(camera.view[0][1], camera.view[1][1], camera.view[2][1]));

    // Align the plane to face the camera
    model[0] = glm::vec4(cameraRight * scale.x, 0.0f);
    model[1] = glm::vec4(cameraUp * scale.y, 0.0f);
    model[2] = glm::vec4(glm::normalize(camera.position - position), 0.0f); // Forward vector
    model[3] = glm::vec4(position, 1.0f);

    // Setup VAO, VBO, EBO like before
    GLuint VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (void*)0);
    glEnableVertexAttribArray(0);

    // Normal attribute
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (void*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(2);

    // Texture coordinate attribute
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (void*)(6 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    // Activate shader program and set uniforms
    glUseProgram(shaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(camera.view));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
    glUniform1i(glGetUniformLocation(shaderProgram, "isCulled"), GL_FALSE);

    glUniform1i(glGetUniformLocation(shaderProgram, "isLight"), useLight ? 1 : 0);
    glUniform1i(glGetUniformLocation(shaderProgram, "isShadow"), GL_TRUE);
    glUniform1i(glGetUniformLocation(shaderProgram, "useSkinning"), GL_FALSE);

    // Bind texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glUniform1i(glGetUniformLocation(shaderProgram, "texture1"), 0);

    // Draw cube
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, sizeof(indices) / sizeof(GLuint), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    // Cleanup
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
}

void drawSphere(const glm::vec3& position, float radius, const Camera& camera, const glm::mat4& projection, GLuint textureID) {

    Frustum frustum;
    frustum.extract(camera, projection);

    glm::vec3 objCenter = position;
    float objRadius = glm::length(radius) * 0.5f;

    if (!frustum.isSphereInside(objCenter, objRadius)) {
        return; // outside view, skip draw
    }

    // Model matrix
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, position);
    model = glm::scale(model, glm::vec3(radius));

    // Activate shader program and set uniforms
    glUseProgram(shaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(camera.view));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
    glUniform1i(glGetUniformLocation(shaderProgram, "useSkinning"), GL_FALSE);

    glUniform1i(glGetUniformLocation(shaderProgram, "isLight"), GL_FALSE);
    glUniform1i(glGetUniformLocation(shaderProgram, "isCulled"), GL_FALSE);

    // Bind texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glUniform1i(glGetUniformLocation(shaderProgram, "texture1"), 0);

    // Draw sphere
    glBindVertexArray(mesh.SphereVAO);
    glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void drawSkybox(const Camera& camera, const glm::mat4& projection, GLuint cubemapTexture) {
    glDepthFunc(GL_LEQUAL);  // allow skybox to pass depth test
    glDepthMask(GL_FALSE);   // disable depth writing

    glUseProgram(cubemapProgram);

    // Strip translation from view matrix so the skybox follows the camera
    glm::mat4 view = glm::mat4(glm::mat3(camera.view));
    glUniformMatrix4fv(glGetUniformLocation(cubemapProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(cubemapProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    // Bind cubemap texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
    glUniform1i(glGetUniformLocation(cubemapProgram, "skybox"), 0);

    // Draw cube
    glBindVertexArray(skyboxVAO);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS); // restore default
}

void drawTerrainPlane(const glm::vec3& position, const glm::vec3& scale, const glm::vec3& rotationAxis, const Camera& camera, const glm::mat4& projection, GLuint textureID, int resolutionX, int resolutionZ, std::function<float(float, float)> heightFunc) {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;

    // Create vertices
    for (int z = 0; z <= resolutionZ; ++z) {
        for (int x = 0; x <= resolutionX; ++x) {
            float u = (float)x / resolutionX;
            float v = (float)z / resolutionZ;

            float worldX = u;       // Centered around origin
            float worldZ = v;
            float y = heightFunc(worldX, worldZ); // Use height function

            // Position      Normal         UV
            vertices.insert(vertices.end(), {
                worldX, y, worldZ,
                0.0f, 1.0f, 0.0f,   // Flat normals for now
                u, v
            });
        }
    }

    // Create indices
    for (int z = 0; z < resolutionZ; ++z) {
        for (int x = 0; x < resolutionX; ++x) {
            int topLeft = z * (resolutionX + 1) + x;
            int topRight = topLeft + 1;
            int bottomLeft = (z + 1) * (resolutionX + 1) + x;
            int bottomRight = bottomLeft + 1;

            indices.insert(indices.end(), {
                topLeft, bottomLeft, topRight,
                topRight, bottomLeft, bottomRight
            });
        }
    }

    // Calculate rotation angle and axis
    float rotationAngle = glm::length(rotationAxis); // Angle in degrees based on the magnitude of rotationAxis
    glm::vec3 normalizedAxis = glm::normalize(rotationAxis); // Normalize the rotation axis

    // Make a copy to modify
    glm::vec3 modifiableRotationAxis = rotationAxis;

    if (glm::length(rotationAxis) > 0.01f) {
        modifiableRotationAxis = glm::normalize(rotationAxis) * glm::clamp(glm::length(rotationAxis), 0.0f, 360.0f);
    }

    // Model matrix
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, position);
    model = glm::rotate(model, glm::radians(rotationAngle), normalizedAxis);
    model = glm::scale(model, scale);

    // Set up buffers
    GLuint VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    // Position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // Normal
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    // Texture
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glUseProgram(shaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(camera.view));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    glUniform1i(glGetUniformLocation(shaderProgram, "isLight"), useLight ? 1 : 0);
    glUniform1i(glGetUniformLocation(shaderProgram, "isCulled"), GL_FALSE);
    glUniform1i(glGetUniformLocation(shaderProgram, "isSkybox"), GL_FALSE);
    glUniform1i(glGetUniformLocation(shaderProgram, "isShadow"), GL_TRUE);
    glUniform1i(glGetUniformLocation(shaderProgram, "useSkinning"), GL_FALSE);

    glUniform1i(glGetUniformLocation(shaderProgram, "texture1"), 0);

    // Bind texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK); // Default
    glFrontFace(GL_CCW); // Counter-clockwise is front face

    // Draw
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    glDisable(GL_CULL_FACE);

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
}

// Simple Perlin Noise with height
float getHeight(float x, float z, float seed = 1, float frequency = 0.1f, float amplitude = 5.0f) {
    glm::vec2 offset = glm::vec2(
        glm::perlin(glm::vec2(seed, seed + 1)) * 1000.0f,
        glm::perlin(glm::vec2(seed + 2, seed + 3)) * 1000.0f
    );

    glm::vec2 pos = glm::vec2(x * frequency, z * frequency) + offset;
    float noiseValue = glm::perlin(pos); // 2D Perlin noise
    return noiseValue * amplitude;
}
// FBM with height
float fBM(float x, float z, float y = 0.0f, int seed = 1, int octaves = 6, float persistence = 0.5f, float scale = 0.01f) {
    float total = 0.0f;
    float frequency = scale;
    float amplitude = 1.0f;
    float maxAmplitude = 0.0f;

    glm::vec3 seedOffset(
        glm::perlin(glm::vec2(seed, seed + 1)) * 1000.0f,
        glm::perlin(glm::vec2(seed + 2, seed + 3)) * 1000.0f,
        glm::perlin(glm::vec2(seed + 4, seed + 5)) * 1000.0f
    );

    for (int i = 0; i < octaves; ++i) {
        glm::vec3 pos = glm::vec3(x * frequency, y * frequency, z * frequency) + seedOffset;
        total += glm::perlin(pos) * amplitude;
        maxAmplitude += amplitude;
        amplitude *= persistence;
        frequency *= 2.0f;
    }

    return total / maxAmplitude;
}

// Model Loader function
Model LoadModel(const std::string& ModelPath, GLuint fallbackTexture) {
    Model model;
    std::string Path = getAssetPath(ModelPath);
    Assimp::Importer importer;

    const aiScene* scene = importer.ReadFile(Path,
        aiProcess_Triangulate |
        aiProcess_FlipUVs |
        aiProcess_GenSmoothNormals);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::cerr << "ERROR::ASSIMP::" << importer.GetErrorString() << std::endl;
        return model;
    }

    std::string modelFolder = std::filesystem::path(Path).parent_path().string();

    for (unsigned int i = 0; i < scene->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[i];

        std::vector<GLfloat> vertices;
        std::vector<GLfloat> normals;
        std::vector<GLuint> indices;

        for (unsigned int j = 0; j < mesh->mNumVertices; j++) {
            aiVector3D pos = mesh->mVertices[j];
            aiVector3D normal = mesh->mNormals[j];
            aiVector3D uv = mesh->HasTextureCoords(0) ? mesh->mTextureCoords[0][j] : aiVector3D(0.0f, 0.0f, 0.0f);

            vertices.push_back(pos.x);
            vertices.push_back(pos.y);
            vertices.push_back(pos.z);
            vertices.push_back(uv.x);
            vertices.push_back(1.0f - uv.y);

            normals.push_back(normal.x);
            normals.push_back(normal.y);
            normals.push_back(normal.z);
        }

        for (unsigned int j = 0; j < mesh->mNumFaces; j++) {
            aiFace face = mesh->mFaces[j];
            for (unsigned int k = 0; k < face.mNumIndices; k++) {
                indices.push_back(face.mIndices[k]);
            }
        }

        // Create VAO, VBO, EBO, NBO
        Mesh newMesh;
        newMesh.indexCount = static_cast<GLsizei>(indices.size());

        glGenVertexArrays(1, &newMesh.vao);
        glGenBuffers(1, &newMesh.vbo);
        glGenBuffers(1, &newMesh.ebo);
        glGenBuffers(1, &newMesh.nbo);

        glBindVertexArray(newMesh.vao);

        glBindBuffer(GL_ARRAY_BUFFER, newMesh.vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(GLfloat), vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, newMesh.ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ARRAY_BUFFER, newMesh.nbo);
        glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(GLfloat), normals.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ARRAY_BUFFER, newMesh.vbo);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (void*)0); // pos
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (void*)(3 * sizeof(GLfloat))); // uv
        glEnableVertexAttribArray(1);

        glBindBuffer(GL_ARRAY_BUFFER, newMesh.nbo);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (void*)0); // normal
        glEnableVertexAttribArray(2);

        glBindVertexArray(0);

        // Load texture for this mesh (from material)
        GLuint texID = fallbackTexture; // start with default texture

        if (mesh->mMaterialIndex >= 0) {
            aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
            if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
                aiString texPath;
                material->GetTexture(aiTextureType_DIFFUSE, 0, &texPath);

                std::string fullPath = modelFolder + "/" + texPath.C_Str();
                GLuint loaded = LoadDirrectPNG(fullPath);
                if (loaded != 0) texID = loaded; // override fallback only if load succeeds
            }
        }else {
            aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
            // No texture → check diffuse color
            aiColor3D diffuseColor(1.0f, 1.0f, 1.0f);
            if (AI_SUCCESS == material->Get(AI_MATKEY_COLOR_DIFFUSE, diffuseColor)) {
                texID = CreateColorTexture(glm::vec3(diffuseColor.r, diffuseColor.g, diffuseColor.b));
            }
        }

        newMesh.textureID = texID;

        model.meshes.push_back(newMesh);
    }

    return model;
}

// Animated model loader function
GLTFModel LoadGLTF(const std::string& ModelPath) {
    GLTFModel model;
    std::string path = getAssetPath(ModelPath);

    cgltf_options options = {};
    cgltf_data* data = nullptr;

    if (cgltf_parse_file(&options, path.c_str(), &data) != cgltf_result_success ||
        cgltf_load_buffers(&options, data, path.c_str()) != cgltf_result_success ||
        cgltf_validate(data) != cgltf_result_success) {
        std::cerr << "Failed to load GLTF: " << path << "\n";
        if (data) cgltf_free(data);
        return model;
    }

    // --- iterate meshes / primitives ---
    for (size_t m = 0; m < data->meshes_count; ++m) {
        cgltf_mesh* mesh = &data->meshes[m];
        for (size_t p = 0; p < mesh->primitives_count; ++p) {
            cgltf_primitive* prim = &mesh->primitives[p];
            if (!prim->indices) continue;

            GLTFMesh newMesh{};

            // ---- Indices
            std::vector<uint32_t> indices(prim->indices->count);
            for (size_t i = 0; i < prim->indices->count; ++i) {
                uint32_t idx = 0;
                ReadIndex(prim->indices, i, idx);
                indices[i] = idx;
            }
            newMesh.indexCount = static_cast<GLsizei>(indices.size());

            // ---- Attributes
            const cgltf_accessor* posAcc = nullptr;
            const cgltf_accessor* nrmAcc = nullptr;
            const cgltf_accessor* uv0Acc = nullptr;
            const cgltf_accessor* jntAcc = nullptr;
            const cgltf_accessor* wgtAcc = nullptr;

            for (size_t a = 0; a < prim->attributes_count; ++a) {
                const auto& attr = prim->attributes[a];
                if (attr.type == cgltf_attribute_type_position) posAcc = attr.data;
                else if (attr.type == cgltf_attribute_type_normal) nrmAcc = attr.data;
                else if (attr.type == cgltf_attribute_type_texcoord && attr.index == 0) uv0Acc = attr.data;
                else if (attr.type == cgltf_attribute_type_joints && attr.index == 0) jntAcc = attr.data;
                else if (attr.type == cgltf_attribute_type_weights && attr.index == 0) wgtAcc = attr.data;
            }
            if (!posAcc) continue;

            std::vector<Vertex> vertices(posAcc->count);
            std::vector<glm::ivec4> boneIDsInts(posAcc->count);

            for (size_t vtx = 0; vtx < posAcc->count; ++vtx) {
                Vertex v{};
                v.position = ReadVec3(posAcc, vtx);
                v.normal   = nrmAcc ? ReadVec3(nrmAcc, vtx) : glm::vec3(0,0,1);
                glm::vec2 uv = uv0Acc ? ReadVec2(uv0Acc, vtx) : glm::vec2(0,0);
                v.uv = glm::vec2(uv.x, 1.0f - uv.y);

                v.boneIDs     = jntAcc ? ReadJoints4(jntAcc, vtx) : glm::ivec4(0);
                v.boneWeights = wgtAcc ? ReadWeights4(wgtAcc, vtx) : glm::vec4(1,0,0,0);

                vertices[vtx]   = v;
                boneIDsInts[vtx]= v.boneIDs;
            }

            // ---- OpenGL buffers
            glGenVertexArrays(1, &newMesh.vao);
            glBindVertexArray(newMesh.vao);

            // interleave position/normal/uv/weights
            std::vector<float> interleaved;
            interleaved.reserve(vertices.size() * 12);
            for (const auto& v : vertices) {
                interleaved.insert(interleaved.end(), {
                    v.position.x, v.position.y, v.position.z,
                    v.normal.x,   v.normal.y,   v.normal.z,
                    v.uv.x,       v.uv.y,
                    v.boneWeights.x, v.boneWeights.y, v.boneWeights.z, v.boneWeights.w
                });
            }

            glGenBuffers(1, &newMesh.vbo);
            glBindBuffer(GL_ARRAY_BUFFER, newMesh.vbo);
            glBufferData(GL_ARRAY_BUFFER, interleaved.size()*sizeof(float), interleaved.data(), GL_STATIC_DRAW);

            glGenBuffers(1, &newMesh.ebo);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, newMesh.ebo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size()*sizeof(uint32_t), indices.data(), GL_STATIC_DRAW);

            glGenBuffers(1, &newMesh.boneIDsVBO);
            glBindBuffer(GL_ARRAY_BUFFER, newMesh.boneIDsVBO);
            glBufferData(GL_ARRAY_BUFFER, boneIDsInts.size()*sizeof(glm::ivec4), boneIDsInts.data(), GL_STATIC_DRAW);

            // attributes
            glBindBuffer(GL_ARRAY_BUFFER, newMesh.vbo);
            GLsizei stride = 12 * sizeof(float);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)(0));
            glEnableVertexAttribArray(0);

            glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3*sizeof(float)));
            glEnableVertexAttribArray(2);

            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6*sizeof(float)));
            glEnableVertexAttribArray(1);

            glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, stride, (void*)(8*sizeof(float)));
            glEnableVertexAttribArray(4);

            glBindBuffer(GL_ARRAY_BUFFER, newMesh.boneIDsVBO);
            glVertexAttribIPointer(3, 4, GL_INT, sizeof(glm::ivec4), (void*)0);
            glEnableVertexAttribArray(3);

            glBindVertexArray(0);

            // ---- Material / texture
            newMesh.textureID = 0;
            glm::vec4 baseColor(1.0f); // default white

            if (prim->material) {
                cgltf_material* mat = prim->material;

                // --- 1) Read baseColorFactor
                baseColor = glm::make_vec4(mat->pbr_metallic_roughness.base_color_factor);

                // --- 2) Try texture
                if (mat->pbr_metallic_roughness.base_color_texture.texture) {
                    cgltf_texture* tex = mat->pbr_metallic_roughness.base_color_texture.texture;
                    cgltf_image* img = tex->image;

                    if (img) {
                        if (img->uri) {
                            // external PNG/JPG
                            std::string texPath = getAssetPath(img->uri);
                            newMesh.textureID = LoadDirrectPNG(texPath.c_str());
                        }
                        else if (img->buffer_view) {
                            // embedded in GLB
                            cgltf_buffer_view* bv = img->buffer_view;
                            const unsigned char* bufferData = (const unsigned char*)bv->buffer->data;
                            const unsigned char* imgData = bufferData + bv->offset;

                            int width, height, nrChannels;
                            stbi_uc* data = stbi_load_from_memory(
                                imgData,
                                (int)bv->size,
                                &width, &height, &nrChannels,
                                0
                            );

                            if (data) {
                                GLuint texID;
                                glGenTextures(1, &texID);
                                glBindTexture(GL_TEXTURE_2D, texID);

                                GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
                                glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
                                glGenerateMipmap(GL_TEXTURE_2D);

                                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

                                stbi_image_free(data);
                                newMesh.textureID = texID;
                            }
                        }
                    }
                }
            }

            // --- 3) If still no texture, make solid color
            if (newMesh.textureID == 0) {
                newMesh.textureID = CreateColorTexture(glm::vec3(baseColor));
            }


            model.meshes.push_back(std::move(newMesh));
        }
    }

    // --- Skin
    if (data->skins_count > 0) {
        cgltf_skin* skin = &data->skins[0];

        Bone bone;
        bone.inverseBindMatrices.resize(skin->joints_count);
        bone.jointMatrices.resize(skin->joints_count, glm::mat4(1.0f));
        bone.jointParents.resize(skin->joints_count);
        bone.jointNames.resize(skin->joints_count);

        // IBMs
        for (size_t i = 0; i < skin->joints_count; ++i) {
            cgltf_node* joint = skin->joints[i];
            bone.jointNames[i] = joint->name ? joint->name : "unnamed";

            // parent index among *skin joints*
            int parentIndex = -1;
            if (joint->parent) {
                for (size_t j = 0; j < skin->joints_count; ++j) {
                    if (skin->joints[j] == joint->parent) { parentIndex = (int)j; break; }
                }
            }
            bone.jointParents[i] = parentIndex;

            if (skin->inverse_bind_matrices) {
                float m[16];
                cgltf_accessor_read_float(skin->inverse_bind_matrices, i, m, 16);
                bone.inverseBindMatrices[i] = glm::make_mat4(m);
            } else {
                bone.inverseBindMatrices[i] = glm::mat4(1.0f);
            }
        }

        model.bones.push_back(std::move(bone));
    }

    // --- Animations
    if (data->animations_count > 0) {
        for (size_t ai = 0; ai < data->animations_count; ++ai) {
            cgltf_animation& gan = data->animations[ai];
            Animation anim;
            if (gan.name) anim.name = gan.name;

            // samplers
            anim.samplers.reserve(gan.samplers_count);
            for (size_t si = 0; si < gan.samplers_count; ++si) {
                cgltf_animation_sampler& gs = gan.samplers[si];
                AnimationSampler s;

                // input times
                if (gs.input) {
                    s.inputs.resize(gs.input->count);
                    for (size_t t = 0; t < gs.input->count; ++t) {
                        float f[1] = {0};
                        cgltf_accessor_read_float(gs.input, t, f, 1);
                        s.inputs[t] = f[0];
                    }
                    if (!s.inputs.empty())
                        anim.duration = std::max(anim.duration, s.inputs.back());
                }

                // outputs as vec4 (we’ll pack translation/scale into xyz, rotation into xyzw)
                if (gs.output) {
                    s.outputsVec4.resize(gs.output->count);
                    for (size_t t = 0; t < gs.output->count; ++t) {
                        float f[4] = {0,0,0,0};
                        const int comps =
                            (gs.output->type == cgltf_type_vec3) ? 3 :
                            (gs.output->type == cgltf_type_vec4) ? 4 : 0;
                        if (comps) cgltf_accessor_read_float(gs.output, t, f, comps);
                        s.outputsVec4[t] = glm::vec4(f[0], f[1], f[2], f[3]);
                    }
                }

                switch (gs.interpolation) {
                    case cgltf_interpolation_type_step:        s.interpolation = "STEP"; break;
                    case cgltf_interpolation_type_cubic_spline:s.interpolation = "CUBICSPLINE"; break;
                    default:                                   s.interpolation = "LINEAR"; break;
                }

                anim.samplers.push_back(std::move(s));
            }

            // channels
            anim.channels.reserve(gan.channels_count);
            for (size_t ci = 0; ci < gan.channels_count; ++ci) {
                cgltf_animation_channel& gc = gan.channels[ci];
                AnimationChannel ch;

                ch.samplerIndex = int(gc.sampler - gan.samplers);
                ch.targetNode = -1;

                if (gc.target_node && !model.bones.empty()) {
                    cgltf_skin* skin = &data->skins[0];
                    for (size_t j = 0; j < skin->joints_count; ++j) {
                        if (skin->joints[j] == gc.target_node) { ch.targetNode = (int)j; break; }
                    }
                }

                switch (gc.target_path) {
                    case cgltf_animation_path_type_translation: ch.path = "translation"; break;
                    case cgltf_animation_path_type_rotation:    ch.path = "rotation";    break;
                    case cgltf_animation_path_type_scale:       ch.path = "scale";       break;
                    default: break;
                }

                anim.channels.push_back(std::move(ch));
            }

            model.animations.push_back(std::move(anim));
            const std::string& nm = model.animations.back().name;
            if (!nm.empty())
                model.animationNameToIndex[nm] = (int)model.animations.size() - 1;
        }
    }

    cgltf_free(data);
    return model;
}

void UpdateSkinning(const GLTFModel& model, GLTFInstance& instance, float deltaTime) {
    if (model.animations.empty() || model.bones.empty()) return;

    const Bone& bone = model.bones[0];
    size_t jointCount = bone.jointNames.size();
    if (jointCount == 0) return;

    // Ensure instance joint count
    if (instance.jointMatrices.size() != jointCount)
        instance.jointMatrices.resize(jointCount, glm::mat4(1.0f));

    // Check for animation switch
    if (instance.currentAnimation != instance.activeAnimation) {
        instance.previousAnimation = instance.currentAnimation;
        instance.currentAnimation = instance.activeAnimation;
        instance.blendProgress = 0.0f; // start blending
    }

    // Advance times
    float tCurrent = instance.currentTime + deltaTime * instance.playbackSpeed;
    instance.currentTime = tCurrent;

    const Animation& animCurrent = model.animations[instance.currentAnimation];
    const Animation& animPrev = model.animations[instance.previousAnimation];

    float timeCurrent = fmod(tCurrent, animCurrent.duration);
    float timePrev = fmod(tCurrent, animPrev.duration);

    float blendFactor = instance.blendProgress / instance.blendTime;
    if (blendFactor > 1.0f) blendFactor = 1.0f;
    instance.blendProgress += deltaTime;

    // Helper function: get joint transforms for a specific animation & time
    auto computeTransforms = [&](const Animation& anim, float time,
                                 std::vector<glm::vec3>& translations,
                                 std::vector<glm::quat>& rotations,
                                 std::vector<glm::vec3>& scales)
    {
        translations.assign(jointCount, glm::vec3(0.0f));
        rotations.assign(jointCount, glm::quat(1,0,0,0));
        scales.assign(jointCount, glm::vec3(1.0f));

        for (const AnimationChannel& channel : anim.channels) {
            if (channel.samplerIndex < 0 || channel.samplerIndex >= (int)anim.samplers.size())
                continue;

            const AnimationSampler& sampler = anim.samplers[channel.samplerIndex];
            int jointIndex = channel.targetNode;
            if (jointIndex < 0 || jointIndex >= (int)jointCount) continue;

            int index = 0;
            while (index + 1 < (int)sampler.inputs.size() && sampler.inputs[index+1] < time)
                ++index;

            if (index + 1 >= (int)sampler.inputs.size()) continue;
            if (index >= (int)sampler.outputsVec4.size() || index+1 >= (int)sampler.outputsVec4.size())
                continue;

            float t1 = sampler.inputs[index];
            float t2 = sampler.inputs[index+1];
            float interp = (t2>t1) ? (time-t1)/(t2-t1) : 0.0f;

            glm::vec4 v1 = sampler.outputsVec4[index];
            glm::vec4 v2 = sampler.outputsVec4[index+1];

            if (channel.path == "translation")
                translations[jointIndex] = glm::mix(glm::vec3(v1), glm::vec3(v2), interp);
            else if (channel.path == "rotation") {
                glm::quat q1(v1.w,v1.x,v1.y,v1.z);
                glm::quat q2(v2.w,v2.x,v2.y,v2.z);
                rotations[jointIndex] = glm::normalize(glm::slerp(q1,q2,interp));
            } else if (channel.path == "scale")
                scales[jointIndex] = glm::mix(glm::vec3(v1), glm::vec3(v2), interp);
        }
    };

    // Compute current & previous transforms
    std::vector<glm::vec3> transCur, transPrev;
    std::vector<glm::quat> rotCur, rotPrev;
    std::vector<glm::vec3> scaleCur, scalePrev;

    computeTransforms(animCurrent, timeCurrent, transCur, rotCur, scaleCur);
    computeTransforms(animPrev, timePrev, transPrev, rotPrev, scalePrev);

    // Blend between previous and current animations
    std::vector<glm::vec3> translations(jointCount);
    std::vector<glm::quat> rotations(jointCount);
    std::vector<glm::vec3> scales(jointCount);

    for (size_t i=0; i<jointCount; ++i) {
        translations[i] = glm::mix(transPrev[i], transCur[i], blendFactor);
        rotations[i] = glm::normalize(glm::slerp(rotPrev[i], rotCur[i], blendFactor));
        scales[i] = glm::mix(scalePrev[i], scaleCur[i], blendFactor);
    }

    // Compute local/global matrices
    std::vector<glm::mat4> localMatrices(jointCount);
    std::vector<glm::mat4> globalMatrices(jointCount);

    for (size_t i=0; i<jointCount; ++i) {
        glm::mat4 T = glm::translate(glm::mat4(1.0f), translations[i]);
        glm::mat4 R = glm::toMat4(rotations[i]);
        glm::mat4 S = glm::scale(glm::mat4(1.0f), scales[i]);

        localMatrices[i] = T*R*S;
        int parent = bone.jointParents[i];
        if (parent >=0 && parent < (int)jointCount)
            globalMatrices[i] = globalMatrices[parent] * localMatrices[i];
        else
            globalMatrices[i] = localMatrices[i];

        instance.jointMatrices[i] = globalMatrices[i] * bone.inverseBindMatrices[i];
    }
}

void uploadBoneMatrices(GLuint program, const std::vector<glm::mat4>& mats) {
    if (mats.empty()) return;
    GLint loc = glGetUniformLocation(program, "boneMatrices[0]");
    if (loc < 0) return;

    // clamp to what the shader declared (100)
    GLsizei count = (GLsizei)std::min<size_t>(mats.size(), 100);
    glUniformMatrix4fv(loc, count, GL_FALSE, glm::value_ptr(mats[0]));
}

//Draw GLFT model
void drawGLTFModel(
    GLTFModel& model,          // Static mesh/animations
    GLTFInstance& instance,          // Per-draw state (time, speed, active anim, joints)
    const glm::vec3& position,
    const glm::vec3& scale,
    const glm::vec3& rotationAxis,
    const Camera& camera,
    const glm::mat4& projection,
    GLuint textureID,
    float deltaTime){

    Frustum frustum;
    frustum.extract(camera, projection);

    glm::vec3 objCenter = position;
    float objRadius = glm::length(scale) * 0.5f;

    if (!frustum.isSphereInside(objCenter, objRadius)) {
        return; // outside view, skip draw
    }

    GLuint defaultWhiteTex = CreateDefaultWhiteTexture();// just makes a white texture

    // --- Update animation for this specific instance ---
    UpdateSkinning(model, instance, deltaTime);

    // Upload skinning matrices for THIS instance
    if (!instance.jointMatrices.empty()) {
        uploadBoneMatrices(shaderProgram, instance.jointMatrices);
        glUniform1i(glGetUniformLocation(shaderProgram, "useSkinning"), GL_TRUE);
    } else {
        glUniform1i(glGetUniformLocation(shaderProgram, "useSkinning"), GL_FALSE);
    }

    glUseProgram(shaderProgram);

    // Build model matrix
    float rotationAngle = glm::length(rotationAxis);
    glm::vec3 axis = rotationAngle > 0.0001f ? glm::normalize(rotationAxis) : glm::vec3(0, 1, 0);

    glm::mat4 modelMatrix(1.0f);
    modelMatrix = glm::translate(modelMatrix, position);
    if (rotationAngle > 0.0001f)
        modelMatrix = glm::rotate(modelMatrix, glm::radians(rotationAngle), axis);
    modelMatrix = glm::scale(modelMatrix, scale);

    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(modelMatrix));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"),  1, GL_FALSE, glm::value_ptr(camera.view));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    // Optional flags
    auto uni = [&](const char* n) { return glGetUniformLocation(shaderProgram, n); };
    if (auto u = uni("isLight"); u >= 0) glUniform1i(u, useLight ? 1 : 0);
    if (auto u = uni("isCulled"); u >= 0) glUniform1i(u, GL_FALSE);
    if (auto u = uni("isSkybox"); u >= 0) glUniform1i(u, GL_FALSE);
    if (auto u = uni("isShadow"); u >= 0) glUniform1i(u, GL_TRUE);

    // Texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);
    if (auto u = uni("texture1"); u >= 0) glUniform1i(u, 0);

    // Draw call
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    for (auto& sub : model.meshes) {
        GLuint texToUse = 0;

        if (textureID) {
            texToUse = textureID;              // override from argument
        } else if (sub.textureID) {
            texToUse = sub.textureID;          // glTF mesh texture
        } else {
            texToUse = defaultWhiteTex;        // fallback
        }

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texToUse);

        if (auto u = uni("texture1"); u >= 0)
            glUniform1i(u, 0);

        glBindVertexArray(sub.vao);
        glDrawElements(GL_TRIANGLES, sub.indexCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }


    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glBindTexture(GL_TEXTURE_2D, 0);
}

// Draw Model function
void drawModel(const Model& model, const glm::vec3& position, const glm::vec3& scale, const glm::vec3& rotationAxis, const Camera& camera, const glm::mat4& projection, GLuint textureID) {
    glUseProgram(shaderProgram);

    Frustum frustum;
    frustum.extract(camera, projection);

    glm::vec3 objCenter = position;
    float objRadius = glm::length(scale) * 0.5f;

    if (!frustum.isSphereInside(objCenter, objRadius)) {
        return; // outside view, skip draw
    }

    // Compute rotation
    float rotationAngle = glm::length(rotationAxis);
    glm::vec3 normalizedAxis = glm::normalize(rotationAxis);

    // Model matrix
    glm::mat4 modelMatrix = glm::mat4(1.0f);
    modelMatrix = glm::translate(modelMatrix, position);
    if (rotationAngle > 0.01f)
        modelMatrix = glm::rotate(modelMatrix, glm::radians(rotationAngle), normalizedAxis);
    modelMatrix = glm::scale(modelMatrix, scale);

    // Send matrices to the shader
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(modelMatrix));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(camera.view));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    // Shader flags
    glUniform1i(glGetUniformLocation(shaderProgram, "isLight"), useLight ? 1 : 0);
    glUniform1i(glGetUniformLocation(shaderProgram, "isCulled"), GL_TRUE);
    glUniform1i(glGetUniformLocation(shaderProgram, "isSkybox"), GL_FALSE);
    glUniform1i(glGetUniformLocation(shaderProgram, "isShadow"), GL_TRUE);
    glUniform1i(glGetUniformLocation(shaderProgram, "useSkinning"), GL_FALSE);

    // Enable blending and backface culling
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    for (const auto& mesh : model.meshes) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mesh.textureID);
        glUniform1i(glGetUniformLocation(shaderProgram, "texture1"), 0);

        glBindVertexArray(mesh.vao);
        glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, 0);
    }

    // Cleanup
    glDisable(GL_CULL_FACE);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_BLEND);
}

// This is just for maps that doesn't use frustum culling
void drawMap(const Model& model, const glm::vec3& position, const glm::vec3& scale, const glm::vec3& rotationAxis, const Camera& camera, const glm::mat4& projection, GLuint textureID) {
    glUseProgram(shaderProgram);

    // Compute rotation
    float rotationAngle = glm::length(rotationAxis);
    glm::vec3 normalizedAxis = glm::normalize(rotationAxis);

    // Model matrix
    glm::mat4 modelMatrix = glm::mat4(1.0f);
    modelMatrix = glm::translate(modelMatrix, position);
    if (rotationAngle > 0.01f)
        modelMatrix = glm::rotate(modelMatrix, glm::radians(rotationAngle), normalizedAxis);
    modelMatrix = glm::scale(modelMatrix, scale);

    // Send matrices to the shader
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(modelMatrix));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(camera.view));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    // Shader flags
    glUniform1i(glGetUniformLocation(shaderProgram, "isLight"), useLight ? 1 : 0);
    glUniform1i(glGetUniformLocation(shaderProgram, "isCulled"), GL_TRUE);
    glUniform1i(glGetUniformLocation(shaderProgram, "isSkybox"), GL_FALSE);
    glUniform1i(glGetUniformLocation(shaderProgram, "isShadow"), GL_TRUE);
    glUniform1i(glGetUniformLocation(shaderProgram, "useSkinning"), GL_FALSE);

    // Enable blending and backface culling
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    for (const auto& mesh : model.meshes) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mesh.textureID);
        glUniform1i(glGetUniformLocation(shaderProgram, "texture1"), 0);

        glBindVertexArray(mesh.vao);
        glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, 0);
    }

    // Cleanup
    glDisable(GL_CULL_FACE);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_BLEND);
}

//PNG loader
GLuint LoadPNG(const std::string& ImagePath) {
    std::string Path = getAssetPath(ImagePath);  // Use your getAssetPath
    int width, height, nrChannels;

    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(Path.c_str(), &width, &height, &nrChannels, 0);

    if (!data) {
        std::cerr << "Failed to load PNG texture: " << Path << std::endl;
        return 0;
    }

    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;

    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(data);

    if (format == GL_RGBA) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    return textureID;
}

GLuint LoadDirrectPNG(const std::string& ImagePath) {
    int width, height, nrChannels;

    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(ImagePath.c_str(), &width, &height, &nrChannels, 0);

    if (!data) {
        std::cerr << "Failed to load PNG texture: " << ImagePath << std::endl;
        return 0;
    }

    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;

    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(data);

    if (format == GL_RGBA) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    return textureID;
}

GLuint CreateColorTexture(const glm::vec3& color) {
    GLuint texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);

    unsigned char data[3] = {
        static_cast<unsigned char>(color.r * 255.0f),
        static_cast<unsigned char>(color.g * 255.0f),
        static_cast<unsigned char>(color.b * 255.0f)
    };

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glBindTexture(GL_TEXTURE_2D, 0);
    return texID;
}

// Create a texture from camera's perspective
GLuint CaptureCameraTexture(const Camera& cam, const glm::mat4& projection, int width, int height) {
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);

    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, -0, 0, width, height, 0);

    // Set params so it maps nicely
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, 0);

    return tex;
}

//For multiple textures inside a single model file
std::vector<GLuint> LoadModelTextures(const std::string& modelFolderRelativePath) {
    std::vector<GLuint> textures;

    std::string folderPath = getAssetPath(modelFolderRelativePath);

    for (const auto& entry : std::filesystem::directory_iterator(folderPath)) {
        if (entry.is_regular_file()) {
            std::string path = entry.path().string(); // FULL path from iterator
            std::string ext = entry.path().extension().string();

            if (ext == ".png" || ext == ".PNG") {
                GLuint texID = LoadDirrectPNG(path); // DO NOT call getAssetPath here
                if (texID != 0) textures.push_back(texID);
            }
        }
    }

    return textures;
}

//Cubemap loader
GLuint LoadCubemap(const std::vector<std::string>& faces) {
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(false);
    for (unsigned int i = 0; i < faces.size(); i++) {
        std::string path = getAssetPath(faces[i]); // Convert to full relative path
        unsigned char *data = stbi_load(path.c_str(), &width, &height, &nrChannels, 0);
        if (data) {
            // Determine the texture format
            GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;

            // Set the texture for the current face
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                         0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);

            // Free the image data after uploading to GPU
            stbi_image_free(data);
        } else {
            std::cout << "Failed to load texture at path: " << path << std::endl;
            stbi_image_free(data); // Make sure to free the image data
        }
    }

    // Set the texture parameters
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return textureID;
}

GLuint CreateDefaultWhiteTexture() {
    GLuint texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);

    unsigned char whitePixel[4] = { 255, 255, 255, 255 }; // RGBA = white

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, whitePixel);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glBindTexture(GL_TEXTURE_2D, 0);
    return texID;
}

//function for unloading textures from the GPU's RAM
void unloadTextures(const std::vector<GLuint>& textures) {
    glDeleteTextures(textures.size(), &textures[0]);
}

void SetGLFWIcon(GLFWwindow* window, const std::string& iconPath) {
    std::string Path = getAssetPath(iconPath);
    int width, height, nrChannels;

    stbi_set_flip_vertically_on_load(false);  // Icons shouldn't be flipped
    unsigned char* data = stbi_load(Path.c_str(), &width, &height, &nrChannels, 4); // Force RGBA

    if (!data) {
        std::cerr << "Failed to load icon: " << Path << std::endl;
        return;
    }

    GLFWimage icon;
    icon.width = width;
    icon.height = height;
    icon.pixels = data;

    glfwSetWindowIcon(window, 1, &icon);

    stbi_image_free(data);
}

int RandomNumberInt(int low, int high, int& seed){
    const int b = 1103515245; // Multiplier
    const int a = 12345;      // Increment
    const int m = 2147483647; // Modulus (2^31 - 1)

    // Update the seed using LCG
    seed = (seed * b + a) % m;

    // Ensure that the range [low, high] is valid
    if (low > high) {
        std::swap(low, high); // Swap if low > high
    }

    // Map the result to the range [low, high]
    int range = high - low + 1;
    int randomValue = seed % range;

    // Ensure the result is non-negative
    if (randomValue < 0) {
        randomValue += range;
    }

    return low + randomValue;
}

float RandomNumberFloat(float low, float high, float& seed) {
    const float b = 1103515245.f;
    const float a = 12345.f;
    const float m = 2147483647.f;

    // Update the seed using LCG
    seed = fmod(seed * b + a, m);  // fmod works for float modulus

    if (low > high) std::swap(low, high);

    float randomValue = seed / m; // normalized [0,1)
    return low + randomValue * (high - low);
}

void SetProjectionMatrix(const glm::mat4& projectionMatrix) {
    projection = projectionMatrix; // Update global projection matrix
    glUseProgram(shaderProgram); // Ensure shader is active
    GLint projLoc = glGetUniformLocation(shaderProgram, "projection");
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));
}

void cleanup() {
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    glDeleteProgram(shaderProgram);

    glDeleteShader(vertexShaderQ);
    glDeleteShader(fragmentShaderQ);
    glDeleteProgram(cubemapProgram);

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);

    glDeleteVertexArrays(1, &CMesh.CubeVAO);
    glDeleteBuffers(1, &CMesh.CubeVBO);
    glDeleteBuffers(1, &CMesh.CubeEBO);

    glDeleteVertexArrays(1, &PMesh.PlaneVAO);
    glDeleteBuffers(1, &PMesh.PlaneVBO);
    glDeleteBuffers(1, &PMesh.PlaneEBO);

    glDeleteVertexArrays(1, &mesh.SphereVAO);
    glDeleteBuffers(1, &mesh.SphereVBO);
    glDeleteBuffers(1, &mesh.SphereEBO);
}

//Culling check with spheres
bool isSphereInsideFrustum(const glm::vec3& center, float radius, const Frustum& frustum) {
    for (int i = 0; i < 6; i++) {
        const FrustumPlane& plane = frustum.planes[i];
        float distance = glm::dot(plane.normal, center) + plane.d;

        // If sphere is completely outside a plane → culled
        if (distance < -radius) {
            return false;
        }
    }
    return true;
}
