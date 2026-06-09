#include "VisualNovel.h"
#include "Texture.h"
#include "TextRenderer.h"
#include "Shader.h"
#include <glm/gtc/type_ptr.hpp>
#include <miniaudio.h>

static const float CHAR_INTERVAL = 0.05f;
static const float TEXT_PADDING_X = 40.0f;
static const float TEXT_PADDING_Y = 20.0f;
static const float BOX_BOTTOM_MARGIN = 10.0f;
static const float DIALOG_TEXT_SCALE = 0.55f;
static const float TRANSITION_DURATION = 1.0f;

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

    auto addLine = [&](const std::string& text, const std::string& sound = "", bool wait = false) {
        mScenes.back().lines.push_back({text, sound, wait});
    };

    // Scene 0: Office
    mScenes.push_back({});
    mScenes.back().bgPath = "assets/textures/oficina.png";
    addLine("Another day in front of the same screen.");
    addLine("The same keys.");
    addLine("The same hallways.");
    addLine("The same lights that never seemed to go out.");
    addLine("I looked at the clock.");
    addLine("22:47.");
    addLine("It was already late.");
    addLine("I saved the last files.");
    addLine("I turned off the screen.");
    addLine("And I finally got up.");
    addLine("*I push the chair back.*", "assets/sounds/ui/arrastrar_silla.mp3", true);
    addLine("*I grab my backpack.*", "assets/sounds/ui/mochila.mp3", true);
    addLine("...and I head home.");

    // Scene 1: Office building (exterior)
    mScenes.push_back({});
    mScenes.back().bgPath = "assets/textures/empresa.png";
    addLine("*The glass doors slide open slowly.*", "assets/sounds/ui/puerta_vidrio.mp3");
    addLine("*A stream of fresh air hits my face.*");
    addLine("Much better.");
    addLine("*I start walking down the street.*", "assets/sounds/ui/pasos_2.mp3");
    addLine("I just wanted to get home.");
    addLine("Nothing else.");

    // Scene 2: Street
    mScenes.push_back({});
    mScenes.back().bgPath = "assets/textures/calle.png";
    mScenes.back().ambientSound = "assets/sounds/ui/viento.mp3";
    addLine("*The streetlights warm the sidewalk with a soft glow.*");
    addLine("*The city seems quieter than usual.*");
    addLine("So quiet...");
    addLine("*I keep walking.*", "assets/sounds/ui/pasos_2.mp3");
    addLine("*The sound of my footsteps echoes in the empty street.*", "assets/sounds/ui/pasos_2.mp3");
    addLine("I guess it's already pretty late.");
    addLine("*I look up at the night sky.*");
    addLine("I don't remember the last time I was out this late.");
    addLine("*A cold breeze sweeps through the street.*");
    addLine("I just want to get home.");
    addLine("Get some rest.");
    addLine("*The silhouette of my house appears at the end of the street.*");

    // Scene 3: Home
    mScenes.push_back({});
    mScenes.back().bgPath = "assets/textures/casa.png";
    addLine("*I open the door and step inside.*", "assets/sounds/ui/puerta_casa.mp3", true);
    addLine("*Silence greets me at once.*");
    addLine("*I leave the keys on the table.*", "assets/sounds/ui/llaves.mp3", true);
    addLine("*The living room lamp casts a gentle light across the room.*");
    addLine("Home.");
    addLine("*I drop my backpack on the floor.*", "assets/sounds/ui/mochila.mp3", true);
    addLine("*My shoulders feel lighter.*");
    addLine("At last.");
    addLine("*I take in the room for a few seconds.*");
    addLine("I never thought such a simple place could feel this good.");
    addLine("*I walk slowly toward the couch.*");
    addLine("I just need to rest a little.");

    // Scene 4: Living room (couch)
    mScenes.push_back({});
    mScenes.back().bgPath = "assets/textures/casa_por_dentro.png";
    addLine("*I let myself fall onto the couch.*");
    addLine("*The exhaustion of the day hits me all at once.*");
    addLine("Ah...");
    addLine("*I rest my head back.*");
    addLine("*I close my eyes for a moment.*");
    addLine("Just a few minutes.");
    addLine("Then I'll get up.");
    addLine("*I take a deep breath.*");
    addLine("That's what I always told myself.");
    addLine("*Silence fills the room.*");
    addLine("*Little by little, everything begins to fade away.*");

    // Scene 5: Dark / blurry house
    mScenes.push_back({});
    mScenes.back().bgPath = "assets/textures/casa_oscura.png";
    addLine("...");
    addLine("...");
    addLine("*The room feels strangely silent.*");
    addLine("...");
    addLine("*For a moment, everything feels distant.*");
    addLine("...");

    // Scene 6: Black
    mScenes.push_back({});
    mScenes.back().bgPath = "assets/textures/negro.png";
    addLine("...");
    addLine("...");
    addLine("...?");

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
        while (mCharTimer >= mCharInterval && mCharIndex < (int)mScenes[mCurrentScene].lines[mCurrentLine].text.size()) {
            mCharTimer -= mCharInterval;
            mCharIndex++;
        }
        if (mCharIndex >= (int)mScenes[mCurrentScene].lines[mCurrentLine].text.size()) {
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
    std::string visible = line.text.substr(0, mCharIndex);
    mTextRenderer->renderText(visible, mTextX, mTextY, DIALOG_TEXT_SCALE, glm::vec3(1.0f, 1.0f, 1.0f));
}

void VisualNovel::setBackground(Texture* bg) {
    delete mBackground;
    mBackground = bg;
}

bool VisualNovel::isFinished() const {
    return mAllDone;
}
