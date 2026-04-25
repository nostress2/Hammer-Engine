#ifndef HAMMER_ADVANCED_PARTICLES_H_INCLUDED
#define HAMMER_ADVANCED_PARTICLES_H_INCLUDED

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

// OpenGL libraries
#include <GL/glew.h>
#include <GLFW/glfw3.h>
// GLM library
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/noise.hpp>
#include <glm/gtx/quaternion.hpp>
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

//OpenGL4.3 particles
struct GPU_Particle {
    glm::vec4 position; // xyz = position, w = life
    glm::vec4 velocity; // xyz = velocity, w = age
    float size; // Size of individual particle
};
extern size_t numParticles;
extern std::vector<GPU_Particle> particles;

//Initializes the shaders for the particles
void ParticleInit() {
    // Vertex Shader source code
    const char* vertexShaderSourceP = R"(
        #version 430
        layout(location = 0) in vec4 inPosition;
        layout(location = 1) in float inSize;

        uniform mat4 model;
        uniform mat4 view;
        uniform mat4 projection;
        uniform float pointSize;

        out float vLife;

        void main() {
            gl_Position = projection * view * model * inPosition;
            gl_PointSize = inSize;
            vLife = inPosition.w;
        }
    )";

    // Compile vertex shader
    GLuint vertexShaderP = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShaderP, 1, &vertexShaderSourceP, nullptr);
    glCompileShader(vertexShaderP);

    // Fragment Shader source code
    const char* fragmentShaderSourceP = R"(
        #version 430
        out vec4 FragColor;
        in float vLife;

        uniform vec4 color; // particle color
        const float maxLife = 5.0;

        void main() {
            float alpha = clamp(vLife / maxLife, 0.0, 1.0);
            FragColor = vec4(color.rgb, alpha * color.a);
        }

    )";

    // Compile fragment shader
    GLuint fragmentShaderP = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShaderP, 1, &fragmentShaderSourceP, nullptr);
    glCompileShader(fragmentShaderP);

    // Link shaders
    ParticleProgram = glCreateProgram();
    glAttachShader(ParticleProgram, vertexShaderP);
    glAttachShader(ParticleProgram, fragmentShaderP);
    glLinkProgram(ParticleProgram);

    // Check for shader linking errors
    GLint success;
    glGetProgramiv(ParticleProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(ParticleProgram, 512, nullptr, infoLog);
        std::cerr << "ERROR::SHADER::LINKING_FAILED\n" << infoLog << std::endl;
    }
    glFrontFace(GL_CCW); // Counter-clockwise winding order

    //the compute shader itself
    const char* computeShaderSrc = R"(
    #version 430

    layout(local_size_x = 128, local_size_y = 1, local_size_z = 1) in; // <<-- must match your dispatch grouping

    struct GPU_Particle {
        vec4 position; // xyz = position, w = life
        vec4 velocity; // xyz = velocity, w = type (0=Fire,1=Water,2=Gas)
        float size; // Size of individual particle
    };

    layout(std430, binding = 0) buffer Particles {
        GPU_Particle particles[];
    };

    uniform float deltaTime;
    uniform uint numParticles; // pass total count to protect out-of-range
    uniform int effectType;  // this is to change what type of particle to use

    // simple hash / random (works with floats)
    float randf(float x) {
        return fract(sin(x) * 43758.5453123);
    }
    float randi(uint i) {
        return randf(float(i));
    }

    float rand(vec2 co){
        return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
    }

    const float maxLife = 5.0;

    void main() {
        uint i = gl_GlobalInvocationID.x;
        if (i >= numParticles) return; // safety

        GPU_Particle p = particles[i];

        if (p.position.w <= 0.0) {
            // dead particle   could respawn here if you want
            particles[i] = p;
            return;
        }

        // Use the particle's life or index as a seed so each particle moves differently.
        float seed = float(i) * 12.9898 + p.position.w;

        // Compute an offset for each type
        vec3 offset = vec3(0.0);
        // 0 = Fire, 1 = Water, 2 = Gas, 3 = Spiral, ...
        if (effectType == 0) { // Fire
            if (i == 0) {
                // ---- Big core particle ----
                p.position.xyz = vec3(0.0, 0.0, 0.0); // stays at origin
                p.size = 50.0;                        // glowing flame core
                particles[i] = p;
                return;
            }

            // ---- Flicker flame particles ----
            float seed = float(i) * 12.9898 + p.position.y;

            // rise upwards
            float rise = (0.5 + randf(seed) * 0.5) * deltaTime;
            p.position.y += rise;

            // swirl/flicker sideways
            float swirlAngle = seed + p.position.y * 2.0;
            float swirlRadius = 0.05 + 0.05 * randf(seed * 3.3);
            p.position.x += cos(swirlAngle) * swirlRadius * deltaTime;
            p.position.z += sin(swirlAngle) * swirlRadius * deltaTime;

            // reset when too high
            if (p.position.y > 1.5) {
                p.position = vec4(
                    (randf(seed*7.7) - 0.5) * 0.1, // horizontal jitter
                    0.0,                           // restart at base
                    (randf(seed*9.1) - 0.5) * 0.1,
                    1.0
                );
            }

            // small flicker sizes
            p.size = 4.0 + randf(seed * 5.5) * 3.0;

            particles[i] = p;
            return;
        }
         else if (effectType == 1) {     // Water
            offset = vec3(
                randi(i*3u) - 0.5,
                -1.0 * deltaTime,
                randi(i*7u) - 0.5
            ) * 0.5;

        } else if (effectType == 2) {     // Gas
            offset = vec3(
                randi(i*11u) - 0.5,
                0.1 * deltaTime,
                randi(i*17u) - 0.5
            ) * 0.25;

        } else if (effectType == 3) {     // Spiral
            float angle = seed + deltaTime * 5.0; // spin speed
            float radius = 1.0 + 0.5 * sin(seed + deltaTime);
            offset = vec3(
                cos(angle) * radius,
                0.5 * deltaTime,
                sin(angle) * radius
            ) * 0.1;
        }

        p.position.xyz += offset;

        // decrease life
        p.position.w -= deltaTime;

        if (p.position.w <= 0.0) {
            // Respawn particle
            p.position.xyz = vec3(randf(float(i)*3.1) - 0.5,
                                  randf(float(i)*7.7) * 0.5,
                                  randf(float(i)*5.3) - 0.5);
            p.position.w = maxLife; // reset life

            // Reset type randomly (Fire=0, Water=1, Gas=2)
            p.velocity.w = float(uint(randf(float(i)*9.99) * 3.0));

            particles[i] = p;
            return;
        }
        else if (effectType == 4) { // Not really fire or anything, but it is funny, you can change the code with whatever you want inside
            if (i == 0) {
                // ---- Big core particle ----
                p.position.xyz = vec3(0.0, 0.0, 0.0); // stays at origin
                p.size = 25.0;                        // glowing flame core
                particles[i] = p;
                return;
            }

            // ---- Flicker flame particles ----
            float seed = float(i) * 12.9898 + p.position.y;

            // rise upwards
            float rise = (0.5 + randf(seed) * 0.5) * deltaTime;
            p.position.y += rise;

            // swirl/flicker sideways
            float swirlAngle = seed + p.position.y * 2.0;
            float swirlRadius = 0.05 + 0.05 * randf(seed * 3.3);
            p.position.x += cos(swirlAngle) * swirlRadius * deltaTime;
            p.position.z += sin(swirlAngle) * swirlRadius * deltaTime;

            // reset when too high
            if (p.position.y > 1.5) {
                p.position = vec4(
                    (randf(seed*7.7) - 0.5) * 0.1, // horizontal jitter
                    0.0,                           // restart at base
                    (randf(seed*9.1) - 0.5) * 0.1,
                    1.0
                );
            }

            // small flicker sizes
            p.size = 4.0 + randf(seed * 5.5) * 3.0;

            particles[i] = p;
            return;
        }

        // write back
        particles[i] = p;
    }

    )";

    computeShader = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(computeShader, 1, &computeShaderSrc, nullptr);
    glCompileShader(computeShader);

    // Create program
    ComputeProgram = glCreateProgram();
    glAttachShader(ComputeProgram, computeShader);
    glLinkProgram(ComputeProgram);

    CheckShaderCompile(vertexShaderP, "Vertex Shader");
    CheckShaderCompile(fragmentShaderP, "Fragment Shader");
    CheckShaderCompile(computeShader, "Compute Shader");
}

