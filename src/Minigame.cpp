#include "Minigame.h"
#include "Shader.h"
#include "TextRenderer.h"
#include "Localization.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <algorithm>

static float randFloat(float a, float b) {
    return a + (static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX)) * (b - a);
}

static int randInt(int a, int b) {
    return a + std::rand() % (b - a + 1);
}

const int SequenceData::LEVEL_SIZES[SequenceData::NUM_LEVELS] = { 3, 4, 5 };

static float easeOutBack(float t) {
    float c1 = 1.70158f;
    float c3 = c1 + 1.0f;
    return 1.0f + c3 * std::pow(t - 1.0f, 3) + c1 * std::pow(t - 1.0f, 2);
}

Minigame::Minigame()
    : mType(MinigameType::QuickTap)
    , mState(MinigameState::None)
    , mTimer(0.0f)
    , mShowResultTimer(0.0f)
    , mOverlayVAO(0)
    , mOverlayVBO(0)
    , mOverlayCreated(false)
    , mShapeVAO(0)
    , mShapeVBO(0)
    , mShapeCreated(false)
    , mCircleVAO(0)
    , mCircleVBO(0)
    , mRingVAO(0)
    , mRingVBO(0)
    , mRingVertexCount(0)
    , mSoundsLoaded(false)
    , mEngine(nullptr)
    , mFlashAlpha(0.0f)
    , mFlashTimer(0.0f)
    , mFlashIsWin(false)
    , mScreenShakeX(0.0f)
    , mScreenShakeY(0.0f)
    , mScreenShakeTimer(0.0f)
    , mTutorialActive(false)
    , mCountdownActive(false)
    , mCountdownTimer(0.0f)
    , mCountdownValue(3)
{
    std::memset(&mQuickTap, 0, sizeof(mQuickTap));
    std::memset(&mColorMatch, 0, sizeof(mColorMatch));
    std::memset(&mSequence, 0, sizeof(mSequence));
    mParticles.clear();
}

Minigame::~Minigame() {
    if (mOverlayVAO) glDeleteVertexArrays(1, &mOverlayVAO);
    if (mOverlayVBO) glDeleteBuffers(1, &mOverlayVBO);
    if (mShapeVAO) glDeleteVertexArrays(1, &mShapeVAO);
    if (mShapeVBO) glDeleteBuffers(1, &mShapeVBO);
    if (mCircleVAO) glDeleteVertexArrays(1, &mCircleVAO);
    if (mCircleVBO) glDeleteBuffers(1, &mCircleVBO);
    if (mRingVAO) glDeleteVertexArrays(1, &mRingVAO);
    if (mRingVBO) glDeleteBuffers(1, &mRingVBO);
    if (mSoundsLoaded) {
        ma_sound_uninit(&mWinSound);
        ma_sound_uninit(&mLoseSound);
        ma_sound_uninit(&mFlipSound);
    }
}

void Minigame::loadSounds(ma_engine* engine) {
    if (mSoundsLoaded || !engine) return;
    mEngine = engine;
    bool ok = true;
    if (ma_sound_init_from_file(mEngine, "assets/sounds/ui/win.wav", 0, nullptr, nullptr, &mWinSound) != MA_SUCCESS) ok = false;
    if (ma_sound_init_from_file(mEngine, "assets/sounds/ui/error.wav", 0, nullptr, nullptr, &mLoseSound) != MA_SUCCESS) ok = false;
    if (ma_sound_init_from_file(mEngine, "assets/sounds/ui/flipcard.wav", 0, nullptr, nullptr, &mFlipSound) != MA_SUCCESS) ok = false;
    mSoundsLoaded = ok;
}

void Minigame::createOverlayQuad() {
    if (mOverlayCreated) return;
    mOverlayCreated = true;
    float vertices[] = {
        0.0f, 0.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 1.0f, 0.0f,
        1.0f, 1.0f, 1.0f, 1.0f,
        0.0f, 0.0f, 0.0f, 0.0f,
        1.0f, 1.0f, 1.0f, 1.0f,
        0.0f, 1.0f, 0.0f, 1.0f
    };
    glGenVertexArrays(1, &mOverlayVAO);
    glGenBuffers(1, &mOverlayVBO);
    glBindVertexArray(mOverlayVAO);
    glBindBuffer(GL_ARRAY_BUFFER, mOverlayVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);
}

void Minigame::createShapeBuffers() {
    if (mShapeCreated) return;
    mShapeCreated = true;
    const float PI2 = 2.0f * 3.14159265f;
    {
        std::vector<float> verts;
        verts.push_back(0.0f); verts.push_back(0.0f);
        for (int i = 0; i <= CIRCLE_SEGMENTS; i++) {
            float angle = (float)i / (float)CIRCLE_SEGMENTS * PI2;
            verts.push_back(std::cos(angle));
            verts.push_back(std::sin(angle));
        }
        glGenVertexArrays(1, &mCircleVAO);
        glGenBuffers(1, &mCircleVBO);
        glBindVertexArray(mCircleVAO);
        glBindBuffer(GL_ARRAY_BUFFER, mCircleVBO);
        glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
        glBindVertexArray(0);
    }
    {
        std::vector<float> verts;
        for (int i = 0; i <= CIRCLE_SEGMENTS; i++) {
            float angle = (float)i / (float)CIRCLE_SEGMENTS * PI2;
            float co = std::cos(angle), si = std::sin(angle);
            verts.push_back(co); verts.push_back(si);
            verts.push_back(co); verts.push_back(si);
        }
        mRingVertexCount = (CIRCLE_SEGMENTS + 1) * 2;
        glGenVertexArrays(1, &mRingVAO);
        glGenBuffers(1, &mRingVBO);
        glBindVertexArray(mRingVAO);
        glBindBuffer(GL_ARRAY_BUFFER, mRingVBO);
        glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
        glBindVertexArray(0);
    }
    {
        glGenVertexArrays(1, &mShapeVAO);
        glGenBuffers(1, &mShapeVBO);
        glBindVertexArray(mShapeVAO);
        glBindBuffer(GL_ARRAY_BUFFER, mShapeVBO);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
        glBindVertexArray(0);
    }
}

void Minigame::renderQuad(Shader& shader, float x, float y, float w, float h, float r, float g, float b, float a) {
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(x, y, 0.0f));
    model = glm::scale(model, glm::vec3(w, h, 1.0f));
    shader.setMat4("uModel", glm::value_ptr(model));
    shader.setVec4("uColor", r, g, b, a);
    glBindVertexArray(mOverlayVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void Minigame::renderCircle(Shader& shader, float cx, float cy, float radius, float r, float g, float b, float a) {
    createShapeBuffers();
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(cx, cy, 0.0f));
    model = glm::scale(model, glm::vec3(radius, radius, 1.0f));
    shader.setMat4("uModel", glm::value_ptr(model));
    shader.setVec4("uColor", r, g, b, a);
    glBindVertexArray(mCircleVAO);
    glDrawArrays(GL_TRIANGLE_FAN, 0, CIRCLE_SEGMENTS + 2);
    glBindVertexArray(0);
}

void Minigame::renderRing(Shader& shader, float cx, float cy, float outerR, float innerR, float r, float g, float b, float a) {
    createShapeBuffers();
    const float PI2 = 2.0f * 3.14159265f;
    float verts[136];
    int idx = 0;
    for (int i = 0; i <= CIRCLE_SEGMENTS; i++) {
        float angle = (float)i / (float)CIRCLE_SEGMENTS * PI2;
        float co = std::cos(angle), si = std::sin(angle);
        verts[idx++] = co * innerR; verts[idx++] = si * innerR;
        verts[idx++] = co * outerR; verts[idx++] = si * outerR;
    }
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(cx, cy, 0.0f));
    shader.setMat4("uModel", glm::value_ptr(model));
    shader.setVec4("uColor", r, g, b, a);
    glBindVertexArray(mShapeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, mShapeVBO);
    glBufferData(GL_ARRAY_BUFFER, idx * sizeof(float), verts, GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, idx / 2);
    glBindVertexArray(0);
}

void Minigame::renderDiamond(Shader& shader, float cx, float cy, float size, float r, float g, float b, float a) {
    float half = size * 0.5f;
    renderQuad(shader, cx - half, cy - half, size, size, r, g, b, a);
}

void Minigame::renderTriangle(Shader& shader, float cx, float cy, float size, float rotation, float r, float g, float b, float a) {
    createShapeBuffers();
    float half = size * 0.5f;
    float pts[3][2] = { { 0.0f, -half }, { -half, half }, { half, half } };
    float co = std::cos(rotation), si = std::sin(rotation);
    float verts[10];
    verts[0] = 0.0f; verts[1] = 0.0f;
    for (int i = 0; i < 3; i++) {
        float rx = pts[i][0] * co - pts[i][1] * si;
        float ry = pts[i][0] * si + pts[i][1] * co;
        verts[2 + i * 2] = rx; verts[3 + i * 2] = ry;
    }
    verts[8] = pts[0][0] * co - pts[0][1] * si;
    verts[9] = pts[0][0] * si + pts[0][1] * co;
    glBindVertexArray(mShapeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, mShapeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);
    shader.setMat4("uModel", glm::value_ptr(glm::translate(glm::mat4(1.0f), glm::vec3(cx, cy, 0.0f))));
    shader.setVec4("uColor", r, g, b, a);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 5);
    glBindVertexArray(0);
}

void Minigame::renderRoundedQuad(Shader& shader, float x, float y, float w, float h, float cornerSize, float r, float g, float b, float a) {
    renderQuad(shader, x + cornerSize, y, w - cornerSize * 2, h, r, g, b, a);
    renderQuad(shader, x, y + cornerSize, w, h - cornerSize * 2, r, g, b, a);
    renderCircle(shader, x + cornerSize, y + cornerSize, cornerSize, r, g, b, a);
    renderCircle(shader, x + w - cornerSize, y + cornerSize, cornerSize, r, g, b, a);
    renderCircle(shader, x + cornerSize, y + h - cornerSize, cornerSize, r, g, b, a);
    renderCircle(shader, x + w - cornerSize, y + h - cornerSize, cornerSize, r, g, b, a);
}

