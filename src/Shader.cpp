#include "Shader.h"
#include <fstream>
#include <sstream>
#include <iostream>

Shader::Shader(const char* vertexPath, const char* fragmentPath) {
    GLuint vertex = compile(vertexPath, GL_VERTEX_SHADER);
    GLuint fragment = compile(fragmentPath, GL_FRAGMENT_SHADER);
    mProgram = glCreateProgram();
    glAttachShader(mProgram, vertex);
    glAttachShader(mProgram, fragment);
    glLinkProgram(mProgram);
    GLint success;
    glGetProgramiv(mProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char info[512];
        glGetProgramInfoLog(mProgram, 512, nullptr, info);
        std::cerr << "Shader link error: " << info << std::endl;
    }
    glDeleteShader(vertex);
    glDeleteShader(fragment);
}

Shader::~Shader() {
    glDeleteProgram(mProgram);
}

void Shader::use() {
    glUseProgram(mProgram);
}

std::string Shader::readFile(const char* path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        std::cerr << "Failed to open shader: " << path << std::endl;
        return "";
    }
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

GLuint Shader::compile(const char* path, GLenum type) {
    std::string src = readFile(path);
    const char* csrc = src.c_str();
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &csrc, nullptr);
    glCompileShader(shader);
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info[512];
        glGetShaderInfoLog(shader, 512, nullptr, info);
        std::cerr << "Shader compile error (" << path << "): " << info << std::endl;
    }
    return shader;
}

GLint Shader::getUniform(const std::string& name) {
    auto it = mUniformCache.find(name);
    if (it != mUniformCache.end()) return it->second;
    GLint loc = glGetUniformLocation(mProgram, name.c_str());
    mUniformCache[name] = loc;
    return loc;
}

void Shader::setInt(const std::string& name, int v) { glUniform1i(getUniform(name), v); }
void Shader::setFloat(const std::string& name, float v) { glUniform1f(getUniform(name), v); }
void Shader::setVec2(const std::string& name, float x, float y) { glUniform2f(getUniform(name), x, y); }
void Shader::setVec3(const std::string& name, float x, float y, float z) { glUniform3f(getUniform(name), x, y, z); }
void Shader::setVec4(const std::string& name, float x, float y, float z, float w) { glUniform4f(getUniform(name), x, y, z, w); }
void Shader::setMat4(const std::string& name, const float* mat) { glUniformMatrix4fv(getUniform(name), 1, GL_FALSE, mat); }
