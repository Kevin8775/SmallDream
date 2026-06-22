#include "BanoGame.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

// ─── helpers internos ─────────────────────────────────────────────────────────

static void drawRect2D_b(Shader& sh, GLuint qvao, const glm::mat4& op,
                          float x, float y, float w, float h,
                          glm::vec4 col, Texture& tex) {
    sh.use();
    glm::mat4 m = glm::scale(glm::translate(glm::mat4(1.f), {x, y, 0.f}), {w, h, 1.f});
    sh.setMat4("uModel",      glm::value_ptr(m));
    sh.setMat4("uProjection", glm::value_ptr(op));
    sh.setVec4("uColor",      col.r, col.g, col.b, col.a);
    sh.setInt("uTexture",     0);
    tex.bind(0);
    glBindVertexArray(qvao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

static float randF(float lo, float hi) {
    return lo + (hi - lo) * ((float)std::rand() / (float)RAND_MAX);
}

// ─── init / destroy ───────────────────────────────────────────────────────────

void BanoGame::init(int /*W*/, int /*H*/) {
    mCamPos   = { 0.f, EYE_H, -(MAP_L - 5.f) };
    mCamFront = glm::normalize(glm::vec3(0.f, 0.f, 1.f));
    mYaw      = 90.f;
    mPitch    = 0.f;
    mCamUp    = { 0.f, 1.f, 0.f };

    mVelocity = { 0.f, 0.f, 0.f };

    mStunned    = false;
    mStunTimer  = 0.f;
    mWetFlash   = 0.f;

    for (auto& d : mDrops) d.active = false;
    mNextSpawn  = 1.5f;
    mHitCount   = 0;
    mResetFlash = 0.f;

    mPhase        = BanoPhase::Instructions;
    mResult       = BanoResult::Playing;
    mCountdown    = 3.f;
    mResultTimer  = 0.f;
    mWantsExit    = false;
    mAnyKeyWaiting = true;

    buildRoom();
    if (!mBoxVAO) buildBoxVAO();
}

void BanoGame::destroy() {
    if (mBoxVAO) { glDeleteVertexArrays(1, &mBoxVAO); mBoxVAO = 0; }
    if (mBoxVBO) { glDeleteBuffers(1, &mBoxVBO);      mBoxVBO = 0; }
}

// ─── diseño del baño a escala gigante ────────────────────────────────────────
// El jugador mide 1.7u. Las paredes miden WALL_H = 30u → todo se ve enorme.
// El corredor mide MAP_W*2 = 40u de ancho y MAP_L*2 = 180u de largo.

void BanoGame::buildRoom() {
    mBoxes.clear();

    const glm::vec3 TILE    = { 0.88f, 0.88f, 0.90f };
    const glm::vec3 STALL   = { 0.76f, 0.78f, 0.82f };
    const glm::vec3 CURTAIN = { 0.50f, 0.65f, 0.88f };
    const glm::vec3 SINK    = { 0.72f, 0.74f, 0.76f };
    const glm::vec3 DIVIDER = { 0.80f, 0.80f, 0.84f };

    auto add = [&](float cx, float cy, float cz,
                   float hw, float hh, float hd, glm::vec3 col) {
        mBoxes.push_back({{ cx, cy, cz }, { hw, hh, hd }, col});
    };

    // ── Cabinas de ducha lado izquierdo ───────────────────────────────────
    // Cada cabina: base 5u de profundidad × 8u de ancho × 12u de alto
    // Separadas ~18u en Z para dejar pasillos amplios
    float stallsL[] = { -60.f, -42.f, -24.f, -6.f, 12.f, 30.f, 48.f, 66.f };
    for (float z : stallsL) {
        // mampara trasera de la cabina
        add(-(MAP_W - 5.f), 6.f, z,       4.f, 6.f, 4.f, STALL);
        // cortina colgante (delgada y alta)
        add(-(MAP_W - 5.f), 8.f, z + 5.5f, 0.2f, 8.f, 1.5f, CURTAIN);
    }

    // ── Cabinas lado derecho (desfasadas para crear camino en S) ──────────
    float stallsR[] = { -70.f, -52.f, -34.f, -16.f, 2.f, 20.f, 38.f, 56.f, 74.f };
    for (float z : stallsR) {
        add((MAP_W - 5.f), 6.f, z,        4.f, 6.f, 4.f, STALL);
        add((MAP_W - 5.f), 8.f, z + 5.5f,  0.2f, 8.f, 1.5f, CURTAIN);
    }

    // ── Divisores centrales (rompen línea de caída de las gotas) ──────────
    add( 4.f, 8.f, -40.f,  2.f, 8.f, 0.4f, DIVIDER);
    add(-4.f, 8.f, -20.f,  2.f, 8.f, 0.4f, DIVIDER);
    add( 6.f, 8.f,   0.f,  2.f, 8.f, 0.4f, DIVIDER);
    add(-6.f, 8.f,  20.f,  2.f, 8.f, 0.4f, DIVIDER);
    add( 3.f, 8.f,  50.f,  2.f, 8.f, 0.4f, DIVIDER);
    add(-3.f, 8.f,  70.f,  2.f, 8.f, 0.4f, DIVIDER);

    // ── Lavabos zona de salida ─────────────────────────────────────────────
    add(-(MAP_W - 3.f), 3.f, 78.f,  3.f, 3.f, 1.5f, SINK);
    add(-(MAP_W - 3.f), 3.f, 84.f,  3.f, 3.f, 1.5f, SINK);
    add( (MAP_W - 3.f), 3.f, 78.f,  3.f, 3.f, 1.5f, SINK);
    add( (MAP_W - 3.f), 3.f, 84.f,  3.f, 3.f, 1.5f, SINK);
}

// ─── VAO cubo unitario ────────────────────────────────────────────────────────

void BanoGame::buildBoxVAO() {
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

// ─── spawn de gota ────────────────────────────────────────────────────────────

void BanoGame::spawnDrop() {
    for (auto& d : mDrops) {
        if (!d.active) {
            // Spawnear en un anillo alrededor del jugador:
            //   radio mínimo → no cae justo encima (da tiempo de reacción)
            //   radio máximo → sigue siendo amenaza cercana
            // Además se sesga hacia adelante (dirección +Z del jugador)
            // para que caigan en su camino y no detrás de él.
            const float R_MIN = 4.0f;
            const float R_MAX = 18.0f;

            float angle  = randF(0.f, 6.2832f);
            float radius = randF(R_MIN, R_MAX);

            // Sesgo hacia adelante: desplazar el centro del anillo
            // en la dirección plana en que mira el jugador
            glm::vec3 flatFront = glm::normalize(
                glm::vec3(mCamFront.x, 0.f, mCamFront.z));
            const float FORWARD_BIAS = 6.0f;

            float spawnX = mCamPos.x + flatFront.x * FORWARD_BIAS
                           + std::cos(angle) * radius;
            float spawnZ = mCamPos.z + flatFront.z * FORWARD_BIAS
                           + std::sin(angle) * radius;

            // Clampear dentro de los límites del baño
            spawnX = glm::clamp(spawnX, -(MAP_W - 3.f), MAP_W - 3.f);
            spawnZ = glm::clamp(spawnZ, -(MAP_L - 8.f), GOAL_Z - 5.f);

            d.pos         = { spawnX, WALL_H - 1.f, spawnZ };
            d.vy          = 0.f;
            d.active      = true;
            d.hit         = false;
            d.splashTimer = 0.f;
            d.splashMax   = 0.70f;
            break;
        }
    }
}

// ─── colisiones ───────────────────────────────────────────────────────────────

void BanoGame::resolveVsWorld(glm::vec3& pos, float r) const {
    for (const auto& b : mBoxes) {
        glm::vec2 p2(pos.x, pos.z);
        glm::vec2 mn(b.center.x - b.half.x, b.center.z - b.half.z);
        glm::vec2 mx(b.center.x + b.half.x, b.center.z + b.half.z);
        glm::vec2 cl   = glm::clamp(p2, mn, mx);
        glm::vec2 diff = p2 - cl;
        float d2 = glm::dot(diff, diff);
        if (d2 < r*r && d2 > 1e-8f) {
            float d = std::sqrt(d2);
            pos.x += diff.x / d * (r - d);
            pos.z += diff.y / d * (r - d);
        }
    }
    pos.x = glm::clamp(pos.x, -MAP_W + r, MAP_W - r);
    pos.z = glm::clamp(pos.z, -(MAP_L - r), MAP_L - r);
}

// ─── update ───────────────────────────────────────────────────────────────────

bool BanoGame::update(float dt, GLFWwindow* window,
                       double& lastMX, double& lastMY, bool& firstMouse) {

    // ── instrucciones ──────────────────────────────────────────────────────
    if (mPhase == BanoPhase::Instructions) {
        if (mAnyKeyWaiting) {
            bool any = false;
            for (int k = GLFW_KEY_SPACE; k <= GLFW_KEY_LAST; ++k)
                if (glfwGetKey(window, k) == GLFW_PRESS) { any = true; break; }
            if (!any) mAnyKeyWaiting = false;
            return false;
        }
        for (int k = GLFW_KEY_SPACE; k <= GLFW_KEY_LAST; ++k) {
            if (glfwGetKey(window, k) == GLFW_PRESS) {
                mPhase     = BanoPhase::Countdown;
                mCountdown = 3.f;
                firstMouse = true;
                return false;
            }
        }
        return false;
    }

    // ── cuenta regresiva ───────────────────────────────────────────────────
    if (mPhase == BanoPhase::Countdown) {
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
        if (mCountdown <= -0.8f) mPhase = BanoPhase::Playing;
        return false;
    }

    // ── resultado ──────────────────────────────────────────────────────────
    if (mPhase == BanoPhase::Result) {
        mResultTimer += dt;
        if (mResultTimer > 3.5f) mWantsExit = true;
        return false;
    }

    // ── jugando ────────────────────────────────────────────────────────────
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) return true;

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

    if (mWetFlash   > 0.f) mWetFlash   = std::max(0.f, mWetFlash   - dt * 1.8f);
    if (mResetFlash > 0.f) mResetFlash = std::max(0.f, mResetFlash - dt * 0.9f);

    // ── aturdimiento ───────────────────────────────────────────────────────
    if (mStunned) {
        mStunTimer -= dt;
        mVelocity.x *= std::pow(0.20f, dt);
        mVelocity.z *= std::pow(0.20f, dt);
        mCamPos     += mVelocity * dt;
        mCamPos.y    = EYE_H;
        resolveVsWorld(mCamPos, 0.45f);
        if (mStunTimer <= 0.f) mStunned = false;
    } else {
        // ── movimiento resbaladizo ─────────────────────────────────────────
        glm::vec3 flatFront = glm::normalize(glm::vec3(mCamFront.x, 0.f, mCamFront.z));
        glm::vec3 right     = glm::normalize(glm::cross(flatFront, mCamUp));
        glm::vec3 wishDir(0.f);
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) wishDir += flatFront;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) wishDir -= flatFront;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) wishDir -= right;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) wishDir += right;

        float wlen = glm::length(wishDir);
        if (wlen > 0.f) {
            wishDir /= wlen;
            glm::vec3 accel = wishDir * ACCEL * dt;
            mVelocity.x += accel.x;
            mVelocity.z += accel.z;
        }

        // Fricción muy baja — suelo empapado
        float fric = std::pow(FRICTION, dt);
        mVelocity.x *= fric;
        mVelocity.z *= fric;

        // Velocidad máxima
        glm::vec2 hv(mVelocity.x, mVelocity.z);
        float sp = glm::length(hv);
        if (sp > MAX_SPEED) {
            hv = hv / sp * MAX_SPEED;
            mVelocity.x = hv.x;
            mVelocity.z = hv.y;
        }

        mCamPos   += mVelocity * dt;
        mCamPos.y  = EYE_H;
        resolveVsWorld(mCamPos, 0.45f);
    }

    // ── victoria ───────────────────────────────────────────────────────────
    if (mCamPos.z >= GOAL_Z) {
        mResult      = BanoResult::Won;
        mPhase       = BanoPhase::Result;
        mResultTimer = 0.f;
        return false;
    }

    // ── gotas ──────────────────────────────────────────────────────────────
    mNextSpawn -= dt;
    if (mNextSpawn <= 0.f) {
        spawnDrop();
        mNextSpawn = randF(DROP_SPAWN_MIN, DROP_SPAWN_MAX);
    }

    for (auto& d : mDrops) {
        if (!d.active) continue;

        if (!d.hit) {
            d.vy    -= DROP_GRAVITY * dt;
            d.pos.y += d.vy * dt;

            if (d.pos.y <= DROP_RADIUS) {
                d.pos.y = DROP_RADIUS;
                d.hit   = true;

                glm::vec2 toPlayer(mCamPos.x - d.pos.x, mCamPos.z - d.pos.z);
                float dist = glm::length(toPlayer);

                if (dist < SPLASH_RADIUS && !mStunned) {
                    float strength = (1.f - dist / SPLASH_RADIUS) * KNOCKBACK;
                    if (dist > 0.05f) {
                        glm::vec2 pushDir = toPlayer / dist;
                        mVelocity.x += pushDir.x * strength;
                        mVelocity.z += pushDir.y * strength;
                    } else {
                        float angle = randF(0.f, 6.28f);
                        mVelocity.x += std::cos(angle) * strength;
                        mVelocity.z += std::sin(angle) * strength;
                    }
                    mWetFlash = 0.55f + (1.f - dist / SPLASH_RADIUS) * 0.45f;

                    if (dist < STUN_RADIUS) {
                        mStunned   = true;
                        mStunTimer = STUN_DURATION;
                        mWetFlash  = 1.f;
                    }

                    // Acumular golpes: al tercer impacto reiniciar al inicio
                    mHitCount++;
                    if (mHitCount >= 3) {
                        mHitCount   = 0;
                        mCamPos     = { 0.f, EYE_H, -(MAP_L - 5.f) };
                        mVelocity   = { 0.f, 0.f, 0.f };
                        mStunned    = false;
                        mResetFlash = 1.f;
                        // Cancelar todas las gotas activas para dar un respiro
                        for (auto& drop : mDrops) drop.active = false;
                        mNextSpawn = 2.0f;
                    }
                }
            }
        } else {
            d.splashTimer += dt;
            if (d.splashTimer >= d.splashMax) d.active = false;
        }
    }

    return false;
}

