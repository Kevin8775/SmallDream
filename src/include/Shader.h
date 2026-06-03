#pragma once
#include <glad/glad.h>
#include <string>
#include <unordered_map>

class Shader {
public:
    Shader(const char* vertexPath, const char* fragmentPath);
    ~Shader();
    void use();
    GLuint id() const { return mProgram; }
    void setInt(const std::string& name, int v);
    void setFloat(const std::string& name, float v);
    void setVec2(const std::string& name, float x, float y);
    void setVec3(const std::string& name, float x, float y, float z);
    void setVec4(const std::string& name, float x, float y, float z, float w);
    void setMat4(const std::string& name, const float* mat);
private:
    GLuint mProgram;
    std::unordered_map<std::string, GLint> mUniformCache;
    GLint getUniform(const std::string& name);
    GLuint compile(const char* path, GLenum type);
    std::string readFile(const char* path);
};
