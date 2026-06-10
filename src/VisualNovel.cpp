#include "VisualNovel.h"
#include "Texture.h"
#include "TextRenderer.h"
#include "Shader.h"
#include "Localization.h"
#include <glm/gtc/type_ptr.hpp>
#include <miniaudio.h>

static const float CHAR_INTERVAL = 0.05f;
static const float TEXT_PADDING_X = 40.0f;
static const float TEXT_PADDING_Y = 20.0f;
static const float BOX_BOTTOM_MARGIN = 10.0f;
static const float DIALOG_TEXT_SCALE = 0.55f;
static const float TRANSITION_DURATION = 1.0f;

static size_t utf8Length(const std::string& text) {
    size_t count = 0;
    const unsigned char* ptr = reinterpret_cast<const unsigned char*>(text.data());
    const unsigned char* end = ptr + text.size();
    while (ptr < end) {
        unsigned char c = *ptr++;
        if (c < 0x80) {
            count++;
        } else if ((c & 0xE0) == 0xC0 && ptr < end) {
            ptr += 1;
            count++;
        } else if ((c & 0xF0) == 0xE0 && ptr + 1 < end) {
            ptr += 2;
            count++;
        } else if ((c & 0xF8) == 0xF0 && ptr + 2 < end) {
            ptr += 3;
            count++;
        } else {
            break;
        }
    }
    return count;
}

static std::string utf8Prefix(const std::string& text, size_t chars) {
    size_t pos = 0;
    size_t count = 0;
    const unsigned char* ptr = reinterpret_cast<const unsigned char*>(text.data());
    const unsigned char* end = ptr + text.size();
    while (ptr < end && count < chars) {
        unsigned char c = *ptr;
        size_t step = 1;
        if (c < 0x80) step = 1;
        else if ((c & 0xE0) == 0xC0) step = 2;
        else if ((c & 0xF0) == 0xE0) step = 3;
        else if ((c & 0xF8) == 0xF0) step = 4;
        else break;
        if (ptr + step > end) break;
        ptr += step;
        pos += step;
        count++;
    }
    return text.substr(0, pos);
}

