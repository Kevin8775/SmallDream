#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>
#include <unordered_map>
#include <cstdint>

class Model;
class Shader;

struct CollisionTriangle {
    glm::vec3 v0, v1, v2;
};

struct MeshCollisionInfo {
    glm::vec3 center;
    float radius;
};

struct MeshCluster {
    glm::vec3 center;
    float radius;
    std::vector<int> meshIndices;
};

class MeshCollider {
public:
    ~MeshCollider();

    void addModel(const Model& model, const glm::mat4& transform = glm::mat4(1.0f));
    void addFloorQuad(float y, float xMin, float xMax, float zMin, float zMax);
    void build();

    bool collideSphere(const glm::vec3& center, float radius,
                       glm::vec3& outNormal, float& outPenetration) const;

    float getFloorHeight(const glm::vec3& position, float maxDist = 500.0f) const;

    void drawDebug(Shader& shader, const glm::mat4& view, const glm::mat4& proj) const;

    bool isBuilt() const { return mBuilt; }
    size_t triangleCount() const { return mTriangles.size(); }

    void destroy();

private:
    std::vector<CollisionTriangle> mTriangles;
    std::vector<MeshCollisionInfo> mMeshInfos;
    std::vector<int> mTriToMesh;
    std::vector<MeshCluster> mClusters;
    bool mBuilt = false;

    static constexpr float CLUSTER_DISTANCE = 4.0f;
    static constexpr float CELL_SIZE = 1.0f;

    struct Cell {
        std::vector<size_t> triIndices;
    };
    std::unordered_map<uint64_t, Cell> mGrid;
    glm::vec3 mGridMin, mGridMax;

    GLuint mDebugVAO = 0;
    GLuint mDebugVBO = 0;
    size_t mDebugVertexCount = 0;

    static glm::vec3 closestPointOnTriangle(const glm::vec3& p,
        const glm::vec3& a, const glm::vec3& b, const glm::vec3& c);

    static uint64_t gridKey(int gx, int gy, int gz);
    void getGridCoords(const glm::vec3& p, int& gx, int& gy, int& gz) const;
    static glm::vec3 triangleNormal(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c);
};
