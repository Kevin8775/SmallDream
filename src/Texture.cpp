#include "Texture.h"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <iostream>

Texture::Texture(const std::string& path) : mID(0) {
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(path.c_str(), &mWidth, &mHeight, &mChannels, 0);
    if (!data) {
        std::cerr << "Failed to load texture: " << path << std::endl;
        mWidth = mHeight = 0;
        return;
    }
    GLenum format, internalFormat;
    switch (mChannels) {
        case 1: format = GL_RED;   internalFormat = GL_R8;   break;
        case 2: format = GL_RG;    internalFormat = GL_RG8;  break;
        case 3: format = GL_RGB;   internalFormat = GL_RGB8; break;
        default: format = GL_RGBA; internalFormat = GL_RGBA8; break;
    }
    glGenTextures(1, &mID);
    glBindTexture(GL_TEXTURE_2D, mID);
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, mWidth, mHeight, 0, format, GL_UNSIGNED_BYTE, data);
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        std::cerr << "OpenGL error " << err << " loading texture: " << path << std::endl;
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    stbi_image_free(data);
}

Texture::Texture(const unsigned char* buffer, int bufferLen) : mID(0) {
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load_from_memory(buffer, bufferLen, &mWidth, &mHeight, &mChannels, 0);
    if (!data) {
        std::cerr << "Failed to load embedded texture from memory" << std::endl;
        mWidth = mHeight = 0;
        return;
    }
    GLenum format, internalFormat;
    switch (mChannels) {
        case 1: format = GL_RED;   internalFormat = GL_R8;   break;
        case 2: format = GL_RG;    internalFormat = GL_RG8;  break;
        case 3: format = GL_RGB;   internalFormat = GL_RGB8; break;
        default: format = GL_RGBA; internalFormat = GL_RGBA8; break;
    }
    glGenTextures(1, &mID);
    glBindTexture(GL_TEXTURE_2D, mID);
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, mWidth, mHeight, 0, format, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    stbi_image_free(data);
}

Texture::Texture(int width, int height, const unsigned char* data, GLenum internalFormat, GLenum format) : mID(0) {
    mWidth = width;
    mHeight = height;
    glGenTextures(1, &mID);
    glBindTexture(GL_TEXTURE_2D, mID);
    GLint sizedFormat;
    switch (format) {
        case GL_RED:  sizedFormat = GL_R8;   break;
        case GL_RG:   sizedFormat = GL_RG8;  break;
        case GL_RGB:  sizedFormat = GL_RGB8; break;
        default:      sizedFormat = GL_RGBA8; break;
    }
    glTexImage2D(GL_TEXTURE_2D, 0, sizedFormat, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        std::cerr << "OpenGL error " << err << " creating procedural texture" << std::endl;
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

Texture::~Texture() {
    if (mID) glDeleteTextures(1, &mID);
}

void Texture::bind(int slot) const {
    if (!mID) return;
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, mID);
}