VisualNovel::VisualNovel(TextRenderer* textRenderer, Shader* spriteShader,
                         GLuint quadVAO, const glm::mat4& proj,
                         int windowWidth, int windowHeight)
    : mTextRenderer(textRenderer)
    , mSpriteShader(spriteShader)
    , mQuadVAO(quadVAO)
    , mProj(proj)
    , mWindowWidth(windowWidth)
    , mWindowHeight(windowHeight)
    , mCurrentScene(0)
    , mCurrentLine(0)
    , mCharIndex(0)
    , mCharTimer(0.0f)
    , mCharInterval(CHAR_INTERVAL)
    , mLineFinished(false)
    , mAllDone(false)
    , mBackground(nullptr)
    , mNextBg(nullptr)
    , mPrevBg(nullptr)
    , mDialogBox(nullptr)
    , mTransitioning(false)
    , mTransitionTimer(0.0f)
    , mTransitionDuration(TRANSITION_DURATION)
    , mSoundEngine(nullptr)
    , mCurrentSound(nullptr)
    , mAmbientSound(nullptr)
    , mWaitingForSound(false)
    , mEndTimer(0.0f)
    , mEndDelay(3.0f)
    , mEndTimerActive(false)
{
    mDialogBox = new Texture("assets/textures/caja_dialogo.png");
    calcLayout();

    auto addLine = [&](TextId textId, const std::string& sound = "", bool wait = false) {
        mScenes.back().lines.push_back({textId, sound, wait});
    };

    // Scene 0: Office
    mScenes.push_back({});
    mScenes.back().bgPath = "assets/textures/oficina.png";
    addLine(TextId::VN_S0_0);
    addLine(TextId::VN_S0_1);
    addLine(TextId::VN_S0_2);
    addLine(TextId::VN_S0_3);
    addLine(TextId::VN_S0_4);
    addLine(TextId::VN_S0_5);
    addLine(TextId::VN_S0_6);
    addLine(TextId::VN_S0_7);
    addLine(TextId::VN_S0_8);
    addLine(TextId::VN_S0_9);
    addLine(TextId::VN_S0_10, "assets/sounds/ui/arrastrar_silla.mp3", true);
    addLine(TextId::VN_S0_11, "assets/sounds/ui/mochila.mp3", true);
    addLine(TextId::VN_S0_12);

    // Scene 1: Office building (exterior)
    mScenes.push_back({});
    mScenes.back().bgPath = "assets/textures/empresa.png";
    addLine(TextId::VN_S1_0, "assets/sounds/ui/puerta_vidrio.mp3");
    addLine(TextId::VN_S1_1);
    addLine(TextId::VN_S1_2);
    addLine(TextId::VN_S1_3, "assets/sounds/ui/pasos_2.mp3");
    addLine(TextId::VN_S1_4);
    addLine(TextId::VN_S1_5);

    // Scene 2: Street
    mScenes.push_back({});
    mScenes.back().bgPath = "assets/textures/calle.png";
    mScenes.back().ambientSound = "assets/sounds/ui/viento.mp3";
    addLine(TextId::VN_S2_0);
    addLine(TextId::VN_S2_1);
    addLine(TextId::VN_S2_2);
    addLine(TextId::VN_S2_3, "assets/sounds/ui/pasos_2.mp3");
    addLine(TextId::VN_S2_4, "assets/sounds/ui/pasos_2.mp3");
    addLine(TextId::VN_S2_5);
    addLine(TextId::VN_S2_6);
    addLine(TextId::VN_S2_7);
    addLine(TextId::VN_S2_8);
    addLine(TextId::VN_S2_9);
    addLine(TextId::VN_S2_10);
    addLine(TextId::VN_S2_11);

    // Scene 3: Home
    mScenes.push_back({});
    mScenes.back().bgPath = "assets/textures/casa.png";
    addLine(TextId::VN_S3_0, "assets/sounds/ui/puerta_casa.mp3", true);
    addLine(TextId::VN_S3_1);
    addLine(TextId::VN_S3_2, "assets/sounds/ui/llaves.mp3", true);
    addLine(TextId::VN_S3_3);
    addLine(TextId::VN_S3_4);
    addLine(TextId::VN_S3_5, "assets/sounds/ui/mochila.mp3", true);
    addLine(TextId::VN_S3_6);
    addLine(TextId::VN_S3_7);
    addLine(TextId::VN_S3_8);
    addLine(TextId::VN_S3_9);
    addLine(TextId::VN_S3_10);
    addLine(TextId::VN_S3_11);

    // Scene 4: Living room (couch)
    mScenes.push_back({});
    mScenes.back().bgPath = "assets/textures/casa_por_dentro.png";
    addLine(TextId::VN_S4_0);
    addLine(TextId::VN_S4_1);
    addLine(TextId::VN_S4_2);
    addLine(TextId::VN_S4_3);
    addLine(TextId::VN_S4_4);
    addLine(TextId::VN_S4_5);
    addLine(TextId::VN_S4_6);
    addLine(TextId::VN_S4_7);
    addLine(TextId::VN_S4_8);
    addLine(TextId::VN_S4_9);

    // Scene 5: Dark / blurry house
    mScenes.push_back({});
    mScenes.back().bgPath = "assets/textures/casa_oscura.png";
    addLine(TextId::VN_S5_0);
    addLine(TextId::VN_S5_1);
    addLine(TextId::VN_S5_2);
    addLine(TextId::VN_S5_3);
    addLine(TextId::VN_S5_4);
    addLine(TextId::VN_S5_5);

    // Scene 6: Black
    mScenes.push_back({});
    mScenes.back().bgPath = "assets/textures/negro.png";
    addLine(TextId::VN_S6_0);
    addLine(TextId::VN_S6_1);
    addLine(TextId::VN_S6_2);

    mBackground = new Texture(mScenes[0].bgPath);
}

VisualNovel::~VisualNovel() {
    stopAmbient();
    stopCurrentSound();
    delete mBackground;
    delete mNextBg;
    delete mDialogBox;
}

void VisualNovel::setSoundEngine(ma_engine* engine) {
    mSoundEngine = engine;
}

