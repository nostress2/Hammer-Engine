#ifndef HAMMER_PHYSICS_H_INCLUDED
#define HAMMER_PHYSICS_H_INCLUDED

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

// GLM library
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
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

struct LevelTriangle {
    glm::vec3 p0, p1, p2;
    glm::vec3 Normal;
};

static std::string getModelPath(const std::string& assetRelativePath) {
    return std::filesystem::current_path().string() + "/Assets/" + assetRelativePath;
}

inline std::vector<LevelTriangle> loadCollisionModel(const std::string& filename) {
    std::vector<LevelTriangle> triangles;

    std::string Path = getModelPath(filename);

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(Path,
        aiProcess_Triangulate |
        aiProcess_JoinIdenticalVertices |
        aiProcess_OptimizeMeshes |
        aiProcess_RemoveRedundantMaterials |
        aiProcess_ImproveCacheLocality);

    if (!scene || !scene->HasMeshes()) {
        std::cerr << "Failed to load collision model: " << Path << std::endl;
        return triangles;
    }

    for (unsigned int m = 0; m < scene->mNumMeshes; ++m) {
        aiMesh* mesh = scene->mMeshes[m];

        for (unsigned int f = 0; f < mesh->mNumFaces; ++f) {
            const aiFace& face = mesh->mFaces[f];
            if (face.mNumIndices != 3) continue;

            aiVector3D a = mesh->mVertices[face.mIndices[0]];
            aiVector3D b = mesh->mVertices[face.mIndices[1]];
            aiVector3D c = mesh->mVertices[face.mIndices[2]];

            LevelTriangle tri;
            tri.p0 = glm::vec3(a.x, a.y, a.z);
            tri.p1 = glm::vec3(b.x, b.y, b.z);
            tri.p2 = glm::vec3(c.x, c.y, c.z);

            glm::vec3 edge1 = tri.p1 - tri.p0;
            glm::vec3 edge2 = tri.p2 - tri.p0;
            tri.Normal = glm::normalize(glm::cross(edge1, edge2));

            triangles.push_back(tri);
        }
    }

    return triangles;
}

struct AABB {
    glm::vec3 min;       // Minimum corner of the box
    glm::vec3 max;       // Maximum corner of the box
    glm::vec3 position;  // Center of the AABB
    glm::vec3 size;      // Size of the AABB (width, height, depth)

    // Constructor: Initialize with position and size
    AABB(const glm::vec3& position, const glm::vec3& size)
        : position(position), size(size) {
        BoxCollider(position, size);
    }

    // Default constructor
    AABB() : min(0.0f), max(0.0f), position(0.0f), size(0.0f) {}

    // Function to set a Box Collider
    inline void BoxCollider(const glm::vec3& position, const glm::vec3& size) {
        this->position = position;
        this->size = size;
        min = position - size * 0.5f;
        max = position + size * 0.5f;
    }


    // Detect collision with another AABB
    inline bool detectCollision(const AABB& other) const {
        return (position.x - size.x * 0.5f <= other.position.x + other.size.x * 0.5f &&
                position.x + size.x * 0.5f >= other.position.x - other.size.x * 0.5f &&
                position.y - size.y * 0.5f <= other.position.y + other.size.y * 0.5f &&
                position.y + size.y * 0.5f >= other.position.y - other.size.y * 0.5f &&
                position.z - size.z * 0.5f <= other.position.z + other.size.z * 0.5f &&
                position.z + size.z * 0.5f >= other.position.z - other.size.z * 0.5f);
    }

    // Check collision with a vector of AABBs
    inline bool detectAABBCollisions(const std::vector<AABB>& others) const {
        for (const auto& other : others) {
            if (detectCollision(other)) {
                return true; // Colliding with at least one object
            }
        }
        return false; // No collisions
    }

    inline void UpdatePosition(const glm::vec3& newPosition) {
        position = newPosition;
    }

    inline void PrintDebug() const {
        std::cout << "AABB pos: (" << position.x << ", " << position.y << ", " << position.z << ")\n";
    }
};

struct Sphere {
    glm::vec3 position;
    float radius;