void Minigame::renderStar(Shader& shader, float cx, float cy, float outerR, float innerR, float rotation, float r, float g, float b, float a, int points) {
    createShapeBuffers();
    const float PI2 = 2.0f * 3.14159265f;
    int totalVerts = points * 2 + 2;
    float verts[32];
    verts[0] = 0.0f; verts[1] = 0.0f;
    for (int i = 0; i <= points * 2; i++) {
        float angle = (float)i / (float)(points * 2) * PI2 - PI2 / 4.0f + rotation;
        float rad = (i % 2 == 0) ? outerR : innerR;
        verts[2 + i * 2] = std::cos(angle) * rad;
        verts[3 + i * 2] = std::sin(angle) * rad;
    }
    glBindVertexArray(mShapeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, mShapeVBO);
    glBufferData(GL_ARRAY_BUFFER, totalVerts * 2 * sizeof(float), verts, GL_DYNAMIC_DRAW);
    shader.setMat4("uModel", glm::value_ptr(glm::translate(glm::mat4(1.0f), glm::vec3(cx, cy, 0.0f))));
    shader.setVec4("uColor", r, g, b, a);
    glDrawArrays(GL_TRIANGLE_FAN, 0, totalVerts);
    glBindVertexArray(0);
}

void Minigame::renderHexagon(Shader& shader, float cx, float cy, float size, float rotation, float r, float g, float b, float a) {
    createShapeBuffers();
    const float PI2 = 2.0f * 3.14159265f;
    float verts[16];
    verts[0] = 0.0f; verts[1] = 0.0f;
    for (int i = 0; i <= 6; i++) {
        float angle = (float)i / 6.0f * PI2 + rotation;
        verts[2 + i * 2] = std::cos(angle) * size;
        verts[3 + i * 2] = std::sin(angle) * size;
    }
    glBindVertexArray(mShapeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, mShapeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);
    shader.setMat4("uModel", glm::value_ptr(glm::translate(glm::mat4(1.0f), glm::vec3(cx, cy, 0.0f))));
    shader.setVec4("uColor", r, g, b, a);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 8);
    glBindVertexArray(0);
}

void Minigame::addFloatingText(const std::string& text, float x, float y, float r, float g, float b) {
    FloatingText ft;
    ft.text = text;
    ft.x = x;
    ft.y = y;
    ft.timer = 0.0f;
    ft.lifetime = 1.2f;
    ft.r = r; ft.g = g; ft.b = b;
    mFloatingTexts.push_back(ft);
}

void Minigame::updateFloatingTexts(float dt) {
    for (int i = (int)mFloatingTexts.size() - 1; i >= 0; i--) {
        mFloatingTexts[i].timer += dt;
        mFloatingTexts[i].y -= 40.0f * dt;
        if (mFloatingTexts[i].timer >= mFloatingTexts[i].lifetime)
            mFloatingTexts.erase(mFloatingTexts.begin() + i);
    }
}

void Minigame::renderFloatingTexts(Shader& textShader, TextRenderer& textRenderer, int windowW, int windowH) {
    glm::mat4 proj = glm::ortho(0.0f, (float)windowW, (float)windowH, 0.0f, -1.0f, 1.0f);
    textShader.use();
    textShader.setMat4("uProjection", glm::value_ptr(proj));
    for (auto& ft : mFloatingTexts) {
        float alpha = 1.0f - (ft.timer / ft.lifetime);
        float scale = 0.4f + 0.1f * easeOutBack(std::min(ft.timer / 0.2f, 1.0f));
        textRenderer.renderText(ft.text, ft.x, ft.y, scale, glm::vec3(ft.r, ft.g, ft.b), alpha);
    }
}

void Minigame::emitParticles(float cx, float cy, float r, float g, float b, int count, float speed, float size) {
    for (int i = 0; i < count; i++) {
        Particle p;
        p.x = cx;
        p.y = cy;
        float angle = randFloat(0.0f, 6.28318f);
        float spd = randFloat(speed * 0.4f, speed);
        p.vx = std::cos(angle) * spd;
        p.vy = std::sin(angle) * spd;
        p.life = 0.0f;
        p.maxLife = randFloat(0.4f, 0.9f);
        p.r = r; p.g = g; p.b = b;
        p.size = randFloat(size * 0.5f, size * 1.5f);
        mParticles.push_back(p);
    }
}

void Minigame::updateParticles(float dt) {
    for (int i = (int)mParticles.size() - 1; i >= 0; i--) {
        mParticles[i].life += dt;
        mParticles[i].x += mParticles[i].vx * dt;
        mParticles[i].y += mParticles[i].vy * dt;
        mParticles[i].vx *= 0.96f;
        mParticles[i].vy *= 0.96f;
        if (mParticles[i].life >= mParticles[i].maxLife)
            mParticles.erase(mParticles.begin() + i);
    }
}

void Minigame::renderParticles(Shader& shader) {
    for (auto& p : mParticles) {
        float t = p.life / p.maxLife;
        float alpha = (1.0f - t) * 0.8f;
        float sz = p.size * (1.0f - t * 0.3f);
        renderCircle(shader, p.x, p.y, sz, p.r, p.g, p.b, alpha);
    }
}

void Minigame::start(MinigameType type) {
    mType = type;
    mState = MinigameState::Playing;
    mTimer = 0.0f;
    mShowResultTimer = 0.0f;
    mFlashAlpha = 0.0f;
    mFlashTimer = 0.0f;
    mScreenShakeX = 0.0f;
    mScreenShakeY = 0.0f;
    mScreenShakeTimer = 0.0f;
    mFloatingTexts.clear();
    mParticles.clear();
    mTutorialActive = true;
    mCountdownActive = false;
    mCountdownTimer = 0.0f;
    mCountdownValue = 3;
    createOverlayQuad();
    createShapeBuffers();

    switch (type) {
    case MinigameType::QuickTap: {
        mQuickTap.spawned = 0;
        mQuickTap.hits = 0;
        mQuickTap.missed = 0;
        mQuickTap.spawnTimer = 0.0f;
        mQuickTap.spawnInterval = 0.8f;
        mQuickTap.targetLife = 2.0f;
        mQuickTap.totalSpawned = 0;
        mQuickTap.difficulty = 0.0f;
        for (int i = 0; i < QuickTapData::MAX_TARGETS; i++) {
            mQuickTap.targets[i].alive = false;
            mQuickTap.targets[i].life = 0.0f;
        }
        break;
    }
    case MinigameType::ColorMatch: {
        mColorMatch.round = 0;
        mColorMatch.score = 0;
        mColorMatch.roundTimer = 0.0f;
        mColorMatch.maxRoundTime = 4.0f;
        mColorMatch.flashTimer = 0.0f;
        mColorMatch.flashType = 0;
        mColorMatch.shakeTimer = 0.0f;
        for (int i = 0; i < ColorMatchData::NUM_OPTIONS; i++) {
            mColorMatch.optionR[i] = randFloat(0.1f, 0.9f);
            mColorMatch.optionG[i] = randFloat(0.1f, 0.9f);
            mColorMatch.optionB[i] = randFloat(0.1f, 0.9f);
        }
        mColorMatch.correctIndex = randInt(0, ColorMatchData::NUM_OPTIONS - 1);
        mColorMatch.targetR = mColorMatch.optionR[mColorMatch.correctIndex];
        mColorMatch.targetG = mColorMatch.optionG[mColorMatch.correctIndex];
        mColorMatch.targetB = mColorMatch.optionB[mColorMatch.correctIndex];
        break;
    }
    case MinigameType::Sequence: {
        mSequence.inputIndex = 0;
        mSequence.showIndex = -1;
        mSequence.showTimer = 0.0f;
        mSequence.showPause = 0.5f;
        mSequence.level = 0;
        mSequence.showingSequence = true;
        mSequence.waitingInput = false;
        mSequence.inputTimeLeft = 8.0f;
        mSequence.inputTimeMax = 8.0f;
        mSequence.flashTimer = 0.0f;
        mSequence.flashCell = -1;
        mSequence.gameOver = false;
        mSequence.gameOverTimer = 0.0f;
        for (int i = 0; i < 9; i++) mSequence.cellGlow[i] = 0.0f;
        for (int i = 0; i < SequenceData::MAX_SEQ; i++) {
            mSequence.sequence[i] = randInt(0, 8);
            mSequence.playerInput[i] = -1;
        }
        break;
    }
    }
}

void Minigame::reset() {
    mState = MinigameState::None;
    mTimer = 0.0f;
    mFloatingTexts.clear();
    mParticles.clear();
}

void Minigame::update(float dt, bool spacePressed, bool mousePressed, float mouseX, float mouseY, int windowW, int windowH) {
    updateFloatingTexts(dt);
    updateParticles(dt);
    if (mScreenShakeTimer > 0.0f) {
        mScreenShakeTimer -= dt;
        float intensity = mScreenShakeTimer * 8.0f;
        mScreenShakeX = std::sin(mTimer * 60.0f) * intensity;
        mScreenShakeY = std::cos(mTimer * 45.0f) * intensity * 0.5f;
    } else {
        mScreenShakeX = 0.0f;
        mScreenShakeY = 0.0f;
    }
    if (mState == MinigameState::Won || mState == MinigameState::Lost) {
        mShowResultTimer += dt;
        mFlashTimer += dt;
        mFlashAlpha = std::max(0.0f, 0.6f - mFlashTimer * 1.2f);
        if (mShowResultTimer > 2.0f) mState = MinigameState::None;
        return;
    }
    if (mState != MinigameState::Playing) return;
    mTimer += dt;

    if (mTutorialActive) {
        if (mousePressed) {
            mTutorialActive = false;
            mCountdownActive = true;
            mCountdownTimer = 0.0f;
            mCountdownValue = 3;
        }
        return;
    }

    if (mCountdownActive) {
        mCountdownTimer += dt;
        if (mCountdownTimer >= 1.0f) {
            mCountdownTimer -= 1.0f;
            mCountdownValue--;
            if (mCountdownValue <= 0) {
                mCountdownActive = false;
            }
        }
        return;
    }

    mFlashTimer += dt;
    if (mFlashAlpha > 0.0f) mFlashAlpha = std::max(0.0f, mFlashAlpha - dt * 3.0f);
    switch (mType) {
    case MinigameType::QuickTap: updateQuickTap(dt, mousePressed, mouseX, mouseY, windowW, windowH); break;
    case MinigameType::ColorMatch: updateColorMatch(dt, mousePressed, mouseX, mouseY, windowW, windowH); break;
    case MinigameType::Sequence: updateSequence(dt, spacePressed, mousePressed, mouseX, mouseY, windowW, windowH); break;
    }
}

