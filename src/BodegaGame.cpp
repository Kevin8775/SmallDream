#include "BodegaGame.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

// ─── init / destroy ───────────────────────────────────────────────────────────

void BodegaGame::init(int /*screenW*/, int /*screenH*/, ma_engine* audioEngine) {
    mCamPos    = {-50.f, EYE_H, -50.f};
    mCamFront  = glm::normalize(glm::vec3(1.f, 0.f, 1.f));
    mYaw       = 45.f;
    mPitch     = 0.f;
    mCamUp     = {0.f, 1.f, 0.f};

    mStamina    = 1.0f;
    mExhausted  = false;
    mBoostActive= false;
    mBoostTimer = 0.f;

    mEnemyPos        = {-30.f, 0.f, -30.f};
    mEnemySpeed      = 8.0f;
    mEnemyYaw        = 0.f;
    mLastSeenPlayer  = {0.f, EYE_H, 0.f};
    mEnemyHasSeen    = false;
    mEnemyWanderDest = {0.f, 0.f, 0.f};
    mWanderTimer     = 0.f;

    mLfoPhase = 0.f;

    mPhase        = BodegaPhase::Instructions;
    mResult       = BodegaResult::Playing;
    mElapsed      = 0.f;
    mCountdown    = 3.f;
    mResultTimer  = 0.f;
    mWantsExit    = false;
    mAnyKeyWaiting= true;  // espera que se suelte la tecla E con la que se entró

    // ── sonido del enemigo (onda diente de sierra, baja frecuencia) ───────
    mAudioEngine = audioEngine;
    if (mAudioEngine && !mEnemySoundReady) {
        ma_waveform_config wfCfg = ma_waveform_config_init(
            ma_format_f32, 1, 44100,
            ma_waveform_type_sawtooth, 0.35, 52.0);
        if (ma_waveform_init(&wfCfg, &mEnemyWaveform) == MA_SUCCESS) {
            if (ma_sound_init_from_data_source(mAudioEngine,
                    &mEnemyWaveform, 0, nullptr, &mEnemySound) == MA_SUCCESS) {
                ma_sound_set_looping(&mEnemySound, MA_TRUE);
                ma_sound_set_volume(&mEnemySound, 0.f);
                ma_sound_start(&mEnemySound);
                mEnemySoundReady = true;
            }
        }
    }

    buildMap();
    if (!mBoxVAO) buildBoxVAO();

    // cargar modelo del enemigo y calcular escala automática
    if (!mEnemyModelLoaded) {
        mEnemyModelLoaded = mEnemyModel.load(ENEMY_MODEL_PATH);
        if (mEnemyModelLoaded) {
            glm::vec3 mn = mEnemyModel.boundsMin();
            glm::vec3 mx = mEnemyModel.boundsMax();
            float modelH = mx.y - mn.y;
            mEnemyComputedScale = (modelH > 0.001f)
                                  ? ENEMY_TARGET_HEIGHT / modelH
                                  : 1.0f;
        }
    }
}

void BodegaGame::destroy() {
    if (mEnemySoundReady) {
        ma_sound_stop(&mEnemySound);
        ma_sound_uninit(&mEnemySound);
        ma_waveform_uninit(&mEnemyWaveform);
        mEnemySoundReady = false;
    }
    if (mBoxVAO) { glDeleteVertexArrays(1, &mBoxVAO); mBoxVAO = 0; }
    if (mBoxVBO) { glDeleteBuffers(1, &mBoxVBO);      mBoxVBO = 0; }
    mEnemyModel.destroy();
    mEnemyModelLoaded = false;
}

// ─── mapa ─────────────────────────────────────────────────────────────────────
// El jugador es diminuto (EYE_H = 1.7u) en una bodega de escala gigante.
// Las cajas miden entre 8 y 18 unidades de alto.

