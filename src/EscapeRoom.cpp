#include "EscapeRoom.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>

static void renderSprite(GLuint vao, Shader& shader, Texture& tex, const glm::mat4& proj,
                         float x, float y, float w, float h, const glm::vec4& color) {
    shader.use();
    glm::mat4 model(1.0f);
    model = glm::translate(model, glm::vec3(x + w * 0.5f, y + h * 0.5f, 0.0f));
    model = glm::scale(model, glm::vec3(w * 0.5f, h * 0.5f, 1.0f));
    shader.setMat4("uModel", glm::value_ptr(model));
    shader.setMat4("uProjection", glm::value_ptr(proj));
    shader.setVec4("uColor", color.x, color.y, color.z, color.w);
    shader.setInt("uTexture", 0);
    tex.bind(0);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

// ── Posiciones de objetos interactivos ──────────────────────────────────
static constexpr glm::vec3 POS_LIBRO      = glm::vec3(-49.762676f, -20.209679f,  35.574062f);
static constexpr glm::vec3 POS_CUADRO     = glm::vec3( 54.740967f, -20.209679f, -7.492583f);
static constexpr glm::vec3 POS_MANZANAS   = glm::vec3( 52.835140f, -20.209679f, 37.520798f);
static constexpr glm::vec3 POS_LAMPARA    = glm::vec3( -1.324061f, -20.209679f, 32.100532f);
static constexpr glm::vec3 POS_PANEL      = glm::vec3( -9.619739f, -20.209679f, 37.842411f);
static constexpr glm::vec3 TAMANO_INTERACCION = glm::vec3(3.5f, 3.0f, 3.5f);
static constexpr glm::vec3 TAMANO_PANEL       = glm::vec3(5.0f, 3.5f, 5.0f);

// ── Inicializaci├│n ────────────────────────────────────────────────────────

void EscapeRoom::init() {
    mFase = Fase::Instrucciones;
    mPistasEncontradas = 0;
    mQuiereSalir = false;
    mCompletadoTimer = 0.0f;
    mNotifTimer = 0.0f;
    mNotifTexto.clear();
    mPanel = PanelCodigo();
    mPanel.seleccion = 0;
    mPanel.activo = false;
    mPrimeraActualizacion = true;
    mTiempoAnim = 0.0f;
    mInventarioAbierto = false;
    configurarPistas();
    buildMarkerVAO();
}

void EscapeRoom::destroy() {
    mObjetos.clear();
    if (mMarkerVAO) { glDeleteVertexArrays(1, &mMarkerVAO); mMarkerVAO = 0; }
    if (mMarkerVBO) { glDeleteBuffers(1, &mMarkerVBO); mMarkerVBO = 0; }
}

// ── Configurar las 5 pistas ─────────────────────────────────────────────────

void EscapeRoom::configurarPistas() {
    mObjetos.clear();

    mObjetos.push_back({
        "libro", POS_LIBRO, TAMANO_INTERACCION,
        "Presiona E para abrir el libro",
        "1",
        "El libro marca pagina 45. Ano 1945. El primer digito es 1."
    });

    mObjetos.push_back({
        "cuadro", POS_CUADRO, TAMANO_INTERACCION,
        "Presiona E para levantar el cuadro",
        "9",
        "Detras del cuadro: el segundo digito es 9."
    });

    mObjetos.push_back({
        "manzanas", POS_MANZANAS, TAMANO_INTERACCION,
        "Presiona E para revisar las manzanas",
        "4",
        "Nota: Quedan 4. Ya encontre 1, 9 y faltaba el 4."
    });

    mObjetos.push_back({
        "lampara", POS_LAMPARA, TAMANO_INTERACCION,
        "Presiona E para inspeccionar la lampara",
        "5",
        "Base de la lampara grabada: 1 - 9 - 4 - 5. El ultimo digito es 5."
    });

    mTotalPistas = (int)mObjetos.size();

    mPanel.posicion = POS_PANEL;
    mPanel.tamano = TAMANO_PANEL;
}

// ── Marcador 3D (octaedro) ──────────────────────────────────────────────────

void EscapeRoom::buildMarkerVAO() {
    struct Vert { float px, py, pz, nx, ny, nz; };
    auto addTri = [](std::vector<Vert>& v, glm::vec3 a, glm::vec3 b, glm::vec3 c) {
        glm::vec3 n = glm::normalize(glm::cross(b - a, c - a));
        v.push_back({a.x,a.y,a.z, n.x,n.y,n.z});
        v.push_back({b.x,b.y,b.z, n.x,n.y,n.z});
        v.push_back({c.x,c.y,c.z, n.x,n.y,n.z});
    };

    std::vector<Vert> verts;
    glm::vec3 top(0,0.4f,0), bot(0,-0.4f,0);
    glm::vec3 f(0,0,0.4f), bk(0,0,-0.4f);
    glm::vec3 l(-0.4f,0,0), r(0.4f,0,0);

    addTri(verts, top, r,  f);
    addTri(verts, top, f,  l);
    addTri(verts, top, l, bk);
    addTri(verts, top, bk, r);
    addTri(verts, bot, f,  r);
    addTri(verts, bot, l,  f);
    addTri(verts, bot, bk, l);
    addTri(verts, bot, r, bk);

    glGenVertexArrays(1, &mMarkerVAO);
    glGenBuffers(1, &mMarkerVBO);
    glBindVertexArray(mMarkerVAO);
    glBindBuffer(GL_ARRAY_BUFFER, mMarkerVBO);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(Vert), verts.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vert), (void*)offsetof(Vert, px));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vert), (void*)offsetof(Vert, nx));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}

