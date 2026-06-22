#pragma once
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <vector>
#include "Shader.h"
#include "TextRenderer.h"
#include "Texture.h"
#include "Model.h"
#include <miniaudio.h>

enum class BodegaResult { Playing, Won, Lost };

// Fases internas del minijuego
enum class BodegaPhase { Instructions, Countdown, Playing, Result };

class BodegaGame {
public:
    // ── mapa (jugador es diminuto, todo es gigante) ──────────────────────────
    static constexpr float MAP_HALF      = 60.0f;
    static constexpr float WALL_H        = 22.0f;
    static constexpr float EYE_H         =  1.7f;   // el jugador sigue siendo "humano"

    // ── stamina ──────────────────────────────────────────────────────────────
    static constexpr float SPEED_NORMAL  = 13.0f;
    static constexpr float SPEED_SPRINT  = 21.0f;
    static constexpr float SPEED_EXHAUST =  7.0f;
    static constexpr float SPEED_BOOST   = 32.0f;
    static constexpr float DRAIN_RATE    =  0.45f;
    static constexpr float REGEN_RATE    =  0.22f;
    static constexpr float BOOST_DURATION=  3.5f;
    static constexpr float SURVIVAL_TIME = 60.0f;

    // ── modelo del enemigo ───────────────────────────────────────────────────
    static constexpr const char* ENEMY_MODEL_PATH   = "assets/models/enemy/scene.gltf";
    static constexpr float       ENEMY_TARGET_HEIGHT = 9.0f;  // unidades de alto deseadas

    struct Box { glm::vec3 center, halfSize, color; };

    void init(int screenW, int screenH, ma_engine* audioEngine = nullptr);
    void destroy();

    // Devuelve true si ESC fue presionado (el caller abre la pausa)
    bool update(float dt, GLFWwindow* window,
                double& lastMX, double& lastMY, bool& firstMouse);

    void render(Shader& modelShader, Shader& spriteShader, GLuint quadVAO,
                TextRenderer& tr, const glm::mat4& orthoProj,
                int W, int H, Texture& whiteTex);

    BodegaResult result()    const { return mResult;    }
    bool         wantsExit() const { return mWantsExit; }

private:
    // cámara
    glm::vec3 mCamPos   = {-50.f, EYE_H, -50.f};
    glm::vec3 mCamFront = { 0.f,  0.f,   -1.f  };
    glm::vec3 mCamUp    = { 0.f,  1.f,    0.f  };
    float mYaw   = 45.f;
    float mPitch =  0.f;

    // stamina
    float mStamina    = 1.0f;
    bool  mExhausted  = false;
    bool  mBoostActive= false;
    float mBoostTimer = 0.f;

    // enemigo
    glm::vec3 mEnemyPos        = {-30.f, 0.f, -30.f};
    float     mEnemySpeed      = 8.0f;
    glm::vec3 mLastSeenPlayer  = {0.f, EYE_H, 0.f};
    bool      mEnemyHasSeen    = false;
    glm::vec3 mEnemyWanderDest = {0.f, 0.f, 0.f};
    float     mWanderTimer     = 0.f;
    float     mEnemyYaw        = 0.f;   // para rotar el modelo hacia el jugador

    // modelo del enemigo
    Model mEnemyModel;
    bool  mEnemyModelLoaded   = false;
    float mEnemyComputedScale = 1.0f;   // calculado tras cargar el modelo

    // estado del juego
    BodegaPhase  mPhase        = BodegaPhase::Instructions;
    BodegaResult mResult       = BodegaResult::Playing;
    float        mElapsed      = 0.f;
    float        mCountdown    = 3.f;    // 3 → 2 → 1 → GO
    float        mResultTimer  = 0.f;
    bool         mWantsExit    = false;
    bool         mAnyKeyWaiting= false;  // espera release de la tecla que abrió la pantalla

    // audio del enemigo
    ma_engine*   mAudioEngine     = nullptr;
    ma_waveform  mEnemyWaveform;
    ma_sound     mEnemySound;
    bool         mEnemySoundReady = false;
    float        mLfoPhase        = 0.f;   // oscilador de vibrato

    std::vector<Box> mBoxes;
    GLuint mBoxVAO = 0, mBoxVBO = 0;

    void buildBoxVAO();
    void buildMap();

    void renderGeom(Shader& sh, glm::vec3 center, glm::vec3 halfSize, glm::vec3 color,
                    const glm::mat4& view, const glm::mat4& proj);
    void renderScene(Shader& modelShader, const glm::mat4& view, const glm::mat4& proj3d);
    void renderHUD(Shader& spriteShader, GLuint quadVAO, TextRenderer& tr,
                   const glm::mat4& orthoProj, int W, int H, Texture& whiteTex);
    void renderInstructions(Shader& spriteShader, GLuint quadVAO, TextRenderer& tr,
                            const glm::mat4& orthoProj, int W, int H, Texture& whiteTex);
    void renderCountdown(Shader& spriteShader, GLuint quadVAO, TextRenderer& tr,
                         const glm::mat4& orthoProj, int W, int H, Texture& whiteTex);

    bool hasLOS(glm::vec3 from, glm::vec3 to) const;
    void resolveVsWorld(glm::vec3& pos, float radius) const;
    static bool segVsAABB2D(glm::vec2 a, glm::vec2 b, glm::vec2 mn, glm::vec2 mx);
};