void BodegaGame::buildMap() {
    mBoxes.clear();
    const glm::vec3 WOOD  = {0.52f, 0.36f, 0.20f};
    const glm::vec3 METAL = {0.50f, 0.52f, 0.56f};
    const glm::vec3 WARN  = {0.82f, 0.58f, 0.08f};
    const glm::vec3 DARK  = {0.30f, 0.22f, 0.14f};

    auto add = [&](float cx, float cz, float hw, float hd, float hh, glm::vec3 col) {
        mBoxes.push_back({{ cx, hh, cz }, { hw, hh, hd }, col});
    };

    // ── bloque central único ──────────────────────────────────────────────
    add(  0.f,   0.f,  8.f,  8.f, 18.f, WARN);

    // ── cuadrante noroeste ────────────────────────────────────────────────
    add(-38.f,  12.f,  5.f,  5.f, 11.f, WOOD);
    add(-20.f,  35.f,  6.f,  4.f, 10.f, METAL);

    // ── cuadrante noreste ─────────────────────────────────────────────────
    add( 22.f,  30.f,  5.f,  5.f, 12.f, WOOD);
    add( 42.f,  20.f,  4.f,  8.f,  9.f, WARN);

    // ── cuadrante sureste ─────────────────────────────────────────────────
    add( 38.f, -12.f,  5.f,  5.f, 11.f, METAL);
    add( 20.f, -38.f,  6.f,  4.f, 10.f, DARK);

    // ── cuadrante suroeste (una caja de paso, no bloquea spawn) ──────────
    add(-20.f, -22.f,  4.f,  4.f,  9.f, WOOD);

    // ── bordes laterales ──────────────────────────────────────────────────
    add(-48.f,  -8.f,  4.f,  8.f, 11.f, WOOD);
    add( 48.f,   8.f,  4.f,  8.f, 11.f, METAL);

    // ── norte y sur ───────────────────────────────────────────────────────
    add(  8.f,  45.f,  7.f,  4.f, 10.f, DARK);
    add( -8.f, -45.f,  7.f,  4.f, 10.f, WARN);

    // ── diagonal sureste ─────────────────────────────────────────────────
    add( 32.f, -35.f,  4.f,  4.f,  9.f, WOOD);
}

// ─── VAO del cubo unitario ────────────────────────────────────────────────────

void BodegaGame::buildBoxVAO() {
    static const float V[] = {
        // +Z
        -0.5f,-0.5f, 0.5f, 0,0,1,  0.5f,-0.5f, 0.5f, 0,0,1,  0.5f, 0.5f, 0.5f, 0,0,1,
        -0.5f,-0.5f, 0.5f, 0,0,1,  0.5f, 0.5f, 0.5f, 0,0,1, -0.5f, 0.5f, 0.5f, 0,0,1,
        // -Z
         0.5f,-0.5f,-0.5f, 0,0,-1,-0.5f,-0.5f,-0.5f, 0,0,-1,-0.5f, 0.5f,-0.5f, 0,0,-1,
         0.5f,-0.5f,-0.5f, 0,0,-1,-0.5f, 0.5f,-0.5f, 0,0,-1, 0.5f, 0.5f,-0.5f, 0,0,-1,
        // -X
        -0.5f,-0.5f,-0.5f,-1,0,0, -0.5f, 0.5f,-0.5f,-1,0,0, -0.5f, 0.5f, 0.5f,-1,0,0,
        -0.5f,-0.5f,-0.5f,-1,0,0, -0.5f, 0.5f, 0.5f,-1,0,0, -0.5f,-0.5f, 0.5f,-1,0,0,
        // +X
         0.5f,-0.5f, 0.5f, 1,0,0,  0.5f, 0.5f, 0.5f, 1,0,0,  0.5f, 0.5f,-0.5f, 1,0,0,
         0.5f,-0.5f, 0.5f, 1,0,0,  0.5f, 0.5f,-0.5f, 1,0,0,  0.5f,-0.5f,-0.5f, 1,0,0,
        // +Y
        -0.5f, 0.5f, 0.5f, 0,1,0,  0.5f, 0.5f, 0.5f, 0,1,0,  0.5f, 0.5f,-0.5f, 0,1,0,
        -0.5f, 0.5f, 0.5f, 0,1,0,  0.5f, 0.5f,-0.5f, 0,1,0, -0.5f, 0.5f,-0.5f, 0,1,0,
        // -Y
        -0.5f,-0.5f,-0.5f, 0,-1,0,  0.5f,-0.5f,-0.5f, 0,-1,0,  0.5f,-0.5f, 0.5f, 0,-1,0,
        -0.5f,-0.5f,-0.5f, 0,-1,0,  0.5f,-0.5f, 0.5f, 0,-1,0, -0.5f,-0.5f, 0.5f, 0,-1,0,
    };
    glGenVertexArrays(1, &mBoxVAO);
    glGenBuffers(1, &mBoxVBO);
    glBindVertexArray(mBoxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, mBoxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(V), V, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)(3*sizeof(float)));
    glBindVertexArray(0);
}

// ─── update ───────────────────────────────────────────────────────────────────

