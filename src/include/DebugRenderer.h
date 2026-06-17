#pragma once
#include <glm/glm.hpp>
#include <vector>
#include "Shader.h"

struct OctreeAABB;
struct OctreeNode;
class Octree;

class DebugRenderer {
public:
    DebugRenderer();
    ~DebugRenderer();

    bool init();
    void drawAABB(const glm::vec3& min, const glm::vec3& max, const glm::vec3& color,
                  const glm::mat4& view, const glm::mat4& projection);
    void drawOctree(const Octree& octree, const glm::mat4& view, const glm::mat4& projection,
                    const glm::vec3& camPos, float maxDist);

private:
    Shader mLineShader;
    unsigned int mVAO = 0, mVBO = 0, mEBO = 0;
    unsigned int mIndexCount = 0;

    void drawNodeRecursive(OctreeNode* node, const glm::mat4& view, const glm::mat4& projection,
                           const glm::vec3& camPos, float maxDist);
};
