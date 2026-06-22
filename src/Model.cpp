#include "Model.h"
#include "Shader.h"
#include "Texture.h"
#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <limits>

struct FrustumPlane {
    glm::vec3 normal;
    float distance;
};

struct Frustum {
    FrustumPlane planes[6];
};

static Frustum extractFrustum(const glm::mat4& vp) {
    Frustum f;
    f.planes[0].normal = glm::vec3(vp[0][3] + vp[0][0], vp[1][3] + vp[1][0], vp[2][3] + vp[2][0]);
    f.planes[0].distance = vp[3][3] + vp[3][0];
    f.planes[1].normal = glm::vec3(vp[0][3] - vp[0][0], vp[1][3] - vp[1][0], vp[2][3] - vp[2][0]);
    f.planes[1].distance = vp[3][3] - vp[3][0];
    f.planes[2].normal = glm::vec3(vp[0][3] + vp[0][1], vp[1][3] + vp[1][1], vp[2][3] + vp[2][1]);
    f.planes[2].distance = vp[3][3] + vp[3][1];
    f.planes[3].normal = glm::vec3(vp[0][3] - vp[0][1], vp[1][3] - vp[1][1], vp[2][3] - vp[2][1]);
    f.planes[3].distance = vp[3][3] - vp[3][1];
    f.planes[4].normal = glm::vec3(vp[0][3] + vp[0][2], vp[1][3] + vp[1][2], vp[2][3] + vp[2][2]);
    f.planes[4].distance = vp[3][3] + vp[3][2];
    f.planes[5].normal = glm::vec3(vp[0][3] - vp[0][2], vp[1][3] - vp[1][2], vp[2][3] - vp[2][2]);
    f.planes[5].distance = vp[3][3] - vp[3][2];
    for (int i = 0; i < 6; ++i) {
        float len = glm::length(f.planes[i].normal);
        if (len > 1e-8f) {
            f.planes[i].normal /= len;
            f.planes[i].distance /= len;
        }
    }
    return f;
}

static bool sphereInFrustum(const Frustum& f, const glm::vec3& center, float radius) {
    for (int i = 0; i < 6; ++i) {
        float dist = glm::dot(f.planes[i].normal, center) + f.planes[i].distance;
        if (dist < -radius) return false;
    }
    return true;
}

static std::string joinPath(const std::string& base, const std::string& rel) {
    if (rel.empty()) return base;
    if (rel.size() > 1 && rel[1] == ':') return rel;
    if (!base.empty() && (base.back() == '/' || base.back() == '\\')) return base + rel;
    return base + "/" + rel;
}

void ModelMesh::draw() const {
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, (GLsizei)indexCount, GL_UNSIGNED_INT, nullptr);
}

void ModelMesh::destroy() {
    delete diffuseTexture;
    diffuseTexture = nullptr;
    if (ebo) glDeleteBuffers(1, &ebo);
    if (vbo) glDeleteBuffers(1, &vbo);
    if (vao) glDeleteVertexArrays(1, &vao);
    vao = vbo = ebo = 0;
    indexCount = 0;
    vertices.clear();
    indices.clear();
}