void VisualNovel::playCurrentLineSound() {
    const auto& line = mScenes[mCurrentScene].lines[mCurrentLine];
    if (!mSoundEngine || line.soundPath.empty()) return;

    stopCurrentSound();

    mCurrentSound = new ma_sound();
    if (ma_sound_init_from_file(mSoundEngine, line.soundPath.c_str(), 0, nullptr, nullptr, mCurrentSound) == MA_SUCCESS) {
        ma_sound_start(mCurrentSound);
        if (line.waitForSound) {
            mWaitingForSound = true;
        }
    } else {
        delete mCurrentSound;
        mCurrentSound = nullptr;
    }
}

void VisualNovel::stopCurrentSound() {
    if (mCurrentSound) {
        if (ma_sound_is_playing(mCurrentSound)) {
            ma_sound_stop(mCurrentSound);
        }
        ma_sound_uninit(mCurrentSound);
        delete mCurrentSound;
        mCurrentSound = nullptr;
    }
    mWaitingForSound = false;
}

void VisualNovel::startAmbient() {
    const auto& scene = mScenes[mCurrentScene];
    if (!mSoundEngine || scene.ambientSound.empty()) return;
    stopAmbient();
    mAmbientSound = new ma_sound();
    if (ma_sound_init_from_file(mSoundEngine, scene.ambientSound.c_str(), 0, nullptr, nullptr, mAmbientSound) == MA_SUCCESS) {
        ma_sound_set_looping(mAmbientSound, MA_TRUE);
        ma_sound_start(mAmbientSound);
    } else {
        delete mAmbientSound;
        mAmbientSound = nullptr;
    }
}

void VisualNovel::stopAmbient() {
    if (mAmbientSound) {
        if (ma_sound_is_playing(mAmbientSound)) {
            ma_sound_stop(mAmbientSound);
        }
        ma_sound_uninit(mAmbientSound);
        delete mAmbientSound;
        mAmbientSound = nullptr;
    }
}

void VisualNovel::reset() {
    stopAmbient();
    stopCurrentSound();
    delete mBackground;
    delete mNextBg;
    mBackground = new Texture(mScenes[0].bgPath);
    mNextBg = nullptr;
    mPrevBg = nullptr;
    mCurrentScene = 0;
    mCurrentLine = 0;
    mCharIndex = 0;
    mCharTimer = 0.0f;
    mLineFinished = false;
    mAllDone = false;
    mTransitioning = false;
    mEndTimerActive = false;
    mEndTimer = 0.0f;
}

void VisualNovel::calcLayout() {
    mBoxW = (float)mDialogBox->getWidth();
    mBoxH = (float)mDialogBox->getHeight();
    mBoxX = (mWindowWidth - mBoxW) / 2.0f;
    mBoxY = (float)mWindowHeight - mBoxH - BOX_BOTTOM_MARGIN;

    mTextX = mBoxX + TEXT_PADDING_X;
    mTextY = mBoxY + TEXT_PADDING_Y;
    mTextW = mBoxW - TEXT_PADDING_X * 2.0f;
    mTextH = mBoxH - TEXT_PADDING_Y * 2.0f;
}

void VisualNovel::update(float dt, bool mouseJustPressed) {
    if (mAllDone) return;

    if (mEndTimerActive) {
        mEndTimer += dt;
        if (mEndTimer >= mEndDelay) {
            mEndTimerActive = false;
            mAllDone = true;
        }
        return;
    }

    if (mTransitioning) {
        mTransitionTimer += dt;
        float progress = std::min(mTransitionTimer / mTransitionDuration, 1.0f);
        if (progress >= 1.0f) {
            mTransitioning = false;
            delete mPrevBg;
            mPrevBg = nullptr;
            mBackground = mNextBg;
            mNextBg = nullptr;
            if (!mScenes[mCurrentScene].ambientSound.empty()) {
                startAmbient();
            }
        }
        return;
    }

    if (!mLineFinished) {
        mCharTimer += dt;
        std::string currentText = Localization::t(mScenes[mCurrentScene].lines[mCurrentLine].textId);
        int currentLength = (int)utf8Length(currentText);
        while (mCharTimer >= mCharInterval && mCharIndex < currentLength) {
            mCharTimer -= mCharInterval;
            mCharIndex++;
        }
        if (mCharIndex >= currentLength) {
            mLineFinished = true;
            const auto& line = mScenes[mCurrentScene].lines[mCurrentLine];
            if (!line.soundPath.empty()) {
                playCurrentLineSound();
            }
        }
    }

    if (mWaitingForSound) {
        if (mCurrentSound && !ma_sound_is_playing(mCurrentSound)) {
            mWaitingForSound = false;
            advanceLine();
        }
        return;
    }

    if (mLineFinished && mouseJustPressed) {
        advanceLine();
    }
}