bool BodegaGame::update(float dt, GLFWwindow* window,
                         double& lastMX, double& lastMY, bool& firstMouse) {
    // ── fase instrucciones ──────────────────────────────────────────────────
    if (mPhase == BodegaPhase::Instructions) {
        // Esperar a que el usuario suelte todas las teclas antes de aceptar input
        if (mAnyKeyWaiting) {
            bool anyDown = false;
            for (int k = GLFW_KEY_SPACE; k <= GLFW_KEY_LAST; ++k)
                if (glfwGetKey(window, k) == GLFW_PRESS) { anyDown = true; break; }
            if (!anyDown) mAnyKeyWaiting = false;
            return false;
        }
        // Cualquier tecla arranca la cuenta regresiva
        for (int k = GLFW_KEY_SPACE; k <= GLFW_KEY_LAST; ++k) {
            if (glfwGetKey(window, k) == GLFW_PRESS) {
                mPhase     = BodegaPhase::Countdown;
                mCountdown = 3.f;
                // Inicializar mouse para que no haya salto al empezar
                firstMouse = true;
                return false;
            }
        }
        return false;
    }

    // ── fase cuenta regresiva ───────────────────────────────────────────────
    if (mPhase == BodegaPhase::Countdown) {
        // Durante la cuenta el jugador puede girar la cámara pero no moverse
        double mx, my;
        glfwGetCursorPos(window, &mx, &my);
        if (firstMouse) { lastMX = mx; lastMY = my; firstMouse = false; }
        float xoff = (float)(mx - lastMX) * 0.08f;
        float yoff = (float)(lastMY - my) * 0.08f;
        lastMX = mx; lastMY = my;
        mYaw   += xoff;
        mPitch  = glm::clamp(mPitch + yoff, -89.f, 89.f);
        glm::vec3 front;
        front.x = std::cos(glm::radians(mYaw)) * std::cos(glm::radians(mPitch));
        front.y = std::sin(glm::radians(mPitch));
        front.z = std::sin(glm::radians(mYaw)) * std::cos(glm::radians(mPitch));
        mCamFront = glm::normalize(front);

        mCountdown -= dt;
        if (mCountdown <= -0.8f) {     // "GO!" se muestra 0.8 seg
            mPhase   = BodegaPhase::Playing;
            mElapsed = 0.f;
        }
        return false;
    }

    // ── fase resultado ──────────────────────────────────────────────────────
    if (mPhase == BodegaPhase::Result) {
        mResultTimer += dt;
        if (mResultTimer > 3.5f) mWantsExit = true;
        return false;
    }

    // ── fase jugando ────────────────────────────────────────────────────────
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        return true;

    // Cámara
    double mx, my;
    glfwGetCursorPos(window, &mx, &my);
    if (firstMouse) { lastMX = mx; lastMY = my; firstMouse = false; }
    float xoff = (float)(mx - lastMX) * 0.08f;
    float yoff = (float)(lastMY - my) * 0.08f;
    lastMX = mx; lastMY = my;
    mYaw   += xoff;
    mPitch  = glm::clamp(mPitch + yoff, -89.f, 89.f);
    glm::vec3 front;
    front.x = std::cos(glm::radians(mYaw)) * std::cos(glm::radians(mPitch));
    front.y = std::sin(glm::radians(mPitch));
    front.z = std::sin(glm::radians(mYaw)) * std::cos(glm::radians(mPitch));
    mCamFront = glm::normalize(front);

    // Stamina / velocidad
    bool wantSprint = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;
    float speed;
    if (mBoostActive) {
        speed = SPEED_BOOST;
        mBoostTimer += dt;
        if (mBoostTimer >= BOOST_DURATION) {
            mBoostActive = false;
            mBoostTimer  = 0.f;
            mStamina     = 0.f;
            mExhausted   = true;
        }
    } else if (mExhausted) {
        speed = SPEED_EXHAUST;
        mStamina += REGEN_RATE * dt;
        if (mStamina >= 1.0f) {
            mStamina     = 1.0f;
            mExhausted   = false;
            mBoostActive = true;
            mBoostTimer  = 0.f;
        }
    } else if (wantSprint) {
        speed = SPEED_SPRINT;
        mStamina -= DRAIN_RATE * dt;
        if (mStamina <= 0.f) { mStamina = 0.f; mExhausted = true; }
    } else {
        speed = SPEED_NORMAL;
        mStamina = std::min(1.f, mStamina + REGEN_RATE * dt);
    }

    // Movimiento jugador
    glm::vec3 flatFront = glm::normalize(glm::vec3(mCamFront.x, 0.f, mCamFront.z));
    glm::vec3 right     = glm::normalize(glm::cross(flatFront, mCamUp));
    glm::vec3 move(0.f);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) move += flatFront;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) move -= flatFront;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) move -= right;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) move += right;
    float mlen = glm::length(move);
    if (mlen > 0.f) {
        mCamPos += (move / mlen) * speed * dt;
        resolveVsWorld(mCamPos, 0.5f);
    }
    mCamPos.y = EYE_H;

    // IA del enemigo
    mElapsed += dt;
    mEnemySpeed = std::min(11.0f + mElapsed * 0.05f, 18.0f);

    // ── sonido por proximidad ─────────────────────────────────────────────
    if (mEnemySoundReady) {
        glm::vec2 de(mCamPos.x - mEnemyPos.x, mCamPos.z - mEnemyPos.z);
        float dist    = glm::length(de);
        float t       = glm::clamp(1.f - dist / MAP_HALF, 0.f, 1.f);
        float volume  = t * t * t;

        mLfoPhase += dt * 0.8f * 6.2832f;
        if (mLfoPhase > 6.2832f) mLfoPhase -= 6.2832f;
        double baseFreq  = 52.0 + 14.0 * (double)t;
        double lfoFreq   = baseFreq + 6.0 * std::sin((double)mLfoPhase);

        ma_waveform_set_frequency(&mEnemyWaveform, lfoFreq);
        ma_sound_set_volume(&mEnemySound, volume * 0.9f);
    }

    bool los = hasLOS(mEnemyPos, mCamPos);
    if (los) {
        mLastSeenPlayer  = mCamPos;
        mEnemyHasSeen    = true;
        mEnemyWanderDest = mCamPos;
        mWanderTimer     = 0.f;
    } else if (mEnemyHasSeen) {
        glm::vec2 toTarget(mEnemyWanderDest.x - mEnemyPos.x,
                           mEnemyWanderDest.z - mEnemyPos.z);
        if (glm::length(toTarget) < 2.f) {
            mWanderTimer += dt;
            if (mWanderTimer > 2.5f) {
                mWanderTimer = 0.f;
                float rx = ((float)std::rand() / RAND_MAX) * 2.f * MAP_HALF - MAP_HALF;
                float rz = ((float)std::rand() / RAND_MAX) * 2.f * MAP_HALF - MAP_HALF;
                mEnemyWanderDest = {rx, 0.f, rz};
            }
        }
    }

    // Mover enemigo
    {
        glm::vec2 toT(mEnemyWanderDest.x - mEnemyPos.x,
                      mEnemyWanderDest.z - mEnemyPos.z);
        float d = glm::length(toT);
        if (d > 0.2f) {
            glm::vec2 dir = toT / d;
            float step = mEnemySpeed * dt;
            glm::vec3 next = mEnemyPos;
            next.x += dir.x * step;
            next.z += dir.y * step;
            resolveVsWorld(next, 2.0f);
            mEnemyPos = next;
            // orientar el modelo hacia donde camina
            mEnemyYaw = glm::degrees(std::atan2(dir.x, dir.y));
        }
        mEnemyPos.y = 0.f;
    }

    // Colisión jugador-enemigo
    glm::vec2 dp(mCamPos.x - mEnemyPos.x, mCamPos.z - mEnemyPos.z);
    if (glm::length(dp) < 3.5f) {
        mResult      = BodegaResult::Lost;
        mPhase       = BodegaPhase::Result;
        mResultTimer = 0.f;
        return false;
    }

    // Victoria
    if (mElapsed >= SURVIVAL_TIME) {
        mResult      = BodegaResult::Won;
        mPhase       = BodegaPhase::Result;
        mResultTimer = 0.f;
    }
    return false;
}