size_t numParticles = 1000;
std::vector<GPU_Particle> particles(numParticles);

void InitParticlesBuffers()
{
    for (size_t i = 0; i < numParticles; ++i) {

        glm::vec3 emitterPos = glm::vec3(0.0f, 0.0f, 0.0f); // or pass it in

        particles[i].position = glm::vec4(emitterPos + glm::vec3(0.0f), 5.0f); // all start at origin
        particles[i].velocity = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
        particles[i].size = 50.0f; // default

        // Choose a type: 0=Fire,1=Water,2=Gas,3=Spiral
        int type = i % 4; // cycles through 4 effects
        particles[i].velocity = glm::vec4(0.0f, 0.0f, 0.0f, float(type));
    }

    particles[0].position = glm::vec4(0.0f, 0.0f, 0.0f, 5.0f);
    particles[0].size = 25.0f; // big glowing center

    glGenBuffers(1, &ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 numParticles * sizeof(GPU_Particle),
                 particles.data(),
                 GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}


void DrawAdvancedParticles(const glm::vec3& position, const glm::vec3& scale, const Camera& camera, const glm::mat4& projection, const glm::vec3& color, int currentEffect, float deltaTime)
{
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, position);
    model = glm::rotate(model, 0.0f, glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::scale(model, scale);

    // 1. Activate shader program and set uniforms
    glUseProgram(ComputeProgram);
    glUniform1f(glGetUniformLocation(ComputeProgram, "deltaTime"), deltaTime);
    glUniform3fv(glGetUniformLocation(ComputeProgram, "origin"), 1, glm::value_ptr(position));
    glUniform1ui(glGetUniformLocation(ComputeProgram, "numParticles"), (GLuint)numParticles);
    glUniform1i(glGetUniformLocation(ComputeProgram, "effectType"), currentEffect);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo);

    // dispatch
    GLuint groups = (numParticles + 127) / 128;
    glDispatchCompute(groups, 1, 1);

    // Wait for SSBO writes to be visible to vertex attribute fetches:
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);


    // 2. Draw particles
    glUseProgram(ParticleProgram);
    glUniformMatrix4fv(glGetUniformLocation(ParticleProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(glGetUniformLocation(ParticleProgram, "view"), 1, GL_FALSE, glm::value_ptr(camera.view));
    glUniformMatrix4fv(glGetUniformLocation(ParticleProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
    glUniform1f(glGetUniformLocation(ParticleProgram, "pointSize"), 10.0f);
    glUniform4f(glGetUniformLocation(ParticleProgram, "color"), color.r, color.g, color.b, 0.5f);

    // bind the same buffer as array buffer for vertex fetch:
    glBindBuffer(GL_ARRAY_BUFFER, ssbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(GPU_Particle), (void*)0);

    // draw
    glDrawArrays(GL_POINTS, 0, (GLsizei)numParticles);

    // cleanup if you want:
    glDisableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

}

#endif // HAMMER_ADVANCED_PARTICLES_H_INCLUDED
