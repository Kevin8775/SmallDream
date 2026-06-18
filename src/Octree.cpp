#include "Octree.h"
#include <algorithm>
#include <limits>

OctreeNode::OctreeNode()
    : isLeaf(true)
{
    for (int i = 0; i < 8; i++) children[i] = nullptr;
}

OctreeNode::OctreeNode(const OctreeAABB& b)
    : bounds(b), isLeaf(true)
{
    for (int i = 0; i < 8; i++) children[i] = nullptr;
}

OctreeNode::~OctreeNode() {
    for (int i = 0; i < 8; i++) {
        delete children[i];
        children[i] = nullptr;
    }
}

Octree::Octree() : mRoot(nullptr) {}

Octree::~Octree() {
    delete mRoot;
    mRoot = nullptr;
}

void Octree::build(const std::vector<OctreeAABB>& worldBoxes, int maxDepth, int maxObjects) {
    delete mRoot;
    mRoot = nullptr;
    mAllBoxes = worldBoxes;
    mMaxDepth = maxDepth;
    mMaxObjects = maxObjects;

    if (mAllBoxes.empty()) return;

    glm::vec3 rootMin(std::numeric_limits<float>::max());
    glm::vec3 rootMax(std::numeric_limits<float>::lowest());
    for (const auto& box : mAllBoxes) {
        rootMin = glm::min(rootMin, box.min);
        rootMax = glm::max(rootMax, box.max);
    }
    glm::vec3 size = rootMax - rootMin;
    if (size.x < 0.01f) size.x = 1.0f;
    if (size.y < 0.01f) size.y = 1.0f;
    if (size.z < 0.01f) size.z = 1.0f;

    mRoot = new OctreeNode({ rootMin, rootMin + size });
    for (size_t i = 0; i < mAllBoxes.size(); i++) {
        mRoot->meshIndices.push_back((int)i);
    }
    subdivide(mRoot, 0);
}

void Octree::subdivide(OctreeNode* node, int depth) {
    if (depth >= mMaxDepth) return;
    if ((int)node->meshIndices.size() <= mMaxObjects) return;

    node->isLeaf = false;
    glm::vec3 center = (node->bounds.min + node->bounds.max) * 0.5f;

    for (int i = 0; i < 8; i++) {
        OctreeAABB childBounds;
        childBounds.min.x = (i & 1) ? center.x : node->bounds.min.x;
        childBounds.max.x = (i & 1) ? node->bounds.max.x : center.x;
        childBounds.min.y = (i & 2) ? center.y : node->bounds.min.y;
        childBounds.max.y = (i & 2) ? node->bounds.max.y : center.y;
        childBounds.min.z = (i & 4) ? center.z : node->bounds.min.z;
        childBounds.max.z = (i & 4) ? node->bounds.max.z : center.z;
        node->children[i] = new OctreeNode(childBounds);
    }

    for (int idx : node->meshIndices) {
        const OctreeAABB& box = mAllBoxes[idx];
        for (int i = 0; i < 8; i++) {
            OctreeNode* child = node->children[i];
            if (box.min.x <= child->bounds.max.x && box.max.x >= child->bounds.min.x &&
                box.min.y <= child->bounds.max.y && box.max.y >= child->bounds.min.y &&
                box.min.z <= child->bounds.max.z && box.max.z >= child->bounds.min.z) {
                child->meshIndices.push_back(idx);
            }
        }
    }
    node->meshIndices.clear();

    for (int i = 0; i < 8; i++) {
        subdivide(node->children[i], depth + 1);
    }
}

void Octree::query(const glm::vec3& position, float radius, std::vector<int>& outIndices) const {
    if (!mRoot) return;
    queryNode(mRoot, position, radius, outIndices);
    std::sort(outIndices.begin(), outIndices.end());
    outIndices.erase(std::unique(outIndices.begin(), outIndices.end()), outIndices.end());
}

void Octree::queryNode(const OctreeNode* node, const glm::vec3& position, float radius, std::vector<int>& outIndices) const {
    if (!node) return;
    if (!aabbSphereOverlap(node->bounds, position, radius)) return;

    if (node->isLeaf) {
        for (int idx : node->meshIndices) {
            if (aabbSphereOverlap(mAllBoxes[idx], position, radius)) {
                outIndices.push_back(idx);
            }
        }
    } else {
        for (int i = 0; i < 8; i++) {
            queryNode(node->children[i], position, radius, outIndices);
        }
    }
}

bool Octree::aabbSphereOverlap(const OctreeAABB& aabb, const glm::vec3& pos, float radius) {
    float closestX = glm::clamp(pos.x, aabb.min.x, aabb.max.x);
    float closestY = glm::clamp(pos.y, aabb.min.y, aabb.max.y);
    float closestZ = glm::clamp(pos.z, aabb.min.z, aabb.max.z);
    float dx = pos.x - closestX;
    float dy = pos.y - closestY;
    float dz = pos.z - closestZ;
    return (dx * dx + dy * dy + dz * dz) <= (radius * radius);
}

const OctreeAABB& Octree::rootBounds() const {
    return mRoot->bounds;
}
