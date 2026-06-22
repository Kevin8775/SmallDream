#include "HouseScene.h"
#include "Shader.h"

bool HouseScene::load(const std::string& directory) {
    mLoaded = mModel.load(directory);
    if (mLoaded) {
        mMin = mModel.boundsMin();
        mMax = mModel.boundsMax();
        mCenter = (mMin + mMax) * 0.5f;
    }
    return mLoaded;
}

void HouseScene::render(Shader& shader, const glm::mat4& view, const glm::mat4& projection, const glm::mat4& model) {
    mModel.draw(shader, view, projection, model);
}

void HouseScene::destroy() {
    mModel.destroy();
    mLoaded = false;
}

bool HouseScene::isLoaded() const { return mLoaded; }

void HouseScene::placeCameraInside(glm::vec3& camPos, glm::vec3& camFront,
                                  float& camYaw, float& camPitch,
                                  float eyeHeight, float floorFactor,
                                  const glm::vec3& housePos,
                                  float modelScale) const {
    glm::vec3 minW = (mMin - mCenter) * modelScale;
    glm::vec3 maxW = (mMax - mCenter) * modelScale;
    glm::vec3 centerW = (minW + maxW) * 0.5f;
    float houseFloorY = minW.y + (maxW.y - minW.y) * floorFactor;
    camPos = housePos + glm::vec3(centerW.x, houseFloorY + eyeHeight, centerW.z);
    camFront = glm::normalize(glm::vec3(0.0f, 0.0f, -1.0f));
    camYaw = -90.0f;
    camPitch = 0.0f;
}

float HouseScene::getFloorY(float floorFactor) const {
    return mMin.y + (mMax.y - mMin.y) * floorFactor;
}
