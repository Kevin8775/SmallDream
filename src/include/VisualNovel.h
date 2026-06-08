#pragma once
#include <glad/glad.h>
#include <string>
#include <vector>
#include <glm/glm.hpp>

struct ma_engine;
struct ma_sound;

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
    void setSoundEngine(ma_engine* engine);

private:
    void advanceLine();
    void calcLayout();
    void playCurrentLineSound();
    void stopCurrentSound();
    void startAmbient();
    void stopAmbient();

    struct LineInfo {
        std::string text;
        std::string soundPath;
        bool waitForSound;
    };

    struct Scene {
        std::vector<LineInfo> lines;
        std::string bgPath;
        std::string ambientSound;
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
    Texture* mNextBg;
    Texture* mPrevBg;
    Texture* mDialogBox;
    bool mTransitioning;
    float mTransitionTimer;
    float mTransitionDuration;
    float mBoxX, mBoxY, mBoxW, mBoxH;
    float mTextX, mTextY, mTextW, mTextH;

    TextRenderer* mTextRenderer;
    Shader* mSpriteShader;
    GLuint mQuadVAO;
    glm::mat4 mProj;
    int mWindowWidth;
    int mWindowHeight;

    ma_engine* mSoundEngine;
    ma_sound* mCurrentSound;
    ma_sound* mAmbientSound;
    bool mWaitingForSound;
};