bool Model::load(const std::string& path) {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_GenNormals | aiProcess_JoinIdenticalVertices | aiProcess_ImproveCacheLocality | aiProcess_GenUVCoords | aiProcess_FlipUVs | aiProcess_PreTransformVertices | aiProcess_SortByPType);
    if (!scene || !scene->mRootNode) {
        mLastError = importer.GetErrorString();
        std::cerr << "Assimp load failed for " << path << ": " << mLastError << std::endl;
        return false;
    }

    destroy();
    mBoundsMin = glm::vec3(1e9f);
    mBoundsMax = glm::vec3(-1e9f);
    std::string modelDir = path;
    size_t slash = modelDir.find_last_of("/\\");
    if (slash != std::string::npos) modelDir = modelDir.substr(0, slash);
    else modelDir.clear();

    for (unsigned i = 0; i < scene->mNumMeshes; ++i) {
        aiMesh* mesh = scene->mMeshes[i];
        std::vector<ModelVertex> vertices;
        std::vector<unsigned int> indices;
        vertices.reserve(mesh->mNumVertices);
        for (unsigned v = 0; v < mesh->mNumVertices; ++v) {
            ModelVertex vert{};
            vert.position = glm::vec3(mesh->mVertices[v].x, mesh->mVertices[v].y, mesh->mVertices[v].z);
            vert.normal = mesh->HasNormals() ? glm::vec3(mesh->mNormals[v].x, mesh->mNormals[v].y, mesh->mNormals[v].z) : glm::vec3(0.0f, 1.0f, 0.0f);
            vert.texCoord = (mesh->HasTextureCoords(0)) ? glm::vec2(mesh->mTextureCoords[0][v].x, mesh->mTextureCoords[0][v].y) : glm::vec2(0.0f);
            vertices.push_back(vert);
            mBoundsMin = glm::min(mBoundsMin, vert.position);
            mBoundsMax = glm::max(mBoundsMax, vert.position);
        }
        for (unsigned f = 0; f < mesh->mNumFaces; ++f) {
            const aiFace& face = mesh->mFaces[f];
            for (unsigned j = 0; j < face.mNumIndices; ++j) indices.push_back(face.mIndices[j]);
        }

        ModelMesh m;
        m.vertices = vertices;
        m.indices = indices;
        m.indexCount = indices.size();
        {
            glm::vec3 center(0.0f);
            for (const auto& v : vertices) center += v.position;
            center /= (float)vertices.size();
            float maxDistSq = 0.0f;
            for (const auto& v : vertices) {
                float d = glm::dot(v.position - center, v.position - center);
                if (d > maxDistSq) maxDistSq = d;
            }
            m.boundingCenter = center;
            m.boundingRadius = std::sqrt(maxDistSq);
        }
        glGenVertexArrays(1, &m.vao);
        glGenBuffers(1, &m.vbo);
        glGenBuffers(1, &m.ebo);
        glBindVertexArray(m.vao);
        glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(vertices.size() * sizeof(ModelVertex)), vertices.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(indices.size() * sizeof(unsigned int)), indices.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ModelVertex), (void*)offsetof(ModelVertex, position));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(ModelVertex), (void*)offsetof(ModelVertex, normal));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(ModelVertex), (void*)offsetof(ModelVertex, texCoord));
        glBindVertexArray(0);

        if (scene->mMaterials && mesh->mMaterialIndex < scene->mNumMaterials) {
            aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
            aiString texPath;
            if (material->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS && texPath.length > 0) {
                m.diffuseTexture = new Texture(joinPath(modelDir, texPath.C_Str()));
            } else if (material->GetTexture(aiTextureType_BASE_COLOR, 0, &texPath) == AI_SUCCESS && texPath.length > 0) {
                m.diffuseTexture = new Texture(joinPath(modelDir, texPath.C_Str()));
            }
            aiColor3D diffuseColor(1.0f, 1.0f, 1.0f);
            if (material->Get(AI_MATKEY_COLOR_DIFFUSE, diffuseColor) == AI_SUCCESS) {
                m.baseColor = glm::vec3(diffuseColor.r, diffuseColor.g, diffuseColor.b);
            }
            aiString matName;
            if (material->Get(AI_MATKEY_NAME, matName) == AI_SUCCESS) {
                std::string name(matName.C_Str());
                if (name.find("material_") == 0) {
                    m.baseColor = glm::vec3(0.0f, 0.0f, 1.0f);
                }
            }
        }

        mMeshes.push_back(m);
    }

    mLoaded = true;
    mLastError.clear();
    return true;
}

void Model::draw(Shader& shader, const glm::mat4& view, const glm::mat4& projection, const glm::mat4& model) const {
    glm::mat4 vp = projection * view;
    Frustum frustum = extractFrustum(vp);
    glm::vec3 modelScale(
        glm::length(glm::vec3(model[0])),
        glm::length(glm::vec3(model[1])),
        glm::length(glm::vec3(model[2]))
    );
    float maxScale = glm::max(modelScale.x, glm::max(modelScale.y, modelScale.z));

    shader.use();
    shader.setVec3("uTintColor", mTintColor.r, mTintColor.g, mTintColor.b);
    for (const auto& mesh : mMeshes) {
        glm::vec4 worldCenter = model * glm::vec4(mesh.boundingCenter, 1.0f);
        float worldRadius = mesh.boundingRadius * maxScale;
        if (!sphereInFrustum(frustum, glm::vec3(worldCenter), worldRadius))
            continue;
        shader.setVec3("uBaseColor", mesh.baseColor.r, mesh.baseColor.g, mesh.baseColor.b);
        if (mesh.diffuseTexture && mesh.diffuseTexture->isValid()) {
            shader.setInt("uHasTexture", 1);
            mesh.diffuseTexture->bind(0);
        } else {
            shader.setInt("uHasTexture", 0);
        }
        mesh.draw();
    }
}

void Model::destroy() {
    for (auto& mesh : mMeshes) mesh.destroy();
    mMeshes.clear();
    mLoaded = false;
}