// ─── colisiones ───────────────────────────────────────────────────────────────

void BodegaGame::resolveVsWorld(glm::vec3& pos, float r) const {
    for (const auto& b : mBoxes) {
        glm::vec2 p2(pos.x, pos.z);
        glm::vec2 mn(b.center.x - b.halfSize.x, b.center.z - b.halfSize.z);
        glm::vec2 mx(b.center.x + b.halfSize.x, b.center.z + b.halfSize.z);
        glm::vec2 cl   = glm::clamp(p2, mn, mx);
        glm::vec2 diff = p2 - cl;
        float d2 = glm::dot(diff, diff);
        if (d2 < r*r && d2 > 1e-8f) {
            float d = std::sqrt(d2);
            pos.x += diff.x / d * (r - d);
            pos.z += diff.y / d * (r - d);
        }
    }
    pos.x = glm::clamp(pos.x, -MAP_HALF + r, MAP_HALF - r);
    pos.z = glm::clamp(pos.z, -MAP_HALF + r, MAP_HALF - r);
}

bool BodegaGame::segVsAABB2D(glm::vec2 a, glm::vec2 b, glm::vec2 mn, glm::vec2 mx) {
    glm::vec2 d = b - a;
    float tmin = 0.f, tmax = 1.f;
    for (int i = 0; i < 2; ++i) {
        float di = i==0 ? d.x  : d.y;
        float ai = i==0 ? a.x  : a.y;
        float lo = i==0 ? mn.x : mn.y;
        float hi = i==0 ? mx.x : mx.y;
        if (std::abs(di) < 1e-6f) {
            if (ai < lo || ai > hi) return false;
        } else {
            float t1 = (lo-ai)/di, t2 = (hi-ai)/di;
            if (t1 > t2) std::swap(t1, t2);
            tmin = std::max(tmin, t1);
            tmax = std::min(tmax, t2);
            if (tmin > tmax) return false;
        }
    }
    return true;
}