    Sphere(const glm::vec3& pos, float r)
        : position(pos), radius(r) {
          SphereCollider(position, radius);
        }

    Sphere() : position(0.0f), radius(0.0f) {}

    inline void SphereCollider(const glm::vec3& pos, float r) {
        position = pos;
        radius = r;
    }

    inline bool detectCollision(const Sphere& other) const {
        float distSq = glm::distance2(position, other.position);
        float radiusSum = radius + other.radius;
        return distSq <= radiusSum * radiusSum;
    }

    inline bool detectCollisions(const std::vector<Sphere>& others) const {
        for (const auto& other : others) {
            if (detectCollision(other)) {
                return true; // Colliding with at least one object
            }
        }
        return false; // No collisions
    }

    inline void UpdatePosition(const glm::vec3& newPosition) {
        position = newPosition;
    }

    inline void PrintDebug() const {
        std::cout << "Sphere pos: (" << position.x << ", " << position.y << ", " << position.z << ")\n";
    }
};

struct Physics {
    glm::vec3 Position;  // Position we're updating
    glm::vec3 Velocity;  // Velocity of the position
    float deltaTime;     // Time step for updates
    float maxSpeed = 5.0f;  // Maximum movement speed

    // Constructor: Initialize with position, velocity, and deltaTime
    Physics(glm::vec3& position, glm::vec3& velocity, float deltaTime)
        : Position(position), Velocity(velocity), deltaTime(deltaTime)
        {
            UpdatePosition(position, velocity, deltaTime);
        }

    // Default constructor
    Physics() : Position(0.0f), Velocity(0.0f), deltaTime(0.0f) {}

    // Update position based on velocity
    inline void UpdatePosition(glm::vec3& Position, glm::vec3& Velocity, float deltaTime) {
        // Preserve vertical movement (y-axis) separately
        float verticalVelocity = Velocity.y;

        // Create a horizontal velocity vector (x, z only)
        glm::vec3 horizontalVelocity = glm::vec3(Velocity.x, 0.0f, Velocity.z);

        // Clamp horizontal speed only
        if (glm::length(horizontalVelocity) > maxSpeed) {
            horizontalVelocity = glm::normalize(horizontalVelocity) * maxSpeed;
        }

        // Restore y velocity
        Velocity = glm::vec3(horizontalVelocity.x, verticalVelocity, horizontalVelocity.z);

        // Update position
        Position += Velocity * deltaTime;
    }
};

inline std::vector<LevelTriangle> CheckTriAABBEdges(const AABB& aabb, const std::vector<LevelTriangle>& triangles)
{
    std::vector<LevelTriangle> touchedTriangles;

    for (const LevelTriangle& tri : triangles) {
        bool p0Inside = (tri.p0.x >= aabb.min.x && tri.p0.x <= aabb.max.x &&
                         tri.p0.y >= aabb.min.y && tri.p0.y <= aabb.max.y &&
                         tri.p0.z >= aabb.min.z && tri.p0.z <= aabb.max.z);

        bool p1Inside = (tri.p1.x >= aabb.min.x && tri.p1.x <= aabb.max.x &&
                         tri.p1.y >= aabb.min.y && tri.p1.y <= aabb.max.y &&
                         tri.p1.z >= aabb.min.z && tri.p1.z <= aabb.max.z);

        bool p2Inside = (tri.p2.x >= aabb.min.x && tri.p2.x <= aabb.max.x &&
                         tri.p2.y >= aabb.min.y && tri.p2.y <= aabb.max.y &&
                         tri.p2.z >= aabb.min.z && tri.p2.z <= aabb.max.z);

        if (p0Inside || p1Inside || p2Inside) {
            touchedTriangles.push_back(tri);
        }
    }

    return touchedTriangles;
}

inline glm::vec3 CalculateNormal(const LevelTriangle& tri) {
    glm::vec3 edge1 = tri.p1 - tri.p0;
    glm::vec3 edge2 = tri.p2 - tri.p0;
    return glm::normalize(glm::cross(edge1, edge2));
}

