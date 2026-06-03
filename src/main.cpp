#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <vector>
#include <cstdio>
#include <cmath>
#include <string>
#include <algorithm>
#include <functional>

#include "Shader.h"
#include "Texture.h"
#include "TextRenderer.h"
#include "Menu.h"
#include "CubemapTexture.h"

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

static const int WINDOW_WIDTH = 1689;
static const int WINDOW_HEIGHT = 917;
static const int SPRITE_FRAMES = 36;
static const float TEXT_SCALE = 0.8f;
static const float SPRITE_X = 900.0f;

enum class AppState {
    Loading,
    Menu
};

static GLFWwindow* gWindow = nullptr;

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
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GLFW_TRUE);
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
    glm::mat4 proj = glm::ortho(0.0f, (float)WINDOW_WIDTH, (float)WINDOW_HEIGHT, 0.0f);
    textRenderer.setProjection(proj);

    Menu menu;
    menu.init(&textRenderer, WINDOW_WIDTH, WINDOW_HEIGHT, TEXT_SCALE);

    GLuint quadVAO = createQuadVAO();
    GLuint skyboxVAO = createCubeVAO();

    ma_engine engine;
    ma_engine_config engineConfig = ma_engine_config_init();
    if (ma_engine_init(&engineConfig, &engine) != MA_SUCCESS) {
        std::cerr << "Failed to init audio engine" << std::endl;
        return -1;
    }

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

    AppState state = AppState::Loading;
    float loadingProgress = 0.0f;
    float loadingPulse = 0.0f;
    float loadingBarWidth = 520.0f;
    float loadingBarHeight = 20.0f;
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
        } else {
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
                glm::vec3 color = (items[i].hovered) ? glm::vec3(1.0f, 0.95f, 0.8f) : glm::vec3(0.9f, 0.9f, 0.9f);
                textRenderer.renderText(items[i].text, items[i].x, items[i].y, TEXT_SCALE, color);

                if (items[i].hovered) {
                    float underlineY = items[i].y + items[i].h + 4.0f;
                    float underlineH = 8.0f;
                    renderSprite(quadVAO, spriteShader, *underlineTex, proj,
                                 items[i].x, underlineY, items[i].w, underlineH,
                                 glm::vec4(1.0f, 0.85f, 0.4f, 0.9f));
                }
            }

            textRenderer.renderText("VERSI\u00d3N 0.9a (BUILD ALPHA)", 20.0f, WINDOW_HEIGHT - 30.0f, 0.35f, glm::vec3(0.6f));

            std::string credits = "Hecho por Torres-Chavez-Torrez";
            glm::vec2 sz = textRenderer.getTextSize(credits, 0.35f);
            textRenderer.renderText(credits, WINDOW_WIDTH - sz.x - 20.0f, WINDOW_HEIGHT - 30.0f, 0.35f, glm::vec3(0.6f));
        }

        glfwSwapBuffers(gWindow);
        glfwPollEvents();
    }

    delete skyboxTex;
    for (auto* t : spriteFrames) delete t;
    delete flashlightIcon;
    delete underlineTex;
    delete loadingBarTex;
    if (hasLoadingDone) ma_sound_uninit(&loadingDone);
    if (hasLoadingLoop) ma_sound_uninit(&loadingLoop);
    if (hasAmbientLoop) ma_sound_uninit(&ambientLoop);
    ma_engine_uninit(&engine);
    glfwTerminate();
    return 0;
}
