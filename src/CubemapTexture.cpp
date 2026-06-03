#include "CubemapTexture.h"
#include <stb_image.h>
#include <iostream>

CubemapTexture::CubemapTexture(const std::array<std::string, 6>& faces) {
    glGenTextures(1, &mID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, mID);

    static const GLenum targets[6] = {
        GL_TEXTURE_CUBE_MAP_POSITIVE_X,
        GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
        GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
        GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
        GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
        GL_TEXTURE_CUBE_MAP_NEGATIVE_Z
    };

    stbi_set_flip_vertically_on_load(false);

    for (int i = 0; i < 6; i++) {
        int w, h, channels;
        unsigned char* data = stbi_load(faces[i].c_str(), &w, &h, &channels, 0);
        if (!data) {
            std::cerr << "Failed to load cubemap face: " << faces[i] << std::endl;
            continue;
        }
        GLenum format = (channels == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(targets[i], 0, (channels == 4) ? GL_RGBA8 : GL_RGB8,
                     w, h, 0, format, GL_UNSIGNED_BYTE, data);
        stbi_image_free(data);
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

CubemapTexture::~CubemapTexture() {
    if (mID) glDeleteTextures(1, &mID);
}

void CubemapTexture::bind(int slot) const {
    if (!mID) return;
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_CUBE_MAP, mID);
}