inline bool TriangleAABBOverlap(const LevelTriangle& tri, const AABB& box) {
    // Convert triangle to local AABB coordinates
    glm::vec3 v0 = tri.p0 - box.position;
    glm::vec3 v1 = tri.p1 - box.position;
    glm::vec3 v2 = tri.p2 - box.position;

    // Compute triangle edges
    glm::vec3 f0 = v1 - v0;
    glm::vec3 f1 = v2 - v1;
    glm::vec3 f2 = v0 - v2;

    // Compute box half size
    glm::vec3 boxHalfSize = box.size * 0.5f;

    // Define axes to test
    glm::vec3 axes[13] = {
        // Box axes
        glm::vec3(1,0,0), glm::vec3(0,1,0), glm::vec3(0,0,1),

        // Triangle normal
        glm::normalize(glm::cross(f0, f1)),

        // 9 cross-product axes (triangle edges x box axes)
        glm::cross(f0, glm::vec3(1,0,0)),
        glm::cross(f0, glm::vec3(0,1,0)),
        glm::cross(f0, glm::vec3(0,0,1)),
        glm::cross(f1, glm::vec3(1,0,0)),
        glm::cross(f1, glm::vec3(0,1,0)),
        glm::cross(f1, glm::vec3(0,0,1)),
        glm::cross(f2, glm::vec3(1,0,0)),
        glm::cross(f2, glm::vec3(0,1,0)),
        glm::cross(f2, glm::vec3(0,0,1))
    };

    // Test all 13 axes
    for (int i = 0; i < 13; ++i) {
        glm::vec3 axis = axes[i];
        if (glm::length(axis) < 1e-6f) continue; // Skip degenerate axis

        // Project triangle onto axis
        float minT = glm::dot(v0, axis);
        float maxT = minT;
        for (auto v : {v1, v2}) {
            float val = glm::dot(v, axis);
            minT = std::min(minT, val);
            maxT = std::max(maxT, val);
        }

        // Project AABB onto axis
        float r = boxHalfSize.x * std::abs(glm::dot(glm::vec3(1,0,0), axis)) +
                  boxHalfSize.y * std::abs(glm::dot(glm::vec3(0,1,0), axis)) +
                  boxHalfSize.z * std::abs(glm::dot(glm::vec3(0,0,1), axis));

        if (maxT < -r || minT > r)
            return false; // Separating axis found
    }

    return true; // No separating axis — overlap exists
}

inline std::vector<LevelTriangle> CheckTriAABBAccurate(const AABB& aabb, const std::vector<LevelTriangle>& triangles) {
    std::vector<LevelTriangle> result;
    for (const auto& tri : triangles) {
        if (TriangleAABBOverlap(tri, aabb)) {
            result.push_back(tri);
        }
    }
    return result;
}

struct TouchedTriangle {
    LevelTriangle tri;
    glm::vec3 normal;
    bool isSlope;
    bool isFloor;
    bool isCeiling;
    bool isWall;
};

