#pragma once
#include <glad/glad.h>
#include <string>

class Texture {
public:
    Texture(const std::string& path);
    Texture(const unsigned char* buffer, int bufferLen);  // carga desde memoria (texturas embebidas)
    Texture(int width, int height, const unsigned char* data, GLenum internalFormat, GLenum format);
    ~Texture();
    void bind(int slot = 0) const;
    bool isValid() const { return mID != 0; }
    int getWidth() const { return mWidth; }
    int getHeight() const { return mHeight; }
private:
    GLuint mID;
    int mWidth, mHeight, mChannels;
};