bool BodegaGame::hasLOS(glm::vec3 from, glm::vec3 to) const {
    glm::vec2 p0(from.x, from.z), p1(to.x, to.z);
    for (const auto& b : mBoxes) {
        glm::vec2 mn(b.center.x - b.halfSize.x, b.center.z - b.halfSize.z);
        glm::vec2 mx(b.center.x + b.halfSize.x, b.center.z + b.halfSize.z);
        if (segVsAABB2D(p0, p1, mn, mx)) return false;
    }
    return true;
}

// ─── helpers de render ────────────────────────────────────────────────────────

void BodegaGame::renderGeom(Shader& sh, glm::vec3 center, glm::vec3 halfSize,
                              glm::vec3 color,
                              const glm::mat4& view, const glm::mat4& proj) {
    glm::mat4 model = glm::scale(
        glm::translate(glm::mat4(1.f), center),
        halfSize * 2.f);
    sh.setMat4("uModel",      glm::value_ptr(model));
    sh.setMat4("uView",       glm::value_ptr(view));
    sh.setMat4("uProjection", glm::value_ptr(proj));
    sh.setVec3("uBaseColor",  color.r, color.g, color.b);
    sh.setInt("uHasTexture",  0);
    glBindVertexArray(mBoxVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
}

static void drawRect2D(Shader& sh, GLuint qvao, const glm::mat4& op,
                       float x, float y, float w, float h,
                       glm::vec4 col, Texture& tex) {
    sh.use();
    glm::mat4 m = glm::scale(glm::translate(glm::mat4(1.f), {x,y,0.f}), {w,h,1.f});
    sh.setMat4("uModel",      glm::value_ptr(m));
    sh.setMat4("uProjection", glm::value_ptr(op));
    sh.setVec4("uColor",      col.r, col.g, col.b, col.a);
    sh.setInt("uTexture",     0);
    tex.bind(0);
    glBindVertexArray(qvao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

// ─── renderScene (geometría 3D compartida) ────────────────────────────────────

void BodegaGame::renderScene(Shader& modelShader,
                              const glm::mat4& view, const glm::mat4& proj3d) {
    modelShader.use();
    modelShader.setVec3("uCamPos",              mCamPos.x, mCamPos.y, mCamPos.z);
    modelShader.setVec3("uAmbientColor",        0.08f, 0.08f, 0.09f);
    modelShader.setVec3("uMainLightDir",        0.f,   1.f,   0.f);
    modelShader.setVec3("uMainLightColor",      1.0f,  0.92f, 0.78f);
    modelShader.setFloat("uMainLightIntensity", 0.80f);
    modelShader.setVec3("uFillLightDir",        0.5f,  0.3f,  0.5f);
    modelShader.setVec3("uFillLightColor",      0.35f, 0.42f, 0.58f);
    modelShader.setFloat("uFillLightIntensity", 0.38f);
    modelShader.setFloat("uShininess",          16.f);
    modelShader.setFloat("uSpecIntensity",      0.12f);

    // Piso
    renderGeom(modelShader, {0.f, -0.1f, 0.f}, {MAP_HALF, 0.1f, MAP_HALF},
               {0.30f, 0.30f, 0.32f}, view, proj3d);
    // Techo
    renderGeom(modelShader, {0.f, WALL_H + 0.1f, 0.f}, {MAP_HALF, 0.1f, MAP_HALF},
               {0.18f, 0.18f, 0.20f}, view, proj3d);
    // Paredes
    const glm::vec3 WC = {0.24f, 0.24f, 0.26f};
    float wh = WALL_H * 0.5f;
    renderGeom(modelShader, { 0.f,    wh, -MAP_HALF}, {MAP_HALF, wh, 0.4f}, WC, view, proj3d);
    renderGeom(modelShader, { 0.f,    wh,  MAP_HALF}, {MAP_HALF, wh, 0.4f}, WC, view, proj3d);
    renderGeom(modelShader, {-MAP_HALF, wh, 0.f},     {0.4f, wh, MAP_HALF}, WC, view, proj3d);
    renderGeom(modelShader, { MAP_HALF, wh, 0.f},     {0.4f, wh, MAP_HALF}, WC, view, proj3d);

    // Cajas
    for (const auto& b : mBoxes)
        renderGeom(modelShader, b.center, b.halfSize, b.color, view, proj3d);

    // Enemigo
    if (mEnemyModelLoaded) {
        // Rotar el modelo para que mire hacia donde camina
        glm::mat4 enemyModel = glm::translate(glm::mat4(1.f), mEnemyPos);
        enemyModel = glm::rotate(enemyModel, glm::radians(mEnemyYaw), {0.f, 1.f, 0.f});
        enemyModel = glm::scale(enemyModel, glm::vec3(mEnemyComputedScale));

        modelShader.setMat4("uModel",      glm::value_ptr(enemyModel));
        modelShader.setMat4("uView",       glm::value_ptr(view));
        modelShader.setMat4("uProjection", glm::value_ptr(proj3d));
        mEnemyModel.draw(modelShader, view, proj3d, enemyModel);
    } else {
        // fallback: cubo rojo
        renderGeom(modelShader,
                   mEnemyPos + glm::vec3(0.f, 4.5f, 0.f),
                   {1.5f, 4.5f, 1.5f},
                   {0.85f, 0.10f, 0.10f}, view, proj3d);
        // "ojos"
        glm::vec3 eyeDir   = glm::normalize(glm::vec3(mCamPos.x - mEnemyPos.x, 0.f,
                                                       mCamPos.z - mEnemyPos.z));
        glm::vec3 eyeRight = glm::normalize(glm::cross(eyeDir, mCamUp));
        auto eyeAt = [&](float s) {
            return mEnemyPos + eyeDir*1.51f + eyeRight*(s*0.6f) + mCamUp*5.f;
        };
        renderGeom(modelShader, eyeAt( 1.f), {0.22f,0.28f,0.06f}, {1.f,1.f,1.f}, view, proj3d);
        renderGeom(modelShader, eyeAt(-1.f), {0.22f,0.28f,0.06f}, {1.f,1.f,1.f}, view, proj3d);
    }
}

// ─── render principal ─────────────────────────────────────────────────────────

void BodegaGame::render(Shader& modelShader, Shader& spriteShader, GLuint quadVAO,
                         TextRenderer& tr, const glm::mat4& orthoProj,
                         int W, int H, Texture& whiteTex) {
    glClearColor(0.05f, 0.05f, 0.07f, 1.f);
    glEnable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glm::mat4 view   = glm::lookAt(mCamPos, mCamPos + mCamFront, mCamUp);
    glm::mat4 proj3d = glm::perspective(glm::radians(70.f), (float)W/(float)H, 0.05f, 500.f);

    // La escena 3D se dibuja siempre (incluso en instrucciones / cuenta regresiva)
    renderScene(modelShader, view, proj3d);

    glDisable(GL_DEPTH_TEST);

    if (mPhase == BodegaPhase::Instructions) {
        renderInstructions(spriteShader, quadVAO, tr, orthoProj, W, H, whiteTex);
    } else if (mPhase == BodegaPhase::Countdown) {
        renderCountdown(spriteShader, quadVAO, tr, orthoProj, W, H, whiteTex);
    } else {
        renderHUD(spriteShader, quadVAO, tr, orthoProj, W, H, whiteTex);
    }
}

// ─── pantalla de instrucciones ────────────────────────────────────────────────

void BodegaGame::renderInstructions(Shader& spriteShader, GLuint quadVAO, TextRenderer& tr,
                                     const glm::mat4& op, int W, int H, Texture& whiteTex) {
    // Panel semitransparente centrado
    const float PW = 680.f, PH = 500.f;
    float px = (W - PW) * 0.5f;
    float py = (H - PH) * 0.5f;

    // Sombra
    drawRect2D(spriteShader, quadVAO, op, px+6, py+6, PW, PH, {0.f,0.f,0.f,0.55f}, whiteTex);
    // Fondo panel
    drawRect2D(spriteShader, quadVAO, op, px, py, PW, PH, {0.06f,0.06f,0.08f,0.94f}, whiteTex);
    // Borde izquierdo naranja (acento)
    drawRect2D(spriteShader, quadVAO, op, px, py, 5.f, PH, {0.90f,0.55f,0.05f,1.f}, whiteTex);
    // Línea separadora bajo el título
    drawRect2D(spriteShader, quadVAO, op, px+16, py+70, PW-32, 2.f, {0.35f,0.35f,0.40f,0.8f}, whiteTex);

    float lx = px + 28.f;   // margen izquierdo del texto
    float cy = py + 18.f;   // cursor Y

    auto line = [&](const std::string& s, float scale, glm::vec3 col, float extraY = 0.f) {
        tr.renderText(s, lx, cy + extraY, scale, col);
        cy += tr.getTextSize(s, scale).y + 8.f + extraY;
    };

    const glm::vec3 ORANGE = {0.95f, 0.60f, 0.10f};
    const glm::vec3 WHITE  = {0.95f, 0.92f, 0.88f};
    const glm::vec3 GRAY   = {0.62f, 0.62f, 0.65f};
    const glm::vec3 CYAN   = {0.25f, 0.85f, 1.00f};

    line("BODEGA  -  MINIJUEGO",  0.70f, ORANGE);
    cy += 8.f; // espacio tras la línea separadora

    line("OBJETIVO", 0.48f, ORANGE);
    line("Sobrevive 60 segundos sin ser atrapado.", 0.40f, WHITE);
    line("Escondete detras de las cajas gigantes", 0.40f, WHITE);
    line("para perder de vista al enemigo.", 0.40f, WHITE);
    cy += 10.f;

    line("CONTROLES", 0.48f, ORANGE);
    line("W A S D      Moverse",  0.40f, WHITE);
    line("Mouse        Girar camara", 0.40f, WHITE);
    line("SHIFT        Correr  (consume stamina)", 0.40f, WHITE);
    cy += 10.f;

    line("STAMINA", 0.48f, ORANGE);
    line("Corre hasta agotar la barra roja.", 0.40f, WHITE);
    line("Al recargarse al 100% se activa un", 0.40f, WHITE);

    // resaltar IMPULSO en cian
    float iy = cy;
    tr.renderText("IMPULSO", lx, iy, 0.40f, CYAN);
    glm::vec2 iw = tr.getTextSize("IMPULSO", 0.40f);
    tr.renderText(" de velocidad automatico.", lx + iw.x, iy, 0.40f, WHITE);
    cy += iw.y + 8.f;

    cy += 18.f;
    // Texto parpadeante "Presiona..."
    std::string prompt = "Presiona cualquier tecla para empezar";
    glm::vec2 psz = tr.getTextSize(prompt, 0.42f);
    float flash = 0.62f + 0.38f * std::sin((float)glfwGetTime() * 3.f);
    tr.renderText(prompt, (W - psz.x) * 0.5f, py + PH - 44.f,
                  0.42f, glm::vec3(1.f) * flash);
}

// ─── cuenta regresiva ─────────────────────────────────────────────────────────

void BodegaGame::renderCountdown(Shader& spriteShader, GLuint quadVAO, TextRenderer& tr,
                                  const glm::mat4& op, int W, int H, Texture& whiteTex) {
    // Overlay oscuro leve
    drawRect2D(spriteShader, quadVAO, op, 0, 0, (float)W, (float)H,
               {0.f, 0.f, 0.f, 0.35f}, whiteTex);

    std::string num;
    glm::vec3   col;
    if      (mCountdown > 2.f) { num = "3"; col = {1.f,  0.35f, 0.20f}; }
    else if (mCountdown > 1.f) { num = "2"; col = {1.f,  0.80f, 0.10f}; }
    else if (mCountdown > 0.f) { num = "1"; col = {0.25f,1.f,   0.35f}; }
    else                        { num = "GO!"; col = {0.25f,0.85f,1.00f}; }

    // pulso de escala según fracción del segundo actual
    float frac  = mCountdown - std::floor(mCountdown);
    float scale = 2.0f + (1.f - frac) * 0.8f;

    glm::vec2 sz = tr.getTextSize(num, scale);
    tr.renderText(num, (W - sz.x) * 0.5f, (H - sz.y) * 0.5f, scale, col);
}

// ─── HUD de juego ─────────────────────────────────────────────────────────────

void BodegaGame::renderHUD(Shader& spriteShader, GLuint quadVAO, TextRenderer& tr,
                            const glm::mat4& op, int W, int H, Texture& whiteTex) {
    // Barra de stamina
    const float BW = 440.f, BH = 24.f;
    float bx = (W - BW) * 0.5f;
    float by = H - 90.f;

    drawRect2D(spriteShader, quadVAO, op, bx-3, by-3, BW+6, BH+6,
               {0.03f,0.03f,0.04f,0.88f}, whiteTex);

    glm::vec3 barCol;
    float fill = mStamina;
    std::string label, hint;

    if (mBoostActive) {
        float p = 0.55f + 0.45f * std::sin(mBoostTimer * 9.f);
        barCol = glm::mix(glm::vec3(0.15f,0.65f,1.f), glm::vec3(0.9f,0.9f,1.f), p);
        fill   = 1.f - mBoostTimer / BOOST_DURATION;
        label  = "! IMPULSO ACTIVO !";
        hint   = "Velocidad maxima temporal";
    } else if (mExhausted) {
        barCol = {0.85f, 0.18f, 0.10f};
        label  = "AGOTADO";
        hint   = "Esperando recarga... al llegar al 100% se activa el IMPULSO";
    } else if (mStamina < 0.30f) {
        barCol = {0.95f, 0.68f, 0.05f};
        label  = "STAMINA BAJA";
        hint   = "Suelta SHIFT para recuperar";
    } else {
        barCol = {0.22f, 0.84f, 0.32f};
        label  = "STAMINA";
        hint   = "SHIFT = correr  |  Al agotar la barra se activa el IMPULSO";
    }

    if (fill > 0.f)
        drawRect2D(spriteShader, quadVAO, op, bx, by, BW*fill, BH,
                   {barCol.r,barCol.g,barCol.b,0.92f}, whiteTex);

    glm::vec2 lsz = tr.getTextSize(label, 0.40f);
    tr.renderText(label, (W-lsz.x)*0.5f, by+BH+6.f, 0.40f, barCol);
    glm::vec2 hsz = tr.getTextSize(hint, 0.29f);
    tr.renderText(hint,  (W-hsz.x)*0.5f, by-20.f,   0.29f, {0.60f,0.60f,0.62f});

    // Timer
    int rem = std::max(0, (int)std::ceil(SURVIVAL_TIME - mElapsed));
    std::string ts = "Sobrevive: " + std::to_string(rem) + "s";
    glm::vec3 tc = rem <= 10 ? glm::vec3(1.f,0.22f,0.22f) : glm::vec3(0.95f,0.90f,0.80f);
    tr.renderText(ts, (float)W - 290.f, 28.f, 0.55f, tc);

    // Mira
    const float CH = 11.f;
    float cx = W * 0.5f, cy2 = H * 0.5f;
    drawRect2D(spriteShader,quadVAO,op, cx-CH, cy2-1.5f, CH*2.f, 3.f, {1.f,1.f,1.f,0.55f}, whiteTex);
    drawRect2D(spriteShader,quadVAO,op, cx-1.5f, cy2-CH, 3.f, CH*2.f, {1.f,1.f,1.f,0.55f}, whiteTex);

    // Overlay resultado
    if (mPhase == BodegaPhase::Result) {
        float alpha = std::min(mResultTimer/0.9f, 0.75f);
        drawRect2D(spriteShader,quadVAO,op, 0,0,(float)W,(float)H, {0.f,0.f,0.f,alpha}, whiteTex);
        std::string msg    = mResult==BodegaResult::Won ? "SOBREVIVISTE!" : "TE ATRAPARON!";
        glm::vec3   msgCol = mResult==BodegaResult::Won ? glm::vec3(0.28f,1.f,0.42f)
                                                        : glm::vec3(1.f,0.18f,0.18f);
        glm::vec2 msz = tr.getTextSize(msg, 1.1f);
        tr.renderText(msg, (W-msz.x)*0.5f, H*0.38f, 1.1f, msgCol);
        std::string sub = "Regresando a la casa...";
        glm::vec2 ssz = tr.getTextSize(sub, 0.45f);
        tr.renderText(sub, (W-ssz.x)*0.5f, H*0.52f, 0.45f, {0.75f,0.75f,0.75f});
    }
}