void EscapeRoom::renderMarker(Shader& sh, const glm::mat4& view, const glm::mat4& proj,
                              const glm::vec3& pos, float tiempo) const {
    if (!mMarkerVAO) return;
    float pulse = 1.0f + std::sin(tiempo * 3.0f) * 0.2f;
    float angle = tiempo * 0.8f;
    glm::mat4 model(1.0f);
    model = glm::translate(model, pos);
    model = glm::rotate(model, angle, glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::scale(model, glm::vec3(pulse));
    sh.use();
    sh.setMat4("uModel", glm::value_ptr(model));
    sh.setMat4("uView", glm::value_ptr(view));
    sh.setMat4("uProjection", glm::value_ptr(proj));
    sh.setVec3("uViewPos", 0.0f, 0.0f, 0.0f);
    sh.setVec3("uSunDir", -0.3f, -0.8f, -0.5f);
    sh.setVec3("uSunColor", 0.9f, 0.85f, 0.7f);
    sh.setFloat("uSunIntensity", 0.8f);
    sh.setVec3("uFillLightDir", -0.6f, 0.1f, 0.8f);
    sh.setVec3("uFillLightColor", 0.3f, 0.4f, 0.7f);
    sh.setFloat("uFillLightIntensity", 0.5f);
    sh.setFloat("uShininess", 64.0f);
    sh.setFloat("uSpecIntensity", 0.8f);
    glBindVertexArray(mMarkerVAO);
    glDrawArrays(GL_TRIANGLES, 0, 24);
    glBindVertexArray(0);
}

// ── Detecci├│n de interacci├│n (AABB + tecla E) ──────────────────────────────

EscapeRoom::Objeto* EscapeRoom::checkInteraction(const glm::vec3& playerPos) {
    for (auto& obj : mObjetos) {
        glm::vec3 d = glm::abs(playerPos - obj.posicion);
        if (d.x < obj.tamano.x && d.y < obj.tamano.y && d.z < obj.tamano.z) {
            return &obj;
        }
    }
    return nullptr;
}

bool EscapeRoom::checkPanelProximity(const glm::vec3& playerPos) {
    glm::vec3 d = glm::abs(playerPos - mPanel.posicion);
    return (d.x < mPanel.tamano.x && d.y < mPanel.tamano.y && d.z < mPanel.tamano.z);
}

// ── Actualizaci├│n ──────────────────────────────────────────────────────────

void EscapeRoom::ocultarPista() {
    mNotifTexto.clear();
    mNotifTimer = 0.0f;
    mFase = Fase::Explorando;
}

void EscapeRoom::update(float dt, const glm::vec3& playerPos,
                        bool eKeyDown, bool qKey, bool rightClick,
                        bool leftKey, bool rightKey, bool upKey, bool downKey,
                        bool escKey)
{
    bool eJustPressed = eKeyDown && !mEKeyAnterior;
    bool escJustPressed = escKey && !mEscapeAnterior;
    bool qJustPressed = qKey && !mQKeyAnterior;
    mEKeyAnterior = eKeyDown;
    mEscapeAnterior = escKey;
    mQKeyAnterior = qKey;

    if (mPrimeraActualizacion) {
        mPrimeraActualizacion = false;
        return;
    }

    mTiempoAnim += dt;

    // Q abre/cierra inventario en cualquier fase excepto completado/instrucciones
    if (qJustPressed && mFase != Fase::Completado && mFase != Fase::Instrucciones) {
        mInventarioAbierto = !mInventarioAbierto;
        if (mInventarioAbierto && mFase == Fase::MostrandoPista) {
            ocultarPista();
        }
        return;
    }

    // Inventario abierto bloquea otras interacciones
    if (mInventarioAbierto) {
        if (escJustPressed || rightClick || qJustPressed) {
            mInventarioAbierto = false;
        }
        return;
    }

    switch (mFase) {

    case Fase::Instrucciones: {
        if (eJustPressed || escJustPressed || mTiempoAnim >= 5.0f) {
            mFase = Fase::Explorando;
            mTiempoAnim = 0.0f;
        }
        break;
    }

    case Fase::Explorando: {
        bool cercaPanel = checkPanelProximity(playerPos);
        if (cercaPanel && eJustPressed) {
            mFase = Fase::IngresandoCodigo;
            mPanel.activo = true;
            mPanel.seleccion = 0;
            mPanelKeyLeft = false;
            mPanelKeyRight = false;
            mPanelKeyUp = false;
            mPanelKeyDown = false;
            break;
        }
        Objeto* obj = checkInteraction(playerPos);
        if (obj && eJustPressed && !obj->encontrado) {
            obj->encontrado = true;
            mPistasEncontradas++;
            mNotifTexto = obj->fragmento;
            mNotifTimer = 0.0f;
            mFase = Fase::MostrandoPista;
        }
        break;
    }

    case Fase::MostrandoPista: {
        mNotifTimer += dt;
        if (eJustPressed || escJustPressed || mNotifTimer >= mNotifDuracion) {
            ocultarPista();
        }
        break;
    }

    case Fase::IngresandoCodigo: {
        if (escJustPressed || rightClick) {
            mPanel.activo = false;
            mFase = Fase::Explorando;
            break;
        }
        if (leftKey && !mPanelKeyLeft) mPanel.seleccion = (mPanel.seleccion - 1 + 4) % 4;
        if (rightKey && !mPanelKeyRight) mPanel.seleccion = (mPanel.seleccion + 1) % 4;
        if (upKey && !mPanelKeyUp) mPanel.digitos[mPanel.seleccion] = (mPanel.digitos[mPanel.seleccion] + 1) % 10;
        if (downKey && !mPanelKeyDown) mPanel.digitos[mPanel.seleccion] = (mPanel.digitos[mPanel.seleccion] - 1 + 10) % 10;

        mPanelKeyLeft = leftKey;
        mPanelKeyRight = rightKey;
        mPanelKeyUp = upKey;
        mPanelKeyDown = downKey;

        if (eJustPressed) {
            bool correcto = true;
            for (int i = 0; i < 4; i++) {
                if (mPanel.digitos[i] != PanelCodigo::CODIGO_CORRECTO[i]) {
                    correcto = false;
                    break;
                }
            }
            if (correcto) {
                mPanel.desbloqueado = true;
                mPanel.exitoTimer = 3.0f;
                mFase = Fase::Completado;
                mCompletadoTimer = 0.0f;
            } else {
                mPanel.errorTimer = 2.0f;
            }
        }
        if (mPanel.errorTimer > 0.0f) mPanel.errorTimer -= dt;
        break;
    }

    case Fase::Completado: {
        mCompletadoTimer += dt;
        if (mPanel.exitoTimer > 0.0f) mPanel.exitoTimer -= dt;
        if (mCompletadoTimer >= 4.0f) mQuiereSalir = true;
        break;
    }

    }
}

// ── Render ──────────────────────────────────────────────────────────────────

void EscapeRoom::render(Shader& modelShader, const glm::mat4& view, const glm::mat4& proj3d,
                        Shader& spriteShader, GLuint quadVAO,
                        TextRenderer& tr, const glm::mat4& orthoProj,
                        int W, int H, Texture& whiteTex)
{
    // Marcadores 3D
    for (const auto& obj : mObjetos) {
        if (!obj.encontrado) {
            renderMarker(modelShader, view, proj3d, obj.posicion, mTiempoAnim);
        }
    }

    glDisable(GL_DEPTH_TEST);

    // ── Instrucciones ──
    if (mFase == Fase::Instrucciones) {
        float alpha = std::min(mTiempoAnim / 0.5f, 1.0f);
        float boxW = 600.0f;
        float boxH = 340.0f;
        float boxX = (W - boxW) / 2.0f;
        float boxY = (H - boxH) / 2.0f;

        renderSprite(quadVAO, spriteShader, whiteTex, orthoProj,
                     0.0f, 0.0f, (float)W, (float)H,
                     glm::vec4(0.0f, 0.0f, 0.0f, 0.7f * alpha));
        renderSprite(quadVAO, spriteShader, whiteTex, orthoProj,
                     boxX, boxY, boxW, boxH,
                     glm::vec4(0.12f, 0.10f, 0.08f, 0.95f * alpha));
        renderSprite(quadVAO, spriteShader, whiteTex, orthoProj,
                     boxX, boxY, boxW, 3.0f,
                     glm::vec4(0.85f, 0.72f, 0.45f, 0.9f * alpha));

        std::string title = "ESCAPE ROOM";
        glm::vec2 tSz = tr.getTextSize(title, 0.65f);
        tr.renderText(title,
                      boxX + (boxW - tSz.x) / 2.0f, boxY + 25.0f,
                      0.65f, glm::vec3(0.95f, 0.85f, 0.55f) * alpha);

        std::vector<std::string> lines = {
            "Has despertado en una habitacion desconocida.",
            "La unica salida esta cerrada con un candado",
            "de 4 digitos.",
            "",
            "Explora la habitacion para encontrar los",
            "4 digitos del codigo ocultos en los objetos.",
            "",
            "Cuando tengas todos, ve al panel numerico",
            "cerca de la puerta e ingresa el codigo.",
            "",
            "[Q] Inventario    [E] Interactuar    [ESC] Salir"
        };

        float yOff = boxY + 75.0f;
        for (const auto& line : lines) {
            glm::vec2 lSz = tr.getTextSize(line, 0.35f);
            tr.renderText(line,
                          boxX + (boxW - lSz.x) / 2.0f, yOff,
                          0.35f, glm::vec3(0.85f, 0.85f, 0.85f) * alpha);
            yOff += 24.0f;
        }

        float fadeIn = std::min((mTiempoAnim - 4.0f) / 1.0f, 1.0f);
        if (fadeIn > 0.0f) {
            std::string hint = "Presiona E para empezar";
            float pulse = 1.0f + std::sin(mTiempoAnim * 4.0f) * 0.05f;
            glm::vec2 hSz = tr.getTextSize(hint, 0.4f * pulse);
            tr.renderText(hint,
                          boxX + (boxW - hSz.x) / 2.0f, boxY + boxH - 35.0f,
                          0.4f * pulse, glm::vec3(1.0f, 1.0f, 1.0f));
        }
        return;
    }

    // ── Notificaci├│n compacta ──
    if (mFase == Fase::MostrandoPista && !mNotifTexto.empty()) {
        float barW = 520.0f;
        float barH = 44.0f;
        float barX = (W - barW) / 2.0f;
        float barY = H - 80.0f;

        float alpha = 1.0f;
        float fadeOut = 0.0f;
        if (mNotifTimer < 0.2f) {
            alpha = mNotifTimer / 0.2f;
        } else if (mNotifTimer > mNotifDuracion - 0.5f) {
            fadeOut = (mNotifTimer - (mNotifDuracion - 0.5f)) / 0.5f;
            alpha = 1.0f - fadeOut;
        }

        renderSprite(quadVAO, spriteShader, whiteTex, orthoProj,
                     barX, barY, barW, barH,
                     glm::vec4(0.08f, 0.07f, 0.06f, 0.92f * alpha));
        renderSprite(quadVAO, spriteShader, whiteTex, orthoProj,
                     barX, barY, barW, 2.0f,
                     glm::vec4(0.85f, 0.72f, 0.45f, 0.9f * alpha));

        std::string notifStr = "Fragmento de codigo: " + mNotifTexto;
        glm::vec2 nSz = tr.getTextSize(notifStr, 0.4f);
        tr.renderText(notifStr,
                      barX + 20.0f, barY + (barH - nSz.y) / 2.0f - 8.0f,
                      0.4f, glm::vec3(0.95f, 0.85f, 0.55f) * alpha);

        std::string progStr = std::to_string(mPistasEncontradas) + "/" + std::to_string(mTotalPistas);
        glm::vec2 progSz = tr.getTextSize(progStr, 0.35f);
        tr.renderText(progStr,
                      barX + barW - progSz.x - 15.0f, barY + (barH - progSz.y) / 2.0f - 6.0f,
                      0.35f, glm::vec3(0.6f, 0.6f, 0.6f) * alpha);
    }

    // ── Inventario ──
    if (mInventarioAbierto) {
        float invW = 440.0f;
        float invH = 340.0f;
        float invX = (W - invW) / 2.0f;
        float invY = (H - invH) / 2.0f;

        renderSprite(quadVAO, spriteShader, whiteTex, orthoProj,
                     0.0f, 0.0f, (float)W, (float)H,
                     glm::vec4(0.0f, 0.0f, 0.0f, 0.65f));
        renderSprite(quadVAO, spriteShader, whiteTex, orthoProj,
                     invX, invY, invW, invH,
                     glm::vec4(0.12f, 0.10f, 0.08f, 0.95f));
        renderSprite(quadVAO, spriteShader, whiteTex, orthoProj,
                     invX, invY, invW, 3.0f,
                     glm::vec4(0.85f, 0.72f, 0.45f, 0.9f));

        std::string invTitle = "FRAGMENTOS RECOLECTADOS";
        glm::vec2 invSz = tr.getTextSize(invTitle, 0.5f);
        tr.renderText(invTitle,
                      invX + (invW - invSz.x) / 2.0f, invY + 15.0f,
                      0.5f, glm::vec3(0.95f, 0.85f, 0.55f));

        float yOff = invY + 65.0f;
        for (int i = 0; i < mTotalPistas; i++) {
            const auto& obj = mObjetos[i];
            if (obj.encontrado) {
                std::string line = obj.fragmento;
                glm::vec2 lSz = tr.getTextSize(line, 0.4f);
                tr.renderText(line,
                              invX + 50.0f, yOff,
                              0.4f, glm::vec3(0.2f, 0.9f, 0.3f));
            } else {
                std::string line = "? ? ? ?";
                tr.renderText(line,
                              invX + 50.0f, yOff,
                              0.4f, glm::vec3(0.4f, 0.4f, 0.4f));
            }
            yOff += 38.0f;
        }

        std::string closeHint = "Clic derecho, Q o ESC para cerrar";
        glm::vec2 chSz = tr.getTextSize(closeHint, 0.3f);
        tr.renderText(closeHint,
                      invX + (invW - chSz.x) / 2.0f, invY + invH - 25.0f,
                      0.3f, glm::vec3(0.5f, 0.5f, 0.5f));
    }

    // ── Panel de c├│digo ──
    if (mFase == Fase::IngresandoCodigo) {
        float panelW = 520.0f;
        float panelH = 260.0f;
        float panelX = (W - panelW) / 2.0f;
        float panelY = (H - panelH) / 2.0f;

        renderSprite(quadVAO, spriteShader, whiteTex, orthoProj,
                     0.0f, 0.0f, (float)W, (float)H,
                     glm::vec4(0.0f, 0.0f, 0.0f, 0.65f));
        renderSprite(quadVAO, spriteShader, whiteTex, orthoProj,
                     panelX, panelY, panelW, panelH,
                     glm::vec4(0.14f, 0.12f, 0.10f, 0.95f));
        renderSprite(quadVAO, spriteShader, whiteTex, orthoProj,
                     panelX, panelY, panelW, 3.0f,
                     glm::vec4(0.45f, 0.72f, 0.85f, 0.9f));

        std::string panelTitle = "PANEL DE CODIGO";
        glm::vec2 ptSz = tr.getTextSize(panelTitle, 0.5f);
        tr.renderText(panelTitle,
                      panelX + (panelW - ptSz.x) / 2.0f, panelY + 15.0f,
                      0.5f, glm::vec3(0.45f, 0.72f, 0.85f));

        float digW = 60.0f, digH = 70.0f, gap = 20.0f;
        float totalDigW = 4.0f * digW + 3.0f * gap;
        float digStartX = panelX + (panelW - totalDigW) / 2.0f;
        float digY = panelY + 70.0f;

        for (int i = 0; i < 4; i++) {
            float dx = digStartX + i * (digW + gap);
            glm::vec4 digBg = (i == mPanel.seleccion)
                ? glm::vec4(0.25f, 0.22f, 0.18f, 1.0f)
                : glm::vec4(0.18f, 0.16f, 0.14f, 1.0f);
            renderSprite(quadVAO, spriteShader, whiteTex, orthoProj,
                         dx, digY, digW, digH, digBg);
            if (i == mPanel.seleccion) {
                renderSprite(quadVAO, spriteShader, whiteTex, orthoProj, dx, digY, digW, 2.0f, glm::vec4(0.85f, 0.72f, 0.45f, 1.0f));
                renderSprite(quadVAO, spriteShader, whiteTex, orthoProj, dx, digY + digH - 2.0f, digW, 2.0f, glm::vec4(0.85f, 0.72f, 0.45f, 1.0f));
                renderSprite(quadVAO, spriteShader, whiteTex, orthoProj, dx, digY, 2.0f, digH, glm::vec4(0.85f, 0.72f, 0.45f, 1.0f));
                renderSprite(quadVAO, spriteShader, whiteTex, orthoProj, dx + digW - 2.0f, digY, 2.0f, digH, glm::vec4(0.85f, 0.72f, 0.45f, 1.0f));
            }
            std::string digitStr = std::to_string(mPanel.digitos[i]);
            glm::vec2 dSz = tr.getTextSize(digitStr, 0.9f);
            tr.renderText(digitStr, dx + (digW - dSz.x) / 2.0f, digY + (digH - dSz.y) / 2.0f - 12.0f, 0.9f, glm::vec3(0.95f));
            if (i == mPanel.seleccion) {
                tr.renderText("▲", dx + (digW - 10.0f) / 2.0f, digY - 18.0f, 0.3f, glm::vec3(0.7f));
                tr.renderText("▼", dx + (digW - 10.0f) / 2.0f, digY + digH + 2.0f, 0.3f, glm::vec3(0.7f));
            }
        }

        std::string controls = "←/→: Seleccionar    ↑/↓: Cambiar valor    E: Confirmar    Clic der: Cancelar";
        glm::vec2 ctrlSz = tr.getTextSize(controls, 0.3f);
        tr.renderText(controls, panelX + (panelW - ctrlSz.x) / 2.0f, digY + digH + 30.0f, 0.3f, glm::vec3(0.6f));

        if (mPanel.errorTimer > 0.0f) {
            float errAlpha = std::min(mPanel.errorTimer / 1.0f, 1.0f);
            std::string errStr = "¡CODIGO INCORRECTO!";
            glm::vec2 errSz = tr.getTextSize(errStr, 0.45f);
            tr.renderText(errStr, panelX + (panelW - errSz.x) / 2.0f, panelY + panelH - 30.0f, 0.45f, glm::vec3(1.0f, 0.2f, 0.2f) * errAlpha);
        }
    }

    // ── Completado ──
    if (mFase == Fase::Completado) {
        float alpha = std::min(mCompletadoTimer / 1.0f, 1.0f);
        float scale = 1.0f + std::sin(mCompletadoTimer * 4.0f) * 0.05f;
        renderSprite(quadVAO, spriteShader, whiteTex, orthoProj, 0.0f, 0.0f, (float)W, (float)H, glm::vec4(0.0f, 0.0f, 0.0f, 0.5f * alpha));
        std::string successStr = "¡HAS ESCAPADO!";
        glm::vec2 sucSz = tr.getTextSize(successStr, 0.9f * scale);
        tr.renderText(successStr, (W - sucSz.x) / 2.0f, H * 0.4f, 0.9f * scale, glm::vec3(0.2f, 1.0f, 0.3f) * alpha);
        std::string subStr = "El codigo era: 1 - 9 - 4 - 5";
        glm::vec2 subSz = tr.getTextSize(subStr, 0.45f);
        tr.renderText(subStr, (W - subSz.x) / 2.0f, H * 0.4f + 60.0f, 0.45f, glm::vec3(0.8f, 0.8f, 0.8f) * alpha);

        for (int i = 0; i < 12; i++) {
            float t = mCompletadoTimer * 2.0f + i * 0.8f;
            float px = (float)W * 0.5f + std::sin(t) * 200.0f;
            float py = (float)H * 0.3f + std::cos(t * 0.7f) * 100.0f - i * 15.0f;
            float sz = 4.0f + std::sin(t * 3.0f + i) * 2.0f;
            float pa = std::max(0.0f, std::sin(t * 2.0f) * alpha);
            renderSprite(quadVAO, spriteShader, whiteTex, orthoProj, px, py, sz, sz, glm::vec4(0.2f, 1.0f, 0.3f, pa * 0.7f));
        }
    }
}
