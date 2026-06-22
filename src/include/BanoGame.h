#pragma once
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <vector>
#include "Shader.h"
#include "TextRenderer.h"
#include "Texture.h"

enum class BanoResult { Playing, Won, Lost };
enum class BanoPhase  { Instructions, Countdown, Playing, Result };

class BanoGame {
public:
    // ── dimensiones del baño a escala gigante ────────────────────────────────
    // El jugador mide 1.7u; las paredes 30u → todo se ve enorme
    static constexpr float MAP_W    = 20.0f;   // semiancho  (total 40 u)
    static constexpr float MAP_L    = 90.0f;   // semilargo  (total 180 u)
    static constexpr float WALL_H   = 30.0f;   // techos altísimos
    static constexpr float EYE_H    =  1.7f;

    // ── movimiento resbaladizo ───────────────────────────────────────────────
    static constexpr float MAX_SPEED    =  9.0f;
    static constexpr float ACCEL        = 55.0f;   // aceleración al pulsar tecla
    static constexpr float FRICTION     =  0.35f;  // fracción de vel. retenida/s (muy resbaladizo)
    static constexpr float KNOCKBACK    = 28.0f;   // impulso horizontal al ser golpeado

    // ── gotas gigantes ────────────────────────────────────────────────────────
    static constexpr float DROP_GRAVITY     = 45.0f;  // caída rápida desde techo alto
    static constexpr float DROP_RADIUS      =  2.5f;  // gota grande (escala gigante)
    static constexpr float SPLASH_RADIUS    =  8.0f;  // onda expansiva amplia
    static constexpr float STUN_RADIUS      =  3.5f;  // radio de derribo total
    static constexpr float STUN_DURATION    =  1.8f;
    static constexpr float DROP_SPAWN_MIN   =  0.6f;  // seg entre spawns (mín)
    static constexpr float DROP_SPAWN_MAX   =  1.4f;  // seg entre spawns (máx)
    static constexpr int   MAX_DROPS        = 25;

    // ── linterna cónica ───────────────────────────────────────────────────────
    static constexpr float FOG_DENSITY      = 0.062f;  // vapor de baño (~10u visible, ~25u blanco)
    static constexpr float FLASH_INNER_DEG  = 16.0f;   // núcleo brillante del cono
    static constexpr float FLASH_OUTER_DEG  = 30.0f;   // borde suave del cono
    static constexpr float FLASH_RANGE      = 55.0f;   // distancia máxima del haz

    // ── meta ─────────────────────────────────────────────────────────────────
    static constexpr float GOAL_Z           = MAP_L - 5.0f;  // zona de victoria

    struct Box  { glm::vec3 center, half, color; };
    struct Drop {
        glm::vec3 pos;
        float     vy   = 0.f;       // velocidad vertical (negativa = cayendo)
        bool      active = false;
        bool      hit    = false;   // ya chocó con el suelo
        float     splashTimer = 0.f;
        float     splashMax   = 0.6f;
    };

    void init(int screenW, int screenH);
    void destroy();

    bool update(float dt, GLFWwindow* window,
                double& lastMX, double& lastMY, bool& firstMouse);

    void render(Shader& modelShader, Shader& spriteShader, GLuint quadVAO,
                TextRenderer& tr, const glm::mat4& orthoProj,
                int W, int H, Texture& whiteTex);

    BanoResult result()    const { return mResult;    }
    bool       wantsExit() const { return mWantsExit; }

private:
    // cámara / jugador
    glm::vec3 mCamPos   = { 0.f, EYE_H, -(MAP_L - 3.f) };
    glm::vec3 mCamFront = { 0.f, 0.f,  1.f };
    glm::vec3 mCamUp    = { 0.f, 1.f,  0.f };
    float     mYaw      = 90.f;
    float     mPitch    =  0.f;

    // física resbaladiza
    glm::vec3 mVelocity = { 0.f, 0.f, 0.f };

    // aturdimiento
    bool  mStunned    = false;
    float mStunTimer  = 0.f;
    float mWetFlash   = 0.f;   // destello azul al ser golpeado

    // gotas
    Drop  mDrops[25];
    int   mHitCount = 0;          // gotas que impactaron al jugador
    float mResetFlash = 0.f;      // flash rojo al reiniciar posición
    float mNextSpawn  = 1.0f;  // tiempo hasta el próximo spawn

    // estado
    BanoPhase  mPhase       = BanoPhase::Instructions;
    BanoResult mResult      = BanoResult::Playing;
    float      mCountdown   = 3.f;
    float      mResultTimer = 0.f;
    bool       mWantsExit   = false;
    bool       mAnyKeyWaiting = false;

    // escena
    std::vector<Box> mBoxes;
    GLuint mBoxVAO = 0, mBoxVBO = 0;

    void buildBoxVAO();
    void buildRoom();
    void spawnDrop();

    void resolveVsWorld(glm::vec3& pos, float r) const;

    void renderGeom(Shader& sh, glm::vec3 center, glm::vec3 half, glm::vec3 col,
                    const glm::mat4& view, const glm::mat4& proj);
    void renderScene(Shader& sh, const glm::mat4& view, const glm::mat4& proj);
    void renderHUD(Shader& sp, GLuint qvao, TextRenderer& tr,
                   const glm::mat4& op, int W, int H, Texture& wt);
    void renderInstructions(Shader& sp, GLuint qvao, TextRenderer& tr,
                            const glm::mat4& op, int W, int H, Texture& wt);
    void renderCountdown(Shader& sp, GLuint qvao, TextRenderer& tr,
                         const glm::mat4& op, int W, int H, Texture& wt);
};
