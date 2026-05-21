#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <objbase.h>
#include <wincodec.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>
#include <cfloat>

#pragma comment(lib, "windowscodecs.lib")

static void glfwErrorCallback(int error, const char* description) {
    std::cerr << "GLFW error " << error << ": " << (description ? description : "(null)") << "\n";
}

static GLuint compileShader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok == GL_TRUE) return shader;

    GLint logLen = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLen);
    std::string log;
    log.resize(logLen > 0 ? logLen : 1);
    glGetShaderInfoLog(shader, logLen, nullptr, log.data());
    std::cerr << "Shader compile failed: " << log << "\n";
    glDeleteShader(shader);
    return 0;
}

static GLuint createProgram(const char* vsSrc, const char* fsSrc) {
    GLuint vs = compileShader(GL_VERTEX_SHADER, vsSrc);
    if (!vs) return 0;
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fsSrc);
    if (!fs) {
        glDeleteShader(vs);
        return 0;
    }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = GL_FALSE;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (ok == GL_TRUE) return prog;

    GLint logLen = 0;
    glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &logLen);
    std::string log;
    log.resize(logLen > 0 ? logLen : 1);
    glGetProgramInfoLog(prog, logLen, nullptr, log.data());
    std::cerr << "Program link failed: " << log << "\n";
    glDeleteProgram(prog);
    return 0;
}

struct GpuMesh {
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    GLsizei indexCount = 0;
    GLuint baseColorTex = 0;
    GLuint normalTex = 0;
    GLuint metallicRoughnessTex = 0;
    GLuint specularF0Tex = 0;

    glm::vec4 baseColorFactor{ 1.0f, 1.0f, 1.0f, 1.0f };
    float metallicFactor = 0.0f;
    float roughnessFactor = 1.0f;
};

