#include "VisualNovel.h"
#include "Texture.h"
#include "TextRenderer.h"
#include "Shader.h"
#include <glm/gtc/type_ptr.hpp>

static const float CHAR_INTERVAL = 0.05f;
static const float TEXT_PADDING_X = 40.0f;
static const float TEXT_PADDING_Y = 20.0f;
static const float BOX_BOTTOM_MARGIN = 10.0f;
static const float DIALOG_TEXT_SCALE = 0.55f;

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
    , mDialogBox(nullptr)
{
    mDialogBox = new Texture("assets/textures/caja_dialogo.png");
    calcLayout();

    Scene scene;
    scene.lines.push_back("Otro d\u00eda frente a la misma pantalla.");
    scene.lines.push_back("Las mismas teclas.");
    scene.lines.push_back("Los mismos pasillos.");
    scene.lines.push_back("Las mismas luces que nunca parec\u00edan apagarse.");
    scene.lines.push_back("Mir\u00e9 el reloj.");
    scene.lines.push_back("22:47.");
    scene.lines.push_back("Ya era tarde.");
    scene.lines.push_back("Guard\u00e9 los \u00faltimos archivos.");
    scene.lines.push_back("Apagu\u00e9 la pantalla.");
    scene.lines.push_back("Y por fin me levant\u00e9.");
    mScenes.push_back(scene);
}

VisualNovel::~VisualNovel() {
    delete mDialogBox;
}

void VisualNovel::reset() {
    mCurrentScene = 0;
    mCurrentLine = 0;
    mCharIndex = 0;
    mCharTimer = 0.0f;
    mLineFinished = false;
    mAllDone = false;
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

    if (!mLineFinished) {
        mCharTimer += dt;
        while (mCharTimer >= mCharInterval && mCharIndex < (int)mScenes[mCurrentScene].lines[mCurrentLine].size()) {
            mCharTimer -= mCharInterval;
            mCharIndex++;
        }
        if (mCharIndex >= (int)mScenes[mCurrentScene].lines[mCurrentLine].size()) {
            mLineFinished = true;
        }
    } else if (mouseJustPressed) {
        advanceLine();
    }
}

void VisualNovel::advanceLine() {
    mCurrentLine++;
    if (mCurrentLine >= (int)mScenes[mCurrentScene].lines.size()) {
        mAllDone = true;
        return;
    }
    mCharIndex = 0;
    mCharTimer = 0.0f;
    mLineFinished = false;
}

void VisualNovel::render() {
    if (mAllDone) return;

    if (mBackground) {
        mSpriteShader->use();
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::scale(model, glm::vec3((float)mWindowWidth, (float)mWindowHeight, 1.0f));
        mSpriteShader->setMat4("uModel", glm::value_ptr(model));
        mSpriteShader->setMat4("uProjection", glm::value_ptr(mProj));
        mSpriteShader->setVec4("uColor", 1.0f, 1.0f, 1.0f, 1.0f);
        mSpriteShader->setInt("uTexture", 0);
        mBackground->bind(0);
        glBindVertexArray(mQuadVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
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

    const std::string& line = mScenes[mCurrentScene].lines[mCurrentLine];
    std::string visible = line.substr(0, mCharIndex);
    mTextRenderer->renderText(visible, mTextX, mTextY, DIALOG_TEXT_SCALE, glm::vec3(1.0f, 1.0f, 1.0f));
}

void VisualNovel::setBackground(Texture* bg) {
    mBackground = bg;
}

bool VisualNovel::isFinished() const {
    return mAllDone;
}
