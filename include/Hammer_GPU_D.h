#ifndef HAMMER_GPU_D_H
#define HAMMER_GPU_D_H

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

#define GLM_ENABLE_EXPERIMENTAL

// OpenGL libraries
#include <GL/glew.h>
#include <GLFW/glfw3.h>
// GLM library
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/noise.hpp>
#include <glm/gtx/quaternion.hpp>
// Assimp includes
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
// Other
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <unordered_map>
#include <map>
#include <functional>
#include <cmath>
#include <numeric>

static std::string getAssetPath(const std::string& assetRelativePath) {
    return std::filesystem::current_path().string() + "/Assets/" + assetRelativePath;
}

class Camera {
public:
    glm::vec3 position;
    glm::vec3 forward;
    glm::vec3 up;
    glm::vec3 right;
    glm::mat4 view;
    float fov;
    float far;
    float near;

    Camera(glm::vec3 startPosition, glm::vec3 startUp, float startFOV = 45.0f , float startNear = 0.01f, float startFar = 100.0f)
        : position(startPosition), up(startUp), fov(startFOV), near(startNear), far(startFar) {
        forward = glm::vec3(0.0f, 0.0f, -1.0f);
        right = glm::normalize(glm::cross(forward, up));
        updateViewMatrix();
    }

    void updateViewMatrix() {
        right = glm::normalize(glm::cross(forward, up));
        view = glm::lookAt(position, position + forward, up);
    }

    void move(const glm::vec3& direction) {
        position += direction;
        updateViewMatrix();
    }

    void rotate(float angle, const glm::vec3& axis, bool global = false) {
        glm::mat4 rotationMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(angle), axis);

        if (global) {
            forward = glm::normalize(glm::vec3(rotationMatrix * glm::vec4(forward, 0.0f)));
            up = glm::normalize(glm::vec3(rotationMatrix * glm::vec4(up, 0.0f)));
        } else {
            forward = glm::normalize(glm::mat3(rotationMatrix) * forward);
            up = glm::normalize(glm::cross(right, forward));
        }

        right = glm::normalize(glm::cross(forward, up));
        updateViewMatrix();
    }

    void moveRelative(float deltaRight, float deltaUp, float deltaForward) {
        position += forward * deltaForward;
        position += right * deltaRight;
        position += up * deltaUp;
        updateViewMatrix();
    }

    void lookAt(const glm::vec3& target) {
        forward = glm::normalize(target - position);
        right = glm::normalize(glm::cross(forward, up));
        up = glm::normalize(glm::cross(right, forward));
        updateViewMatrix();
    }

    void followTarget(const glm::vec3& target, float distance, float yaw, float pitch) {
        // Convert spherical coords (yaw, pitch, distance) into cartesian offset
        glm::vec3 offset;
        offset.x = distance * cos(glm::radians(pitch)) * cos(glm::radians(yaw));
        offset.y = distance * sin(glm::radians(pitch));
        offset.z = distance * cos(glm::radians(pitch)) * sin(glm::radians(yaw));

        // Position camera behind the target
        position = target - offset;

        // Always look at target
        lookAt(target);
    }
};

struct FrustumPlane {
    glm::vec3 normal;
    float d;
};

struct Frustum {
    FrustumPlane planes[6]; // left, right, bottom, top, near, far

    // Build the frustum from camera + projection
    void extract(const Camera& camera, const glm::mat4& projection) {
        glm::mat4 vp = projection * camera.view;

        // Left
        planes[0].normal = glm::vec3(vp[0][3] + vp[0][0],
                                     vp[1][3] + vp[1][0],
                                     vp[2][3] + vp[2][0]);
        planes[0].d = vp[3][3] + vp[3][0];

        // Right
        planes[1].normal = glm::vec3(vp[0][3] - vp[0][0],
                                     vp[1][3] - vp[1][0],
                                     vp[2][3] - vp[2][0]);
        planes[1].d = vp[3][3] - vp[3][0];

        // Bottom
        planes[2].normal = glm::vec3(vp[0][3] + vp[0][1],
                                     vp[1][3] + vp[1][1],
                                     vp[2][3] + vp[2][1]);
        planes[2].d = vp[3][3] + vp[3][1];

        // Top
        planes[3].normal = glm::vec3(vp[0][3] - vp[0][1],
                                     vp[1][3] - vp[1][1],
                                     vp[2][3] - vp[2][1]);
        planes[3].d = vp[3][3] - vp[3][1];

        // Near
        planes[4].normal = glm::vec3(vp[0][3] + vp[0][2],
                                     vp[1][3] + vp[1][2],
                                     vp[2][3] + vp[2][2]);
        planes[4].d = vp[3][3] + vp[3][2];

        // Far
        planes[5].normal = glm::vec3(vp[0][3] - vp[0][2],
                                     vp[1][3] - vp[1][2],
                                     vp[2][3] - vp[2][2]);
        planes[5].d = vp[3][3] - vp[3][2];

        // Normalize planes
        for (int i = 0; i < 6; i++) {
            float len = glm::length(planes[i].normal);
            planes[i].normal /= len;
            planes[i].d /= len;
        }
    }

