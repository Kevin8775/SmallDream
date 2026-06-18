#pragma once
#include <glm/glm.hpp>
#include <vector>

struct OctreeAABB {
    glm::vec3 min;
    glm::vec3 max;
};

struct OctreeNode {
    OctreeAABB bounds;
    std::vector<int> meshIndices;
    OctreeNode* children[8];
    bool isLeaf;

    OctreeNode();
    explicit OctreeNode(const OctreeAABB& b);
    ~OctreeNode();
};

class Octree {
public:
    Octree();
    ~Octree();

    void build(const std::vector<OctreeAABB>& worldBoxes, int maxDepth = 5, int maxObjects = 8);
    void query(const glm::vec3& position, float radius, std::vector<int>& outIndices) const;
    bool isValid() const { return mRoot != nullptr; }
    const OctreeAABB& rootBounds() const;
    OctreeNode* rootNode() const { return mRoot; }

private:
    OctreeNode* mRoot = nullptr;
    int mMaxDepth = 5;
    int mMaxObjects = 8;
    std::vector<OctreeAABB> mAllBoxes;

    void subdivide(OctreeNode* node, int depth);
    void queryNode(const OctreeNode* node, const glm::vec3& position, float radius, std::vector<int>& outIndices) const;
    static bool aabbSphereOverlap(const OctreeAABB& aabb, const glm::vec3& pos, float radius);
};
