#pragma once
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include "Shader.h"
#include "TextRenderer.h"
#include "Texture.h"

class EscapeRoom {
public:
    enum class Fase { Instrucciones, Explorando, MostrandoPista, IngresandoCodigo, Completado };

    struct Objeto {
        std::string id;
        glm::vec3 posicion;
        glm::vec3 tamano;
        std::string mensajeAccion;
        std::string fragmento;
        std::string texto;
        bool encontrado = false;
    };

    struct PanelCodigo {
        glm::vec3 posicion;
        glm::vec3 tamano;
        int digitos[4] = { 0,0,0,0 };
        int seleccion = 0;
        bool activo = false;
        bool desbloqueado = false;
        float errorTimer = 0.0f;
        float exitoTimer = 0.0f;
        static constexpr int CODIGO_CORRECTO[4] = { 1,9,4,5 };
    };

    void init();
    void destroy();

    void update(float dt, const glm::vec3& playerPos,
                bool eKeyDown, bool qKey, bool rightClick,
                bool leftKey, bool rightKey, bool upKey, bool downKey,
                bool escKey);

    void render(Shader& modelShader, const glm::mat4& view, const glm::mat4& proj3d,
                Shader& spriteShader, GLuint quadVAO,
                TextRenderer& tr, const glm::mat4& orthoProj,
                int W, int H, Texture& whiteTex);

    Fase fase() const { return mFase; }
    bool completo() const { return mFase == Fase::Completado; }
    bool quiereSalir() const { return mQuiereSalir; }
    int pistasEncontradas() const { return mPistasEncontradas; }
    int totalPistas() const { return mTotalPistas; }
    const std::vector<Objeto>& objetos() const { return mObjetos; }
    bool checkPanelProximity(const glm::vec3& playerPos);
    bool inventarioAbierto() const { return mInventarioAbierto; }

private:
    Fase mFase = Fase::Explorando;
    std::vector<Objeto> mObjetos;
    PanelCodigo mPanel;
    std::string mNotifTexto;
    float mNotifTimer = 0.0f;
    float mNotifDuracion = 3.0f;
    int mPistasEncontradas = 0;
    int mTotalPistas = 4;
    bool mQuiereSalir = false;
    float mCompletadoTimer = 0.0f;
    bool mPrimeraActualizacion = true;
    bool mEKeyAnterior = false;
    bool mQKeyAnterior = false;
    bool mInventarioAbierto = false;
    bool mPanelKeyLeft = false;
    bool mPanelKeyRight = false;
    bool mPanelKeyUp = false;
    bool mPanelKeyDown = false;
    bool mEscapeAnterior = false;
    float mTiempoAnim = 0.0f;
    GLuint mMarkerVAO = 0;
    GLuint mMarkerVBO = 0;

    void configurarPistas();
    Objeto* checkInteraction(const glm::vec3& playerPos);
    void ocultarPista();
    void buildMarkerVAO();
    void renderMarker(Shader& sh, const glm::mat4& view, const glm::mat4& proj,
                      const glm::vec3& pos, float tiempo) const;
};
