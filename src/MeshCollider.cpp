#include "MeshCollider.h"
#include "Model.h"
#include "Shader.h"
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>
#include <limits>

MeshCollider::~MeshCollider() {
    destroy();
}

void MeshCollider::destroy() {
    if (mDebugVAO) glDeleteVertexArrays(1, &mDebugVAO);
    if (mDebugVBO) glDeleteBuffers(1, &mDebugVBO);
    mDebugVAO = 0;
    mDebugVBO = 0;
    mDebugVertexCount = 0;
    mTriangles.clear();
    mGrid.clear();
    mBuilt = false;
}

void MeshCollider::addModel(const Model& model, const glm::mat4& transform) {
    const auto& meshes = model.meshes();
    for (const auto& mesh : meshes) {
        const auto& verts = mesh.vertices;
        const auto& idx = mesh.indices;
        if (verts.empty() || idx.empty()) continue;

        for (size_t i = 0; i + 2 < idx.size(); i += 3) {
            glm::vec3 v0 = verts[idx[i]].position;
            glm::vec3 v1 = verts[idx[i + 1]].position;
            glm::vec3 v2 = verts[idx[i + 2]].position;

            glm::vec4 t0 = transform * glm::vec4(v0, 1.0f);
            glm::vec4 t1 = transform * glm::vec4(v1, 1.0f);
            glm::vec4 t2 = transform * glm::vec4(v2, 1.0f);

            CollisionTriangle tri;
            tri.v0 = glm::vec3(t0);
            tri.v1 = glm::vec3(t1);
            tri.v2 = glm::vec3(t2);
            mTriangles.push_back(tri);
        }
    }
}

void MeshCollider::addFloorQuad(float y, float xMin, float xMax, float zMin, float zMax) {
    CollisionTriangle t0, t1;
    t0.v0 = glm::vec3(xMin, y, zMin);
    t0.v1 = glm::vec3(xMax, y, zMin);
    t0.v2 = glm::vec3(xMax, y, zMax);
    t1.v0 = glm::vec3(xMin, y, zMin);
    t1.v1 = glm::vec3(xMax, y, zMax);
    t1.v2 = glm::vec3(xMin, y, zMax);
    mTriangles.push_back(t0);
    mTriangles.push_back(t1);
}

void MeshCollider::addFloorCap(float y, float xMin, float xMax, float zMin, float zMax) {
    glm::vec3 center((xMin + xMax) * 0.5f, y, (zMin + zMax) * 0.5f);
    float rx = (xMax - xMin) * 0.5f;
    float rz = (zMax - zMin) * 0.5f;
    MeshCollisionInfo info;
    info.center = center;
    info.radius = std::sqrt(rx * rx + rz * rz);
    mMeshInfos.push_back(info);
    int meshIdx = (int)mMeshInfos.size() - 1;

    // Winding so cross(e1,e2).y > 0 — upward-facing, detected by getFloorHeight.
    CollisionTriangle t0, t1;
    t0.v0 = glm::vec3(xMin, y, zMin);
    t0.v1 = glm::vec3(xMax, y, zMax);
    t0.v2 = glm::vec3(xMax, y, zMin);
    t1.v0 = glm::vec3(xMin, y, zMin);
    t1.v1 = glm::vec3(xMin, y, zMax);
    t1.v2 = glm::vec3(xMax, y, zMax);
    mTriangles.push_back(t0); mTriToMesh.push_back(meshIdx);
    mTriangles.push_back(t1); mTriToMesh.push_back(meshIdx);
}

