#pragma once
#include <string>
#include <vector>
#include <glm/glm.hpp>

#include "Localization.h"

struct MenuItem {
    std::string text;
    float x, y, w, h;
    bool hovered;
};

class TextRenderer;

class Menu {
public:
    Menu();
    void init(TextRenderer* textRenderer, int windowWidth, int windowHeight, float textScale);
    void setLanguage(Language language);
    void update(float mouseX, float mouseY);
    int getHoveredIndex() const { return mHoveredIndex; }
    const std::vector<MenuItem>& getItems() const { return mItems; }
    float getTextScale() const { return mTextScale; }
    float getSlideOffset() const { return mSlideOffset; }
    void setSlideOffset(float offset) { mSlideOffset = offset; }
private:
    std::vector<MenuItem> mItems;
    TextRenderer* mTextRenderer;
    int mWindowWidth, mWindowHeight;
    int mHoveredIndex;
    float mTextScale;
    float mSlideOffset = 0.0f;
    Language mLanguage;
    void recalcPositions();
    void refreshLabels();
};