static GLuint createSolidTextureRGBA(unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    unsigned char pixel[4] = { r, g, b, a };
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

static GLuint loadTextureWIC(const std::filesystem::path& path, bool srgb) {
    static bool comInit = false;
    if (!comInit) {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        comInit = true;
    }

    IWICImagingFactory* factory = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (FAILED(hr) || !factory) return 0;

    IWICBitmapDecoder* decoder = nullptr;
    hr = factory->CreateDecoderFromFilename(path.wstring().c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder);
    if (FAILED(hr) || !decoder) {
        factory->Release();
        return 0;
    }

    IWICBitmapFrameDecode* frame = nullptr;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr) || !frame) {
        decoder->Release();
        factory->Release();
        return 0;
    }

    IWICFormatConverter* converter = nullptr;
    hr = factory->CreateFormatConverter(&converter);
    if (FAILED(hr) || !converter) {
        frame->Release();
        decoder->Release();
        factory->Release();
        return 0;
    }

    hr = converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) {
        converter->Release();
        frame->Release();
        decoder->Release();
        factory->Release();
        return 0;
    }

    UINT w = 0, h = 0;
    converter->GetSize(&w, &h);
    if (w == 0 || h == 0) {
        converter->Release();
        frame->Release();
        decoder->Release();
        factory->Release();
        return 0;
    }

    std::vector<unsigned char> pixels;
    pixels.resize(static_cast<size_t>(w) * static_cast<size_t>(h) * 4);
    const UINT stride = w * 4;
    hr = converter->CopyPixels(nullptr, stride, (UINT)pixels.size(), pixels.data());

    converter->Release();
    frame->Release();
    decoder->Release();
    factory->Release();

    if (FAILED(hr)) return 0;

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    const GLint internalFmt = srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8;
    glTexImage2D(GL_TEXTURE_2D, 0, internalFmt, (GLsizei)w, (GLsizei)h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

struct Camera {
    glm::vec3 pos{ 0.0f, 0.0f, 3.0f };
    float yaw = -90.0f;   // mirando hacia -Z
    float pitch = 0.0f;
    float fov = 60.0f;
};

static glm::vec3 cameraFront(const Camera& c) {
    glm::vec3 front;
    front.x = cosf(glm::radians(c.yaw)) * cosf(glm::radians(c.pitch));
    front.y = sinf(glm::radians(c.pitch));
    front.z = sinf(glm::radians(c.yaw)) * cosf(glm::radians(c.pitch));
    return glm::normalize(front);
}

static glm::mat4 cameraView(const Camera& c) {
    const glm::vec3 front = cameraFront(c);
    return glm::lookAt(c.pos, c.pos + front, glm::vec3(0, 1, 0));
}

struct InputState {
    Camera cam;
    bool firstMouse = true;
    double lastX = 0.0;
    double lastY = 0.0;
    float mouseSensitivity = 0.18f;
    float baseSpeed = 6.0f;

    bool cursorCaptured = true;
    bool escWasDown = false;

    // Very simple player physics/collision (AABB of the whole scene).
    glm::vec3 worldMin{ -1.0f };
    glm::vec3 worldMax{  1.0f };
    float playerRadius = 0.25f; // in scene units
    float eyeHeight = 1.6f;     // in scene units
    float verticalVel = 0.0f;
    bool onGround = false;

    struct Tri { glm::vec3 a, b, c; };
    const std::vector<Tri>* floorTris = nullptr; // triangles used for downward floor collision
};

static bool rayTriangle(const glm::vec3& orig, const glm::vec3& dir, const InputState::Tri& tri, float& tOut) {
    // Moller-Trumbore. Returns t along ray if hit.
    const glm::vec3 e1 = tri.b - tri.a;
    const glm::vec3 e2 = tri.c - tri.a;
    const glm::vec3 pvec = glm::cross(dir, e2);
    const float det = glm::dot(e1, pvec);
    if (fabsf(det) < 1e-8f) return false;
    const float invDet = 1.0f / det;
    const glm::vec3 tvec = orig - tri.a;
    const float u = glm::dot(tvec, pvec) * invDet;
    if (u < 0.0f || u > 1.0f) return false;
    const glm::vec3 qvec = glm::cross(tvec, e1);
    const float v = glm::dot(dir, qvec) * invDet;
    if (v < 0.0f || u + v > 1.0f) return false;
    const float t = glm::dot(e2, qvec) * invDet;
    if (t <= 0.0f) return false;
    tOut = t;
    return true;
}

static bool queryFloorHit(const InputState& st, const glm::vec3& pos, float& outY) {
    if (!st.floorTris) return false;
    const glm::vec3 dir(0.0f, -1.0f, 0.0f);
    // Cast from well above so we still hit when the camera is high.
    const float startY = st.worldMax.y + 2.0f;
    const glm::vec3 orig(pos.x, startY, pos.z);
    float bestT = FLT_MAX;
    bool hit = false;
    for (const auto& tri : *st.floorTris) {
        float t = 0.0f;
        if (rayTriangle(orig, dir, tri, t)) {
            if (t < bestT) {
                bestT = t;
                hit = true;
            }
        }
    }
    if (!hit) return false;
    outY = orig.y - bestT;
    return true;
}

static float queryFloorYOrAabb(const InputState& st, const glm::vec3& pos) {
    float y = 0.0f;
    if (queryFloorHit(st, pos, y)) return y;
    return st.worldMin.y;
}

static void mouseCallback(GLFWwindow* window, double xpos, double ypos) {
    auto* st = reinterpret_cast<InputState*>(glfwGetWindowUserPointer(window));
    if (!st) return;
    if (!st->cursorCaptured) return;

    if (st->firstMouse) {
        st->lastX = xpos;
        st->lastY = ypos;
        st->firstMouse = false;
        return;
    }

    const double xoffset = xpos - st->lastX;
    const double yoffset = st->lastY - ypos; // invertido: arriba = positivo
    st->lastX = xpos;
    st->lastY = ypos;

    st->cam.yaw += static_cast<float>(xoffset) * st->mouseSensitivity;
    st->cam.pitch += static_cast<float>(yoffset) * st->mouseSensitivity;

    if (st->cam.pitch > 89.0f) st->cam.pitch = 89.0f;
    if (st->cam.pitch < -89.0f) st->cam.pitch = -89.0f;
}

static void processInput(GLFWwindow* window, InputState& st, float dt) {
    // ESC toggles mouse capture (do not exit).
    const bool escDown = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
    if (escDown && !st.escWasDown) {
        st.cursorCaptured = !st.cursorCaptured;
        glfwSetInputMode(window, GLFW_CURSOR, st.cursorCaptured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
        st.firstMouse = true; // reset delta on re-capture
    }
    st.escWasDown = escDown;

    // Click to re-capture.
    if (!st.cursorCaptured && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
        st.cursorCaptured = true;
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        st.firstMouse = true;
    }

    // Ajuste rapido de velocidad: '-' baja, '=' sube
    if (glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS) st.baseSpeed = (st.baseSpeed > 1.0f) ? (st.baseSpeed - 0.1f) : st.baseSpeed;
    if (glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS) st.baseSpeed = st.baseSpeed + 0.1f;

    float speed = st.baseSpeed;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) speed *= 3.0f;
    const float vel = speed * dt;

    // If the cursor isn't captured, don't move the player.
    if (!st.cursorCaptured) return;

    // Horizontal move (XZ): derive from yaw only so it's always well-defined.
    const float yawRad = glm::radians(st.cam.yaw);
    glm::vec3 forwardXZ(cosf(yawRad), 0.0f, sinf(yawRad));
    forwardXZ = glm::normalize(forwardXZ);
    const glm::vec3 right = glm::normalize(glm::cross(forwardXZ, glm::vec3(0, 1, 0)));

    glm::vec3 delta(0.0f);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) delta += forwardXZ;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) delta -= forwardXZ;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) delta -= right;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) delta += right;
    if (glm::dot(delta, delta) > 0.0001f) delta = glm::normalize(delta);
    // Prevent walking outside the real floor area: only allow horizontal movement
    // if we still have a valid floor under the candidate position.
    const glm::vec3 oldPos = st.cam.pos;
    glm::vec3 candidate = st.cam.pos;

    // Move X then Z (simple slide).
    candidate.x += delta.x * vel;
    float fy = 0.0f;
    if (queryFloorHit(st, candidate, fy)) {
        st.cam.pos.x = candidate.x;
    }

    candidate = st.cam.pos;
    candidate.z += delta.z * vel;
    if (queryFloorHit(st, candidate, fy)) {
        st.cam.pos.z = candidate.z;
    }

    // Gravity + jump. Space = jump, Ctrl = fast descend only when in air (debug).
    const float gravity = -18.0f;
    const float jumpSpeed = 6.5f;

    const float floorY = queryFloorYOrAabb(st, st.cam.pos);
    const float ceilY = st.worldMax.y;
    const float minEyeY = floorY + st.eyeHeight;
    const float maxEyeY = ceilY - 0.05f;

    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS && st.onGround) {
        st.verticalVel = jumpSpeed;
        st.onGround = false;
    }

    if (!st.onGround && glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
        st.verticalVel += gravity * 1.5f * dt;
    }

    st.verticalVel += gravity * dt;
    st.cam.pos.y += st.verticalVel * dt;

    // Collide with floor/ceiling (eye point).
    if (st.cam.pos.y < (minEyeY + 0.02f)) {
        st.cam.pos.y = (minEyeY + 0.02f);
        st.verticalVel = 0.0f;
        st.onGround = true;
    }
    if (st.cam.pos.y > maxEyeY) {
        st.cam.pos.y = maxEyeY;
        if (st.verticalVel > 0.0f) st.verticalVel = 0.0f;
    }

    // Collide with walls: keep the camera inside the scene AABB.
    const float r = st.playerRadius;
    st.cam.pos.x = glm::clamp(st.cam.pos.x, st.worldMin.x + r, st.worldMax.x - r);
    st.cam.pos.z = glm::clamp(st.cam.pos.z, st.worldMin.z + r, st.worldMax.z - r);
}

