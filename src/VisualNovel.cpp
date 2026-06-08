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
{
    mDialogBox = new Texture("assets/textures/caja_dialogo.png");
    calcLayout();

    auto addLine = [&](const std::string& text, const std::string& sound = "", bool wait = false) {
        mScenes.back().lines.push_back({text, sound, wait});
    };

    // Scene 0: Oficina
    mScenes.push_back({});
    mScenes.back().bgPath = "assets/textures/oficina.png";
    addLine("Otro d\u00eda frente a la misma pantalla.");
    addLine("Las mismas teclas.");
    addLine("Los mismos pasillos.");
    addLine("Las mismas luces que nunca parec\u00edan apagarse.");
    addLine("Mir\u00e9 el reloj.");
    addLine("22:47.");
    addLine("Ya era tarde.");
    addLine("Guard\u00e9 los \u00faltimos archivos.");
    addLine("Apagu\u00e9 la pantalla.");
    addLine("Y por fin me levant\u00e9.");
    addLine("*Empujo la silla hacia atr\u00e1s.*", "assets/sounds/ui/arrastrar_silla.mp3", true);
    addLine("*Recojo mi mochila.*", "assets/sounds/ui/pasos.mp3", true);
    addLine("...y me voy a mi casa.");

    // Scene 1: Empresa (exterior)
    mScenes.push_back({});
    mScenes.back().bgPath = "assets/textures/empresa.png";
    addLine("*Las puertas de vidrio se abren lentamente.*");
    addLine("*Una corriente de aire fresco golpea mi rostro.*");
    addLine("> Mucho mejor.");
    addLine("*Comienzo a caminar por la calle.*");
    addLine("> Solo quer\u00eda llegar a casa.");
    addLine("> Nada m\u00e1s.");

    // Scene 2: Calle
    mScenes.push_back({});
    mScenes.back().bgPath = "assets/textures/calle.png";
    mScenes.back().ambientSound = "assets/sounds/ui/viento.mp3";
    addLine("*Las farolas iluminan la acera con una luz c\u00e1lida.*");
    addLine("*La ciudad parece m\u00e1s tranquila de lo habitual.*");
    addLine("> Qu\u00e9 silencio...");
    addLine("*Contin\u00fao caminando.*", "assets/sounds/ui/pasos_2.mp3");
    addLine("*El sonido de mis pasos resuena en la calle vac\u00eda.*", "assets/sounds/ui/pasos_2.mp3");
    addLine("> Supongo que ya es bastante tarde.");
    addLine("*Levanto la vista hacia el cielo nocturno.*");
    addLine("> No recuerdo la \u00faltima vez que sal\u00ed tan tarde.");
    addLine("*Una brisa fr\u00eda recorre la calle.*");
    addLine("> Solo quiero llegar a casa.");
    addLine("> Descansar un poco.");
    addLine("*La silueta de mi casa aparece al final de la calle.*");

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
        mAllDone = true;
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