void Minigame::updateQuickTap(float dt, bool mousePressed, float mouseX, float mouseY, int windowW, int windowH) {
    float padding = 60.0f;
    mQuickTap.spawnTimer += dt;
    float interval = std::max(0.3f, mQuickTap.spawnInterval - mQuickTap.difficulty * 0.1f);
    if (mQuickTap.spawnTimer >= interval && mQuickTap.totalSpawned < QuickTapData::MAX_TARGETS) {
        mQuickTap.spawnTimer = 0.0f;
        for (int i = 0; i < QuickTapData::MAX_TARGETS; i++) {
            if (!mQuickTap.targets[i].alive) {
                mQuickTap.targets[i].x = randFloat(padding, windowW - padding);
                mQuickTap.targets[i].y = randFloat(padding + 80.0f, windowH - padding);
                mQuickTap.targets[i].life = 0.0f;
                float maxLife = std::max(0.8f, mQuickTap.targetLife - mQuickTap.difficulty * 0.15f);
                mQuickTap.targets[i].maxLife = maxLife;
                mQuickTap.targets[i].alive = true;
                mQuickTap.targets[i].scale = 0.0f;
                mQuickTap.targets[i].glowPhase = randFloat(0.0f, 6.28f);
                mQuickTap.totalSpawned++;
                mQuickTap.spawned++;
                break;
            }
        }
    }

    for (int i = 0; i < QuickTapData::MAX_TARGETS; i++) {
        if (!mQuickTap.targets[i].alive) continue;
        mQuickTap.targets[i].life += dt;
        mQuickTap.targets[i].scale = std::min(1.0f, mQuickTap.targets[i].scale + dt * 6.0f);
        mQuickTap.targets[i].glowPhase += dt * 8.0f;
        if (mQuickTap.targets[i].life >= mQuickTap.targets[i].maxLife) {
            mQuickTap.targets[i].alive = false;
            mQuickTap.spawned--;
            mQuickTap.missed++;
        }
    }

    if (mousePressed) {
        for (int i = 0; i < QuickTapData::MAX_TARGETS; i++) {
            if (!mQuickTap.targets[i].alive) continue;
            float dx = mouseX - mQuickTap.targets[i].x;
            float dy = mouseY - mQuickTap.targets[i].y;
            float dist = std::sqrt(dx * dx + dy * dy);
            float radius = 28.0f * mQuickTap.targets[i].scale;
            if (dist < radius) {
                mQuickTap.targets[i].alive = false;
                mQuickTap.spawned--;
                mQuickTap.hits++;
                mQuickTap.difficulty += 0.15f;
                emitParticles(mQuickTap.targets[i].x, mQuickTap.targets[i].y, 0.2f, 1.0f, 0.4f, 15, 160.0f, 5.0f);
                addFloatingText("+1", mQuickTap.targets[i].x - 10.0f, mQuickTap.targets[i].y - 20.0f, 0.2f, 1.0f, 0.4f);
                if (mSoundsLoaded) ma_sound_start(&mFlipSound);
                break;
            }
        }
    }

    if (mQuickTap.totalSpawned >= QuickTapData::MAX_TARGETS && mQuickTap.spawned == 0) {
        if (mQuickTap.hits >= QuickTapData::REQUIRED_HITS) {
            mState = MinigameState::Won;
            mShowResultTimer = 0.0f;
            mFlashAlpha = 0.6f; mFlashTimer = 0.0f; mFlashIsWin = true;
            if (mSoundsLoaded) ma_sound_start(&mWinSound);
        } else {
            mState = MinigameState::Lost;
            mShowResultTimer = 0.0f;
            mFlashAlpha = 0.6f; mFlashTimer = 0.0f; mFlashIsWin = false;
            if (mSoundsLoaded) ma_sound_start(&mLoseSound);
        }
    }
}

void Minigame::updateColorMatch(float dt, bool mousePressed, float mouseX, float mouseY, int windowW, int windowH) {
    mColorMatch.roundTimer += dt;
    mColorMatch.flashTimer += dt;
    if (mColorMatch.flashTimer > 0.3f) mColorMatch.flashType = 0;
    if (mColorMatch.shakeTimer > 0.0f) mColorMatch.shakeTimer -= dt;

    if (mColorMatch.roundTimer >= mColorMatch.maxRoundTime) {
        mColorMatch.round++;
        if (mColorMatch.round >= ColorMatchData::TOTAL_ROUNDS) {
            if (mColorMatch.score >= ColorMatchData::ROUNDS_TO_WIN) {
                mState = MinigameState::Won;
                mShowResultTimer = 0.0f;
                mFlashAlpha = 0.6f; mFlashTimer = 0.0f; mFlashIsWin = true;
                if (mSoundsLoaded) ma_sound_start(&mWinSound);
            } else {
                mState = MinigameState::Lost;
                mShowResultTimer = 0.0f;
                mFlashAlpha = 0.6f; mFlashTimer = 0.0f; mFlashIsWin = false;
                if (mSoundsLoaded) ma_sound_start(&mLoseSound);
            }
            return;
        }
        mColorMatch.roundTimer = 0.0f;
        for (int i = 0; i < ColorMatchData::NUM_OPTIONS; i++) {
            float variation = std::max(0.05f, 0.3f - mColorMatch.round * 0.04f);
            mColorMatch.optionR[i] = std::max(0.0f, std::min(1.0f, mColorMatch.targetR + randFloat(-variation, variation)));
            mColorMatch.optionG[i] = std::max(0.0f, std::min(1.0f, mColorMatch.targetG + randFloat(-variation, variation)));
            mColorMatch.optionB[i] = std::max(0.0f, std::min(1.0f, mColorMatch.targetB + randFloat(-variation, variation)));
        }
        mColorMatch.correctIndex = randInt(0, ColorMatchData::NUM_OPTIONS - 1);
        mColorMatch.optionR[mColorMatch.correctIndex] = mColorMatch.targetR;
        mColorMatch.optionG[mColorMatch.correctIndex] = mColorMatch.targetG;
        mColorMatch.optionB[mColorMatch.correctIndex] = mColorMatch.targetB;
    }

    if (!mousePressed) return;
    float boxSize = 65.0f;
    float gap = 18.0f;
    float cols = 4.0f;
    float rows = 2.0f;
    float totalW = cols * boxSize + (cols - 1) * gap;
    float totalH = rows * boxSize + (rows - 1) * gap;
    float startX = (windowW - totalW) / 2.0f;
    float startY = (windowH - totalH) / 2.0f + 30.0f;

    for (int i = 0; i < ColorMatchData::NUM_OPTIONS; i++) {
        int col = i % (int)cols;
        int row = i / (int)cols;
        float bx = startX + col * (boxSize + gap);
        float by = startY + row * (boxSize + gap);
        if (mouseX >= bx && mouseX <= bx + boxSize && mouseY >= by && mouseY <= by + boxSize) {
            if (i == mColorMatch.correctIndex) {
                mColorMatch.score++;
                mColorMatch.flashType = 1;
                mColorMatch.flashTimer = 0.0f;
                emitParticles(bx + boxSize / 2.0f, by + boxSize / 2.0f, 1.0f, 0.85f, 0.1f, 20, 160.0f, 6.0f);
                addFloatingText("\u00a1Correcto!", windowW / 2.0f - 50.0f, startY - 40.0f, 1.0f, 0.85f, 0.1f);
                if (mSoundsLoaded) ma_sound_start(&mFlipSound);
            } else {
                mColorMatch.flashType = -1;
                mColorMatch.flashTimer = 0.0f;
                mColorMatch.shakeTimer = 0.2f;
                emitParticles(bx + boxSize / 2.0f, by + boxSize / 2.0f, 0.9f, 0.2f, 0.1f, 12, 120.0f, 5.0f);
                addFloatingText("X", windowW / 2.0f - 10.0f, startY - 40.0f, 0.9f, 0.2f, 0.1f);
                if (mSoundsLoaded) ma_sound_start(&mLoseSound);
            }
            mColorMatch.round++;
            mColorMatch.roundTimer = 0.0f;
            if (mColorMatch.round >= ColorMatchData::TOTAL_ROUNDS) {
                if (mColorMatch.score >= ColorMatchData::ROUNDS_TO_WIN) {
                    mState = MinigameState::Won;
                    mShowResultTimer = 0.0f;
                    mFlashAlpha = 0.6f; mFlashTimer = 0.0f; mFlashIsWin = true;
                    if (mSoundsLoaded) ma_sound_start(&mWinSound);
                } else {
                    mState = MinigameState::Lost;
                    mShowResultTimer = 0.0f;
                    mFlashAlpha = 0.6f; mFlashTimer = 0.0f; mFlashIsWin = false;
                    if (mSoundsLoaded) ma_sound_start(&mLoseSound);
                }
                return;
            }
            for (int j = 0; j < ColorMatchData::NUM_OPTIONS; j++) {
                float variation = std::max(0.05f, 0.3f - mColorMatch.round * 0.04f);
                mColorMatch.optionR[j] = std::max(0.0f, std::min(1.0f, mColorMatch.targetR + randFloat(-variation, variation)));
                mColorMatch.optionG[j] = std::max(0.0f, std::min(1.0f, mColorMatch.targetG + randFloat(-variation, variation)));
                mColorMatch.optionB[j] = std::max(0.0f, std::min(1.0f, mColorMatch.targetB + randFloat(-variation, variation)));
            }
            mColorMatch.correctIndex = randInt(0, ColorMatchData::NUM_OPTIONS - 1);
            mColorMatch.optionR[mColorMatch.correctIndex] = mColorMatch.targetR;
            mColorMatch.optionG[mColorMatch.correctIndex] = mColorMatch.targetG;
            mColorMatch.optionB[mColorMatch.correctIndex] = mColorMatch.targetB;
            break;
        }
    }
}