static GpuMesh uploadMesh(const aiMesh* mesh) {
    // Interleaved: position (3), normal (3), uv (2), tangent (3)
    std::vector<float> verts;
    verts.reserve(static_cast<size_t>(mesh->mNumVertices) * 11);
    for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
        const aiVector3D& p = mesh->mVertices[i];
        aiVector3D n(0, 1, 0);
        if (mesh->HasNormals()) n = mesh->mNormals[i];
        float u = 0.0f, v = 0.0f;
        if (mesh->HasTextureCoords(0) && mesh->mTextureCoords[0]) {
            u = mesh->mTextureCoords[0][i].x;
            v = mesh->mTextureCoords[0][i].y;
        }
        aiVector3D t(1, 0, 0);
        if (mesh->HasTangentsAndBitangents() && mesh->mTangents) {
            t = mesh->mTangents[i];
        }
        verts.push_back(p.x);
        verts.push_back(p.y);
        verts.push_back(p.z);
        verts.push_back(n.x);
        verts.push_back(n.y);
        verts.push_back(n.z);
        verts.push_back(u);
        verts.push_back(v);
        verts.push_back(t.x);
        verts.push_back(t.y);
        verts.push_back(t.z);
    }

    std::vector<unsigned int> indices;
    indices.reserve(static_cast<size_t>(mesh->mNumFaces) * 3);
    for (unsigned int f = 0; f < mesh->mNumFaces; f++) {
        const aiFace& face = mesh->mFaces[f];
        if (face.mNumIndices != 3) continue;
        indices.push_back(face.mIndices[0]);
        indices.push_back(face.mIndices[1]);
        indices.push_back(face.mIndices[2]);
    }

    GpuMesh out;
    out.indexCount = static_cast<GLsizei>(indices.size());

    glGenVertexArrays(1, &out.vao);
    glBindVertexArray(out.vao);

    glGenBuffers(1, &out.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, out.vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);

    glGenBuffers(1, &out.ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, out.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)(8 * sizeof(float)));

    glBindVertexArray(0);
    return out;
}

static GpuMesh createCeilingPlane(const glm::vec3& worldMin, const glm::vec3& worldMax) {
    // Plane with normal pointing down, in centered/world space.
    const float y = worldMax.y;
    const glm::vec3 p0(worldMin.x, y, worldMin.z);
    const glm::vec3 p1(worldMax.x, y, worldMin.z);
    const glm::vec3 p2(worldMax.x, y, worldMax.z);
    const glm::vec3 p3(worldMin.x, y, worldMax.z);

    const glm::vec3 n(0.0f, -1.0f, 0.0f);
    const glm::vec3 t(1.0f, 0.0f, 0.0f);

    // Interleaved: position (3), normal (3), uv (2), tangent (3)
    const float verts[] = {
        // p0
        p0.x, p0.y, p0.z,  n.x, n.y, n.z,  0.0f, 0.0f,  t.x, t.y, t.z,
        // p1
        p1.x, p1.y, p1.z,  n.x, n.y, n.z,  1.0f, 0.0f,  t.x, t.y, t.z,
        // p2
        p2.x, p2.y, p2.z,  n.x, n.y, n.z,  1.0f, 1.0f,  t.x, t.y, t.z,
        // p3
        p3.x, p3.y, p3.z,  n.x, n.y, n.z,  0.0f, 1.0f,  t.x, t.y, t.z,
    };
    const unsigned int idx[] = { 0, 1, 2, 0, 2, 3 };

    GpuMesh out;
    out.indexCount = 6;
    out.baseColorFactor = glm::vec4(1.0f);
    out.metallicFactor = 0.0f;
    out.roughnessFactor = 1.0f;

    glGenVertexArrays(1, &out.vao);
    glBindVertexArray(out.vao);

    glGenBuffers(1, &out.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, out.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

    glGenBuffers(1, &out.ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, out.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)(8 * sizeof(float)));

    glBindVertexArray(0);
    return out;
}