    // Check if a sphere is inside the frustum
    bool isSphereInside(const glm::vec3& center, float radius) const {
        float margin = 20.0f; // tweak this value for smoother culling
        for (int i = 0; i < 6; i++) {
            float distance = glm::dot(planes[i].normal, center) + planes[i].d;
            if (distance < -radius - margin) {
                return false; // completely outside with margin
            }
        }
        return true;
    }
};


struct Mesh {
    GLuint vao, vbo, ebo, nbo;
    GLsizei indexCount;
    GLuint textureID;  // each mesh gets its own texture
};

struct Model {
    std::vector<Mesh> meshes;
};


struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;

    glm::ivec4 boneIndices = glm::ivec4(0);
    glm::ivec4 boneIDs = glm::ivec4(0);
    glm::vec4 boneWeights = glm::vec4(0.0f);
};

// Simple GLTFModel struct holding OpenGL buffers + skin data
struct Bone {
    std::string name;
    int parentIndex;

    glm::mat4 inverseBindMatrix;
    glm::mat4 localTransform;
    glm::mat4 globalTransform;

    // Added to satisfy compiler
    std::vector<glm::mat4> inverseBindMatrices; // may represent per-joint IBMs
    std::vector<glm::mat4> jointMatrices;       // final pose transforms per joint
    std::vector<int> jointParents;              // parent indices per joint
    std::vector<std::string> jointNames;        // joint names for mapping
};

struct AnimationSampler {
    std::vector<float> inputs; // Times
    std::vector<glm::vec4> outputsVec4; // Transforms (vec3 or quat)
    std::string interpolation; // Usually "LINEAR"
};

struct AnimationChannel {
    std::string path; // "rotation", "translation", "scale"
    int samplerIndex;
    int targetNode;
};

struct Animation {
    std::string name;
    std::vector<AnimationSampler> samplers;
    std::vector<AnimationChannel> channels;
    float duration = 0.0f;
};

struct GLTFMesh{
    GLuint vao, vbo, ebo;
    GLuint boneIDsVBO = 0;
    GLsizei indexCount;
    GLuint textureID;  // each mesh gets its own texture
};

struct GLTFModel {
    std::vector<GLTFMesh> meshes; //Holds every submesh

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    std::vector<Bone> bones;
    std::unordered_map<std::string, int> boneNameToIndex;

    std::vector<int> jointNodeIndices;

    std::vector<Animation> animations;
    std::unordered_map<std::string, int> animationNameToIndex;

    glm::mat4 globalInverseTransform = glm::mat4(1.0f);

    float currentTime; // Needed for playback tracking
};

struct GLTFInstance {
    float currentTime = 0.0f;
    int activeAnimation = 0;    // desired animation
    int currentAnimation = 0;   // currently playing animation
    float playbackSpeed = 1.0f;

    std::vector<glm::mat4> jointMatrices; // final skinning matrices

    // For blending
    int previousAnimation = 0;
    float blendTime = 0.2f;       // duration of blend in seconds
    float blendProgress = 1.0f;   // 1.0 = fully on currentAnimation

    GLTFInstance(size_t jointCount = 0) {
        jointMatrices.resize(jointCount, glm::mat4(1.0f));
    }
};

//structure that holds plane mesh creation
struct SphereMesh {
    GLuint SphereVAO, SphereVBO, SphereEBO;
    GLsizei indexCount;
};

