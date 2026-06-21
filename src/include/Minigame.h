#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>
#include <string>

class Shader;
class TextRenderer;

extern "C" {
    typedef struct ma_engine ma_engine;
    typedef struct ma_sound ma_sound;
}

enum class MinigameType { QuickTap, ColorMatch, Sequence };
enum class MinigameState { None, Playing, Won, Lost };

struct FloatingText {
    std::string text;
    float x, y;
    float timer;
    float lifetime;
    float r, g, b;
};

struct Particle {
    float x, y;
    float vx, vy;
    float life, maxLife;
    float r, g, b;
    float size;
};

struct QuickTapTarget {
    float x, y;
    float life;
    float maxLife;
    bool alive;
    float scale;
    float glowPhase;
};

struct QuickTapData {
    static const int MAX_TARGETS = 12;
    static const int REQUIRED_HITS = 8;
    QuickTapTarget targets[MAX_TARGETS];
    int spawned;
    int hits;
    int missed;
    float spawnTimer;
    float spawnInterval;
    float targetLife;
    int totalSpawned;
    float difficulty;
};

struct ColorMatchData {
    static const int ROUNDS_TO_WIN = 4;
    static const int TOTAL_ROUNDS = 6;
    static const int NUM_OPTIONS = 8;
    float targetR, targetG, targetB;
    float optionR[NUM_OPTIONS], optionG[NUM_OPTIONS], optionB[NUM_OPTIONS];
    int correctIndex;
    int round;
    int score;
    float roundTimer;
    float maxRoundTime;
    float flashTimer;
    int flashType;
    float shakeTimer;
};

struct SequenceData {
    static const int MAX_SEQ = 5;
    static const int NUM_LEVELS = 3;
    static const int LEVEL_SIZES[NUM_LEVELS];
    int sequence[MAX_SEQ];
    int playerInput[MAX_SEQ];
    int inputIndex;
    int showIndex;
    float showTimer;
    float showPause;
    int level;
    bool showingSequence;
    bool waitingInput;
    float inputTimeLeft;
    float inputTimeMax;
    float flashTimer;
    int flashCell;
    float cellGlow[9];
    bool gameOver;
    float gameOverTimer;
};

class Minigame {
public:
    Minigame();
    ~Minigame();

    void loadSounds(ma_engine* engine);
    void start(MinigameType type);
    void reset();
    void update(float dt, bool spacePressed, bool mousePressed, float mouseX, float mouseY, int windowW, int windowH);
    void render(Shader& spriteShader, Shader& textShader, TextRenderer& textRenderer, int windowW, int windowH);
    void renderMouseCursor(Shader& spriteShader, float mouseX, float mouseY);

    MinigameState getState() const { return mState; }
    MinigameType getType() const { return mType; }
    bool isTutorialOrCountdown() const { return mTutorialActive || mCountdownActive; }

private:
    void updateQuickTap(float dt, bool mousePressed, float mouseX, float mouseY, int windowW, int windowH);
    void updateColorMatch(float dt, bool mousePressed, float mouseX, float mouseY, int windowW, int windowH);
    void updateSequence(float dt, bool spacePressed, bool mousePressed, float mouseX, float mouseY, int windowW, int windowH);

    void renderQuickTap(Shader& spriteShader, Shader& textShader, TextRenderer& textRenderer, int windowW, int windowH);
    void renderColorMatch(Shader& spriteShader, Shader& textShader, TextRenderer& textRenderer, int windowW, int windowH);
    void renderSequence(Shader& spriteShader, Shader& textShader, TextRenderer& textRenderer, int windowW, int windowH);

    void renderTutorial(Shader& spriteShader, Shader& textShader, TextRenderer& textRenderer, int windowW, int windowH);
    void renderCountdown(Shader& spriteShader, Shader& textShader, TextRenderer& textRenderer, int windowW, int windowH);

    void addFloatingText(const std::string& text, float x, float y, float r, float g, float b);
    void updateFloatingTexts(float dt);
    void renderFloatingTexts(Shader& textShader, TextRenderer& textRenderer, int windowW, int windowH);

    void emitParticles(float cx, float cy, float r, float g, float b, int count, float speed, float size);
    void updateParticles(float dt);
    void renderParticles(Shader& shader);

    MinigameType mType;
    MinigameState mState;
    float mTimer;
    float mShowResultTimer;

    QuickTapData mQuickTap;
    ColorMatchData mColorMatch;
    SequenceData mSequence;

    GLuint mOverlayVAO, mOverlayVBO;
    bool mOverlayCreated;
    void createOverlayQuad();

    GLuint mShapeVAO, mShapeVBO;
    bool mShapeCreated;
    void renderQuad(Shader& shader, float x, float y, float w, float h, float r, float g, float b, float a);

    static const int CIRCLE_SEGMENTS = 32;
    GLuint mCircleVAO, mCircleVBO;
    GLuint mRingVAO, mRingVBO;
    int mRingVertexCount;
    void createShapeBuffers();
    void renderCircle(Shader& shader, float cx, float cy, float radius, float r, float g, float b, float a);
    void renderRing(Shader& shader, float cx, float cy, float outerR, float innerR, float r, float g, float b, float a);
    void renderDiamond(Shader& shader, float cx, float cy, float size, float r, float g, float b, float a);
    void renderTriangle(Shader& shader, float cx, float cy, float size, float rotation, float r, float g, float b, float a);
    void renderRoundedQuad(Shader& shader, float x, float y, float w, float h, float cornerSize, float r, float g, float b, float a);
    void renderStar(Shader& shader, float cx, float cy, float outerR, float innerR, float rotation, float r, float g, float b, float a, int points = 5);
    void renderHexagon(Shader& shader, float cx, float cy, float size, float rotation, float r, float g, float b, float a);

    ma_sound* mWinSound;
    ma_sound* mLoseSound;
    ma_sound* mFlipSound;
    bool mSoundsLoaded;
    ma_engine* mEngine;

    float mFlashAlpha;
    float mFlashTimer;
    bool mFlashIsWin;

    float mScreenShakeX;
    float mScreenShakeY;
    float mScreenShakeTimer;

    std::vector<FloatingText> mFloatingTexts;
    std::vector<Particle> mParticles;

    bool mTutorialActive;
    bool mCountdownActive;
    float mCountdownTimer;
    int mCountdownValue;
};