static void destroyMesh(GpuMesh& m) {
    if (m.ebo) glDeleteBuffers(1, &m.ebo);
    if (m.vbo) glDeleteBuffers(1, &m.vbo);
    if (m.vao) glDeleteVertexArrays(1, &m.vao);
    if (m.baseColorTex) glDeleteTextures(1, &m.baseColorTex);
    if (m.normalTex) glDeleteTextures(1, &m.normalTex);
    if (m.metallicRoughnessTex) glDeleteTextures(1, &m.metallicRoughnessTex);
    if (m.specularF0Tex) glDeleteTextures(1, &m.specularF0Tex);
    m = {};
}

struct Bounds {
    glm::vec3 min{ 0.0f };
    glm::vec3 max{ 0.0f };
    glm::vec3 center{ 0.0f };
    glm::vec3 size{ 1.0f };
    float radius = 1.0f;
};

static Bounds computeSceneBounds(const aiScene* scene) {
    Bounds b;
    if (!scene || scene->mNumMeshes == 0) return b;

    glm::vec3 minP(FLT_MAX), maxP(-FLT_MAX);
    bool any = false;

    for (unsigned int mi = 0; mi < scene->mNumMeshes; mi++) {
        const aiMesh* mesh = scene->mMeshes[mi];
        if (!mesh || !mesh->HasPositions()) continue;
        for (unsigned int vi = 0; vi < mesh->mNumVertices; vi++) {
            const aiVector3D& p = mesh->mVertices[vi];
            glm::vec3 v(p.x, p.y, p.z);
            minP = glm::min(minP, v);
            maxP = glm::max(maxP, v);
            any = true;
        }
    }

    if (!any) return b;

    b.min = minP;
    b.max = maxP;
    b.center = (minP + maxP) * 0.5f;
    b.size = (maxP - minP);
    b.radius = glm::length(b.size) * 0.5f;
    if (b.radius < 0.001f) b.radius = 0.001f;
    return b;
}

static void printSceneInfo(const aiScene* scene) {
    if (!scene) {
        std::cout << "Error: Scene is null!\n";
        return;
    }

    std::cout << "\n========== Model Information ==========\n";
    std::cout << "Meshes: " << scene->mNumMeshes << "\n";
    std::cout << "Materials: " << scene->mNumMaterials << "\n";
    std::cout << "Animations: " << scene->mNumAnimations << "\n";
    std::cout << "Lights: " << scene->mNumLights << "\n";
    std::cout << "Cameras: " << scene->mNumCameras << "\n";

    if (scene->mNumMeshes > 0) {
        std::cout << "\n--- Mesh Details ---\n";
        for (unsigned int i = 0; i < scene->mNumMeshes; i++) {
            aiMesh* mesh = scene->mMeshes[i];
            std::cout << "Mesh " << i << ": " << mesh->mName.C_Str() << "\n";
            std::cout << "  Vertices: " << mesh->mNumVertices << "\n";
            std::cout << "  Faces: " << mesh->mNumFaces << "\n";
        }
    }
    std::cout << "=====================================\n\n";
}