void VisualNovel::advanceLine() {
    stopCurrentSound();
    mCurrentLine++;
    if (mCurrentLine >= (int)mScenes[mCurrentScene].lines.size()) {
        int nextScene = mCurrentScene + 1;
        if (nextScene < (int)mScenes.size()) {
            stopAmbient();
            mPrevBg = mBackground;
            mNextBg = new Texture(mScenes[nextScene].bgPath);
            mBackground = nullptr;
            mTransitioning = true;
            mTransitionTimer = 0.0f;
            mCurrentScene = nextScene;
            mCurrentLine = 0;
            mCharIndex = 0;
            mCharTimer = 0.0f;
            mLineFinished = false;
            return;
        }
        stopAmbient();
        mEndTimerActive = true;
        mEndTimer = 0.0f;
        return;
    }
    mCharIndex = 0;
    mCharTimer = 0.0f;
    mLineFinished = false;
}

static void renderBg(Shader& shader, GLuint vao, const glm::mat4& proj,
                     Texture& tex, float alpha, int w, int h) {
    shader.use();
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::scale(model, glm::vec3((float)w, (float)h, 1.0f));
    shader.setMat4("uModel", glm::value_ptr(model));
    shader.setMat4("uProjection", glm::value_ptr(proj));
    shader.setVec4("uColor", 1.0f, 1.0f, 1.0f, alpha);
    shader.setInt("uTexture", 0);
    tex.bind(0);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void VisualNovel::render() {
    if (mAllDone) return;

    if (mEndTimerActive) {
        if (mBackground) {
            renderBg(*mSpriteShader, mQuadVAO, mProj, *mBackground, 1.0f, mWindowWidth, mWindowHeight);
        }
        return;
    }

    if (mTransitioning) {
        float progress = std::min(mTransitionTimer / mTransitionDuration, 1.0f);
        if (mPrevBg) {
            renderBg(*mSpriteShader, mQuadVAO, mProj, *mPrevBg, 1.0f, mWindowWidth, mWindowHeight);
        }
        if (mNextBg) {
            renderBg(*mSpriteShader, mQuadVAO, mProj, *mNextBg, progress, mWindowWidth, mWindowHeight);
        }
        return;
    }

    if (mBackground) {
        renderBg(*mSpriteShader, mQuadVAO, mProj, *mBackground, 1.0f, mWindowWidth, mWindowHeight);
    }

    mSpriteShader->use();
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(mBoxX, mBoxY, 0.0f));
    model = glm::scale(model, glm::vec3(mBoxW, mBoxH, 1.0f));
    mSpriteShader->setMat4("uModel", glm::value_ptr(model));
    mSpriteShader->setMat4("uProjection", glm::value_ptr(mProj));
    mSpriteShader->setVec4("uColor", 1.0f, 1.0f, 1.0f, 1.0f);
    mSpriteShader->setInt("uTexture", 0);
    mDialogBox->bind(0);
    glBindVertexArray(mQuadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    const auto& line = mScenes[mCurrentScene].lines[mCurrentLine];
    std::string visible = utf8Prefix(Localization::t(line.textId), (size_t)mCharIndex);
    mTextRenderer->renderText(visible, mTextX, mTextY, DIALOG_TEXT_SCALE, glm::vec3(1.0f, 1.0f, 1.0f));
}

void VisualNovel::setBackground(Texture* bg) {
    delete mBackground;
    mBackground = bg;
}

bool VisualNovel::isFinished() const {
    return mAllDone;
}
