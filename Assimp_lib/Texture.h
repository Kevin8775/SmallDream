#pragma once

#include <glad/glad.h>

#include <filesystem>
#include <string>

class Texture {
public:
    GLuint ID = 0;
    std::string type;
    GLuint unit = 0;

    // Loads image from disk using WIC (png/jpg) and uploads to OpenGL.
    Texture(const std::filesystem::path& imagePath, const char* texType, GLuint slot, bool srgb = false);

    void Bind() const;
    void Unbind() const;
    void Delete();
};