inline std::vector<TouchedTriangle> GetTouchedTriangles_AABB(const AABB& aabb, const std::vector<LevelTriangle>& triangles, float SlopeAngleMin = 30.0f, float SlopeAngleMax = 50.0f) {
    std::vector<TouchedTriangle> result;
    glm::vec3 boxHalfSize = aabb.size * 0.5f;

    for (const auto& tri : triangles) {
        glm::vec3 v0 = tri.p0 - aabb.position;
        glm::vec3 v1 = tri.p1 - aabb.position;
        glm::vec3 v2 = tri.p2 - aabb.position;

        glm::vec3 f0 = v1 - v0;
        glm::vec3 f1 = v2 - v1;
        glm::vec3 f2 = v0 - v2;

        glm::vec3 axes[13] = {
            glm::vec3(1,0,0), glm::vec3(0,1,0), glm::vec3(0,0,1),
            glm::normalize(glm::cross(f0, f1)),
            glm::cross(f0, glm::vec3(1,0,0)), glm::cross(f0, glm::vec3(0,1,0)), glm::cross(f0, glm::vec3(0,0,1)),
            glm::cross(f1, glm::vec3(1,0,0)), glm::cross(f1, glm::vec3(0,1,0)), glm::cross(f1, glm::vec3(0,0,1)),
            glm::cross(f2, glm::vec3(1,0,0)), glm::cross(f2, glm::vec3(0,1,0)), glm::cross(f2, glm::vec3(0,0,1))
        };

        bool overlap = true;
        for (int i = 0; i < 13; ++i) {
            glm::vec3 axis = axes[i];
            if (glm::length(axis) < 1e-6f) continue;

            float minT = glm::dot(v0, axis), maxT = minT;
            for (auto v : {v1, v2}) {
                float val = glm::dot(v, axis);
                minT = std::min(minT, val);
                maxT = std::max(maxT, val);
            }

            float r = boxHalfSize.x * std::abs(glm::dot(glm::vec3(1,0,0), axis)) +
                      boxHalfSize.y * std::abs(glm::dot(glm::vec3(0,1,0), axis)) +
                      boxHalfSize.z * std::abs(glm::dot(glm::vec3(0,0,1), axis));

            if (maxT < -r || minT > r) {
                overlap = false;
                break;
            }
        }

        if (overlap) {
            glm::vec3 normal = tri.Normal;
            glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
            float angle = glm::degrees(glm::acos(glm::clamp(glm::dot(normal, up), -1.0f, 1.0f)));

            bool isSlope   = angle >= SlopeAngleMin && angle <= SlopeAngleMax;
            bool isFloor   = angle < SlopeAngleMin;
            bool isCeiling = angle > 135.0f;
            bool isWall    = !isSlope && !isFloor && !isCeiling;

            result.push_back({ tri, normal, isSlope, isFloor, isCeiling, isWall });
        }
    }

    return result;
}

/*
code for checking with the triangle to AABB function
auto touched = GetTouchedTriangles_AABB(aabb, triangles);
for (const auto& t : touched) {
    std::cout << "Touched triangle normal: (" << t.normal.x << ", " << t.normal.y << ", " << t.normal.z << ")\n";
    if (t.isSlope) {
        std::cout << "This triangle is a slope!\n";
    }
}
code for loading the level triangles
the model needs to be triangulated
auto triangles = loadCollisionModel("CollisionMap.obj");
*/

inline AABB SphereToAABB(const Sphere& sphere) {
    return AABB{
        sphere.position - glm::vec3(sphere.radius),
        sphere.position + glm::vec3(sphere.radius)
    };
}

inline bool detectSphereAABBCollision(const Sphere& sphere, const AABB& box) {
    // Clamp each coordinate of the sphere center into the box range
    glm::vec3 closestPoint = glm::clamp(sphere.position, box.min, box.max);

    // Vector from sphere center to that closest point
    glm::vec3 diff = sphere.position - closestPoint;

    // Collision if the distance <= radius
    return glm::dot(diff, diff) <= sphere.radius * sphere.radius;
}

inline bool detectSphereAABBCollisions(const std::vector<Sphere>& spheres, const std::vector<AABB>& aabbs) {
    // Check each sphere against all other spheres
    for (size_t i = 0; i < spheres.size(); ++i) {
        const Sphere& s = spheres[i];

        for (size_t j = i + 1; j < spheres.size(); ++j) {
            if (s.detectCollision(spheres[j])) {
                return true;
            }
        }

        // Check against AABBs
        for (const auto& box : aabbs) {
            if (detectSphereAABBCollision(s, box)) {
                return true;
            }
        }
    }

    return false;
}

