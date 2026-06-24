#pragma once
#include "Model.h"
#include <glm/glm.hpp>
#include <string>

class Shader;

class HouseScene {
public:
    bool load(const std::string& directory);
    void render(Shader& shader, const glm::mat4& view, const glm::mat4& projection, const glm::mat4& model);
    void destroy();
    bool isLoaded() const;

    void placeCameraInside(glm::vec3& camPos, glm::vec3& camFront,
                          float& camYaw, float& camPitch,
                          float eyeHeight, float floorFactor,
                          const glm::vec3& housePos,
                          float modelScale) const;
    float getFloorY(float floorFactor) const;
    glm::vec3 getCenter() const { return mCenter; }
    glm::vec3 getSize() const { return mMax - mMin; }
    glm::vec3 getMin() const { return mMin; }
    glm::vec3 getMax() const { return mMax; }

private:
    Model mModel;
    glm::vec3 mMin{0.0f}, mMax{0.0f}, mCenter{0.0f};
    bool mLoaded = false;
};
