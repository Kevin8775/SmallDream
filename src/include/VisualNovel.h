#pragma once
#include <glad/glad.h>
#include <string>
#include <vector>
#include <glm/glm.hpp>

class Texture;
class TextRenderer;
class Shader;

class VisualNovel {
public:
    VisualNovel(TextRenderer* textRenderer, Shader* spriteShader,
                GLuint quadVAO, const glm::mat4& proj,
                int windowWidth, int windowHeight);
    ~VisualNovel();

    void reset();
    void update(float dt, bool mouseJustPressed);
    void render();
    bool isFinished() const;
    void setBackground(Texture* bg);

private:
    void advanceLine();
    void calcLayout();

    struct Scene {
        std::vector<std::string> lines;
    };

    std::vector<Scene> mScenes;
    int mCurrentScene;
    int mCurrentLine;
    int mCharIndex;
    float mCharTimer;
    float mCharInterval;
    bool mLineFinished;
    bool mAllDone;

    Texture* mBackground;
    Texture* mDialogBox;
    float mBoxX, mBoxY, mBoxW, mBoxH;
    float mTextX, mTextY, mTextW, mTextH;

    TextRenderer* mTextRenderer;
    Shader* mSpriteShader;
    GLuint mQuadVAO;
    glm::mat4 mProj;
    int mWindowWidth;
    int mWindowHeight;
};