void MeshCollider::build() {
    if (mTriangles.empty()) return;

    // Rebuild clusters from ALL meshInfos (including any added via addFloorQuad
    // after addModel). addModel builds clusters only for its own meshes, so
    // floor quads added later are excluded from the broad-phase without this step.
    mClusters.clear();
    for (int i = 0; i < (int)mMeshInfos.size(); ++i) {
        bool added = false;
        for (auto& cluster : mClusters) {
            float dist = glm::distance(mMeshInfos[i].center, cluster.center);
            if (dist < CLUSTER_DISTANCE) {
                cluster.meshIndices.push_back(i);
                added = true;
                break;
            }
        }
        if (!added) {
            MeshCluster c;
            c.center = mMeshInfos[i].center;
            c.radius = mMeshInfos[i].radius;
            c.meshIndices.push_back(i);
            mClusters.push_back(c);
        }
    }
    for (auto& cluster : mClusters) {
        glm::vec3 sum(0.0f);
        for (int idx : cluster.meshIndices)
            sum += mMeshInfos[idx].center;
        cluster.center = sum / (float)cluster.meshIndices.size();
        float maxDist = 0.0f;
        for (int idx : cluster.meshIndices) {
            float d = glm::length(cluster.center - mMeshInfos[idx].center) + mMeshInfos[idx].radius;
            if (d > maxDist) maxDist = d;
        }
        cluster.radius = maxDist;
    }

    if (mDebugVAO) glDeleteVertexArrays(1, &mDebugVAO);
    if (mDebugVBO) glDeleteBuffers(1, &mDebugVBO);
    mDebugVAO = 0;
    mDebugVBO = 0;
    mDebugVertexCount = 0;
    mGrid.clear();

    mGridMin = glm::vec3(1e9f);
    mGridMax = glm::vec3(-1e9f);
    for (const auto& tri : mTriangles) {
        mGridMin = glm::min(mGridMin, tri.v0);
        mGridMin = glm::min(mGridMin, tri.v1);
        mGridMin = glm::min(mGridMin, tri.v2);
        mGridMax = glm::max(mGridMax, tri.v0);
        mGridMax = glm::max(mGridMax, tri.v1);
        mGridMax = glm::max(mGridMax, tri.v2);
    }

    glm::vec3 expand(0.01f);
    mGridMin -= expand;
    mGridMax += expand;

    for (size_t triIdx = 0; triIdx < mTriangles.size(); ++triIdx) {
        const auto& tri = mTriangles[triIdx];
        glm::vec3 tmin = glm::min(glm::min(tri.v0, tri.v1), tri.v2);
        glm::vec3 tmax = glm::max(glm::max(tri.v0, tri.v1), tri.v2);

        int gx0, gy0, gz0, gx1, gy1, gz1;
        getGridCoords(tmin, gx0, gy0, gz0);
        getGridCoords(tmax, gx1, gy1, gz1);

        for (int gx = gx0; gx <= gx1; ++gx) {
            for (int gy = gy0; gy <= gy1; ++gy) {
                for (int gz = gz0; gz <= gz1; ++gz) {
                    uint64_t key = gridKey(gx, gy, gz);
                    mGrid[key].triIndices.push_back(triIdx);
                }
            }
        }
    }

    std::vector<glm::vec3> debugVerts;
    debugVerts.reserve(mTriangles.size() * 3);
    for (const auto& tri : mTriangles) {
        debugVerts.push_back(tri.v0);
        debugVerts.push_back(tri.v1);
        debugVerts.push_back(tri.v2);
    }

    mDebugVertexCount = debugVerts.size();
    glGenVertexArrays(1, &mDebugVAO);
    glGenBuffers(1, &mDebugVBO);
    glBindVertexArray(mDebugVAO);
    glBindBuffer(GL_ARRAY_BUFFER, mDebugVBO);
    glBufferData(GL_ARRAY_BUFFER, debugVerts.size() * sizeof(glm::vec3), debugVerts.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glBindVertexArray(0);

    mBuilt = true;
}

uint64_t MeshCollider::gridKey(int gx, int gy, int gz) {
    uint64_t k = 0;
    k |= (uint64_t)(uint32_t)gx;
    k |= (uint64_t)(uint32_t)gy << 20;
    k |= (uint64_t)(uint32_t)gz << 40;
    return k;
}

void MeshCollider::getGridCoords(const glm::vec3& p, int& gx, int& gy, int& gz) const {
    gx = (int)std::floor((p.x - mGridMin.x) / CELL_SIZE);
    gy = (int)std::floor((p.y - mGridMin.y) / CELL_SIZE);
    gz = (int)std::floor((p.z - mGridMin.z) / CELL_SIZE);
}

glm::vec3 MeshCollider::triangleNormal(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c) {
    return glm::normalize(glm::cross(b - a, c - a));
}

glm::vec3 MeshCollider::closestPointOnTriangle(const glm::vec3& p,
    const glm::vec3& a, const glm::vec3& b, const glm::vec3& c) {
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

    float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
        float v = d1 / (d1 - d3);
        return a + v * ab;
    }

    glm::vec3 cp = p - c;
    float d5 = glm::dot(ab, cp);
    float d6 = glm::dot(ac, cp);
    if (d6 >= 0.0f && d5 <= d6) return c;

    float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
        float w = d2 / (d2 - d6);
        return a + w * ac;
    }

    float va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
        float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        return b + w * (c - b);
    }

    float denom = 1.0f / (va + vb + vc);
    float v = vb * denom;
    float w = vc * denom;
    return a + v * ab + w * ac;
}

