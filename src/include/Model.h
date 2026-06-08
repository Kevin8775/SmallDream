#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>

class Shader;
class Texture;

struct ModelVertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;
};

struct ModelMesh {
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    Texture* diffuseTexture = nullptr;
    std::vector<unsigned int> indices;
    size_t indexCount = 0;

    void draw() const;
    void destroy();
};

class Model {
public:
    bool load(const std::string& path);
    void draw(Shader& shader) const;
    void destroy();
    bool isLoaded() const { return mLoaded; }
    glm::vec3 boundsMin() const { return mBoundsMin; }
    glm::vec3 boundsMax() const { return mBoundsMax; }
    const std::string& lastError() const { return mLastError; }

private:
    std::vector<ModelMesh> mMeshes;
    glm::vec3 mBoundsMin = glm::vec3(0.0f);
    glm::vec3 mBoundsMax = glm::vec3(0.0f);
    bool mLoaded = false;
    std::string mLastError;
};
