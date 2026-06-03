#pragma once
#include <glad/glad.h>
#include <string>
#include <array>

class CubemapTexture {
public:
    CubemapTexture(const std::array<std::string, 6>& faces);
    ~CubemapTexture();
    void bind(int slot = 0) const;
private:
    GLuint mID;
};