struct CubeMesh {
    GLuint CubeVAO, CubeVBO, CubeEBO;
    GLsizei indexCount;
};

struct PlaneMesh {
    GLuint PlaneVAO, PlaneVBO, PlaneEBO;
    GLsizei indexCount;
};

//OpenGl3.0 particles
struct Particle {
    glm::vec3 position;
    float life;         // Remaining life
    float age;                // used for animation, e.g., Fire spin
    std::string type;   // "Fire", "Water", "Gas"
    size_t maxDrawParticles; // Maximum number of particles being drawn
};

    // Function declarations
    void init();
    void CubemapInit();
    void SetProjectionMatrix(const glm::mat4& projectionMatrix);

    void AddPhongLight(glm::vec3 lightPos, float intensity, glm::vec3 lightColor, glm::vec3 objectColor, glm::vec3 viewPos, const glm::mat4& model, const Camera& camera, const glm::mat4& projection);
    void SetPhongLights(const std::vector<glm::vec3>& lightPositions, const std::vector<glm::vec3>& lightColors, float intensity, const glm::mat4& model, const Camera& camera, const glm::mat4& projection, const glm::vec3& viewPos);

    void drawCube(const glm::vec3& position, const glm::vec3& scale, const glm::vec3& rotationAxis, const Camera& camera, const glm::mat4& projection, GLuint textureID);
    void drawPlane(const glm::vec3& position, const glm::vec3& scale, const glm::vec3& rotationAxis, const Camera& camera, const glm::mat4& projection, GLuint textureID);

    void drawBillboardPlane(const glm::vec3& position, const glm::vec3& scale, const Camera& camera, const glm::mat4& projection, GLuint textureID);
    void drawAnimatedBillboard(const glm::vec3& position, const glm::vec3& scale, const Camera& camera, const glm::mat4& projection, GLuint textureID, const glm::vec3& Info, float speed, float elapsedTime, int currentRow, int StopFrame);

    void drawSphere(const glm::vec3& position, float radius, const Camera& camera, const glm::mat4& projection, GLuint textureID);

    void drawModel(const Model& model, const glm::vec3& position, const glm::vec3& scale, const glm::vec3& rotationAxis, const Camera& camera, const glm::mat4& projection, GLuint textureID);
    void drawAnimatedModel(Model& model, const glm::vec3& position, const glm::vec3& scale, const glm::vec3& rotationAxis, const Camera& camera, const glm::mat4& projection, GLuint textureID, float ElapsedTime);
    void drawMap(const Model& model, const glm::vec3& position, const glm::vec3& scale, const glm::vec3& rotationAxis, const Camera& camera, const glm::mat4& projection, GLuint textureID);

    void drawGLTFModel(GLTFModel& model, GLTFInstance& instance, const glm::vec3& position, const glm::vec3& scale, const glm::vec3& rotationAxis, const Camera& camera, const glm::mat4& projection, GLuint textureID, float ElapsedTime);
    void UpdateSkinning(GLTFModel& model, float elapsedTime);
    void uploadBoneMatrices(GLuint program, const std::vector<glm::mat4>& mats);

    void drawTerrainPlane(const glm::vec3& position, const glm::vec3& scale, const glm::vec3& rotationAxis, const Camera& camera, const glm::mat4& projection, GLuint textureID, int resolutionX = 64, int resolutionZ = 64, std::function<float(float, float)> heightFunc = [](float x, float z) { return 0.0f; });
    void drawSkybox(const Camera& camera, const glm::mat4& projection, GLuint Faces);

    void unloadTextures(const std::vector<GLuint>& textures);
    void UnloadModels(const std::vector<Model>& models);

    int RandomNumberInt(int low, int high, int& seed);
    float RandomNumberFloat(float low, float high, float& seed);

    float getHeight(float x, float z, float seed, float frequency, float amplitude);

    float fBM(float x, float z, float y, int seed, int octaves, float persistence, float scale);

    void SetGLFWIcon(GLFWwindow* window, const std::string& iconPath);

    void cleanup();

    void UseLightUniversaly(bool set);

    bool isSphereInsideFrustum(const glm::vec3& center, float radius, const Frustum& frustum);

    //Mesh declarations
    void InitMeshes(int stacks, int slices);
    void InitCubeMesh();
    void InitPlaneMesh();
    void initSphereMesh(int stacks, int slices);
    void InitSkyboxMesh();

    GLuint LoadPNG(const std::string& ImagePath);
    GLuint LoadDirrectPNG(const std::string& ImagePath);
    std::vector<GLuint> LoadModelTextures(const std::string& modelFolderRelativePath);
    GLuint CreateColorTexture(const glm::vec3& color);
    GLuint CaptureCameraTexture(const Camera& cam, const glm::mat4& projection, int width, int height);

    Model LoadModel(const std::string& ModelPath, GLuint fallbackTexture = 0);
    GLTFModel LoadGLTF(const std::string& ModelPath);

    GLuint LoadCubemap(const std::vector<std::string>& faces);

    GLuint CreateDefaultWhiteTexture();

    void CheckShaderCompile(GLuint shader, const std::string& name);

    //Remember these are particles for OpenGL 3.0 a version that is for older hardware
    //This is also good for prototyping and stuff
    class ParticleSystem {
    public:
        std::vector<Particle> particles;
         // limit how many particles are drawn per frame

        void spawnParticle(const glm::vec3& position, float life, float age, const std::string& type, size_t maxDrawParticles = 50) {
            particles.push_back({position, life, age, type, maxDrawParticles});
        }

        void updateAndDraw(float deltaTime, const Camera& camera, const glm::mat4& projection, GLuint textureID) {
            std::vector<Particle> aliveParticles;

            float maxSpeed = 1.0f;

            // First, update all particles
            for (auto& p : particles) {
                glm::vec3 offset(0.0f);
                float random = ((rand() % 1000) / 1000.0f) * 2.0f - 1.0f;

                if (p.type == "Fire") {
                    glm::vec3 FireVelocity = glm::vec3(random * 2.0f, 0.5f, random * 2.0f);

                    // Clamp
                    if (glm::length(FireVelocity) > maxSpeed)
                        FireVelocity = glm::normalize(FireVelocity) * maxSpeed;

                    offset = FireVelocity * deltaTime;
                }
                else if (p.type == "Water") {
                    glm::vec3 WaterVelocity = glm::vec3(-random, -1.0f, -random);

                    if (glm::length(WaterVelocity) > maxSpeed)
                        WaterVelocity = glm::normalize(WaterVelocity) * maxSpeed;

                    offset = WaterVelocity * deltaTime;
                }
                else if (p.type == "Gas") {
                    glm::vec3 GasVelocity = glm::vec3(random * 5.0f, 0.1f, random * 5.0f);

                    if (glm::length(GasVelocity) > maxSpeed)
                        GasVelocity = glm::normalize(GasVelocity) * maxSpeed;

                    offset = -GasVelocity * deltaTime;
                }

                p.position += offset;
                p.life -= deltaTime;

                if (p.life > 0.0f) aliveParticles.push_back(p);
            }

            size_t totalAlive = aliveParticles.size();
            size_t drawCount = 0;

            if (!aliveParticles.empty()) {
                drawCount = std::min(totalAlive, aliveParticles[0].maxDrawParticles);
            }

            for (size_t i = 0; i < drawCount; ++i) {
                size_t index = i * totalAlive / drawCount;
                drawBillboardPlane(aliveParticles[index].position, glm::vec3(1.0f), camera, projection, textureID);
            }

            particles = aliveParticles; // remove dead particles
        }

    };

    // Shader program and buffers
    extern GLuint shaderProgram;
    extern GLuint vertexShader, fragmentShader;

    extern GLuint cubemapProgram;
    extern GLuint vertexShaderQ, fragmentShaderQ;

    extern GLuint ParticleProgram;
    extern GLuint vertexShaderP, fragmentShaderP;

    extern GLuint computeShader;
    extern GLuint ComputeProgram;
    extern GLuint ssbo;

    extern GLuint quadVAO, quadVBO, quadEBO;

    extern bool useLight;

    extern GLuint VAO, VBO, EBO;
    extern GLuint skyboxVAO, skyboxVBO, skyboxEBO;
    extern glm::mat4 projection;

#endif // HAMMER_GPU_D_H
