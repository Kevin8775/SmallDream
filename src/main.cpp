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
#include "Model.h"
#include "MeshCollider.h"

#define NOMINMAX
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

static int WINDOW_WIDTH = 1689;
static int WINDOW_HEIGHT = 917;
static const int SPRITE_FRAMES = 36;
static const float TEXT_SCALE = 1.2f;
static float SPRITE_X = 900.0f;

enum class AppState {
    Loading,
    Menu,
    StoryChoice,
    Credits,
    Controls,
    CloudTransition,
    DreamLoading,
    DreamBlack,
    VisualNovel,
    DreamState,
    HouseLoading,
    HouseWalk
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
static bool gStoryCloudOnly = false;
static bool gHouseCapturedMouse = false;
static bool gShowCollisionDebug = false;

static bool pointInRect(double x, double y, float rx, float ry, float rw, float rh) {
    return x >= rx && x <= rx + rw && y >= ry && y <= ry + rh;
}

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
                *gStatePtr = AppState::StoryChoice;
                gCreditsPaused = false;
                gCreditsPauseStarted = 0.0;
                gCreditsPauseAccum = 0.0;
            } else if (hovered == 2) {
                *gStatePtr = AppState::Controls;
                gCreditsPaused = false;
                gCreditsPauseStarted = 0.0;
                gCreditsPauseAccum = 0.0;
            } else if (hovered == 3) {
                *gStatePtr = AppState::Credits;
                gCreditsPaused = false;
                gCreditsPauseStarted = 0.0;
                gCreditsPauseAccum = glfwGetTime();
            } else if (hovered == 4) {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }
        } else if (gStatePtr && *gStatePtr == AppState::StoryChoice) {
            double mx, my;
            glfwGetCursorPos(window, &mx, &my);
            float btnW = 480.0f;
            float btnH = 76.0f;
            float btnX = (WINDOW_WIDTH - btnW) / 2.0f;
            float skipY = WINDOW_HEIGHT * 0.44f;
            float viewY = WINDOW_HEIGHT * 0.58f;

            if (pointInRect(mx, my, btnX, skipY, btnW, btnH)) {
                *gStatePtr = AppState::HouseLoading;
                gHouseCapturedMouse = false;
                return;
            }
            if (pointInRect(mx, my, btnX, viewY, btnW, btnH)) {
                gStoryCloudOnly = true;
                *gStatePtr = AppState::CloudTransition;
            }
        } else if (gStatePtr && *gStatePtr == AppState::Credits) {
            gCreditsPaused = false;
            gCreditsPauseStarted = 0.0;
            gCreditsPauseAccum = 0.0;
            *gStatePtr = AppState::Menu;
        } else if (gStatePtr && *gStatePtr == AppState::Controls) {
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
        if (gStatePtr && *gStatePtr == AppState::Controls) {
            *gStatePtr = AppState::Menu;
            return;
        }
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
    if (key == GLFW_KEY_B && action == GLFW_PRESS && gStatePtr && *gStatePtr == AppState::HouseWalk) {
        gShowCollisionDebug = !gShowCollisionDebug;
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

    GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* videoMode = glfwGetVideoMode(primaryMonitor);
    WINDOW_WIDTH = videoMode->width;
    WINDOW_HEIGHT = videoMode->height;
    SPRITE_X = WINDOW_WIDTH * 0.58f;

    gWindow = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "SmallDream", primaryMonitor, nullptr);
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
    Texture storyBgTex("assets/textures/fondo.png");

    Texture* keyW = new Texture("assets/textures/letter_w.jpg");
    Texture* keyA = new Texture("assets/textures/letter_a.jpg");
    Texture* keyS = new Texture("assets/textures/letter_s.jpg");
    Texture* keyD = new Texture("assets/textures/letter_d.jpg");
    Texture* keyMouse = new Texture("assets/textures/mouse.jpg");
    Texture* keySpace = new Texture("assets/textures/space.jpeg");

    TextRenderer textRenderer("assets/fonts/Simplest.otf", 35, &textShader);
    TextRenderer creditsTextRenderer("assets/fonts/Simplest.otf", 60, &textShader);
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
    Shader modelShader("assets/shaders/model.vert", "assets/shaders/model.frag");
    Model houseModel;
    bool houseLoaded = false;
    glm::vec3 housePos(0.0f);
    glm::vec3 camPos(0.0f, 1.8f, 6.0f);
    glm::vec3 camFront(0.0f, 0.0f, -1.0f);
    glm::vec3 camUp(0.0f, 1.0f, 0.0f);
    float camYaw = -90.0f;
    float camPitch = 0.0f;
    double lastMouseX = WINDOW_WIDTH * 0.5;
    double lastMouseY = WINDOW_HEIGHT * 0.5;
    bool firstMouse = true;
    float houseModelScale = 1.0f;
    glm::vec3 houseCenter(0.0f);
    glm::vec3 houseMin(0.0f), houseMax(0.0f);
    float houseLoadingTimer = 0.0f;
    bool houseLoadingStarted = false;
    const float houseTargetSize = 120.0f;
    float houseVerticalVelocity = 0.0f;

    const float eyeHeight = 1.5f;
    const float playerRadius = 0.3f;
    MeshCollider bedroomCollider;

    // Transform: V_world = housePos + scale * V_model - center
    // For the model to be centered at origin, housePos must be (1 - scale) * center
    auto computeHousePos = [&]() {
        housePos = (1.0f - houseModelScale) * houseCenter;
    };

    auto buildColliderTransform = [&]() -> glm::mat4 {
        return glm::translate(glm::mat4(1.0f), housePos)
             * glm::translate(glm::mat4(1.0f), -houseCenter)
             * glm::scale(glm::mat4(1.0f), glm::vec3(houseModelScale));
    };

    auto placeCameraInsideBedroom = [&]() {
        glm::vec3 minW = housePos + houseModelScale * houseMin - houseCenter;
        glm::vec3 maxW = housePos + houseModelScale * houseMax - houseCenter;
        float roughFloorY = minW.y + (maxW.y - minW.y) * 0.05f;
        float floorY = roughFloorY;
        if (bedroomCollider.isBuilt()) {
            float sampleOffsets[] = {-20.0f, -10.0f, -5.0f, 0.0f, 5.0f, 10.0f, 20.0f};
            for (float dx : sampleOffsets) {
                for (float dz : sampleOffsets) {
                    // Start ray from inside room (below ceiling) so first hit is the floor surface
                    float startY = minW.y + (maxW.y - minW.y) * 0.8f;
                    float f = bedroomCollider.getFloorHeight(
                        glm::vec3(dx, startY, dz));
                    if (f > floorY) floorY = f;
                }
            }
        }
        if (floorY <= roughFloorY - 1.0f || floorY > maxW.y) floorY = roughFloorY;
        camPos = glm::vec3(0.0f, floorY + eyeHeight, 0.0f);
        camYaw = -90.0f;
        camPitch = 0.0f;
        firstMouse = true;
    };

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
    ma_sound stepsSound;
    bool hasStepsSound = false;
    bool tecladoPlayed = false;
    bool prevMouseDown = false;

    VisualNovel* visualNovel = new VisualNovel(&textRenderer, &spriteShader, quadVAO, proj, WINDOW_WIDTH, WINDOW_HEIGHT);
    visualNovel->setSoundEngine(&engine);

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

            textRenderer.renderText("LOADING RESOURCES...", (WINDOW_WIDTH - 230.0f) / 2.0f, barY - 36.0f, 0.5f, glm::vec3(0.9f));
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
                    float cy = items[i].y + items[i].h / 2.0f + 23.0f;
                    renderSprite(quadVAO, spriteShader, *glowTex, proj,
                                 cx - 300.0f, cy - 75.0f, 600.0f, 150.0f,
                                 glm::vec4(1.0f, 0.95f, 0.8f, 0.5f));
                }
                glm::vec3 color = items[i].hovered ? glm::vec3(1.0f, 0.95f, 0.8f) : glm::vec3(0.9f, 0.9f, 0.9f);
                textRenderer.renderText(items[i].text, items[i].x, items[i].y, TEXT_SCALE, color);
            }

            textRenderer.renderText("VERSI\u00d3N 0.9a (BUILD ALPHA)", 20.0f, WINDOW_HEIGHT - 30.0f, 0.35f, glm::vec3(0.6f));

            std::string credits = "Made by Torres-Chavez-Torrez";
            glm::vec2 sz = textRenderer.getTextSize(credits, 0.35f);
            textRenderer.renderText(credits, WINDOW_WIDTH - sz.x - 20.0f, WINDOW_HEIGHT - 30.0f, 0.35f, glm::vec3(0.6f));
        } else if (state == AppState::StoryChoice) {
            glDisable(GL_DEPTH_TEST);
            renderSprite(quadVAO, spriteShader, storyBgTex, proj,
                         0.0f, 0.0f, (float)WINDOW_WIDTH, (float)WINDOW_HEIGHT,
                         glm::vec4(1.0f));

            float btnW = 480.0f;
            float btnH = 76.0f;
            float btnX = (WINDOW_WIDTH - btnW) / 2.0f;
            float skipY = WINDOW_HEIGHT * 0.44f;
            float viewY = WINDOW_HEIGHT * 0.58f;

            textRenderer.renderText("Choose how to start",
                (WINDOW_WIDTH - 360.0f) / 2.0f + 60.0f, 120.0f,
                0.72f, glm::vec3(0.96f, 0.92f, 0.80f));

            renderSprite(quadVAO, spriteShader, *underlineTex, proj,
                         btnX, skipY, btnW, btnH,
                         glm::vec4(0.18f, 0.16f, 0.13f, 0.82f));
            renderSprite(quadVAO, spriteShader, *underlineTex, proj,
                         btnX, viewY, btnW, btnH,
                         glm::vec4(0.18f, 0.16f, 0.13f, 0.82f));

            renderSprite(quadVAO, spriteShader, *underlineTex, proj,
                         btnX + 6.0f, skipY + 6.0f, btnW - 12.0f, btnH - 12.0f,
                         glm::vec4(0.04f, 0.04f, 0.05f, 0.22f));
            renderSprite(quadVAO, spriteShader, *underlineTex, proj,
                         btnX + 6.0f, viewY + 6.0f, btnW - 12.0f, btnH - 12.0f,
                         glm::vec4(0.04f, 0.04f, 0.05f, 0.22f));

            glm::vec2 skipSz = textRenderer.getTextSize("Skip Story", 0.82f);
            glm::vec2 viewSz = textRenderer.getTextSize("View Story", 0.82f);
            textRenderer.renderText("Skip Story",
                btnX + (btnW - skipSz.x) / 2.0f,
                skipY + (btnH - skipSz.y) / 2.0f - 18.0f,
                0.82f, glm::vec3(0.98f, 0.94f, 0.86f));
            textRenderer.renderText("View Story",
                btnX + (btnW - viewSz.x) / 2.0f,
                viewY + (btnH - viewSz.y) / 2.0f - 18.0f,
                0.82f, glm::vec3(0.98f, 0.94f, 0.86f));
        } else if (state == AppState::Credits) {
            double creditsNow = glfwGetTime();
            double creditsElapsed = gCreditsPaused ? (gCreditsPauseStarted - gCreditsPauseAccum) : (creditsNow - gCreditsPauseAccum);
            float creditsAnim = std::min((float)creditsElapsed * 1.5f, 1.0f);
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
            const float scrollSpeed = 80.0f;
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
        } else if (state == AppState::Controls) {
            glDisable(GL_DEPTH_TEST);
            bgShader.use();
            bgShader.setInt("uTexture", 0);
            bgShader.setFloat("uBlurAmount", 3.5f);
            bgTex.bind(0);
            glm::mat4 cModel = glm::scale(glm::mat4(1.0f), glm::vec3(WINDOW_WIDTH, WINDOW_HEIGHT, 1.0f));
            bgShader.setMat4("uModel", glm::value_ptr(cModel));
            bgShader.setMat4("uProjection", glm::value_ptr(proj));
            glBindVertexArray(quadVAO);
            glDrawArrays(GL_TRIANGLES, 0, 6);

            float cx = WINDOW_WIDTH * 0.5f;
            float cy = WINDOW_HEIGHT * 0.5f;
            float pw = 580.0f;
            float ph = 540.0f;
            renderSprite(quadVAO, spriteShader, *glowTex, proj,
                         cx - pw * 0.5f, cy - ph * 0.5f, pw, ph,
                         glm::vec4(0.14f, 0.12f, 0.10f, 0.82f));
            renderSprite(quadVAO, spriteShader, *glowTex, proj,
                         cx - pw * 0.5f + 10.0f, cy - ph * 0.5f + 10.0f, pw - 20.0f, ph - 20.0f,
                         glm::vec4(0.08f, 0.08f, 0.10f, 0.60f));

            float startY = cy - 195.0f;
            glm::vec2 titleSz = creditsTextRenderer.getTextSize("CONTROLS", 1.0f);
            creditsTextRenderer.renderText("CONTROLS", cx - titleSz.x * 0.5f, startY, 1.0f, glm::vec3(0.97f, 0.92f, 0.79f));

            float keySize = 68.0f;
            float wasdY = startY + 65.0f;
            float keyGap = keySize + 20.0f;
            renderSprite(quadVAO, spriteShader, *keyW, proj, cx - keySize * 0.5f, wasdY, keySize, keySize, glm::vec4(1.0f));
            renderSprite(quadVAO, spriteShader, *keyA, proj, cx - keyGap - keySize * 0.5f, wasdY + keyGap, keySize, keySize, glm::vec4(1.0f));
            renderSprite(quadVAO, spriteShader, *keyS, proj, cx - keySize * 0.5f, wasdY + keyGap, keySize, keySize, glm::vec4(1.0f));
            renderSprite(quadVAO, spriteShader, *keyD, proj, cx + keyGap - keySize * 0.5f, wasdY + keyGap, keySize, keySize, glm::vec4(1.0f));

            float wasdLabelY = wasdY + keyGap + keySize + 12.0f;
            glm::vec2 wasdLabelSz = textRenderer.getTextSize("Walk / Strafe", 0.55f);
            textRenderer.renderText("Walk / Strafe", cx - wasdLabelSz.x * 0.5f, wasdLabelY, 0.55f, glm::vec3(0.92f, 0.88f, 0.80f));

            float mouseY = wasdLabelY + 40.0f;
            float mouseW = 110.0f;
            float mouseH = 74.0f;
            renderSprite(quadVAO, spriteShader, *keyMouse, proj, cx - mouseW * 0.5f, mouseY, mouseW, mouseH, glm::vec4(1.0f));
            glm::vec2 mouseLabelSz = textRenderer.getTextSize("Look around", 0.55f);
            textRenderer.renderText("Look around", cx - mouseLabelSz.x * 0.5f, mouseY + mouseH + 8.0f, 0.55f, glm::vec3(0.92f, 0.88f, 0.80f));

            float spaceY = mouseY + mouseH + 50.0f;
            float spaceW = 260.0f;
            float spaceH = 56.0f;
            renderSprite(quadVAO, spriteShader, *keySpace, proj, cx - spaceW * 0.5f, spaceY, spaceW, spaceH, glm::vec4(1.0f));
            glm::vec2 spaceLabelSz = textRenderer.getTextSize("Jump", 0.55f);
            textRenderer.renderText("Jump", cx - spaceLabelSz.x * 0.5f, spaceY + spaceH + 8.0f, 0.55f, glm::vec3(0.92f, 0.88f, 0.80f));


            creditsTextRenderer.renderText("Click anywhere to go back",
                20.0f, WINDOW_HEIGHT - 40.0f, 0.40f, glm::vec3(0.80f, 0.76f, 0.66f));
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

            if (!gStoryCloudOnly) {
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
                            float cy = itemY + items[i].h / 2.0f + 23.0f;
                            renderSprite(quadVAO, spriteShader, *glowTex, proj,
                                         cx - 300.0f, cy - 75.0f, 600.0f, 150.0f,
                                         glm::vec4(1.0f, 0.95f, 0.8f, 0.5f));
                        }
                        glm::vec3 color = items[i].hovered ? glm::vec3(1.0f, 0.95f, 0.8f) : glm::vec3(0.9f, 0.9f, 0.9f);
                        textRenderer.renderText(items[i].text, items[i].x, itemY, TEXT_SCALE, color);
                    }
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
                gStoryCloudOnly = false;
                state = AppState::DreamLoading;
                dreamLoadingTimer = 0.0f;
            }
        } else if (state == AppState::HouseLoading) {
            if (!houseLoadingStarted) {
                houseLoadingStarted = true;
                houseLoadingTimer = 0.0f;
                bedroomCollider.destroy();
                houseLoaded = houseModel.load("assets/models/bedroom/scene.gltf");
                if (houseLoaded) {
                    houseMin = houseModel.boundsMin();
                    houseMax = houseModel.boundsMax();
                    houseCenter = (houseMin + houseMax) * 0.5f;
                    glm::vec3 size = houseMax - houseMin;
                    float biggest = std::max(size.x, std::max(size.y, size.z));
                    houseModelScale = (biggest > 0.0f) ? houseTargetSize / biggest : 1.0f;
                    computeHousePos();
                    bedroomCollider.addModel(houseModel, buildColliderTransform());
                    // Wide collision floor at model-floor level
                    {
                        glm::vec3 minW = housePos + houseModelScale * houseMin - houseCenter;
                        glm::vec3 maxW = housePos + houseModelScale * houseMax - houseCenter;
                        float floorY = minW.y + (maxW.y - minW.y) * 0.05f;
                        float margin = 50.0f;
                        bedroomCollider.addFloorQuad(floorY, minW.x - margin, maxW.x + margin, minW.z - margin, maxW.z + margin);
                    }
                    bedroomCollider.build();
                    placeCameraInsideBedroom();
                }
            }

            houseLoadingTimer += dt;
            float p = std::min(houseLoadingTimer / 1.2f, 1.0f);
            glDisable(GL_DEPTH_TEST);
            bgShader.use();
            bgShader.setInt("uTexture", 0);
            bgShader.setFloat("uBlurAmount", 6.0f);
            bgTex.bind(0);
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::scale(model, glm::vec3(WINDOW_WIDTH, WINDOW_HEIGHT, 1.0f));
            bgShader.setMat4("uModel", glm::value_ptr(model));
            bgShader.setMat4("uProjection", glm::value_ptr(proj));
            glBindVertexArray(quadVAO);
            glDrawArrays(GL_TRIANGLES, 0, 6);

            float barW = 620.0f;
            float barH = 26.0f;
            float barX = (WINDOW_WIDTH - barW) / 2.0f;
            float barY = WINDOW_HEIGHT * 0.72f;
            renderSprite(quadVAO, spriteShader, *loadingBarTex, proj,
                         barX, barY, barW, barH,
                         glm::vec4(0.15f, 0.15f, 0.15f, 0.92f));
            renderSprite(quadVAO, spriteShader, *loadingBarTex, proj,
                         barX, barY, barW * p, barH,
                         glm::vec4(0.82f, 0.74f, 0.55f, 0.95f));
            textRenderer.renderText("Preparing scene...", (WINDOW_WIDTH - 240.0f) / 2.0f, barY - 36.0f, 0.5f, glm::vec3(0.95f));

            if (p >= 1.0f) {
                state = AppState::HouseWalk;
                houseLoadingStarted = false;
            }
        } else if (state == AppState::HouseWalk) {
            if (!houseLoaded) {
                bedroomCollider.destroy();
                houseLoaded = houseModel.load("assets/models/bedroom/scene.gltf");
                if (houseLoaded) {
                    houseMin = houseModel.boundsMin();
                    houseMax = houseModel.boundsMax();
                    houseCenter = (houseMin + houseMax) * 0.5f;
                    glm::vec3 size = houseMax - houseMin;
                    float biggest = std::max(size.x, std::max(size.y, size.z));
                    houseModelScale = (biggest > 0.0f) ? houseTargetSize / biggest : 1.0f;
                    computeHousePos();
                    bedroomCollider.addModel(houseModel, buildColliderTransform());
                    {
                        glm::vec3 minW = housePos + houseModelScale * houseMin - houseCenter;
                        glm::vec3 maxW = housePos + houseModelScale * houseMax - houseCenter;
                        float floorY = minW.y + (maxW.y - minW.y) * 0.05f;
                        float margin = 50.0f;
                        bedroomCollider.addFloorQuad(floorY, minW.x - margin, maxW.x + margin, minW.z - margin, maxW.z + margin);
                    }
                    bedroomCollider.build();
                    placeCameraInsideBedroom();
                }
            }
            if (!gHouseCapturedMouse) {
                glfwSetInputMode(gWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                gHouseCapturedMouse = true;
                firstMouse = true;
            }
            if (!hasStepsSound) {
                hasStepsSound = ma_sound_init_from_file(&engine, "assets/sounds/effects/steps.wav", 0, nullptr, nullptr, &stepsSound) == MA_SUCCESS;
                if (hasStepsSound) ma_sound_set_looping(&stepsSound, MA_TRUE);
            }
            double mx2, my2;
            glfwGetCursorPos(gWindow, &mx2, &my2);
            if (firstMouse) { lastMouseX = mx2; lastMouseY = my2; firstMouse = false; }
            float xoffset = (float)(mx2 - lastMouseX) * 0.08f;
            float yoffset = (float)(lastMouseY - my2) * 0.08f;
            lastMouseX = mx2; lastMouseY = my2;
            camYaw += xoffset;
            camPitch += yoffset;
            if (camPitch > 89.0f) camPitch = 89.0f;
            if (camPitch < -89.0f) camPitch = -89.0f;
            glm::vec3 front;
            front.x = cos(glm::radians(camYaw)) * cos(glm::radians(camPitch));
            front.y = sin(glm::radians(camPitch));
            front.z = sin(glm::radians(camYaw)) * cos(glm::radians(camPitch));
            camFront = glm::normalize(front);
            glm::vec3 right = glm::normalize(glm::cross(camFront, camUp));
            bool sprinting = glfwGetKey(gWindow, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;
            float speed = 7.5f * dt * (sprinting ? 2.0f : 1.0f);

            // Desired horizontal movement
            glm::vec3 move(0.0f);
            if (glfwGetKey(gWindow, GLFW_KEY_W) == GLFW_PRESS) move += camFront * speed;
            if (glfwGetKey(gWindow, GLFW_KEY_S) == GLFW_PRESS) move -= camFront * speed;
            if (glfwGetKey(gWindow, GLFW_KEY_A) == GLFW_PRESS) move -= right * speed;
            if (glfwGetKey(gWindow, GLFW_KEY_D) == GLFW_PRESS) move += right * speed;

            // Gravity
            houseVerticalVelocity -= 9.8f * dt;
            move.y = houseVerticalVelocity * dt;

            // Floor detection at current XZ position
            float floorY = camPos.y - 100.0f;
            if (bedroomCollider.isBuilt()) {
                float f = bedroomCollider.getFloorHeight(
                    camPos + glm::vec3(0.0f, eyeHeight + 0.5f, 0.0f));
                if (f > camPos.y - 100.0f) floorY = f;
            }

            // Apply movement with collision sliding
            glm::vec3 nextPos = camPos + move;
            if (bedroomCollider.isBuilt()) {
                for (int iter = 0; iter < 2; ++iter) {
                    glm::vec3 normal;
                    float penetration;
                    if (bedroomCollider.collideSphere(nextPos, playerRadius, normal, penetration)) {
                        nextPos += normal * (penetration + 0.001f);
                        glm::vec3 vel = nextPos - camPos;
                        vel = vel - glm::dot(vel, normal) * normal;
                        nextPos = camPos + vel;
                    } else break;
                }
            }

            // Floor constraint
            float footY = nextPos.y - eyeHeight;
            bool grounded = footY <= floorY + 0.02f && (floorY - footY) < 1.0f;
            if (grounded) {
                nextPos.y = floorY + eyeHeight;
                houseVerticalVelocity = 0.0f;
                if (glfwGetKey(gWindow, GLFW_KEY_SPACE) == GLFW_PRESS) {
                    houseVerticalVelocity = 5.0f;
                }
            }

            // Footstep sound
            {
                bool moving = move.x != 0.0f || move.z != 0.0f;
                if (hasStepsSound) {
                    if (moving && grounded) {
                        if (!ma_sound_is_playing(&stepsSound)) ma_sound_start(&stepsSound);
                        ma_sound_set_pitch(&stepsSound, sprinting ? 1.8f : 1.0f);
                    } else {
                        if (ma_sound_is_playing(&stepsSound)) ma_sound_stop(&stepsSound);
                    }
                }
            }

            camPos = nextPos;
            glDisable(GL_CULL_FACE);
            glEnable(GL_DEPTH_TEST);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glm::mat4 view = glm::lookAt(camPos, camPos + camFront, camUp);
            glm::mat4 projection = glm::perspective(glm::radians(70.0f), (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT, 0.05f, 500.0f);
            glm::mat4 model = glm::translate(glm::mat4(1.0f), housePos);
            model = glm::translate(model, -houseCenter);
            model = glm::scale(model, glm::vec3(houseModelScale));
            modelShader.use();
            modelShader.setMat4("uModel", glm::value_ptr(model));
            modelShader.setMat4("uView", glm::value_ptr(view));
            modelShader.setMat4("uProjection", glm::value_ptr(projection));
            modelShader.setVec3("uCamPos", camPos.x, camPos.y, camPos.z);
            modelShader.setVec3("uAmbientColor", 0.18f, 0.15f, 0.12f);
            modelShader.setVec3("uMainLightDir", 0.0f, 1.0f, 0.3f);
            modelShader.setVec3("uMainLightColor", 1.0f, 0.85f, 0.55f);
            modelShader.setFloat("uMainLightIntensity", 1.0f);
            modelShader.setVec3("uFillLightDir", -0.6f, 0.1f, 0.8f);
            modelShader.setVec3("uFillLightColor", 0.3f, 0.4f, 0.7f);
            modelShader.setFloat("uFillLightIntensity", 0.5f);
            modelShader.setFloat("uShininess", 24.0f);
            modelShader.setFloat("uSpecIntensity", 0.25f);
            if (houseLoaded) {
                houseModel.draw(modelShader);
                if (gShowCollisionDebug) {
                    glEnable(GL_DEPTH_TEST);
                    bedroomCollider.drawDebug(modelShader, view, projection);
                }
            } else {
                glDisable(GL_DEPTH_TEST);
                textRenderer.renderText("Failed to load model", 40.0f, 40.0f, 0.6f, glm::vec3(1.0f, 0.6f, 0.6f));
                textRenderer.renderText(houseModel.lastError(), 40.0f, 80.0f, 0.4f, glm::vec3(1.0f, 0.8f, 0.8f));
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
                    visualNovel->reset();
                    state = AppState::VisualNovel;
                }
            }
        } else if (state == AppState::VisualNovel) {
            visualNovel->update(dt, mouseJustPressed);
            visualNovel->render();
            if (visualNovel->isFinished()) {
                state = AppState::HouseLoading;
                gHouseCapturedMouse = false;
                houseLoadingStarted = false;
                gCreditsPaused = false;
                gCreditsPauseStarted = 0.0;
                gCreditsPauseAccum = 0.0;
            }
        } else if (state == AppState::DreamState) {
            // Render just the logo and a centered message
            float lw = (float)logoTex.getWidth();
            float lh = (float)logoTex.getHeight();
            float lx = (WINDOW_WIDTH - lw) / 2.0f;
            float ly = WINDOW_HEIGHT * 0.22f;
            renderSprite(quadVAO, spriteShader, logoTex, proj, lx, ly, lw, lh, glm::vec4(1.0f));

            textRenderer.renderText("NEW DREAM",
                (WINDOW_WIDTH - 200.0f) / 2.0f, WINDOW_HEIGHT / 2.0f + 50.0f,
                0.6f, glm::vec3(1.0f, 0.95f, 0.8f));
        }

        glfwSwapBuffers(gWindow);
        glfwPollEvents();
    }

    delete skyboxTex;
    for (auto* t : cloudFrames) delete t;
    delete glowTex;
    for (auto* t : spriteFrames) delete t;
    delete flashlightIcon;
    delete underlineTex;
    delete loadingBarTex;
    delete visualNovel;
    delete keyW; delete keyA; delete keyS; delete keyD;
    delete keyMouse; delete keySpace;
    if (hasLoadingDone) ma_sound_uninit(&loadingDone);
    if (hasLoadingLoop) ma_sound_uninit(&loadingLoop);
    if (hasAmbientLoop) ma_sound_uninit(&ambientLoop);
    if (hasTecladoSound) ma_sound_uninit(&tecladoSound);
    if (hasStepsSound) { ma_sound_stop(&stepsSound); ma_sound_uninit(&stepsSound); }
    ma_engine_uninit(&engine);
    glfwTerminate();
    return 0;
}