void Minigame::updateSequence(float dt, bool spacePressed, bool mousePressed, float mouseX, float mouseY, int windowW, int windowH) {
    mSequence.flashTimer += dt;
    if (mSequence.flashTimer > 0.25f) mSequence.flashCell = -1;
    for (int i = 0; i < 9; i++) {
        if (mSequence.cellGlow[i] > 0.0f)
            mSequence.cellGlow[i] = std::max(0.0f, mSequence.cellGlow[i] - dt * 5.0f);
    }

    if (mSequence.gameOver) {
        mSequence.gameOverTimer += dt;
        mScreenShakeTimer = std::max(mScreenShakeTimer, dt * 0.3f);
        if (mSequence.gameOverTimer >= 1.5f && spacePressed) {
            mSequence.gameOver = false;
            mSequence.gameOverTimer = 0.0f;
            mSequence.level = 0;
            mSequence.inputIndex = 0;
            mSequence.showIndex = -1;
            mSequence.showTimer = 0.0f;
            mSequence.showingSequence = true;
            mSequence.waitingInput = false;
            mSequence.inputTimeLeft = mSequence.inputTimeMax;
            for (int i = 0; i < 9; i++) mSequence.cellGlow[i] = 0.0f;
            for (int j = 0; j < SequenceData::MAX_SEQ; j++) {
                mSequence.sequence[j] = randInt(0, 8);
                mSequence.playerInput[j] = -1;
            }
        }
        return;
    }

    int seqLen = SequenceData::LEVEL_SIZES[mSequence.level];

    if (mSequence.showingSequence) {
        mSequence.showTimer += dt;
        float pauseTime = 0.6f;
        if (mSequence.showTimer < pauseTime) {
            mSequence.showIndex = -1;
        } else {
            float seqTimer = mSequence.showTimer - pauseTime;
            float speed = std::max(0.4f, 0.7f - mSequence.level * 0.1f);
            int idx = static_cast<int>(seqTimer / speed);
            if (idx < seqLen) {
                mSequence.showIndex = idx;
                mSequence.flashCell = mSequence.sequence[idx];
                mSequence.flashTimer = 0.0f;
                mSequence.cellGlow[mSequence.sequence[idx]] = 1.0f;
            } else {
                mSequence.showIndex = -1;
                mSequence.showingSequence = false;
                mSequence.waitingInput = true;
                mSequence.inputIndex = 0;
                mSequence.inputTimeLeft = mSequence.inputTimeMax;
            }
        }
        return;
    }

    if (mSequence.waitingInput) {
        mSequence.inputTimeLeft -= dt;
        if (mSequence.inputTimeLeft <= 0.0f) {
            mSequence.gameOver = true;
            mSequence.gameOverTimer = 0.0f;
            mState = MinigameState::Lost;
            mShowResultTimer = 0.0f;
            mFlashAlpha = 0.6f; mFlashTimer = 0.0f; mFlashIsWin = false;
            mScreenShakeTimer = 0.3f;
            if (mSoundsLoaded) ma_sound_start(&mLoseSound);
            return;
        }
    }

    if (!mousePressed || !mSequence.waitingInput) return;
    float boxSize = 100.0f;
    float gap = 15.0f;
    float totalW = 3 * boxSize + 2 * gap;
    float totalH = 3 * boxSize + 2 * gap;
    float startX = (windowW - totalW) / 2.0f;
    float startY = (windowH - totalH) / 2.0f;

    for (int row = 0; row < 3; row++) {
        for (int col = 0; col < 3; col++) {
            int cellIdx = row * 3 + col;
            float bx = startX + col * (boxSize + gap);
            float by = startY + row * (boxSize + gap);
            if (mouseX >= bx && mouseX <= bx + boxSize && mouseY >= by && mouseY <= by + boxSize) {
                mSequence.playerInput[mSequence.inputIndex] = cellIdx;
                mSequence.flashCell = cellIdx;
                mSequence.flashTimer = 0.0f;
                mSequence.cellGlow[cellIdx] = 1.0f;
                if (mSoundsLoaded) ma_sound_start(&mFlipSound);

                bool correctSoFar = true;
                for (int j = 0; j <= mSequence.inputIndex; j++) {
                    if (mSequence.playerInput[j] != mSequence.sequence[j]) {
                        correctSoFar = false;
                        break;
                    }
                }

                if (!correctSoFar) {
                    emitParticles(bx + boxSize / 2.0f, by + boxSize / 2.0f, 0.9f, 0.15f, 0.1f, 20, 180.0f, 6.0f);
                    mSequence.gameOver = true;
                    mSequence.gameOverTimer = 0.0f;
                    mState = MinigameState::Lost;
                    mShowResultTimer = 0.0f;
                    mFlashAlpha = 0.6f; mFlashTimer = 0.0f; mFlashIsWin = false;
                    mScreenShakeTimer = 0.3f;
                    if (mSoundsLoaded) ma_sound_start(&mLoseSound);
                    return;
                }

                emitParticles(bx + boxSize / 2.0f, by + boxSize / 2.0f, 0.3f, 0.8f, 1.0f, 8, 100.0f, 4.0f);
                mSequence.inputIndex++;

                if (mSequence.inputIndex >= seqLen) {
                    mSequence.level++;
                    if (mSequence.level >= SequenceData::NUM_LEVELS) {
                        mState = MinigameState::Won;
                        mShowResultTimer = 0.0f;
                        mFlashAlpha = 0.6f; mFlashTimer = 0.0f; mFlashIsWin = true;
                        if (mSoundsLoaded) ma_sound_start(&mWinSound);
                        for (int r2 = 0; r2 < 3; r2++) {
                            for (int c2 = 0; c2 < 3; c2++) {
                                float px = startX + c2 * (boxSize + gap) + boxSize / 2.0f;
                                float py = startY + r2 * (boxSize + gap) + boxSize / 2.0f;
                                emitParticles(px, py, 0.2f, 1.0f, 0.4f, 8, 120.0f, 4.0f);
                            }
                        }
                        return;
                    }
                    addFloatingText(Localization::t(TextId::MinigameLevel) + std::to_string(mSequence.level + 1),
                        windowW / 2.0f - 40.0f, startY + totalH + 40.0f, 0.2f, 1.0f, 0.3f);
                    if (mSoundsLoaded) ma_sound_start(&mWinSound);
                    mSequence.inputIndex = 0;
                    mSequence.showingSequence = true;
                    mSequence.waitingInput = false;
                    mSequence.showTimer = 0.0f;
                    int newLen = SequenceData::LEVEL_SIZES[mSequence.level];
                    for (int j = 0; j < SequenceData::MAX_SEQ; j++) {
                        mSequence.sequence[j] = randInt(0, 8);
                        mSequence.playerInput[j] = -1;
                    }
                    mSequence.inputTimeLeft = mSequence.inputTimeMax;
                }
                return;
            }
        }
    }
}

void Minigame::render(Shader& spriteShader, Shader& textShader, TextRenderer& textRenderer, int windowW, int windowH) {
    if (mState == MinigameState::None) return;
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    float shakeX = mScreenShakeX, shakeY = mScreenShakeY;
    glm::mat4 proj = glm::ortho(0.0f, (float)windowW, (float)windowH, 0.0f, -1.0f, 1.0f);
    spriteShader.use();
    spriteShader.setMat4("uProjection", glm::value_ptr(proj));
    float W = (float)windowW, H = (float)windowH;
    renderQuad(spriteShader, shakeX, shakeY, W, H, 0.04f, 0.03f, 0.08f, 0.92f);
    renderQuad(spriteShader, shakeX, shakeY + H * 0.5f, W, H * 0.5f, 0.02f, 0.02f, 0.05f, 0.4f);
    for (int i = 0; i < 6; i++) {
        float fx = (float)windowW * (0.15f + 0.12f * i);
        float fy = (float)windowH * (0.2f + 0.12f * ((i + 2) % 5));
        float fR = 40.0f + 15.0f * std::sin(mTimer * 0.7f + i * 1.1f);
        float fA = 0.04f + 0.02f * std::sin(mTimer * 1.2f + i * 0.9f);
        renderRing(spriteShader, fx + shakeX, fy + shakeY, fR, fR * 0.6f, 0.3f, 0.4f, 0.8f, fA);
    }
    for (int i = 0; i < 4; i++) {
        float dx = (float)windowW * (0.1f + 0.25f * i);
        float dy = (float)windowH * (0.35f + 0.15f * std::sin(mTimer * 0.5f + i * 1.7f));
        float dA = 0.03f + 0.015f * std::sin(mTimer * 0.9f + i * 2.1f);
        renderDiamond(spriteShader, dx + shakeX, dy + shakeY, 30.0f, 0.5f, 0.3f, 0.7f, dA);
    }
    for (int i = 0; i < 3; i++) {
        float tx = (float)windowW * (0.2f + 0.3f * i);
        float ty = (float)windowH * (0.75f + 0.1f * std::sin(mTimer * 0.6f + i * 1.4f));
        float tA = 0.025f + 0.015f * std::sin(mTimer * 1.1f + i * 1.8f);
        renderTriangle(spriteShader, tx + shakeX, ty + shakeY, 25.0f, mTimer * 0.3f + i, 0.6f, 0.4f, 0.3f, tA);
    }
    renderRoundedQuad(spriteShader, shakeX + 8.0f, shakeY + 8.0f, W - 16.0f, H - 16.0f, 12.0f, 0.2f, 0.15f, 0.1f, 0.0f);
    renderRoundedQuad(spriteShader, shakeX + 10.0f, shakeY + 10.0f, W - 20.0f, H - 20.0f, 10.0f, 0.6f, 0.5f, 0.3f, 0.08f);
    renderRoundedQuad(spriteShader, shakeX + 12.0f, shakeY + 12.0f, W - 24.0f, H - 24.0f, 8.0f, 0.2f, 0.15f, 0.1f, 0.0f);

    if (mTutorialActive) {
        renderTutorial(spriteShader, textShader, textRenderer, windowW, windowH);
        renderMouseCursor(spriteShader, 0, 0);
        glEnable(GL_DEPTH_TEST);
        return;
    }
    if (mCountdownActive) {
        renderCountdown(spriteShader, textShader, textRenderer, windowW, windowH);
        renderMouseCursor(spriteShader, 0, 0);
        glEnable(GL_DEPTH_TEST);
        return;
    }

    switch (mType) {
    case MinigameType::QuickTap: renderQuickTap(spriteShader, textShader, textRenderer, windowW, windowH); break;
    case MinigameType::ColorMatch: renderColorMatch(spriteShader, textShader, textRenderer, windowW, windowH); break;
    case MinigameType::Sequence: renderSequence(spriteShader, textShader, textRenderer, windowW, windowH); break;
    }
    if (mFlashAlpha > 0.0f) {
        if (mFlashIsWin) renderQuad(spriteShader, 0, 0, (float)windowW, (float)windowH, 0.1f, 0.9f, 0.2f, mFlashAlpha);
        else renderQuad(spriteShader, 0, 0, (float)windowW, (float)windowH, 0.9f, 0.15f, 0.1f, mFlashAlpha);
    }
    glBlendFunc(GL_ONE, GL_ONE);
    renderParticles(spriteShader);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    renderFloatingTexts(textShader, textRenderer, windowW, windowH);
    if (mState == MinigameState::Won) {
        float pulse = 1.0f + std::sin(mShowResultTimer * 6.0f) * 0.08f;
        std::string winText = Localization::t(TextId::MinigameWin);
        textShader.use(); textShader.setMat4("uProjection", glm::value_ptr(proj));
        textRenderer.renderText(winText, (windowW - textRenderer.getTextSize(winText, 0.7f * pulse).x) / 2.0f,
            windowH / 2.0f + 60.0f, 0.7f * pulse, glm::vec3(0.2f, 1.0f, 0.3f));
    } else if (mState == MinigameState::Lost) {
        float pulse = 1.0f + std::sin(mShowResultTimer * 6.0f) * 0.06f;
        std::string loseText = Localization::t(TextId::MinigameLose);
        textShader.use(); textShader.setMat4("uProjection", glm::value_ptr(proj));
        textRenderer.renderText(loseText, (windowW - textRenderer.getTextSize(loseText, 0.6f * pulse).x) / 2.0f,
            windowH / 2.0f + 60.0f, 0.6f * pulse, glm::vec3(1.0f, 0.3f, 0.3f));
    }
    glEnable(GL_DEPTH_TEST);
}