// ─── render helpers ───────────────────────────────────────────────────────────

void BanoGame::renderGeom(Shader& sh, glm::vec3 center, glm::vec3 half,
                           glm::vec3 col,
                           const glm::mat4& view, const glm::mat4& proj) {
    glm::mat4 model = glm::scale(glm::translate(glm::mat4(1.f), center), half * 2.f);
    sh.setMat4("uModel",      glm::value_ptr(model));
    sh.setMat4("uView",       glm::value_ptr(view));
    sh.setMat4("uProjection", glm::value_ptr(proj));
    sh.setVec3("uBaseColor",  col.r, col.g, col.b);
    sh.setInt("uHasTexture",  0);
    glBindVertexArray(mBoxVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
}

// ─── renderScene ──────────────────────────────────────────────────────────────

void BanoGame::renderScene(Shader& sh, const glm::mat4& view, const glm::mat4& proj) {
    sh.use();
    sh.setVec3("uCamPos", mCamPos.x, mCamPos.y, mCamPos.z);

    // Iluminación suave y uniforme — el baño tiene luz tenue de techo
    sh.setVec3("uAmbientColor",        0.38f, 0.40f, 0.42f);
    sh.setVec3("uMainLightDir",        0.f,   1.f,   0.f);      // luz cenital
    sh.setVec3("uMainLightColor",      0.90f, 0.92f, 0.95f);
    sh.setFloat("uMainLightIntensity", 0.55f);
    sh.setVec3("uFillLightDir",        0.f,  -1.f,   0.f);      // rebote suave del suelo
    sh.setVec3("uFillLightColor",      0.80f, 0.84f, 0.88f);
    sh.setFloat("uFillLightIntensity", 0.18f);
    sh.setFloat("uShininess",          24.f);
    sh.setFloat("uSpecIntensity",      0.08f);

    // Linterna desactivada — la visibilidad la limita el vapor, no la oscuridad
    sh.setInt("uFlashlightEnabled", 0);

    // ── Niebla de vapor: blanca/gris azulada, densidad media ─────────────
    // A ~12u ya empieza a ser perceptible; a ~28u casi todo es vapor blanco
    sh.setInt  ("uFogEnabled",  1);
    sh.setFloat("uFogDensity",  FOG_DENSITY);
    sh.setVec3 ("uFogColor",    0.86f, 0.89f, 0.92f);   // blanco con tinte frío/vapor

    // Piso (baldosas alternadas)
    const glm::vec3 TILE_A = { 0.84f, 0.84f, 0.87f };
    const glm::vec3 TILE_B = { 0.68f, 0.68f, 0.72f };
    int tilesZ = (int)(MAP_L / 5.f) + 1;
    for (int i = -tilesZ; i <= tilesZ; ++i) {
        glm::vec3 fc = ((i & 1) == 0) ? TILE_A : TILE_B;
        renderGeom(sh, { 0.f, -0.05f, i * 5.f }, { MAP_W, 0.05f, 5.f }, fc, view, proj);
    }

    // Techo
    renderGeom(sh, { 0.f, WALL_H + 0.05f, 0.f }, { MAP_W, 0.05f, MAP_L },
               { 0.75f, 0.75f, 0.78f }, view, proj);

    // Paredes laterales
    const glm::vec3 WC = { 0.70f, 0.71f, 0.74f };
    float wh = WALL_H * 0.5f;
    renderGeom(sh, { -MAP_W, wh, 0.f }, { 0.4f, wh, MAP_L }, WC, view, proj);
    renderGeom(sh, {  MAP_W, wh, 0.f }, { 0.4f, wh, MAP_L }, WC, view, proj);
    // Pared trasera
    renderGeom(sh, { 0.f, wh, -(MAP_L + 0.4f) }, { MAP_W, wh, 0.4f }, WC, view, proj);
    // Pared de la meta (verde)
    renderGeom(sh, { 0.f, wh, MAP_L + 0.4f }, { MAP_W, wh, 0.4f },
               { 0.25f, 0.70f, 0.40f }, view, proj);

    // Línea de meta en el suelo
    renderGeom(sh, { 0.f, 0.02f, GOAL_Z }, { MAP_W * 0.9f, 0.02f, 0.5f },
               { 0.18f, 1.00f, 0.42f }, view, proj);

    // Franja indicadora en la pared del fondo
    renderGeom(sh, { 0.f, wh * 0.5f, MAP_L - 0.2f }, { MAP_W * 0.7f, wh * 0.15f, 0.2f },
               { 0.20f, 0.90f, 0.50f }, view, proj);

    // Obstáculos
    for (const auto& b : mBoxes)
        renderGeom(sh, b.center, b.half, b.color, view, proj);

    // ── gotas ──────────────────────────────────────────────────────────────
    for (const auto& d : mDrops) {
        if (!d.active) continue;
        if (!d.hit) {
            // Gota cayendo — esfera aproximada, azul brillante para que el jugador la vea
            renderGeom(sh, d.pos,
                       { DROP_RADIUS, DROP_RADIUS * 1.5f, DROP_RADIUS },
                       { 0.30f, 0.60f, 1.00f }, view, proj);
            // Sombra de advertencia en el suelo (crece y oscurece al acercarse)
            float t = 1.f - glm::clamp(d.pos.y / WALL_H, 0.f, 1.f);
            float sr = 1.5f + t * (SPLASH_RADIUS * 0.8f);
            renderGeom(sh, { d.pos.x, 0.03f, d.pos.z }, { sr, 0.03f, sr },
                       { 0.08f, 0.12f, 0.38f }, view, proj);
        } else {
            // Splash expansivo
            float p = d.splashTimer / d.splashMax;
            float sr = SPLASH_RADIUS * p;
            glm::vec3 sc = glm::mix(glm::vec3(0.28f, 0.58f, 1.f),
                                    glm::vec3(0.65f, 0.80f, 1.f), p);
            renderGeom(sh, { d.pos.x, 0.05f + 0.1f * p, d.pos.z },
                       { sr, 0.05f, sr }, sc, view, proj);
        }
    }

    // Desactivar linterna y niebla para no afectar el resto del juego
    sh.setInt("uFlashlightEnabled", 0);
    sh.setInt("uFogEnabled", 0);
}

// ─── render principal ─────────────────────────────────────────────────────────

void BanoGame::render(Shader& modelShader, Shader& spriteShader, GLuint quadVAO,
                       TextRenderer& tr, const glm::mat4& orthoProj,
                       int W, int H, Texture& whiteTex) {
    glClearColor(0.86f, 0.89f, 0.92f, 1.f);   // mismo color que el vapor
    glEnable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glm::mat4 view   = glm::lookAt(mCamPos, mCamPos + mCamFront, mCamUp);
    glm::mat4 proj3d = glm::perspective(glm::radians(70.f),
                                        (float)W / (float)H, 0.05f, 300.f);

    renderScene(modelShader, view, proj3d);

    glDisable(GL_DEPTH_TEST);

    // Flash azul al ser golpeado por una gota
    if (mWetFlash > 0.01f) {
        drawRect2D_b(spriteShader, quadVAO, orthoProj,
                     0, 0, (float)W, (float)H,
                     { 0.12f, 0.38f, 0.85f, mWetFlash * 0.50f }, whiteTex);
    }
    // Flash rojo intenso al ser reiniciado al inicio
    if (mResetFlash > 0.01f) {
        drawRect2D_b(spriteShader, quadVAO, orthoProj,
                     0, 0, (float)W, (float)H,
                     { 0.80f, 0.05f, 0.05f, mResetFlash * 0.70f }, whiteTex);
    }

    // Overlay de aturdimiento
    if (mStunned) {
        float a = (mStunTimer / STUN_DURATION) * 0.45f;
        drawRect2D_b(spriteShader, quadVAO, orthoProj,
                     0, 0, (float)W, (float)H,
                     { 0.04f, 0.12f, 0.48f, a }, whiteTex);
    }

    if (mPhase == BanoPhase::Instructions) {
        renderInstructions(spriteShader, quadVAO, tr, orthoProj, W, H, whiteTex);
    } else if (mPhase == BanoPhase::Countdown) {
        renderCountdown(spriteShader, quadVAO, tr, orthoProj, W, H, whiteTex);
    } else {
        renderHUD(spriteShader, quadVAO, tr, orthoProj, W, H, whiteTex);
    }
}

// ─── instrucciones ────────────────────────────────────────────────────────────

void BanoGame::renderInstructions(Shader& sp, GLuint qvao, TextRenderer& tr,
                                   const glm::mat4& op, int W, int H, Texture& wt) {
    const float PW = 700.f, PH = 520.f;
    float px = (W - PW) * 0.5f;
    float py = (H - PH) * 0.5f;

    drawRect2D_b(sp, qvao, op, px+6, py+6, PW, PH, {0.f,0.f,0.f,0.55f}, wt);
    drawRect2D_b(sp, qvao, op, px, py, PW, PH, {0.03f,0.05f,0.10f,0.95f}, wt);
    drawRect2D_b(sp, qvao, op, px, py, 5.f, PH, {0.25f,0.60f,1.00f,1.f}, wt);
    drawRect2D_b(sp, qvao, op, px+16, py+70, PW-32, 2.f, {0.20f,0.35f,0.55f,0.8f}, wt);

    float lx = px + 28.f;
    float cy = py + 18.f;

    auto line = [&](const std::string& s, float scale, glm::vec3 col, float extra = 0.f) {
        tr.renderText(s, lx, cy + extra, scale, col);
        cy += tr.getTextSize(s, scale).y + 8.f + extra;
    };

    const glm::vec3 BLUE  = { 0.35f, 0.72f, 1.00f };
    const glm::vec3 WHITE = { 0.92f, 0.94f, 0.96f };
    const glm::vec3 CYAN  = { 0.25f, 0.95f, 0.95f };
    const glm::vec3 RED   = { 1.00f, 0.32f, 0.25f };

    line("BANO  -  MINIJUEGO", 0.70f, BLUE);
    cy += 8.f;

    line("OBJETIVO", 0.48f, BLUE);
    line("Llega al otro extremo del bano.", 0.40f, WHITE);
    line("La salida esta marcada en verde al final.", 0.40f, WHITE);
    cy += 8.f;

    line("PELIGROS", 0.48f, BLUE);
    line("Gotas gigantes caen del techo.", 0.40f, WHITE);
    line("El impacto te empuja lejos de tu camino.", 0.40f, WHITE);
    {
        float ry = cy;
        tr.renderText("Caer muy cerca: ", lx, ry, 0.40f, WHITE);
        glm::vec2 rw = tr.getTextSize("Caer muy cerca: ", 0.40f);
        tr.renderText("DERRIBO TOTAL", lx + rw.x, ry, 0.40f, RED);
        cy += tr.getTextSize("Caer muy cerca: ", 0.40f).y + 8.f;
    }
    cy += 8.f;

    line("SUELO", 0.48f, BLUE);
    {
        float sy = cy;
        tr.renderText("Suelo mojado:", lx, sy, 0.40f, WHITE);
        glm::vec2 sw = tr.getTextSize("Suelo mojado:", 0.40f);
        tr.renderText(" controla tu inercia.", lx + sw.x, sy, 0.40f, CYAN);
        cy += tr.getTextSize("Suelo mojado:", 0.40f).y + 8.f;
    }
    cy += 8.f;

    line("CONTROLES", 0.48f, BLUE);
    line("W A S D   Moverse    |    Mouse   Girar camara", 0.38f, WHITE);

    std::string prompt = "Presiona cualquier tecla para empezar";
    glm::vec2 psz = tr.getTextSize(prompt, 0.42f);
    float flash = 0.62f + 0.38f * std::sin((float)glfwGetTime() * 3.f);
    tr.renderText(prompt, (W - psz.x) * 0.5f, py + PH - 44.f,
                  0.42f, glm::vec3(1.f) * flash);
}

// ─── cuenta regresiva ─────────────────────────────────────────────────────────

void BanoGame::renderCountdown(Shader& sp, GLuint qvao, TextRenderer& tr,
                                const glm::mat4& op, int W, int H, Texture& wt) {
    drawRect2D_b(sp, qvao, op, 0, 0, (float)W, (float)H, {0.f,0.f,0.f,0.35f}, wt);

    std::string num;
    glm::vec3   col;
    if      (mCountdown > 2.f) { num = "3";   col = {1.f,  0.35f, 0.20f}; }
    else if (mCountdown > 1.f) { num = "2";   col = {1.f,  0.80f, 0.10f}; }
    else if (mCountdown > 0.f) { num = "1";   col = {0.25f, 1.f,  0.35f}; }
    else                        { num = "YA!"; col = {0.25f, 0.85f, 1.f }; }

    float frac  = mCountdown - std::floor(mCountdown);
    float scale = 2.0f + (1.f - frac) * 0.8f;
    glm::vec2 sz = tr.getTextSize(num, scale);
    tr.renderText(num, (W - sz.x) * 0.5f, (H - sz.y) * 0.5f, scale, col);
}

// ─── HUD ──────────────────────────────────────────────────────────────────────

void BanoGame::renderHUD(Shader& sp, GLuint qvao, TextRenderer& tr,
                          const glm::mat4& op, int W, int H, Texture& wt) {
    // Barra de progreso
    const float PW = 360.f, PH = 18.f;
    float px = (W - PW) * 0.5f;
    float py = (float)H - 80.f;

    float startZ   = -(MAP_L - 5.f);
    float progress = glm::clamp((mCamPos.z - startZ) / (GOAL_Z - startZ), 0.f, 1.f);

    drawRect2D_b(sp, qvao, op, px-3, py-3, PW+6, PH+6, {0.03f,0.04f,0.08f,0.88f}, wt);
    if (progress > 0.f)
        drawRect2D_b(sp, qvao, op, px, py, PW * progress, PH,
                     {0.20f, 0.90f, 0.50f, 0.90f}, wt);

    std::string progLabel = "Progreso";
    glm::vec2 plsz = tr.getTextSize(progLabel, 0.36f);
    tr.renderText(progLabel, (W - plsz.x) * 0.5f, py + PH + 6.f,
                  0.36f, { 0.50f, 0.88f, 0.62f });

    // Mira
    const float CH = 10.f;
    float cx = W * 0.5f, cy2 = H * 0.5f;
    drawRect2D_b(sp, qvao, op, cx-CH, cy2-1.f, CH*2.f, 2.f, {1.f,1.f,1.f,0.50f}, wt);
    drawRect2D_b(sp, qvao, op, cx-1.f, cy2-CH, 2.f, CH*2.f, {1.f,1.f,1.f,0.50f}, wt);

    // Indicador de aturdimiento
    if (mStunned) {
        std::string stunMsg = "! DERRIBADO !";
        glm::vec2 smz = tr.getTextSize(stunMsg, 0.65f);
        float pulse = 0.7f + 0.3f * std::sin((float)glfwGetTime() * 6.f);
        tr.renderText(stunMsg, (W - smz.x) * 0.5f, H * 0.35f,
                      0.65f, glm::vec3(0.25f, 0.60f, 1.f) * pulse);
    }

    // Contador de golpes (corazones / vidas antes del reinicio)
    {
        const glm::vec3 HIT_COL[3] = {
            {0.30f, 0.65f, 1.00f},   // 0 golpes — azul
            {1.00f, 0.72f, 0.10f},   // 1 golpe  — naranja
            {1.00f, 0.20f, 0.20f},   // 2 golpes  — rojo
        };
        std::string dots = "";
        for (int i = 0; i < 3; ++i) dots += (i < mHitCount) ? "X " : "O ";
        glm::vec3 hcol = HIT_COL[glm::clamp(mHitCount, 0, 2)];
        tr.renderText(dots, 24.f, (float)H - 52.f, 0.55f, hcol);
        std::string hlabel = "Golpes (3 = reinicio)";
        tr.renderText(hlabel, 24.f, (float)H - 26.f, 0.30f, {0.55f, 0.58f, 0.62f});
    }

    // Aviso de gota cercana
    float closestDrop = 9999.f;
    for (const auto& d : mDrops) {
        if (!d.active || d.hit) continue;
        glm::vec2 dh(d.pos.x - mCamPos.x, d.pos.z - mCamPos.z);
        if (glm::length(dh) < closestDrop) closestDrop = glm::length(dh);
    }
    if (closestDrop < SPLASH_RADIUS + 2.f) {
        float warnPulse = 0.6f + 0.4f * std::sin((float)glfwGetTime() * 8.f);
        std::string warn = "! GOTA CERCA !";
        glm::vec2 wsz = tr.getTextSize(warn, 0.50f);
        tr.renderText(warn, (W - wsz.x) * 0.5f, H * 0.72f,
                      0.50f, glm::vec3(0.35f, 0.70f, 1.f) * warnPulse);
    }

    // Overlay resultado
    if (mPhase == BanoPhase::Result) {
        float alpha = std::min(mResultTimer / 0.9f, 0.78f);
        drawRect2D_b(sp, qvao, op, 0, 0, (float)W, (float)H, {0.f,0.f,0.f,alpha}, wt);
        bool won = mResult == BanoResult::Won;
        std::string msg    = won ? "LLEGASTE!" : "DERRIBADO SIN PODER CONTINUAR";
        glm::vec3   msgCol = won ? glm::vec3(0.25f, 1.f, 0.50f)
                                 : glm::vec3(0.30f, 0.60f, 1.f);
        float msc = won ? 1.1f : 0.75f;
        glm::vec2 msz = tr.getTextSize(msg, msc);
        tr.renderText(msg, (W - msz.x) * 0.5f, H * 0.38f, msc, msgCol);
        std::string sub = "Regresando a la casa...";
        glm::vec2 ssz = tr.getTextSize(sub, 0.45f);
        tr.renderText(sub, (W - ssz.x) * 0.5f, H * 0.52f, 0.45f, {0.70f,0.72f,0.75f});
    }
}
