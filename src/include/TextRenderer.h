#ifndef TEXT_RENDERER_H
#define TEXT_RENDERER_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <map>
#include <string>
#include <vector>

class Shader;

struct Character {
    GLuint textureID;
    glm::ivec2 size;
    glm::ivec2 bearing;
    unsigned int advance;
};

class TextRenderer {
public:
    TextRenderer(const char* fontPath, unsigned int fontSize, Shader* shader);
    ~TextRenderer();

    void renderText(const std::string& text, float x, float y, float scale, const glm::vec3& color);
    glm::vec2 getTextSize(const std::string& text, float scale);
    void setProjection(const glm::mat4& proj) { mProjection = proj; }

private:
    std::map<unsigned int, Character> mCharacters;
    Shader* mShader;
    GLuint mVAO, mVBO;
    glm::mat4 mProjection;

    int decodeUTF8(const char*& ptr, const char* end);
};

#endif