void Minigame::renderMouseCursor(Shader& spriteShader, float mouseX, float mouseY) {
    glm::mat4 proj = glm::ortho(0.0f, 1.0f, 1.0f, 0.0f, -1.0f, 1.0f);
    spriteShader.use();
    spriteShader.setMat4("uProjection", glm::value_ptr(proj));
    renderCircle(spriteShader, 0.005f, 0.005f, 0.008f, 1.0f, 1.0f, 1.0f, 0.6f);
}

void Minigame::renderTutorial(Shader& spriteShader, Shader& textShader, TextRenderer& textRenderer, int windowW, int windowH) {
    glm::mat4 proj = glm::ortho(0.0f, (float)windowW, (float)windowH, 0.0f, -1.0f, 1.0f);
    float W = (float)windowW, H = (float)windowH;
    spriteShader.use();
    spriteShader.setMat4("uProjection", glm::value_ptr(proj));
    renderRoundedQuad(spriteShader, W * 0.12f, H * 0.08f, W * 0.76f, H * 0.84f, 16.0f, 0.06f, 0.05f, 0.12f, 0.95f);
    renderRoundedQuad(spriteShader, W * 0.13f, H * 0.09f, W * 0.74f, H * 0.82f, 14.0f, 0.3f, 0.25f, 0.15f, 0.12f);

    textShader.use();
    textShader.setMat4("uProjection", glm::value_ptr(proj));
    std::string title;
    std::string instruction;
    float titleR, titleG, titleB;
    switch (mType) {
    case MinigameType::QuickTap:
        title = Localization::t(TextId::MinigameQuickTapTitle);
        instruction = Localization::t(TextId::MinigameTutorialQuickTap);
        titleR = 0.2f; titleG = 0.95f; titleB = 1.0f;
        renderStar(spriteShader, W * 0.5f, H * 0.42f, 55.0f, 22.0f, mTimer * 0.8f, 0.3f, 0.9f, 1.0f, 0.8f, 5);
        renderRing(spriteShader, W * 0.5f, H * 0.42f, 70.0f, 55.0f, 0.2f, 0.8f, 1.0f, 0.15f + 0.1f * std::sin(mTimer * 3.0f));
        break;
    case MinigameType::ColorMatch:
        title = Localization::t(TextId::MinigameColorMatchTitle);
        instruction = Localization::t(TextId::MinigameTutorialColorMatch);
        titleR = 1.0f; titleG = 0.85f; titleB = 0.2f;
        renderRoundedQuad(spriteShader, W * 0.42f, H * 0.35f, 70.0f, 70.0f, 10.0f, 0.8f, 0.2f, 0.2f, 0.8f);
        renderTriangle(spriteShader, W * 0.5f, H * 0.35f + 35.0f, 18.0f, 3.14159f, 1.0f, 1.0f, 1.0f, 0.5f);
        for (int i = 0; i < 3; i++) {
            float cx = W * 0.35f + i * W * 0.15f;
            float colors[3][3] = {{0.2f, 0.6f, 0.9f}, {0.9f, 0.3f, 0.2f}, {0.2f, 0.8f, 0.3f}};
            renderRoundedQuad(spriteShader, cx - 30.0f, H * 0.55f, 60.0f, 60.0f, 8.0f, colors[i][0], colors[i][1], colors[i][2], 0.85f);
        }
        renderTriangle(spriteShader, W * 0.5f, H * 0.5f, 14.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.7f);
        break;
    case MinigameType::Sequence:
        title = Localization::t(TextId::MinigameSequenceTitle);
        instruction = Localization::t(TextId::MinigameTutorialSequence);
        titleR = 0.5f; titleG = 0.85f; titleB = 1.0f;
        for (int r = 0; r < 3; r++) {
            for (int c = 0; c < 3; c++) {
                float cx = W * 0.4f + c * 50.0f;
                float cy = H * 0.35f + r * 50.0f;
                float glow = (r == 0 && c == 0) ? (0.5f + 0.3f * std::sin(mTimer * 4.0f)) : 0.15f;
                renderRoundedQuad(spriteShader, cx - 18.0f, cy - 18.0f, 36.0f, 36.0f, 6.0f, 0.15f, 0.15f, 0.25f, 0.8f);
                renderRoundedQuad(spriteShader, cx - 14.0f, cy - 14.0f, 28.0f, 28.0f, 4.0f, 0.3f, 0.5f, 0.8f, glow);
            }
        }
        break;
    }
    textRenderer.renderText(title, (W - textRenderer.getTextSize(title, 0.6f).x) / 2.0f, H * 0.18f, 0.6f, glm::vec3(titleR, titleG, titleB));

    std::string line1, line2;
    size_t nl = instruction.find('\n');
    if (nl != std::string::npos) {
        line1 = instruction.substr(0, nl);
        line2 = instruction.substr(nl + 1);
    } else {
        line1 = instruction;
    }
    textRenderer.renderText(line1, (W - textRenderer.getTextSize(line1, 0.36f).x) / 2.0f, H * 0.62f, 0.36f, glm::vec3(0.85f, 0.88f, 0.95f));
    if (!line2.empty())
        textRenderer.renderText(line2, (W - textRenderer.getTextSize(line2, 0.30f).x) / 2.0f, H * 0.68f, 0.30f, glm::vec3(0.7f, 0.75f, 0.85f));

    float pulse = 1.0f + std::sin(mTimer * 3.5f) * 0.12f;
    std::string startText = Localization::t(TextId::MinigameTapToStart);
    textRenderer.renderText(startText, (W - textRenderer.getTextSize(startText, 0.42f * pulse).x) / 2.0f, H * 0.80f, 0.42f * pulse, glm::vec3(1.0f, 0.9f, 0.3f));

    std::string escText = Localization::t(TextId::MinigameExitHint);
    textRenderer.renderText(escText, W - 140.0f, H - 30.0f, 0.25f, glm::vec3(0.5f, 0.5f, 0.6f));
}

void Minigame::renderCountdown(Shader& spriteShader, Shader& textShader, TextRenderer& textRenderer, int windowW, int windowH) {
    glm::mat4 proj = glm::ortho(0.0f, (float)windowW, (float)windowH, 0.0f, -1.0f, 1.0f);
    float W = (float)windowW, H = (float)windowH;
    spriteShader.use();
    spriteShader.setMat4("uProjection", glm::value_ptr(proj));
    renderRoundedQuad(spriteShader, 0, 0, W, H, 0.0f, 0.0f, 0.0f, 0.0f, 0.5f);

    textShader.use();
    textShader.setMat4("uProjection", glm::value_ptr(proj));
    float t = mCountdownTimer;
    float scale = 1.5f + std::sin(t * 12.0f) * 0.3f;
    float alpha = 0.7f + 0.3f * std::sin(t * 8.0f);
    float r, g, b;
    std::string numText;
    if (mCountdownValue > 0) {
        numText = std::to_string(mCountdownValue);
        if (mCountdownValue == 3) { r = 0.9f; g = 0.3f; b = 0.2f; }
        else if (mCountdownValue == 2) { r = 0.9f; g = 0.7f; b = 0.1f; }
        else { r = 0.2f; g = 0.9f; b = 0.3f; }
    } else {
        numText = Localization::t(TextId::MinigameCountdownGo);
        r = 0.1f; g = 1.0f; b = 0.4f;
        scale = 2.0f + std::sin(t * 20.0f) * 0.4f;
    }
    textRenderer.renderText(numText, (W - textRenderer.getTextSize(numText, scale).x) / 2.0f, H * 0.45f, scale, glm::vec3(r, g, b), alpha);

    for (int i = 0; i < 8; i++) {
        float angle = (float)i / 8.0f * 6.28318f + mTimer * 2.0f;
        float dist = 120.0f + 30.0f * std::sin(mTimer * 3.0f + i);
        float px = W * 0.5f + std::cos(angle) * dist;
        float py = H * 0.45f + std::sin(angle) * dist * 0.5f;
        float pA = 0.15f + 0.1f * std::sin(mTimer * 4.0f + i * 1.5f);
        renderCircle(spriteShader, px, py, 6.0f, r, g, b, pA);
    }

    std::string escText = Localization::t(TextId::MinigameExitHint);
    textShader.use(); textShader.setMat4("uProjection", glm::value_ptr(proj));
    textRenderer.renderText(escText, W - 140.0f, H - 30.0f, 0.25f, glm::vec3(0.5f, 0.5f, 0.6f));
}

