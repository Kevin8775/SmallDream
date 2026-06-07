#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <vector>
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <string>
#include <algorithm>
#include <functional>
#include <array>

#include "Shader.h"
#include "Texture.h"
#include "TextRenderer.h"
#include "Menu.h"
#include "CubemapTexture.h"
#include "VisualNovel.h"

#define NOMINMAX
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

static const int WINDOW_WIDTH = 1689;
static const int WINDOW_HEIGHT = 917;
static const int SPRITE_FRAMES = 36;
static const float TEXT_SCALE = 0.8f;
static const float SPRITE_X = 900.0f;

enum class AppState {
    Loading,
    Menu,
    Credits,
    CloudTransition,
    DreamLoading,
    DreamBlack,
    VisualNovel,
    DreamState
};

struct CloudTile {
    float startX, startY;
    float endX, endY;
    float delay;
    float sizeScale;
    int frameIndex;
};

static GLFWwindow* gWindow = nullptr;
static Menu* gMenu = nullptr;
static AppState* gStatePtr = nullptr;
static bool gCreditsPaused = false;
static double gCreditsPauseStarted = 0.0;
static double gCreditsPauseAccum = 0.0;

static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS && gStatePtr && *gStatePtr == AppState::Credits) {
        double now = glfwGetTime();
        if (!gCreditsPaused) {
            gCreditsPaused = true;
            gCreditsPauseStarted = now;
        } else {
            gCreditsPaused = false;
            gCreditsPauseAccum += now - gCreditsPauseStarted;
        }
        return;
    }

    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        if (gMenu && gStatePtr && *gStatePtr == AppState::Menu) {
            int hovered = gMenu->getHoveredIndex();
            if (hovered == 0) {
                *gStatePtr = AppState::CloudTransition;
            } else if (hovered == 3) {
                *gStatePtr = AppState::Credits;
                gCreditsPaused = false;
                gCreditsPauseStarted = 0.0;
                gCreditsPauseAccum = 0.0;
            } else if (hovered == 4) {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }
        } else if (gStatePtr && *gStatePtr == AppState::Credits) {
            gCreditsPaused = false;
            gCreditsPauseStarted = 0.0;
            gCreditsPauseAccum = 0.0;
            *gStatePtr = AppState::Menu;
        }
    }
}

static GLuint createQuadVAO() {
    float verts[] = {
        0.0f, 0.0f,  0.0f, 1.0f,
        0.0f, 1.0f,  0.0f, 0.0f,
        1.0f, 1.0f,  1.0f, 0.0f,
        0.0f, 0.0f,  0.0f, 1.0f,
        1.0f, 1.0f,  1.0f, 0.0f,
        1.0f, 0.0f,  1.0f, 1.0f
    };
    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    return vao;
}

static GLuint createCubeVAO() {
    float verts[] = {
        // Front (+Z)
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        // Back (-Z)
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
        // Top (+Y)
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f, -1.0f,
        // Bottom (-Y)
        -1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,
         1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,
        // Right (+X)
         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
        // Left (-X)
        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,
    };
    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    return vao;
}

static Texture* createFlashlightIcon() {
    int size = 32;
    std::vector<unsigned char> pixels(size * size * 4, 0);
    float cx = size / 2.0f, cy = size / 2.0f, r = size / 2.5f;
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            float dx = x - cx, dy = y - cy, dist = std::sqrt(dx * dx + dy * dy);
            int idx = (y * size + x) * 4;
            if (dist < r) {
                float t = dist / r;
                float alpha = (1.0f - t * t) * 255.0f;
                pixels[idx + 0] = 255;
                pixels[idx + 1] = 240;
                pixels[idx + 2] = 180;
                pixels[idx + 3] = (unsigned char)alpha;
            }
        }
    }
    return new Texture(size, size, pixels.data(), GL_RGBA, GL_RGBA);
}

static Texture* createGlowTexture() {
    int size = 128;
    std::vector<unsigned char> pixels(size * size * 4, 0);
    float cx = size / 2.0f, cy = size / 2.0f, r = size / 2.0f;
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            float dx = x - cx, dy = y - cy;
            float dist = std::sqrt(dx * dx + dy * dy) / r;
            if (dist < 1.0f) {
                float alpha = (1.0f - dist * dist) * 255.0f;
                int idx = (y * size + x) * 4;
                pixels[idx + 0] = 255;
                pixels[idx + 1] = 240;
                pixels[idx + 2] = 200;
                pixels[idx + 3] = (unsigned char)alpha;
            }
        }
    }
    return new Texture(size, size, pixels.data(), GL_RGBA, GL_RGBA);
}