int main(int argc, char** argv) {
    // Default model (you can override by passing a file or directory path as argv[1]).
    std::string modelPath = "C:\\Users\\Kevin\\Downloads\\child_bedroom_2025190401";
    if (argc >= 2 && argv[1] && argv[1][0] != '\0') {
        modelPath = argv[1];
    }

    // If a directory is passed, try common filenames.
    try {
        std::filesystem::path p(modelPath);
        if (std::filesystem::is_directory(p)) {
            const std::filesystem::path candidates[] = {
                p / "scene.gltf",
                p / "scene.glb",
                p / "model.gltf",
                p / "model.glb",
            };
            for (const auto& c : candidates) {
                if (std::filesystem::exists(c)) {
                    modelPath = c.string();
                    break;
                }
            }
        }
    } catch (...) {
        // Keep whatever modelPath we had.
    }
    std::cout << "Loading 3D model: " << modelPath << "\n";

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(
        modelPath,
        aiProcess_Triangulate |
        aiProcess_GenNormals |
        aiProcess_CalcTangentSpace |
        aiProcess_SplitLargeMeshes |
        aiProcess_ValidateDataStructure
    );

    if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode) {
        std::cerr << "Error loading model: " << importer.GetErrorString() << "\n";
        return 1;
    }

    std::cout << "Model loaded successfully!\n";
    printSceneInfo(scene);

    glfwSetErrorCallback(glfwErrorCallback);
    if (!glfwInit()) {
        std::cerr << "Failed to init GLFW\n";
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Assimp Viewer", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return 1;
    }
    std::cout << "Window created. Controls: Mouse look, WASD move, Space/Ctrl up/down, Shift sprint, Esc exit.\n";
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    // Mouse look (FPS): ocultar/bloquear cursor y capturar deltas
    InputState input;
    glfwSetWindowUserPointer(window, &input);
    input.cursorCaptured = true;
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetCursorPosCallback(window, mouseCallback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to init GLAD\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_FRAMEBUFFER_SRGB);

    const char* vsSrc = R"GLSL(
        #version 330 core
        layout (location = 0) in vec3 aPos;
        layout (location = 1) in vec3 aNormal;
        layout (location = 2) in vec2 aUV;
        layout (location = 3) in vec3 aTangent;

        uniform mat4 uMVP;
        uniform mat4 uModel;

        out vec3 vNormal;
        out vec2 vUV;
        out vec3 vTangent;
        out vec3 vWorldPos;

        void main() {
            vec4 world = uModel * vec4(aPos, 1.0);
            vNormal = mat3(transpose(inverse(uModel))) * aNormal;
            vUV = aUV;
            vTangent = mat3(uModel) * aTangent;
            vWorldPos = world.xyz;
            gl_Position = uMVP * vec4(aPos, 1.0);
        }
    )GLSL";

    const char* fsSrc = R"GLSL(
        #version 330 core
        in vec3 vNormal;
        in vec2 vUV;
        in vec3 vTangent;
        in vec3 vWorldPos;
        out vec4 FragColor;

        uniform sampler2D uBaseColor;
        uniform sampler2D uNormal;
        uniform sampler2D uMetallicRoughness;
        uniform sampler2D uSpecularF0;
        uniform int uUseBaseColor;
        uniform int uUseNormal;
        uniform int uUseMetallicRoughness;
        uniform int uUseSpecularF0;

        uniform vec4 uBaseColorFactor;
        uniform float uMetallicFactor;
        uniform float uRoughnessFactor;
        uniform vec3 uCameraPos;

        void main() {
            vec3 N = normalize(vNormal);
            if (uUseNormal != 0) {
                vec3 T = normalize(vTangent);
                // Re-orthonormalize T vs N
                T = normalize(T - N * dot(N, T));
                vec3 B = normalize(cross(N, T));
                mat3 TBN = mat3(T, B, N);
                vec3 nTex = texture(uNormal, vUV).xyz * 2.0 - 1.0;
                N = normalize(TBN * nTex);
            }
            vec3 L = normalize(vec3(0.3, 1.0, 0.4));
            float ndotl = max(dot(N, L), 0.0);
            vec3 base = uBaseColorFactor.rgb;
            if (uUseBaseColor != 0) {
                base *= texture(uBaseColor, vUV).rgb;
            }

            float metallic = 0.0;
            float roughness = 1.0;
            if (uUseMetallicRoughness != 0) {
                vec4 mr = texture(uMetallicRoughness, vUV);
                roughness = clamp(mr.g, 0.04, 1.0);
                metallic = clamp(mr.b, 0.0, 1.0);
            }

            metallic = clamp(metallic * uMetallicFactor, 0.0, 1.0);
            roughness = clamp(roughness * uRoughnessFactor, 0.04, 1.0);

            vec3 F0 = mix(vec3(0.04), base, metallic);
            if (uUseSpecularF0 != 0) {
                F0 = texture(uSpecularF0, vUV).rgb;
            }

            // Cheap specular term (not full PBR, but closer than diffuse-only)
            vec3 V = normalize(uCameraPos - vWorldPos);
            vec3 H = normalize(L + V);
            float ndoth = max(dot(N, H), 0.0);
            float specPow = mix(128.0, 8.0, roughness);
            vec3 spec = F0 * pow(ndoth, specPow);

            vec3 diffuse = base * (0.15 + 0.85 * ndotl) * (1.0 - metallic);
            vec3 color = diffuse + spec * ndotl;
            FragColor = vec4(color, 1.0);
        }
    )GLSL";

    GLuint prog = createProgram(vsSrc, fsSrc);
    if (!prog) {
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    if (scene->mNumMeshes == 0) {
        std::cerr << "No meshes to render.\n";
        glDeleteProgram(prog);
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    const std::filesystem::path modelDir = std::filesystem::path(modelPath).parent_path();
    std::unordered_map<std::string, GLuint> texCache;
    GLuint fallbackTex = createSolidTextureRGBA(191, 191, 191, 255);
    GLuint fallbackNormal = createSolidTextureRGBA(128, 128, 255, 255);
    GLuint fallbackMR = createSolidTextureRGBA(0, 255, 0, 255); // roughness=1 (G=1), metallic=0 (B=0)

    std::vector<GpuMesh> meshes;
    meshes.reserve(scene->mNumMeshes);
    for (unsigned int i = 0; i < scene->mNumMeshes; i++) {
        const aiMesh* m = scene->mMeshes[i];
        GpuMesh gpu = uploadMesh(m);
        if (gpu.indexCount == 0) continue;

        GLuint baseColor = 0;
        GLuint normal = 0;
        GLuint mr = 0;
        GLuint specF0 = 0;
        if (m->mMaterialIndex < scene->mNumMaterials) {
            aiMaterial* mat = scene->mMaterials[m->mMaterialIndex];

            auto loadCached = [&](const std::filesystem::path& p, bool srgb) -> GLuint {
                const std::string key = p.string() + (srgb ? "|srgb" : "|lin");
                auto it = texCache.find(key);
                if (it != texCache.end()) return it->second;
                GLuint t = loadTextureWIC(p, srgb);
                if (t) texCache.emplace(key, t);
                return t;
            };

            aiString rel;
            // BaseColor (sRGB)
            if (mat->GetTexture(aiTextureType_BASE_COLOR, 0, &rel) != AI_SUCCESS) {
                mat->GetTexture(aiTextureType_DIFFUSE, 0, &rel);
            }
            if (rel.length > 0) {
                auto p = (modelDir / std::filesystem::path(rel.C_Str())).lexically_normal();
                baseColor = loadCached(p, true);
            }

            // Normal (linear)
            rel.Clear();
            if (mat->GetTexture(aiTextureType_NORMALS, 0, &rel) == AI_SUCCESS && rel.length > 0) {
                auto p = (modelDir / std::filesystem::path(rel.C_Str())).lexically_normal();
                normal = loadCached(p, false);
            }

            // MetallicRoughness (linear)
            rel.Clear();
            if (mat->GetTexture(aiTextureType_METALNESS, 0, &rel) != AI_SUCCESS) {
                // Assimp's glTF importer commonly maps metallicRoughnessTexture here.
                // As a fallback, also try UNKNOWN to cover older mappings.
                mat->GetTexture(aiTextureType_UNKNOWN, 0, &rel);
            }
            if (rel.length > 0) {
                auto p = (modelDir / std::filesystem::path(rel.C_Str())).lexically_normal();
                mr = loadCached(p, false);
            }

            // Fallback for glTF: metallicRoughness sometimes lands in DIFFUSE_ROUGHNESS.
            if (!mr) {
                rel.Clear();
                if (mat->GetTexture(aiTextureType_DIFFUSE_ROUGHNESS, 0, &rel) == AI_SUCCESS && rel.length > 0) {
                    auto p = (modelDir / std::filesystem::path(rel.C_Str())).lexically_normal();
                    mr = loadCached(p, false);
                }
            }

            // Specular F0 (linear) from KHR_materials_specular
            rel.Clear();
            if (mat->GetTexture(aiTextureType_SPECULAR, 0, &rel) == AI_SUCCESS && rel.length > 0) {
                auto p = (modelDir / std::filesystem::path(rel.C_Str())).lexically_normal();
                specF0 = loadCached(p, false);
            }
        }

        // Factors (works even when there are no textures)
        if (m->mMaterialIndex < scene->mNumMaterials) {
            aiMaterial* mat = scene->mMaterials[m->mMaterialIndex];

            aiColor4D c;
            if (aiGetMaterialColor(mat, AI_MATKEY_BASE_COLOR, &c) != AI_SUCCESS) {
                aiGetMaterialColor(mat, AI_MATKEY_COLOR_DIFFUSE, &c);
            }
            gpu.baseColorFactor = glm::vec4(c.r, c.g, c.b, c.a);

            float f = 0.0f;
            if (aiGetMaterialFloat(mat, AI_MATKEY_METALLIC_FACTOR, &f) == AI_SUCCESS) gpu.metallicFactor = f;
            if (aiGetMaterialFloat(mat, AI_MATKEY_ROUGHNESS_FACTOR, &f) == AI_SUCCESS) gpu.roughnessFactor = f;
        }

        gpu.baseColorTex = baseColor ? baseColor : fallbackTex;
        gpu.normalTex = normal ? normal : fallbackNormal;
        gpu.metallicRoughnessTex = mr ? mr : fallbackMR;
        gpu.specularF0Tex = specF0;
        meshes.push_back(gpu);
    }

    if (meshes.empty()) {
        std::cerr << "No drawable meshes (no triangle indices).\n";
        glDeleteTextures(1, &fallbackTex);
        glDeleteProgram(prog);
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    // Auto-frame model: center it at origin and place camera to see it.
    const Bounds bounds = computeSceneBounds(scene);
    const glm::vec3 sceneCenter = bounds.center;
    const float sceneRadius = bounds.radius;
    const float fitDistance = (sceneRadius / tanf(glm::radians(input.cam.fov * 0.5f))) * 1.2f;
    glm::mat4 model = glm::translate(glm::mat4(1.0f), -sceneCenter);

    // Build a simple floor collision triangle list in centered space.
    // We keep only triangles whose geometric normal points mostly up.
    std::vector<InputState::Tri> floorTris;
    floorTris.reserve(20000);
    for (unsigned int mi = 0; mi < scene->mNumMeshes; mi++) {
        const aiMesh* m = scene->mMeshes[mi];
        if (!m || !m->HasPositions()) continue;
        for (unsigned int fi = 0; fi < m->mNumFaces; fi++) {
            const aiFace& f = m->mFaces[fi];
            if (f.mNumIndices != 3) continue;
            const aiVector3D& pa = m->mVertices[f.mIndices[0]];
            const aiVector3D& pb = m->mVertices[f.mIndices[1]];
            const aiVector3D& pc = m->mVertices[f.mIndices[2]];
            glm::vec3 a(pa.x, pa.y, pa.z);
            glm::vec3 b(pb.x, pb.y, pb.z);
            glm::vec3 c(pc.x, pc.y, pc.z);
            // Center the triangle by subtracting sceneCenter (same as model translate).
            a -= sceneCenter;
            b -= sceneCenter;
            c -= sceneCenter;
            const glm::vec3 n = glm::normalize(glm::cross(b - a, c - a));
            if (!std::isfinite(n.x) || !std::isfinite(n.y) || !std::isfinite(n.z)) continue;
            if (n.y < 0.6f) continue; // only floor-like surfaces
            floorTris.push_back({ a, b, c });
        }
    }
    input.floorTris = &floorTris;

    // Scene bounds in centered space (since model translates by -sceneCenter).
    input.worldMin = -bounds.size * 0.5f;
    input.worldMax =  bounds.size * 0.5f;
    input.playerRadius = glm::clamp(sceneRadius * 0.02f, 0.15f, 0.60f);
    // Eye height controls how "high" the user feels above the floor.
    input.eyeHeight = glm::clamp(bounds.size.y * 0.2f, 0.8f, 1.7f);
    input.eyeHeight = std::min(input.eyeHeight + 0.45f, bounds.size.y - 0.10f);
    // Movement speed in "scene units per second". Scale by scene size so large rooms don't feel slow.
    input.baseSpeed = glm::clamp(sceneRadius * 1.2f, 4.0f, 35.0f);

    // Initial FPS spawn: center of the scene, slightly above the floor.
    // Since we translate the scene by -sceneCenter, the center becomes (0,0,0) in model space.
    const float floorY = queryFloorYOrAabb(input, glm::vec3(0.0f, 0.0f, 0.0f));
    input.cam.pos = glm::vec3(0.0f, floorY + input.eyeHeight + 0.02f, 0.0f);
    input.verticalVel = 0.0f;
    input.onGround = true;
    input.cam.yaw = -90.0f;
    input.cam.pitch = 0.0f;

    // Keep generous clip planes for large scenes; avoid tiny zNear.
    const float zNear = std::max(0.01f, sceneRadius * 0.001f);
    const float zFar = std::max(10.0f, sceneRadius * 20.0f);

    // Add a simple ceiling so you don't see outside/void when looking up.
    GpuMesh ceiling = createCeilingPlane(input.worldMin, input.worldMax);
    // Ceiling texture (sRGB). If it fails, fall back to solid gray.
    {
        const std::filesystem::path ceilTexPath = "C:\\Users\\Kevin\\Downloads\\piso_textura_madera_8_c.png";
        GLuint ceilBase = loadTextureWIC(ceilTexPath, true);
        std::cout << "Ceiling texture load: " << ceilTexPath.string() << " -> " << (ceilBase ? "OK" : "FAILED") << "\n";
        ceiling.baseColorTex = ceilBase ? ceilBase : fallbackTex;
    }
    ceiling.normalTex = fallbackNormal;
    ceiling.metallicRoughnessTex = fallbackMR;
    ceiling.specularF0Tex = 0;

    float lastTime = static_cast<float>(glfwGetTime());
    while (!glfwWindowShouldClose(window)) {
        const float now = static_cast<float>(glfwGetTime());
        const float dt = now - lastTime;
        lastTime = now;

        glfwPollEvents();
        processInput(window, input, dt);

        int w = 0, h = 0;
        glfwGetFramebufferSize(window, &w, &h);
        if (w <= 0 || h <= 0) continue;
        glViewport(0, 0, w, h);

        glClearColor(0.08f, 0.09f, 0.11f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 view = cameraView(input.cam);
        glm::mat4 proj = glm::perspective(glm::radians(input.cam.fov), (float)w / (float)h, zNear, zFar);
        glm::mat4 mvp = proj * view * model;

        glUseProgram(prog);
        glUniformMatrix4fv(glGetUniformLocation(prog, "uMVP"), 1, GL_FALSE, glm::value_ptr(mvp));
        glUniformMatrix4fv(glGetUniformLocation(prog, "uModel"), 1, GL_FALSE, glm::value_ptr(model));
        glUniform3fv(glGetUniformLocation(prog, "uCameraPos"), 1, glm::value_ptr(input.cam.pos));

        glActiveTexture(GL_TEXTURE0);
        glUniform1i(glGetUniformLocation(prog, "uBaseColor"), 0);
        glActiveTexture(GL_TEXTURE1);
        glUniform1i(glGetUniformLocation(prog, "uNormal"), 1);
        glActiveTexture(GL_TEXTURE2);
        glUniform1i(glGetUniformLocation(prog, "uMetallicRoughness"), 2);
        glActiveTexture(GL_TEXTURE3);
        glUniform1i(glGetUniformLocation(prog, "uSpecularF0"), 3);

        // Draw ceiling in centered space (identity model).
        {
            const glm::mat4 ceilModel(1.0f);
            const glm::mat4 ceilMvp = proj * view * ceilModel;
            glUniformMatrix4fv(glGetUniformLocation(prog, "uMVP"), 1, GL_FALSE, glm::value_ptr(ceilMvp));
            glUniformMatrix4fv(glGetUniformLocation(prog, "uModel"), 1, GL_FALSE, glm::value_ptr(ceilModel));

            glUniform4fv(glGetUniformLocation(prog, "uBaseColorFactor"), 1, glm::value_ptr(ceiling.baseColorFactor));
            glUniform1f(glGetUniformLocation(prog, "uMetallicFactor"), ceiling.metallicFactor);
            glUniform1f(glGetUniformLocation(prog, "uRoughnessFactor"), ceiling.roughnessFactor);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, ceiling.baseColorTex);
            glUniform1i(glGetUniformLocation(prog, "uUseBaseColor"), ceiling.baseColorTex != fallbackTex ? 1 : 0);

            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, ceiling.normalTex);
            glUniform1i(glGetUniformLocation(prog, "uUseNormal"), 0);

            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, ceiling.metallicRoughnessTex);
            glUniform1i(glGetUniformLocation(prog, "uUseMetallicRoughness"), 0);

            glActiveTexture(GL_TEXTURE3);
            glBindTexture(GL_TEXTURE_2D, 0);
            glUniform1i(glGetUniformLocation(prog, "uUseSpecularF0"), 0);

            glBindVertexArray(ceiling.vao);
            glDrawElements(GL_TRIANGLES, ceiling.indexCount, GL_UNSIGNED_INT, (void*)0);
        }

        // Restore model uniforms for the imported scene.
        glUniformMatrix4fv(glGetUniformLocation(prog, "uMVP"), 1, GL_FALSE, glm::value_ptr(mvp));
        glUniformMatrix4fv(glGetUniformLocation(prog, "uModel"), 1, GL_FALSE, glm::value_ptr(model));

        for (const auto& mesh : meshes) {
            glUniform4fv(glGetUniformLocation(prog, "uBaseColorFactor"), 1, glm::value_ptr(mesh.baseColorFactor));
            glUniform1f(glGetUniformLocation(prog, "uMetallicFactor"), mesh.metallicFactor);
            glUniform1f(glGetUniformLocation(prog, "uRoughnessFactor"), mesh.roughnessFactor);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, mesh.baseColorTex);
            glUniform1i(glGetUniformLocation(prog, "uUseBaseColor"), mesh.baseColorTex != fallbackTex ? 1 : 0);

            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, mesh.normalTex);
            glUniform1i(glGetUniformLocation(prog, "uUseNormal"), mesh.normalTex != fallbackNormal ? 1 : 0);

            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, mesh.metallicRoughnessTex);
            glUniform1i(glGetUniformLocation(prog, "uUseMetallicRoughness"), mesh.metallicRoughnessTex != fallbackMR ? 1 : 0);

            glActiveTexture(GL_TEXTURE3);
            glBindTexture(GL_TEXTURE_2D, mesh.specularF0Tex ? mesh.specularF0Tex : 0);
            glUniform1i(glGetUniformLocation(prog, "uUseSpecularF0"), mesh.specularF0Tex ? 1 : 0);

            glBindVertexArray(mesh.vao);
            glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, (void*)0);
        }
        glBindVertexArray(0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0);

        glfwSwapBuffers(window);
    }

    // Destroy GPU resources
    for (auto& m : meshes) {
        // Avoid deleting shared cached textures here.
        m.baseColorTex = 0;
        m.normalTex = 0;
        m.metallicRoughnessTex = 0;
        m.specularF0Tex = 0;
        destroyMesh(m);
    }
    for (auto& kv : texCache) {
        if (kv.second) glDeleteTextures(1, &kv.second);
    }

    destroyMesh(ceiling);

    glDeleteTextures(1, &fallbackTex);
    glDeleteTextures(1, &fallbackNormal);
    glDeleteTextures(1, &fallbackMR);
    glDeleteProgram(prog);
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