void Minigame::renderQuickTap(Shader& spriteShader, Shader& textShader, TextRenderer& textRenderer, int windowW, int windowH) {
    glm::mat4 proj = glm::ortho(0.0f, (float)windowW, (float)windowH, 0.0f, -1.0f, 1.0f);
    textShader.use(); textShader.setMat4("uProjection", glm::value_ptr(proj));
    std::string title = Localization::t(TextId::MinigameQuickTapTitle);
    textRenderer.renderText(title, (windowW - textRenderer.getTextSize(title, 0.55f).x) / 2.0f, 40.0f, 0.55f, glm::vec3(0.2f, 0.95f, 1.0f));
    std::string instrText = Localization::t(TextId::MinigameQuickTapInstruction);
    textRenderer.renderText(instrText, (windowW - textRenderer.getTextSize(instrText, 0.32f).x) / 2.0f, 80.0f, 0.32f, glm::vec3(0.6f, 0.7f, 0.9f));

    spriteShader.use();
    spriteShader.setMat4("uProjection", glm::value_ptr(proj));
    float barW = 400.0f, barH = 16.0f;
    float barX = (windowW - barW) / 2.0f, barY = 105.0f;
    renderRoundedQuad(spriteShader, barX - 2, barY - 2, barW + 4, barH + 4, 8.0f, 0.0f, 0.0f, 0.0f, 0.5f);
    renderRoundedQuad(spriteShader, barX, barY, barW, barH, 6.0f, 0.12f, 0.14f, 0.2f, 0.9f);
    float prog = (float)mQuickTap.hits / (float)QuickTapData::REQUIRED_HITS;
    float pR = 0.1f + prog * 0.1f;
    float pG = 0.7f + prog * 0.25f;
    float pB = 0.3f + prog * 0.1f;
    renderRoundedQuad(spriteShader, barX, barY, barW * std::min(prog, 1.0f), barH, 6.0f, pR, pG, pB, 0.95f);
    renderRoundedQuad(spriteShader, barX + 2, barY + 2, barW * std::min(prog, 1.0f) - 4, barH * 0.4f, 3.0f, 1.0f, 1.0f, 1.0f, 0.15f);
    std::string hitsText = Localization::t(TextId::MinigameQuickTapHits) + std::to_string(mQuickTap.hits) + "/" + std::to_string(QuickTapData::REQUIRED_HITS);
    textShader.use(); textShader.setMat4("uProjection", glm::value_ptr(proj));
    textRenderer.renderText(hitsText, (windowW - textRenderer.getTextSize(hitsText, 0.35f).x) / 2.0f, barY + barH + 18.0f, 0.35f, glm::vec3(0.3f, 1.0f, 0.5f));

    spriteShader.use();
    spriteShader.setMat4("uProjection", glm::value_ptr(proj));
    for (int i = 0; i < QuickTapData::MAX_TARGETS; i++) {
        if (!mQuickTap.targets[i].alive) continue;
        float t = mQuickTap.targets[i].life / mQuickTap.targets[i].maxLife;
        float alpha = (t < 0.7f) ? 1.0f : 1.0f - (t - 0.7f) / 0.3f;
        float scale = mQuickTap.targets[i].scale * (0.85f + 0.15f * std::sin(mQuickTap.targets[i].glowPhase));
        float cx = mQuickTap.targets[i].x;
        float cy = mQuickTap.targets[i].y;
        int shapeType = i % 6;
        float cR = 0.0f, cG = 0.0f, cB = 0.0f;
        switch (shapeType) {
            case 0: cR = 0.1f; cG = 0.9f; cB = 1.0f; break;
            case 1: cR = 1.0f; cG = 0.2f; cB = 0.8f; break;
            case 2: cR = 0.2f; cG = 1.0f; cB = 0.4f; break;
            case 3: cR = 1.0f; cG = 0.9f; cB = 0.1f; break;
            case 4: cR = 1.0f; cG = 0.5f; cB = 0.1f; break;
            case 5: cR = 0.9f; cG = 0.2f; cB = 0.5f; break;
        }
        float baseR = 24.0f * scale;
        renderCircle(spriteShader, cx, cy, baseR + 10.0f, cR, cG, cB, 0.08f * alpha);
        renderCircle(spriteShader, cx, cy, baseR + 5.0f, cR, cG, cB, 0.12f * alpha);
        float pulseRing = baseR + 3.0f + 3.0f * std::sin(mTimer * 5.0f + i * 1.1f);
        renderRing(spriteShader, cx, cy, pulseRing, pulseRing - 2.0f, cR, cG, cB, 0.3f * alpha);
        switch (shapeType) {
            case 0: renderStar(spriteShader, cx, cy, baseR * 0.7f, baseR * 0.3f, mTimer * 1.5f + i, 1.0f, 1.0f, 1.0f, 0.9f * alpha, 5); break;
            case 1: renderDiamond(spriteShader, cx, cy, baseR * 1.0f, 1.0f, 1.0f, 1.0f, 0.85f * alpha); break;
            case 2: renderTriangle(spriteShader, cx, cy, baseR * 1.1f, mTimer * 0.8f + i, 1.0f, 1.0f, 1.0f, 0.85f * alpha); break;
            case 3: renderHexagon(spriteShader, cx, cy, baseR * 0.65f, 0.0f, 1.0f, 1.0f, 1.0f, 0.85f * alpha); break;
            case 4: renderCircle(spriteShader, cx, cy, baseR * 0.55f, 1.0f, 1.0f, 1.0f, 0.9f * alpha); break;
            case 5: {
                float cs = baseR * 0.45f;
                renderQuad(spriteShader, cx - cs, cy - cs * 0.25f, cs * 2, cs * 0.5f, 1.0f, 1.0f, 1.0f, 0.85f * alpha);
                renderQuad(spriteShader, cx - cs * 0.25f, cy - cs, cs * 0.5f, cs * 2, 1.0f, 1.0f, 1.0f, 0.85f * alpha);
                break;
            }
        }
    }
    textShader.use(); textShader.setMat4("uProjection", glm::value_ptr(proj));
    std::string escText = Localization::t(TextId::MinigameExitHint);
    textRenderer.renderText(escText, (float)windowW - 140.0f, (float)windowH - 30.0f, 0.25f, glm::vec3(0.5f, 0.5f, 0.6f));
    std::string targetsText = Localization::t(TextId::MinigameTargets) + std::to_string(mQuickTap.totalSpawned) + "/" + std::to_string(QuickTapData::MAX_TARGETS);
    textRenderer.renderText(targetsText, 30.0f, (float)windowH - 30.0f, 0.30f, glm::vec3(0.6f, 0.7f, 0.9f));
}

