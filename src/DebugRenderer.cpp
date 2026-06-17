#include "DebugRenderer.h"
#include "Octree.h"
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>

static const float cubeVerts[] = {
    -0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f,
     0.5f, -0.5f, -0.5f,  0.5f,  0.5f, -0.5f,
     0.5f,  0.5f, -0.5f, -0.5f,  0.5f, -0.5f,
    -0.5f,  0.5f, -0.5f, -0.5f, -0.5f, -0.5f,
    -0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f,
     0.5f, -0.5f,  0.5f,  0.5f,  0.5f,  0.5f,
     0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f,
    -0.5f,  0.5f,  0.5f, -0.5f, -0.5f,  0.5f,
    -0.5f, -0.5f, -0.5f, -0.5f, -0.5f,  0.5f,
     0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,
     0.5f,  0.5f, -0.5f,  0.5f,  0.5f,  0.5f,
    -0.5f,  0.5f, -0.5f, -0.5f,  0.5f,  0.5f,
};

static const unsigned int cubeIndices[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23
};

DebugRenderer::DebugRenderer() : mLineShader("assets/shaders/line.vert", "assets/shaders/line.frag") {}

DebugRenderer::~DebugRenderer() {
    if (mEBO) glDeleteBuffers(1, &mEBO);
    if (mVBO) glDeleteBuffers(1, &mVBO);
    if (mVAO) glDeleteVertexArrays(1, &mVAO);
}

bool DebugRenderer::init() {
    mIndexCount = 24;
    glGenVertexArrays(1, &mVAO);
    glGenBuffers(1, &mVBO);
    glGenBuffers(1, &mEBO);

    glBindVertexArray(mVAO);
    glBindBuffer(GL_ARRAY_BUFFER, mVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVerts), cubeVerts, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeIndices), cubeIndices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glBindVertexArray(0);

    return true;
}

void DebugRenderer::drawAABB(const glm::vec3& min, const glm::vec3& max, const glm::vec3& color,
                             const glm::mat4& view, const glm::mat4& projection) {
    glm::vec3 size = max - min;
    glm::vec3 center = (min + max) * 0.5f;
    glm::mat4 model = glm::translate(glm::mat4(1.0f), center);
    model = glm::scale(model, size);

    mLineShader.use();
    mLineShader.setMat4("uModel", glm::value_ptr(model));
    mLineShader.setMat4("uView", glm::value_ptr(view));
    mLineShader.setMat4("uProjection", glm::value_ptr(projection));
    mLineShader.setVec3("uColor", color.x, color.y, color.z);

    glBindVertexArray(mVAO);
    glDrawElements(GL_LINES, (GLsizei)mIndexCount, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

void DebugRenderer::drawOctree(const Octree& octree, const glm::mat4& view, const glm::mat4& projection,
                               const glm::vec3& camPos, float maxDist) {
    OctreeNode* root = octree.rootNode();
    if (!root) return;
    drawNodeRecursive(root, view, projection, camPos, maxDist);
}

void DebugRenderer::drawNodeRecursive(OctreeNode* node, const glm::mat4& view, const glm::mat4& projection,
                                      const glm::vec3& camPos, float maxDist) {
    glm::vec3 center = (node->bounds.min + node->bounds.max) * 0.5f;
    float dist = glm::distance(camPos, center);
    if (dist > maxDist) return;

    if (node->isLeaf) {
        drawAABB(node->bounds.min, node->bounds.max, glm::vec3(0.0f, 1.0f, 0.0f), view, projection);
    } else {
        drawAABB(node->bounds.min, node->bounds.max, glm::vec3(0.3f, 0.6f, 1.0f), view, projection);
        for (int i = 0; i < 8; i++) {
            drawNodeRecursive(node->children[i], view, projection, camPos, maxDist);
        }
    }
}