static Texture* createWhiteTexture() {
    unsigned char pixel[4] = {255, 255, 255, 255};
    return new Texture(1, 1, pixel, GL_RGBA, GL_RGBA);
}

static void renderSprite(GLuint vao, Shader& shader, Texture& tex, const glm::mat4& proj,
                         float x, float y, float w, float h, const glm::vec4& color) {
    shader.use();
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(x, y, 0.0f));
    model = glm::scale(model, glm::vec3(w, h, 1.0f));
    shader.setMat4("uModel", glm::value_ptr(model));
    shader.setMat4("uProjection", glm::value_ptr(proj));
    shader.setVec4("uColor", color.r, color.g, color.b, color.a);
    shader.setInt("uTexture", 0);
    tex.bind(0);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        if (gStatePtr && *gStatePtr == AppState::Credits) {
            *gStatePtr = AppState::Menu;
            return;
        }
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
}

int main() {
    if (!glfwInit()) {
        std::cerr << "Failed to init GLFW" << std::endl;
        return -1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    gWindow = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "SmallDream", nullptr, nullptr);
    if (!gWindow) {
        std::cerr << "Failed to create window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(gWindow);
    glfwSwapInterval(1);
    glfwSetKeyCallback(gWindow, keyCallback);
    glfwSetMouseButtonCallback(gWindow, mouseButtonCallback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to init GLAD" << std::endl;
        return -1;
    }

    glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    Shader bgShader("assets/shaders/background.vert", "assets/shaders/background.frag");
    Shader spriteShader("assets/shaders/sprite.vert", "assets/shaders/sprite.frag");
    Shader textShader("assets/shaders/text.vert", "assets/shaders/text.frag");

    Texture bgTex("assets/textures/fondo.png");
    Texture logoTex("assets/textures/logo.png");

    Shader skyboxShader("assets/shaders/skybox.vert", "assets/shaders/skybox.frag");

    std::array<std::string, 6> skyboxFaces = {
        "assets/textures/skybox-menu/px.png",
        "assets/textures/skybox-menu/nx.png",
        "assets/textures/skybox-menu/py.png",
        "assets/textures/skybox-menu/ny.png",
        "assets/textures/skybox-menu/pz.png",
        "assets/textures/skybox-menu/nz.png"
    };
    CubemapTexture* skyboxTex = new CubemapTexture(skyboxFaces);

    std::vector<Texture*> spriteFrames;
    for (int i = 0; i < SPRITE_FRAMES; i++) {
        char path[128];
        snprintf(path, sizeof(path), "assets/textures/sprites/frame_%04d.png", SPRITE_FRAMES - 1 - i);
        spriteFrames.push_back(new Texture(path));
    }

    Texture* flashlightIcon = createFlashlightIcon();
    Texture* underlineTex = createWhiteTexture();
    Texture* loadingBarTex = createWhiteTexture();

    TextRenderer textRenderer("assets/fonts/Roboto.ttf", 35, &textShader);
    TextRenderer creditsTextRenderer("assets/fonts/Roboto.ttf", 60, &textShader);
    glm::mat4 proj = glm::ortho(0.0f, (float)WINDOW_WIDTH, (float)WINDOW_HEIGHT, 0.0f);
    textRenderer.setProjection(proj);
    creditsTextRenderer.setProjection(proj);

    Menu menu;
    menu.init(&textRenderer, WINDOW_WIDTH, WINDOW_HEIGHT, TEXT_SCALE);
    gMenu = &menu;

    GLuint quadVAO = createQuadVAO();
    GLuint skyboxVAO = createCubeVAO();

    ma_engine engine;
    ma_engine_config engineConfig = ma_engine_config_init();
    if (ma_engine_init(&engineConfig, &engine) != MA_SUCCESS) {
        std::cerr << "Failed to init audio engine" << std::endl;
        return -1;
    }

    // Load cloud sprites (sprites-nube)
    std::srand((unsigned int)std::time(nullptr));
    std::vector<Texture*> cloudFrames;
    for (int i = 0; i < SPRITE_FRAMES; i++) {
        char path[128];
        snprintf(path, sizeof(path), "assets/textures/sprites-nube/frame_%03d.png", SPRITE_FRAMES - 1 - i);
        cloudFrames.push_back(new Texture(path));
    }

    Texture* glowTex = createGlowTexture();
    Texture* oficinaTex = new Texture("assets/textures/oficina.png");

    // Cloud transition animation
    float cloudAnimTimer = 0.0f;
    float cloudAnimDuration = 4.0f;
    std::vector<CloudTile> cloudTiles;
    bool cloudTilesGenerated = false;
    float dreamLoadingTimer = 0.0f;

    ma_sound loadingLoop;
    bool hasLoadingLoop = ma_sound_init_from_file(&engine, "assets/sounds/ui/loading_loop.wav", 0, nullptr, nullptr, &loadingLoop) == MA_SUCCESS;
    if (hasLoadingLoop) {
        ma_sound_set_looping(&loadingLoop, MA_TRUE);
        ma_sound_start(&loadingLoop);
    }

    ma_sound loadingDone;
    bool hasLoadingDone = false;
    ma_sound ambientLoop;
    bool hasAmbientLoop = false;

    // DreamBlack / VisualNovel
    float dreamBlackTimer = 0.0f;
    ma_sound tecladoSound;
    bool hasTecladoSound = false;
    bool tecladoPlayed = false;
    bool prevMouseDown = false;

    VisualNovel* visualNovel = new VisualNovel(&textRenderer, &spriteShader, quadVAO, proj, WINDOW_WIDTH, WINDOW_HEIGHT);
    visualNovel->setBackground(oficinaTex);

    AppState state = AppState::Loading;
    gStatePtr = &state;
    float loadingProgress = 0.0f;
    float loadingPulse = 0.0f;
    float loadingBarWidth = 600.0f;
    float loadingBarHeight = 26.0f;
    float loadingStepTimer = 0.0f;
    const float loadingStepInterval = 0.18f;
    bool loadingFinished = false;
    float loadingFinishTimer = 0.0f;
    const float loadingFinishHold = 0.9f;

    std::vector<std::function<void()>> loadingTasks;
    size_t loadingTaskIndex = 0;
    for (int i = 0; i < SPRITE_FRAMES; i++) {
        loadingTasks.push_back([&, i]() {
            char path[128];
            snprintf(path, sizeof(path), "assets/textures/sprites/frame_%04d.png", SPRITE_FRAMES - 1 - i);
            spriteFrames.push_back(new Texture(path));
        });
    }

    int currentFrame = 0;

    float spriteY = WINDOW_HEIGHT / 2.0f;

    // For delta time tracking
    double lastTime = glfwGetTime();

    while (!glfwWindowShouldClose(gWindow)) {
        double now = glfwGetTime();
        float dt = (float)(now - lastTime);
        lastTime = now;
        if (dt > 0.05f) dt = 0.05f; // clamp

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        double mx, my;
        glfwGetCursorPos(gWindow, &mx, &my);

        bool mouseDown = glfwGetMouseButton(gWindow, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        bool mouseJustPressed = mouseDown && !prevMouseDown;
        prevMouseDown = mouseDown;

        if (state == AppState::Loading) {
            loadingPulse += dt * 4.0f;
            loadingStepTimer += dt;

            if (!loadingFinished && loadingTaskIndex < loadingTasks.size() && loadingStepTimer >= loadingStepInterval) {
                loadingStepTimer = 0.0f;
                loadingTasks[loadingTaskIndex++]();
                loadingProgress = (float)loadingTaskIndex / (float)loadingTasks.size();
            }

            if (!loadingFinished && loadingTaskIndex >= loadingTasks.size()) {
                loadingFinished = true;
                loadingProgress = 1.0f;
                if (!hasLoadingDone) {
                    hasLoadingDone = ma_sound_init_from_file(&engine, "assets/sounds/ui/loading_done.wav", 0, nullptr, nullptr, &loadingDone) == MA_SUCCESS;
                    if (hasLoadingDone) {
                        ma_sound_start(&loadingDone);
                    }
                }
                if (hasLoadingLoop) {
                    ma_sound_stop(&loadingLoop);
                    ma_sound_uninit(&loadingLoop);
                    hasLoadingLoop = false;
                }
            }

            if (loadingFinished) {
                loadingFinishTimer += dt;
                if (loadingFinishTimer >= loadingFinishHold && hasLoadingDone) {
                    if (!hasAmbientLoop) {
                        hasAmbientLoop = ma_sound_init_from_file(&engine, "assets/sounds/ambient/ambient_loop.wav", 0, nullptr, nullptr, &ambientLoop) == MA_SUCCESS;
                        if (hasAmbientLoop) {
                            ma_sound_set_looping(&ambientLoop, MA_TRUE);
                            ma_sound_start(&ambientLoop);
                        }
                    }
                    state = AppState::Menu;
                }
            }

            bgShader.use();
            bgShader.setInt("uTexture", 0);
            bgShader.setFloat("uBlurAmount", 7.0f);
            bgTex.bind(0);
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::scale(model, glm::vec3(WINDOW_WIDTH, WINDOW_HEIGHT, 1.0f));
            bgShader.setMat4("uModel", glm::value_ptr(model));
            bgShader.setMat4("uProjection", glm::value_ptr(proj));
            glBindVertexArray(quadVAO);
            glDrawArrays(GL_TRIANGLES, 0, 6);

            float lw = (float)logoTex.getWidth();
            float lh = (float)logoTex.getHeight();
            float scalePulse = 0.92f + std::sin(loadingPulse) * 0.025f;
            float logoW = lw * scalePulse;
            float logoH = lh * scalePulse;
            renderSprite(quadVAO, spriteShader, logoTex, proj,
                         (WINDOW_WIDTH - logoW) / 2.0f,
                         WINDOW_HEIGHT * 0.22f,
                         logoW, logoH, glm::vec4(1.0f));

            float barX = (WINDOW_WIDTH - loadingBarWidth) / 2.0f;
            float barY = WINDOW_HEIGHT * 0.72f;
            renderSprite(quadVAO, spriteShader, *loadingBarTex, proj,
                         barX, barY, loadingBarWidth, loadingBarHeight,
                         glm::vec4(0.15f, 0.15f, 0.15f, 0.9f));
            renderSprite(quadVAO, spriteShader, *loadingBarTex, proj,
                         barX, barY, loadingBarWidth * loadingProgress, loadingBarHeight,
                         glm::vec4(0.85f, 0.78f, 0.55f, 0.95f));

            textRenderer.renderText("CARGANDO RECURSOS...", (WINDOW_WIDTH - 220.0f) / 2.0f, barY - 36.0f, 0.5f, glm::vec3(0.9f));
        } else if (state == AppState::Menu) {
            {
                float normalizedY = (float)my / (float)WINDOW_HEIGHT;
                if (normalizedY < 0.0f) normalizedY = 0.0f;
                if (normalizedY > 1.0f) normalizedY = 1.0f;
                currentFrame = (int)(normalizedY * (SPRITE_FRAMES - 1));
            }

            menu.update((float)mx, (float)my);

            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_LEQUAL);

            float rotAngle = (float)glfwGetTime() * 10.0f;
            glm::mat4 skyModel = glm::rotate(glm::mat4(1.0f), glm::radians(rotAngle), glm::vec3(0.0f, 1.0f, 0.0f));
            glm::mat4 skyProj = glm::perspective(glm::radians(90.0f), (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT, 0.1f, 100.0f);
            glm::mat4 skyView = glm::mat4(1.0f);

            skyboxShader.use();
            skyboxShader.setMat4("uProjection", glm::value_ptr(skyProj));
            skyboxShader.setMat4("uView", glm::value_ptr(skyView));
            skyboxShader.setMat4("uModel", glm::value_ptr(skyModel));
            skyboxShader.setInt("uSkybox", 0);
            skyboxTex->bind(0);
            glBindVertexArray(skyboxVAO);
            glDrawArrays(GL_TRIANGLES, 0, 36);

            glDisable(GL_DEPTH_TEST);

            float sw = (float)spriteFrames[0]->getWidth();
            float sh = (float)spriteFrames[0]->getHeight();
            renderSprite(quadVAO, spriteShader, *spriteFrames[currentFrame], proj,
                         SPRITE_X, spriteY - sh / 2.0f, sw, sh, glm::vec4(1.0f));

            float lw = (float)logoTex.getWidth();
            float lh = (float)logoTex.getHeight();
            float lx = (WINDOW_WIDTH - lw) / 2.0f;
            float ly = 30.0f;
            renderSprite(quadVAO, spriteShader, logoTex, proj, lx, ly, lw, lh, glm::vec4(1.0f));

            const auto& items = menu.getItems();
            for (size_t i = 0; i < items.size(); i++) {
                if (items[i].hovered) {
                    float cx = items[i].x + items[i].w / 2.0f;
                    float cy = items[i].y + items[i].h / 2.0f + 15.0f;
                    renderSprite(quadVAO, spriteShader, *glowTex, proj,
                                 cx - 200.0f, cy - 50.0f, 400.0f, 100.0f,
                                 glm::vec4(1.0f, 0.95f, 0.8f, 0.5f));
                }
                glm::vec3 color = items[i].hovered ? glm::vec3(1.0f, 0.95f, 0.8f) : glm::vec3(0.9f, 0.9f, 0.9f);
                textRenderer.renderText(items[i].text, items[i].x, items[i].y, TEXT_SCALE, color);
            }

            textRenderer.renderText("VERSI\u00d3N 0.9a (BUILD ALPHA)", 20.0f, WINDOW_HEIGHT - 30.0f, 0.35f, glm::vec3(0.6f));

            std::string credits = "Hecho por Torres-Chavez-Torrez";
            glm::vec2 sz = textRenderer.getTextSize(credits, 0.35f);
            textRenderer.renderText(credits, WINDOW_WIDTH - sz.x - 20.0f, WINDOW_HEIGHT - 30.0f, 0.35f, glm::vec3(0.6f));
        } else if (state == AppState::Credits) {
            double creditsNow = glfwGetTime();
            double creditsElapsed = gCreditsPaused ? (gCreditsPauseStarted - gCreditsPauseAccum) : (creditsNow - gCreditsPauseAccum);
            float creditsAnim = std::min((float)creditsElapsed * 0.75f, 1.0f);
            float creditsRise = (1.0f - creditsAnim) * 120.0f;
            float creditsAlpha = creditsAnim * creditsAnim;
            float creditsFloat = std::sin((float)creditsElapsed * 0.9f) * 4.0f;

            bgShader.use();
            bgShader.setInt("uTexture", 0);
            bgShader.setFloat("uBlurAmount", 3.5f);
            bgTex.bind(0);
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::scale(model, glm::vec3(WINDOW_WIDTH, WINDOW_HEIGHT, 1.0f));
            bgShader.setMat4("uModel", glm::value_ptr(model));
            bgShader.setMat4("uProjection", glm::value_ptr(proj));
            glBindVertexArray(quadVAO);
            glDrawArrays(GL_TRIANGLES, 0, 6);

            float lw = (float)logoTex.getWidth();
            float lh = (float)logoTex.getHeight();
            float logoScale = 0.36f;
            renderSprite(quadVAO, spriteShader, logoTex, proj,
                         (WINDOW_WIDTH - lw * logoScale) / 2.0f,
                         28.0f,
                         lw * logoScale,
                         lh * logoScale,
                         glm::vec4(1.0f));

            const float centerX = WINDOW_WIDTH * 0.5f;
            const float lineGap = 64.0f;
            const float scrollSpeed = 40.0f;
            const float scrollSpan = 1120.0f;
            const float scrollOffset = std::fmod((float)creditsElapsed * scrollSpeed, scrollSpan);
            const float textCenterY = 220.0f + creditsRise + creditsFloat;

            float panelW = 680.0f;
            float panelH = 430.0f;
            float panelX = (WINDOW_WIDTH - panelW) / 2.0f;
            float panelY = 184.0f + creditsRise;
            renderSprite(quadVAO, spriteShader, *glowTex, proj,
                         panelX, panelY, panelW, panelH,
                         glm::vec4(0.16f, 0.14f, 0.12f, 0.65f * creditsAlpha));

            renderSprite(quadVAO, spriteShader, *glowTex, proj,
                         panelX + 10.0f, panelY + 10.0f, panelW - 20.0f, panelH - 20.0f,
                         glm::vec4(0.08f, 0.08f, 0.10f, 0.45f * creditsAlpha));

            auto drawCentered = [&](const std::string& text, float y, float scale, const glm::vec3& color, float delay = 0.0f) {
                glm::vec2 size = creditsTextRenderer.getTextSize(text, scale);
                float a = std::max(0.0f, std::min(1.0f, creditsAlpha - delay));
                if (y < 96.0f || y > WINDOW_HEIGHT - 40.0f) {
                    a *= 0.0f;
                } else if (y < 160.0f) {
                    a *= (y - 96.0f) / 64.0f;
                }
                creditsTextRenderer.renderText(text, centerX - size.x * 0.5f, y, scale, color, a);
            };

            auto drawRoleBlock = [&](const std::string& role, const std::string& name, float y, float roleDelay, float nameDelay) {
                drawCentered(role, y, 0.42f, glm::vec3(0.74f, 0.80f, 0.88f), roleDelay);
                drawCentered(name, y + 24.0f, 0.58f, glm::vec3(0.92f, 0.92f, 0.90f), nameDelay);
            };

            struct CreditLine {
                std::string text;
                float scale;
                glm::vec3 color;
                float delay;
            };

            std::vector<CreditLine> lines = {
                {"CREDITS", 1.18f, glm::vec3(0.97f, 0.92f, 0.79f), 0.00f},
                {"SmallDream", 0.68f, glm::vec3(0.78f, 0.84f, 0.92f), 0.04f},
                {"Team", 0.82f, glm::vec3(0.92f, 0.90f, 0.85f), 0.08f},          
                {"Torres Guadamuz Miguel Angel", 0.58f, glm::vec3(0.92f, 0.92f, 0.90f), 0.15f},
                {"Torrez Urbina Kevin Gael", 0.58f, glm::vec3(0.92f, 0.92f, 0.90f), 0.21f},        
                {"Ch\u00e1vez Martinez Kevin Fernando", 0.58f, glm::vec3(0.92f, 0.92f, 0.90f), 0.27f},
                {"Program", 0.82f, glm::vec3(0.92f, 0.90f, 0.85f), 0.30f},
                {"National University of Engineering (UNI)", 0.58f, glm::vec3(0.84f), 0.33f},
                {"Computer Engineering", 0.58f, glm::vec3(0.84f), 0.36f},
                {"Graphic Programming course project", 0.54f, glm::vec3(0.80f), 0.39f},
                {"Thank you for playing", 0.50f, glm::vec3(0.80f, 0.76f, 0.66f), 0.42f}
            };

            float startY = WINDOW_HEIGHT + 140.0f;
            float baseY = startY - scrollOffset;
            float cursorY = baseY;

            for (const auto& line : lines) {
                glm::vec2 size = creditsTextRenderer.getTextSize(line.text, line.scale);
                float x = centerX - size.x * 0.5f;
                float a = std::max(0.0f, std::min(1.0f, creditsAlpha - line.delay));
                if (cursorY < 96.0f || cursorY > WINDOW_HEIGHT - 40.0f) {
                    a = 0.0f;
                } else if (cursorY < 160.0f) {
                    a *= (cursorY - 96.0f) / 64.0f;
                }
                creditsTextRenderer.renderText(line.text, x, cursorY, line.scale, line.color, a);
                cursorY += lineGap;
            }

            renderSprite(quadVAO, spriteShader, *glowTex, proj,
                         centerX - 170.0f, 702.0f, 340.0f, 3.0f,
                         glm::vec4(0.95f, 0.82f, 0.55f, 0.9f * creditsAlpha));

            float backW = 320.0f;
            float backH = 54.0f;
            float backX = 24.0f;
            float backY = WINDOW_HEIGHT - backH - 24.0f;
            renderSprite(quadVAO, spriteShader, *glowTex, proj,
                         backX, backY, backW, backH,
                         glm::vec4(0.20f, 0.17f, 0.14f, 0.55f * creditsAlpha));
            creditsTextRenderer.renderText("Click or press ESC to go back", backX + 18.0f, backY + 13.0f, 0.40f, glm::vec3(0.92f, 0.88f, 0.80f), creditsAlpha);
        } else if (state == AppState::CloudTransition) {
            // Generate mosaic tiles on first frame
            if (!cloudTilesGenerated) {
                cloudTilesGenerated = true;
                cloudAnimTimer = 0.0f;
                const float TILE_W = 320.0f;
                const float TILE_H = 154.0f;
                const float OVERLAP = 0.5f;
                int cols = (int)(WINDOW_WIDTH / (TILE_W * OVERLAP)) + 2;
                int rows = (int)(WINDOW_HEIGHT / (TILE_H * OVERLAP)) + 2;
                float centerX = WINDOW_WIDTH / 2.0f;
                float centerY = WINDOW_HEIGHT / 2.0f;
                for (int r = 0; r < rows; r++) {
                    for (int c = 0; c < cols; c++) {
                        CloudTile tile;
                        tile.endX = c * TILE_W * OVERLAP - TILE_W * 0.5f;
                        tile.endY = r * TILE_H * OVERLAP - TILE_H * 0.5f;
                        tile.endX += ((float)std::rand() / (float)RAND_MAX - 0.5f) * 120.0f;
                        tile.endY += ((float)std::rand() / (float)RAND_MAX - 0.5f) * 120.0f;
                        tile.startX = centerX - TILE_W / 2.0f;
                        tile.startY = centerY - TILE_H / 2.0f;
                        tile.frameIndex = 0;
                        tile.sizeScale = 0.8f + ((float)std::rand() / (float)RAND_MAX) * 0.4f;
                        tile.delay = ((float)std::rand() / (float)RAND_MAX) * 0.6f;
                        cloudTiles.push_back(tile);
                    }
                }
            }

            cloudAnimTimer += dt;

            // Update menu items with slide offset
            float slideProgress = std::min(cloudAnimTimer / 1.5f, 1.0f);
            float slideOffset = slideProgress * 400.0f;
            menu.setSlideOffset(slideOffset);

            // Still render skybox and character
            menu.update((float)mx, (float)my);
            {
                float normalizedY = (float)my / (float)WINDOW_HEIGHT;
                if (normalizedY < 0.0f) normalizedY = 0.0f;
                if (normalizedY > 1.0f) normalizedY = 1.0f;
                currentFrame = (int)(normalizedY * (SPRITE_FRAMES - 1));
            }

            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_LEQUAL);
            float rotAngle = (float)glfwGetTime() * 10.0f;
            glm::mat4 skyModel = glm::rotate(glm::mat4(1.0f), glm::radians(rotAngle), glm::vec3(0.0f, 1.0f, 0.0f));
            glm::mat4 skyProj = glm::perspective(glm::radians(90.0f), (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT, 0.1f, 100.0f);
            glm::mat4 skyView = glm::mat4(1.0f);
            skyboxShader.use();
            skyboxShader.setMat4("uProjection", glm::value_ptr(skyProj));
            skyboxShader.setMat4("uView", glm::value_ptr(skyView));
            skyboxShader.setMat4("uModel", glm::value_ptr(skyModel));
            skyboxShader.setInt("uSkybox", 0);
            skyboxTex->bind(0);
            glBindVertexArray(skyboxVAO);
            glDrawArrays(GL_TRIANGLES, 0, 36);
            glDisable(GL_DEPTH_TEST);

            // Character sprite
            float sw = (float)spriteFrames[0]->getWidth();
            float sh = (float)spriteFrames[0]->getHeight();
            renderSprite(quadVAO, spriteShader, *spriteFrames[currentFrame], proj,
                         SPRITE_X, spriteY - sh / 2.0f, sw, sh, glm::vec4(1.0f));

            // Logo
            float lw = (float)logoTex.getWidth();
            float lh = (float)logoTex.getHeight();
            float lx = (WINDOW_WIDTH - lw) / 2.0f;
            float ly = 30.0f;
            renderSprite(quadVAO, spriteShader, logoTex, proj, lx, ly, lw, lh, glm::vec4(1.0f));

            // Render menu items with slide offset
            {
                const auto& items = menu.getItems();
                for (size_t i = 0; i < items.size(); i++) {
                    float itemY = items[i].y + slideOffset;
                    if (items[i].hovered) {
                        float cx = items[i].x + items[i].w / 2.0f;
                        float cy = itemY + items[i].h / 2.0f + 15.0f;
                        renderSprite(quadVAO, spriteShader, *glowTex, proj,
                                     cx - 200.0f, cy - 50.0f, 400.0f, 100.0f,
                                     glm::vec4(1.0f, 0.95f, 0.8f, 0.5f));
                    }
                    glm::vec3 color = items[i].hovered ? glm::vec3(1.0f, 0.95f, 0.8f) : glm::vec3(0.9f, 0.9f, 0.9f);
                    textRenderer.renderText(items[i].text, items[i].x, itemY, TEXT_SCALE, color);
                }
            }

            // ============================================================
            // NUBES (MOSAICO): Se renderizan los tiles de nubes en mosaico
            // Cada tile entra desde el borde más cercano
            // Los sprites se reproducen al revés: frame_035 → frame_000
            // Cubren toda la pantalla en formación de mosaico solapado
            // ============================================================
            float tileW = 320.0f;
            float tileH = 154.0f;
            float totalDuration = cloudAnimDuration;
            for (auto& tile : cloudTiles) {
                float t = (cloudAnimTimer - tile.delay) / (totalDuration - tile.delay);
                if (t < 0.0f) t = 0.0f;
                if (t > 1.0f) t = 1.0f;
                float ease = 1.0f - (1.0f - t) * (1.0f - t);
                float x = tile.startX + (tile.endX - tile.startX) * ease;
                float y = tile.startY + (tile.endY - tile.startY) * ease;

                // Sprite en orden normal: 0 → 35 según avanza t
                int frameIdx = (int)(t * (SPRITE_FRAMES - 1));
                float centerTileX = x + tileW / 2.0f;
                float centerTileY = y + tileH / 2.0f;
                float scale = ease;
                float renW = tileW * scale * tile.sizeScale;
                float renH = tileH * scale * tile.sizeScale;
                renderSprite(quadVAO, spriteShader, *cloudFrames[frameIdx], proj,
                             centerTileX - renW / 2.0f, centerTileY - renH / 2.0f,
                             renW, renH, glm::vec4(1.0f));
            }

            // Check if animation is done
            if (cloudAnimTimer >= cloudAnimDuration) {
                state = AppState::DreamLoading;
                dreamLoadingTimer = 0.0f;
            }
        } else if (state == AppState::DreamLoading) {
            dreamLoadingTimer += dt;

            // Logo fade in during first 0.5s
            float logoAlpha = std::min(dreamLoadingTimer / 0.5f, 1.0f);
            float logoLw = (float)logoTex.getWidth();
            float logoLh = (float)logoTex.getHeight();
            float logoLx = (WINDOW_WIDTH - logoLw) / 2.0f;
            float logoLy = WINDOW_HEIGHT * 0.22f;
            renderSprite(quadVAO, spriteShader, logoTex, proj,
                         logoLx, logoLy, logoLw, logoLh,
                         glm::vec4(1.0f, 1.0f, 1.0f, logoAlpha));

            // Loading bar appears after logo is fully visible, slow
            float barStart = 1.0f;
            float barEnd = 12.0f;
            float barAlpha = std::min((dreamLoadingTimer - barStart) / 0.3f, 1.0f);
            if (barAlpha > 0.0f && dreamLoadingTimer >= barStart) {
                float raw = (dreamLoadingTimer - barStart) / (barEnd - barStart);
                float barProgress = std::min(raw, 1.0f);
                float percent = barProgress * 100.0f;

                float barW = 600.0f;
                float barH = 26.0f;
                float barX = (WINDOW_WIDTH - barW) / 2.0f;
                float barY = WINDOW_HEIGHT * 0.72f;
                renderSprite(quadVAO, spriteShader, *loadingBarTex, proj,
                             barX, barY, barW, barH,
                             glm::vec4(0.15f, 0.15f, 0.15f, 0.9f * barAlpha));
                renderSprite(quadVAO, spriteShader, *loadingBarTex, proj,
                             barX, barY, barW * barProgress, barH,
                             glm::vec4(0.85f, 0.78f, 0.55f, 0.95f * barAlpha));

                // Percentage text with pulsing effect
                float pulse = 1.0f + std::sin(dreamLoadingTimer * 3.0f) * 0.06f;
                float textScale = 0.5f * pulse;
                int pct = (int)percent;
                char pctText[8];
                snprintf(pctText, sizeof(pctText), "%d%%", pct);
                glm::vec2 pctSz = textRenderer.getTextSize(pctText, textScale);
                textRenderer.renderText(pctText,
                    (WINDOW_WIDTH - pctSz.x) / 2.0f, barY - pctSz.y - 12.0f,
                    textScale, glm::vec3(1.0f, 0.95f, 0.8f));
            }

            // Transition to DreamBlack when bar is done
            if (dreamLoadingTimer >= barEnd) {
                state = AppState::DreamBlack;
                dreamBlackTimer = 0.0f;
                if (hasAmbientLoop) {
                    ma_sound_stop(&ambientLoop);
                }
            }
        } else if (state == AppState::DreamBlack) {
            // Black screen for 2 seconds, then play typing sound
            if (dreamBlackTimer < 2.0f) {
                dreamBlackTimer += dt;
            } else if (!tecladoPlayed) {
                // Init and play the teclado sound once
                hasTecladoSound = ma_sound_init_from_file(&engine, "assets/sounds/ui/teclado.mp3", 0, nullptr, nullptr, &tecladoSound) == MA_SUCCESS;
                if (hasTecladoSound) {
                    ma_sound_start(&tecladoSound);
                }
                tecladoPlayed = true;
            } else {
                // Wait for sound to finish playing
                if (!hasTecladoSound || !ma_sound_is_playing(&tecladoSound)) {
                    state = AppState::VisualNovel;
                }
            }
        } else if (state == AppState::VisualNovel) {
            visualNovel->update(dt, mouseJustPressed);
            visualNovel->render();
        } else if (state == AppState::DreamState) {
            // Render just the logo and a centered message
            float lw = (float)logoTex.getWidth();
            float lh = (float)logoTex.getHeight();
            float lx = (WINDOW_WIDTH - lw) / 2.0f;
            float ly = WINDOW_HEIGHT * 0.22f;
            renderSprite(quadVAO, spriteShader, logoTex, proj, lx, ly, lw, lh, glm::vec4(1.0f));

            textRenderer.renderText("NUEVO SUE\u00d1O",
                (WINDOW_WIDTH - 200.0f) / 2.0f, WINDOW_HEIGHT / 2.0f + 50.0f,
                0.6f, glm::vec3(1.0f, 0.95f, 0.8f));
        }

        glfwSwapBuffers(gWindow);
        glfwPollEvents();
    }

    delete skyboxTex;
    for (auto* t : cloudFrames) delete t;
    delete glowTex;
    delete oficinaTex;
    for (auto* t : spriteFrames) delete t;
    delete flashlightIcon;
    delete underlineTex;
    delete loadingBarTex;
    delete visualNovel;
    if (hasLoadingDone) ma_sound_uninit(&loadingDone);
    if (hasLoadingLoop) ma_sound_uninit(&loadingLoop);
    if (hasAmbientLoop) ma_sound_uninit(&ambientLoop);
    if (hasTecladoSound) ma_sound_uninit(&tecladoSound);
    ma_engine_uninit(&engine);
    glfwTerminate();
    return 0;
}