bool MeshCollider::collideSphere(const glm::vec3& center, float radius,
                                 glm::vec3& outNormal, float& outPenetration) const {
    if (!mBuilt) return false;

    bool hit = false;
    glm::vec3 bestNormal(0.0f);
    float bestPen = 0.0f;

    glm::vec3 sphereMin = center - glm::vec3(radius);
    glm::vec3 sphereMax = center + glm::vec3(radius);

    int gx0, gy0, gz0, gx1, gy1, gz1;
    getGridCoords(sphereMin, gx0, gy0, gz0);
    getGridCoords(sphereMax, gx1, gy1, gz1);

    for (int gx = gx0; gx <= gx1; ++gx) {
        for (int gy = gy0; gy <= gy1; ++gy) {
            for (int gz = gz0; gz <= gz1; ++gz) {
                uint64_t key = gridKey(gx, gy, gz);
                auto it = mGrid.find(key);
                if (it == mGrid.end()) continue;

                const auto& cell = it->second;
                for (size_t triIdx : cell.triIndices) {
                    const auto& tri = mTriangles[triIdx];

                    glm::vec3 te1 = tri.v1 - tri.v0;
                    glm::vec3 te2 = tri.v2 - tri.v0;
                    glm::vec3 tn = glm::cross(te1, te2);
                    float tnLen = glm::length(tn);
                    if (tnLen > 1e-8f) {
                        float ny = tn.y / tnLen;
                        // Skip downward-facing triangles: they are the undersides of objects
                        // (e.g. sphere/ball bottoms) and trap the player from below.
                        if (ny < -0.15f) continue;
                        if (ny > 0.3f) {
                            glm::vec3 planeN = tn / tnLen;
                            if (glm::dot(planeN, center - tri.v0) < 0.0f) continue;
                        }
                    }

                    glm::vec3 closest = closestPointOnTriangle(center, tri.v0, tri.v1, tri.v2);
                    glm::vec3 diff = center - closest;
                    float distSq = glm::dot(diff, diff);

                    if (distSq < radius * radius) {
                        float dist = std::sqrt(distSq);
                        glm::vec3 n;
                        float pen;
                        if (dist < 1e-6f) {
                            n = triangleNormal(tri.v0, tri.v1, tri.v2);
                            pen = radius;
                        } else {
                            n = diff / dist;
                            pen = radius - dist;
                        }

                        if (pen > bestPen) {
                            bestPen = pen;
                            bestNormal = n;
                            hit = true;
                        }
                    }
                }
            }
        }
    }

    if (hit) {
        outNormal = bestNormal;
        outPenetration = bestPen;
    }
    return hit;
}

float MeshCollider::getFloorHeight(const glm::vec3& position, float maxDist) const {
    if (!mBuilt) return position.y - maxDist;

    float bestT = 1e9f;
    bool found = false;

    glm::vec3 rayEnd = position - glm::vec3(0.0f, maxDist, 0.0f);
    glm::vec3 rmin = glm::min(position, rayEnd);
    glm::vec3 rmax = glm::max(position, rayEnd);

    int gx0, gy0, gz0, gx1, gy1, gz1;
    getGridCoords(rmin, gx0, gy0, gz0);
    getGridCoords(rmax, gx1, gy1, gz1);

    for (int gx = gx0; gx <= gx1; ++gx) {
        for (int gy = gy0; gy <= gy1; ++gy) {
            for (int gz = gz0; gz <= gz1; ++gz) {
                uint64_t key = gridKey(gx, gy, gz);
                auto it = mGrid.find(key);
                if (it == mGrid.end()) continue;

                const auto& cell = it->second;
                for (size_t triIdx : cell.triIndices) {
                    const auto& tri = mTriangles[triIdx];

                    glm::vec3 e1 = tri.v1 - tri.v0;
                    glm::vec3 e2 = tri.v2 - tri.v0;
                    if (glm::cross(e1, e2).y < -0.01f) continue;

                    glm::vec3 pvec = glm::cross(glm::vec3(0.0f, -1.0f, 0.0f), e2);
                    float det = glm::dot(e1, pvec);
                    if (std::abs(det) < 1e-8f) continue;

                    float invDet = 1.0f / det;
                    glm::vec3 tvec = position - tri.v0;
                    float u = glm::dot(tvec, pvec) * invDet;
                    if (u < 0.0f || u > 1.0f) continue;

                    glm::vec3 qvec = glm::cross(tvec, e1);
                    float v = glm::dot(glm::vec3(0.0f, -1.0f, 0.0f), qvec) * invDet;
                    if (v < 0.0f || u + v > 1.0f) continue;

                    float t = glm::dot(e2, qvec) * invDet;
                    if (t > 0.0f && t < maxDist && t < bestT) {
                        bestT = t;
                        found = true;
                    }
                }
            }
        }
    }

    return found ? position.y - bestT : position.y - maxDist;
}

void MeshCollider::drawDebug(Shader& shader, const glm::mat4& view, const glm::mat4& proj) const {
    if (!mBuilt || mDebugVertexCount == 0) return;

    shader.use();
    glm::mat4 model(1.0f);
    shader.setMat4("uModel", glm::value_ptr(model));
    shader.setMat4("uView", glm::value_ptr(view));
    shader.setMat4("uProjection", glm::value_ptr(proj));
    shader.setVec3("uBaseColor", 0.0f, 1.0f, 0.5f);
    shader.setInt("uHasTexture", 0);

    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glLineWidth(1.0f);

    glBindVertexArray(mDebugVAO);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)mDebugVertexCount);
    glBindVertexArray(0);

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}