// Compute the closest point on a triangle to a point
inline glm::vec3 ClosestPointOnTriangle(const glm::vec3& p, const LevelTriangle& tri) {
    const glm::vec3& a = tri.p0;
    const glm::vec3& b = tri.p1;
    const glm::vec3& c = tri.p2;

    glm::vec3 ab = b - a;
    glm::vec3 ac = c - a;
    glm::vec3 ap = p - a;

    float d1 = glm::dot(ab, ap);
    float d2 = glm::dot(ac, ap);
    if (d1 <= 0.0f && d2 <= 0.0f) return a;

    glm::vec3 bp = p - b;
    float d3 = glm::dot(ab, bp);
    float d4 = glm::dot(ac, bp);
    if (d3 >= 0.0f && d4 <= d3) return b;

    glm::vec3 cp = p - c;
    float d5 = glm::dot(ab, cp);
    float d6 = glm::dot(ac, cp);
    if (d6 >= 0.0f && d5 <= d6) return c;

    float vc = d1*d4 - d3*d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
        float v = d1 / (d1 - d3);
        return a + v * ab;
    }

    float vb = d5*d2 - d1*d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
        float w = d2 / (d2 - d6);
        return a + w * ac;
    }

    float va = d3*d6 - d5*d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
        float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        return b + w * (c - b);
    }

    float denom = 1.0f / (va + vb + vc);
    float v = vb * denom;
    float w = vc * denom;
    return a + ab * v + ac * w;
}

// Narrow-phase sphere-triangle collision
inline bool SphereTriangleCollision(const Sphere& sphere, const LevelTriangle& tri) {
    glm::vec3 closest = ClosestPointOnTriangle(sphere.position, tri);
    glm::vec3 diff = sphere.position - closest;
    return glm::dot(diff, diff) <= sphere.radius * sphere.radius;
}

// Broad-phase filter: keep only triangles whose bounding box overlaps the sphere
inline std::vector<LevelTriangle> BroadPhaseSphereTriangles(
    const Sphere& sphere, const std::vector<LevelTriangle>& triangles)
{
    std::vector<LevelTriangle> candidates;
    AABB sphereBox = SphereToAABB(sphere);

    for (const auto& tri : triangles) {
        glm::vec3 triMin = glm::min(glm::min(tri.p0, tri.p1), tri.p2);
        glm::vec3 triMax = glm::max(glm::max(tri.p0, tri.p1), tri.p2);

        bool overlap = (sphereBox.min.x <= triMax.x && sphereBox.max.x >= triMin.x) &&
                       (sphereBox.min.y <= triMax.y && sphereBox.max.y >= triMin.y) &&
                       (sphereBox.min.z <= triMax.z && sphereBox.max.z >= triMin.z);

        if (overlap)
            candidates.push_back(tri);
    }

    return candidates;
}

// Full function: get all touched triangles by sphere with normals and slope info
inline std::vector<TouchedTriangle> GetTouchedTriangles_Sphere(
    const Sphere& sphere, const std::vector<LevelTriangle>& triangles,
    float SlopeAngleMin = 30.0f, float SlopeAngleMax = 50.0f){
    std::vector<TouchedTriangle> result;

    // Broad-phase
    std::vector<LevelTriangle> candidates = BroadPhaseSphereTriangles(sphere, triangles);

    // Narrow-phase
    for (const auto& tri : candidates) {
        if (SphereTriangleCollision(sphere, tri)) {
            glm::vec3 normal = tri.Normal;
            glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
            float angle = glm::degrees(glm::acos(glm::clamp(glm::dot(normal, up), -1.0f, 1.0f)));

            bool isSlope   = angle >= SlopeAngleMin && angle <= SlopeAngleMax;
            bool isFloor   = angle < SlopeAngleMin;
            bool isCeiling = angle > 135.0f;
            bool isWall    = !isSlope && !isFloor && !isCeiling;

            result.push_back({ tri, normal, isSlope, isFloor, isCeiling, isWall });
        }
    }

    return result;
}

/*
code for checking with the triangle to Sphere function
auto touched = GetTouchedTriangles_Sphere(aabb, triangles);
for (const auto& t : touched) {
    std::cout << "Touched triangle normal: (" << t.normal.x << ", " << t.normal.y << ", " << t.normal.z << ")\n";
    if (t.isSlope) {
        std::cout << "This triangle is a slope!\n";
    }
}
code for loading the level triangles
the model needs to be triangulated
auto triangles = loadCollisionModel("CollisionMap.obj");
*/

#endif // HAMMER_PHYSICS_H_INCLUDED