void Minigame::renderColorMatch(Shader& spriteShader, Shader& textShader, TextRenderer& textRenderer, int windowW, int windowH) {
    glm::mat4 proj = glm::ortho(0.0f, (float)windowW, (float)windowH, 0.0f, -1.0f, 1.0f);
    textShader.use(); textShader.setMat4("uProjection", glm::value_ptr(proj));
    std::string title = Localization::t(TextId::MinigameColorMatchTitle);
    textRenderer.renderText(title, (windowW - textRenderer.getTextSize(title, 0.55f).x) / 2.0f, 35.0f, 0.55f, glm::vec3(1.0f, 0.85f, 0.2f));
    std::string instrText = Localization::t(TextId::MinigameColorMatchInstruction);
    textRenderer.renderText(instrText, (windowW - textRenderer.getTextSize(instrText, 0.30f).x) / 2.0f, 72.0f, 0.30f, glm::vec3(0.65f, 0.7f, 0.85f));
    std::string roundText = Localization::t(TextId::MinigameColorMatchRound) + std::to_string(mColorMatch.round + 1) + "/" + std::to_string(ColorMatchData::TOTAL_ROUNDS);
    textRenderer.renderText(roundText, 30.0f, 28.0f, 0.4f, glm::vec3(0.8f));
    std::string scoreText = Localization::t(TextId::MinigameColorMatchScore) + std::to_string(mColorMatch.score);
    textRenderer.renderText(scoreText, windowW - 200.0f, 28.0f, 0.4f, glm::vec3(1.0f, 0.85f, 0.1f));

    spriteShader.use();
    spriteShader.setMat4("uProjection", glm::value_ptr(proj));
    float targetSize = 80.0f;
    float targetX = (windowW - targetSize) / 2.0f;
    float targetY = 100.0f;
    float pulseTarget = 1.0f + std::sin(mTimer * 3.0f) * 0.06f;
    float tRingR = targetSize * 0.55f * pulseTarget;
    renderRing(spriteShader, targetX + targetSize / 2.0f, targetY + targetSize / 2.0f, tRingR + 8.0f, tRingR, mColorMatch.targetR, mColorMatch.targetG, mColorMatch.targetB, 0.2f);
    renderRing(spriteShader, targetX + targetSize / 2.0f, targetY + targetSize / 2.0f, tRingR + 4.0f, tRingR - 2.0f, mColorMatch.targetR, mColorMatch.targetG, mColorMatch.targetB, 0.35f);
    renderRoundedQuad(spriteShader, targetX - 6, targetY - 6, targetSize + 12, targetSize + 12, 14.0f, 0.12f, 0.1f, 0.18f, 0.95f);
    renderRoundedQuad(spriteShader, targetX - 2, targetY - 2, targetSize + 4, targetSize + 4, 10.0f, 0.8f, 0.7f, 0.3f, 0.5f);
    renderRoundedQuad(spriteShader, targetX, targetY, targetSize, targetSize, 8.0f, mColorMatch.targetR, mColorMatch.targetG, mColorMatch.targetB, 1.0f);
    renderRoundedQuad(spriteShader, targetX + 5, targetY + 5, targetSize * 0.45f, targetSize * 0.45f, 4.0f,
        std::min(1.0f, mColorMatch.targetR + 0.25f), std::min(1.0f, mColorMatch.targetG + 0.25f), std::min(1.0f, mColorMatch.targetB + 0.25f), 0.25f);
    renderStar(spriteShader, targetX + targetSize / 2.0f, targetY + targetSize / 2.0f, targetSize * 0.22f, targetSize * 0.1f, mTimer * 0.8f, 1.0f, 1.0f, 1.0f, 0.5f, 5);

    float boxSize = 90.0f;
    float gap = 16.0f;
    float cols = 4.0f;
    float rows = 2.0f;
    float totalW = cols * boxSize + (cols - 1) * gap;
    float totalH = rows * boxSize + (rows - 1) * gap;
    float startX = (windowW - totalW) / 2.0f;
    float startY = (windowH - totalH) / 2.0f + 50.0f;
    float shakeX = (mColorMatch.shakeTimer > 0.0f) ? std::sin(mTimer * 40.0f) * 5.0f : 0.0f;

    for (int i = 0; i < ColorMatchData::NUM_OPTIONS; i++) {
        int col = i % (int)cols;
        int row = i / (int)cols;
        float bx = startX + col * (boxSize + gap) + shakeX;
        float by = startY + row * (boxSize + gap);
        float bcx = bx + boxSize / 2.0f, bcy = by + boxSize / 2.0f;
        renderRoundedQuad(spriteShader, bx - 3, by - 3, boxSize + 6, boxSize + 6, 12.0f, 0.05f, 0.05f, 0.08f, 0.95f);
        renderRoundedQuad(spriteShader, bx - 1, by - 1, boxSize + 2, boxSize + 2, 10.0f, 0.25f, 0.2f, 0.15f, 0.8f);
        renderRoundedQuad(spriteShader, bx, by, boxSize, boxSize, 8.0f, mColorMatch.optionR[i], mColorMatch.optionG[i], mColorMatch.optionB[i], 1.0f);
        renderRoundedQuad(spriteShader, bx + 6, by + 6, boxSize * 0.4f, boxSize * 0.4f, 3.0f,
            std::min(1.0f, mColorMatch.optionR[i] + 0.2f), std::min(1.0f, mColorMatch.optionG[i] + 0.2f), std::min(1.0f, mColorMatch.optionB[i] + 0.2f), 0.2f);
        float symAlpha = 0.85f;
        float symSize = boxSize * 0.18f;
        switch (i) {
            case 0: renderStar(spriteShader, bcx, bcy, symSize, symSize * 0.4f, 0.0f, 1.0f, 1.0f, 1.0f, symAlpha, 5); break;
            case 1: renderDiamond(spriteShader, bcx, bcy, symSize * 1.6f, 1.0f, 1.0f, 1.0f, symAlpha); break;
            case 2: renderTriangle(spriteShader, bcx, bcy, symSize * 1.8f, 0.0f, 1.0f, 1.0f, 1.0f, symAlpha); break;
            case 3: renderHexagon(spriteShader, bcx, bcy, symSize, 0.0f, 1.0f, 1.0f, 1.0f, symAlpha); break;
            case 4: renderCircle(spriteShader, bcx, bcy, symSize * 0.8f, 1.0f, 1.0f, 1.0f, symAlpha); break;
            case 5: {
                float cs = symSize * 0.7f;
                renderQuad(spriteShader, bcx - cs, bcy - cs * 0.3f, cs * 2, cs * 0.6f, 1.0f, 1.0f, 1.0f, symAlpha);
                renderQuad(spriteShader, bcx - cs * 0.3f, bcy - cs, cs * 0.6f, cs * 2, 1.0f, 1.0f, 1.0f, symAlpha);
                break;
            }
            case 6: renderRoundedQuad(spriteShader, bcx - symSize * 0.6f, bcy - symSize * 0.6f, symSize * 1.2f, symSize * 1.2f, 3.0f, 1.0f, 1.0f, 1.0f, symAlpha); break;
            case 7: renderHexagon(spriteShader, bcx, bcy, symSize * 0.7f, 0.0f, 1.0f, 1.0f, 1.0f, symAlpha); break;
        }
    }

    float timeRatio = 1.0f - (mColorMatch.roundTimer / mColorMatch.maxRoundTime);
    float timerBarW = 360.0f, timerBarH = 12.0f;
    float timerBarX = (windowW - timerBarW) / 2.0f, timerBarY = startY + totalH + 28.0f;
    renderRoundedQuad(spriteShader, timerBarX - 2, timerBarY - 2, timerBarW + 4, timerBarH + 4, 6.0f, 0.0f, 0.0f, 0.0f, 0.5f);
    renderRoundedQuad(spriteShader, timerBarX, timerBarY, timerBarW, timerBarH, 5.0f, 0.12f, 0.12f, 0.18f, 0.9f);
    float tR = timeRatio > 0.3f ? 0.15f : 0.95f;
    float tG = timeRatio > 0.3f ? 0.65f : 0.2f;
    renderRoundedQuad(spriteShader, timerBarX, timerBarY, timerBarW * timeRatio, timerBarH, 5.0f, tR, tG, 0.1f, 0.95f);
    renderRoundedQuad(spriteShader, timerBarX + 2, timerBarY + 2, timerBarW * timeRatio - 4, timerBarH * 0.35f, 3.0f, 1.0f, 1.0f, 1.0f, 0.12f);
    float secsLeft = mColorMatch.maxRoundTime - mColorMatch.roundTimer;
    if (secsLeft < 0.0f) secsLeft = 0.0f;
    char secBuf[16];
    std::snprintf(secBuf, sizeof(secBuf), "%.1fs", secsLeft);
    std::string secStr(secBuf);
    float secColorR = secsLeft > 1.5f ? 0.7f : 1.0f;
    float secColorG = secsLeft > 1.5f ? 0.8f : 0.2f;
    textShader.use(); textShader.setMat4("uProjection", glm::value_ptr(proj));
    textRenderer.renderText(secStr, (windowW - textRenderer.getTextSize(secStr, 0.32f).x) / 2.0f, timerBarY + timerBarH + 12.0f, 0.32f, glm::vec3(secColorR, secColorG, 0.1f));
    std::string escText = Localization::t(TextId::MinigameExitHint);
    textRenderer.renderText(escText, (float)windowW - 140.0f, (float)windowH - 30.0f, 0.25f, glm::vec3(0.5f, 0.5f, 0.6f));
    textShader.use(); textShader.setMat4("uProjection", glm::value_ptr(proj));
    std::string findColorText = Localization::t(TextId::MinigameFindColor);
    float findPulse = 1.0f + std::sin(mTimer * 4.0f) * 0.08f;
    textRenderer.renderText(findColorText, (windowW - textRenderer.getTextSize(findColorText, 0.38f * findPulse).x) / 2.0f, targetY - 18.0f, 0.38f * findPulse, glm::vec3(1.0f, 0.95f, 0.3f));
    float arrowY = targetY + targetSize + 5.0f;
    renderTriangle(spriteShader, targetX + targetSize / 2.0f, arrowY, 10.0f, 3.14159f, 1.0f, 0.9f, 0.2f, 0.7f);
}

void Minigame::renderSequence(Shader& spriteShader, Shader& textShader, TextRenderer& textRenderer, int windowW, int windowH) {
    glm::mat4 proj = glm::ortho(0.0f, (float)windowW, (float)windowH, 0.0f, -1.0f, 1.0f);
    textShader.use(); textShader.setMat4("uProjection", glm::value_ptr(proj));
    std::string title = Localization::t(TextId::MinigameSequenceTitle);
    textRenderer.renderText(title, (windowW - textRenderer.getTextSize(title, 0.55f).x) / 2.0f, 35.0f, 0.55f, glm::vec3(0.5f, 0.8f, 1.0f));
    std::string levelText = Localization::t(TextId::MinigameLevel) + std::to_string(mSequence.level + 1) + "/" + std::to_string(SequenceData::NUM_LEVELS);
    textRenderer.renderText(levelText, 30.0f, 28.0f, 0.4f, glm::vec3(0.7f, 0.9f, 1.0f));

    spriteShader.use();
    spriteShader.setMat4("uProjection", glm::value_ptr(proj));
    float progBarW = 220.0f, progBarH = 10.0f;
    float progBarX = (windowW - progBarW) / 2.0f, progBarY = 78.0f;
    renderRoundedQuad(spriteShader, progBarX - 1, progBarY - 1, progBarW + 2, progBarH + 2, 5.0f, 0.0f, 0.0f, 0.0f, 0.5f);
    renderRoundedQuad(spriteShader, progBarX, progBarY, progBarW, progBarH, 4.0f, 0.12f, 0.12f, 0.18f, 0.9f);
    float levelProg = (float)mSequence.level / (float)SequenceData::NUM_LEVELS;
    renderRoundedQuad(spriteShader, progBarX, progBarY, progBarW * levelProg, progBarH, 4.0f, 0.2f, 0.65f, 1.0f, 0.9f);
    renderRoundedQuad(spriteShader, progBarX + 2, progBarY + 2, progBarW * levelProg - 4, progBarH * 0.35f, 2.0f, 1.0f, 1.0f, 1.0f, 0.12f);

    float boxSize = 110.0f;
    float gap = 18.0f;
    float totalW = 3 * boxSize + 2 * gap;
    float totalH = 3 * boxSize + 2 * gap;
    float startX = (windowW - totalW) / 2.0f;
    float startY = (windowH - totalH) / 2.0f + 10.0f;

    float colorR[9] = { 0.95f, 0.7f, 0.25f, 0.2f, 0.25f, 0.18f, 0.55f, 0.35f, 0.75f };
    float colorG[9] = { 0.25f, 0.18f, 0.55f, 0.4f, 0.8f, 0.65f, 0.35f, 0.65f, 0.45f };
    float colorB[9] = { 0.25f, 0.35f, 0.95f, 0.8f, 0.35f, 0.25f, 0.85f, 0.25f, 0.95f };

    for (int row = 0; row < 3; row++) {
        for (int col = 0; col < 3; col++) {
            int i = row * 3 + col;
            float bx = startX + col * (boxSize + gap);
            float by = startY + row * (boxSize + gap);
            float bcx = bx + boxSize / 2.0f, bcy = by + boxSize / 2.0f;
            float glow = mSequence.cellGlow[i];
            float r = colorR[i] * 0.25f, g = colorG[i] * 0.25f, b = colorB[i] * 0.25f;
            float borderR = 0.15f, borderG = 0.15f, borderB = 0.22f;
            float shapeAlpha = 0.3f;
            if (mSequence.flashCell == i) {
                r = colorR[i]; g = colorG[i]; b = colorB[i];
                borderR = colorR[i] * 0.8f; borderG = colorG[i] * 0.8f; borderB = colorB[i] * 0.8f;
                shapeAlpha = 1.0f;
                float expandR = boxSize * 0.48f + 10.0f + 4.0f * std::sin(mTimer * 10.0f);
                renderRing(spriteShader, bcx, bcy, expandR, expandR - 3.0f, colorR[i], colorG[i], colorB[i], 0.3f);
                renderRing(spriteShader, bcx, bcy, expandR - 6.0f, expandR - 10.0f, colorR[i], colorG[i], colorB[i], 0.2f);
            }
            if (glow > 0.1f && mSequence.flashCell != i) {
                float blend = 0.25f + 0.75f * glow;
                r = colorR[i] * blend;
                g = colorG[i] * blend;
                b = colorB[i] * blend;
                borderR = colorR[i] * 0.6f * glow;
                borderG = colorG[i] * 0.6f * glow;
                borderB = colorB[i] * 0.6f * glow;
                shapeAlpha = 0.3f + 0.7f * glow;
            }
            if (mSequence.gameOver) {
                float flash = 0.5f + 0.5f * std::sin(mSequence.gameOverTimer * 15.0f + i * 1.5f);
                r = r * (1.0f - flash) + 0.95f * flash;
                g = g * (1.0f - flash) * 0.3f;
                b = b * (1.0f - flash) * 0.3f;
                borderR = 0.9f * flash;
                borderG = 0.15f * flash;
                borderB = 0.1f * flash;
                shapeAlpha = 0.3f + 0.7f * flash;
            }
            renderRoundedQuad(spriteShader, bx - 3, by - 3, boxSize + 6, boxSize + 6, 14.0f, borderR, borderG, borderB, 0.7f);
            renderRoundedQuad(spriteShader, bx, by, boxSize, boxSize, 10.0f, r, g, b, 1.0f);
            renderRoundedQuad(spriteShader, bx + 4, by + 4, boxSize - 8, (boxSize - 8) * 0.35f, 5.0f,
                std::min(1.0f, r + 0.15f), std::min(1.0f, g + 0.15f), std::min(1.0f, b + 0.15f), 0.18f);
            float symSize = boxSize * 0.22f;
            switch (i) {
                case 0: renderStar(spriteShader, bcx, bcy, symSize, symSize * 0.4f, 0.0f, 1.0f, 1.0f, 1.0f, shapeAlpha, 5); break;
                case 1: renderDiamond(spriteShader, bcx, bcy, symSize * 1.6f, 1.0f, 1.0f, 1.0f, shapeAlpha); break;
                case 2: renderTriangle(spriteShader, bcx, bcy, symSize * 1.8f, 0.0f, 1.0f, 1.0f, 1.0f, shapeAlpha); break;
                case 3: renderHexagon(spriteShader, bcx, bcy, symSize, 0.0f, 1.0f, 1.0f, 1.0f, shapeAlpha); break;
                case 4: renderCircle(spriteShader, bcx, bcy, symSize * 0.8f, 1.0f, 1.0f, 1.0f, shapeAlpha); break;
                case 5: {
                    float cs = symSize * 0.7f;
                    renderQuad(spriteShader, bcx - cs, bcy - cs * 0.25f, cs * 2, cs * 0.5f, 1.0f, 1.0f, 1.0f, shapeAlpha);
                    renderQuad(spriteShader, bcx - cs * 0.25f, bcy - cs, cs * 0.5f, cs * 2, 1.0f, 1.0f, 1.0f, shapeAlpha);
                    break;
                }
                case 6: renderRoundedQuad(spriteShader, bcx - symSize * 0.55f, bcy - symSize * 0.55f, symSize * 1.1f, symSize * 1.1f, 3.0f, 1.0f, 1.0f, 1.0f, shapeAlpha); break;
                case 7: renderHexagon(spriteShader, bcx, bcy, symSize * 0.75f, 0.0f, 1.0f, 1.0f, 1.0f, shapeAlpha); break;
                case 8: renderTriangle(spriteShader, bcx, bcy - symSize * 0.1f, symSize * 0.9f, 3.14159f, 1.0f, 1.0f, 1.0f, shapeAlpha); break;
            }
        }
    }

    textShader.use(); textShader.setMat4("uProjection", glm::value_ptr(proj));
    if (mSequence.showingSequence) {
        std::string observeText = Localization::t(TextId::MinigameSequenceObserve);
        float bannerPulse = 1.0f + std::sin(mTimer * 3.0f) * 0.06f;
        renderRoundedQuad(spriteShader, (windowW - textRenderer.getTextSize(observeText, 0.50f * bannerPulse).x) / 2.0f - 30.0f, startY - 85.0f, textRenderer.getTextSize(observeText, 0.50f * bannerPulse).x + 60.0f, 45.0f, 10.0f, 0.1f, 0.25f, 0.5f, 0.85f);
        textRenderer.renderText(observeText, (windowW - textRenderer.getTextSize(observeText, 0.50f * bannerPulse).x) / 2.0f, startY - 72.0f, 0.50f * bannerPulse, glm::vec3(0.5f, 0.85f, 1.0f));
        if (mSequence.showIndex >= 0) {
            int seqLen = SequenceData::LEVEL_SIZES[mSequence.level];
            for (int s = 0; s <= mSequence.showIndex && s < seqLen; s++) {
                int cellIdx = mSequence.sequence[s];
                int row = cellIdx / 3, col = cellIdx % 3;
                float nx = startX + col * (boxSize + gap) + boxSize / 2.0f - 8.0f;
                float ny = startY + row * (boxSize + gap) - 20.0f;
                std::string numStr = std::to_string(s + 1);
                textRenderer.renderText(numStr, nx, ny, 0.30f, glm::vec3(1.0f, 1.0f, 0.3f));
            }
            std::string progressText = std::to_string(mSequence.showIndex + 1) + "/" + std::to_string(seqLen);
            textRenderer.renderText(progressText, (windowW - textRenderer.getTextSize(progressText, 0.35f).x) / 2.0f, startY + totalH + 25.0f, 0.35f, glm::vec3(0.7f, 0.9f, 1.0f));
        }
    } else if (!mSequence.gameOver) {
        std::string repeatText = Localization::t(TextId::MinigameYourTurn);
        float turnPulse = 1.0f + std::sin(mTimer * 4.0f) * 0.08f;
        renderRoundedQuad(spriteShader, (windowW - textRenderer.getTextSize(repeatText, 0.50f * turnPulse).x) / 2.0f - 30.0f, startY - 85.0f, textRenderer.getTextSize(repeatText, 0.50f * turnPulse).x + 60.0f, 45.0f, 10.0f, 0.1f, 0.4f, 0.2f, 0.85f);
        textRenderer.renderText(repeatText, (windowW - textRenderer.getTextSize(repeatText, 0.50f * turnPulse).x) / 2.0f, startY - 72.0f, 0.50f * turnPulse, glm::vec3(0.3f, 1.0f, 0.5f));
        int seqLen = SequenceData::LEVEL_SIZES[mSequence.level];
        for (int s = 0; s < mSequence.inputIndex && s < seqLen; s++) {
            int cellIdx = mSequence.sequence[s];
            int row = cellIdx / 3, col = cellIdx % 3;
            float nx = startX + col * (boxSize + gap) + boxSize / 2.0f - 5.0f;
            float ny = startY + row * (boxSize + gap) + boxSize + 5.0f;
            textRenderer.renderText("+", nx, ny, 0.28f, glm::vec3(0.3f, 1.0f, 0.4f));
        }
        std::string inputProgress = std::to_string(mSequence.inputIndex) + "/" + std::to_string(seqLen);
        textRenderer.renderText(inputProgress, (windowW - textRenderer.getTextSize(inputProgress, 0.35f).x) / 2.0f, startY + totalH + 25.0f, 0.35f, glm::vec3(0.7f, 0.9f, 1.0f));
        float timerRatio = mSequence.inputTimeLeft / mSequence.inputTimeMax;
        float timerBarW = 260.0f, timerBarH = 10.0f;
        float timerBarX = (windowW - timerBarW) / 2.0f, timerBarY = startY + totalH + 48.0f;
        renderRoundedQuad(spriteShader, timerBarX - 1, timerBarY - 1, timerBarW + 2, timerBarH + 2, 5.0f, 0.0f, 0.0f, 0.0f, 0.5f);
        renderRoundedQuad(spriteShader, timerBarX, timerBarY, timerBarW, timerBarH, 4.0f, 0.12f, 0.12f, 0.18f, 0.8f);
        float tR = timerRatio > 0.3f ? 0.15f : 0.95f;
        float tG = timerRatio > 0.3f ? 0.65f : 0.2f;
        renderRoundedQuad(spriteShader, timerBarX, timerBarY, timerBarW * timerRatio, timerBarH, 4.0f, tR, tG, 0.1f, 0.9f);
        renderRoundedQuad(spriteShader, timerBarX + 2, timerBarY + 2, timerBarW * timerRatio - 4, timerBarH * 0.35f, 2.0f, 1.0f, 1.0f, 1.0f, 0.12f);
        std::string escText = Localization::t(TextId::MinigameExitHint);
        textRenderer.renderText(escText, (float)windowW - 140.0f, (float)windowH - 30.0f, 0.25f, glm::vec3(0.5f, 0.5f, 0.6f));
    } else {
        std::string gameOverText = Localization::t(TextId::MinigameGameOver);
        textRenderer.renderText(gameOverText, (windowW - textRenderer.getTextSize(gameOverText, 0.55f).x) / 2.0f, startY - 65.0f, 0.55f, glm::vec3(1.0f, 0.3f, 0.3f));
        if (mSequence.gameOverTimer >= 1.5f) {
            std::string retryText = Localization::t(TextId::MinigameRetry);
            float blink = 0.5f + 0.5f * std::sin(mTimer * 5.0f);
            textRenderer.renderText(retryText, (windowW - textRenderer.getTextSize(retryText, 0.38f).x) / 2.0f, startY + totalH + 55.0f, 0.38f, glm::vec3(0.9f, 0.85f, 0.7f), blink);
        }
        std::string escText = Localization::t(TextId::MinigameExitHint);
        textRenderer.renderText(escText, (float)windowW - 140.0f, (float)windowH - 30.0f, 0.25f, glm::vec3(0.5f, 0.5f, 0.6f));
    }
}
