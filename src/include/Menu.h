#pragma once
#include <string>
#include <vector>
#include <glm/glm.hpp>

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
    void update(float mouseX, float mouseY);
    int getHoveredIndex() const { return mHoveredIndex; }
    const std::vector<MenuItem>& getItems() const { return mItems; }
    float getTextScale() const { return mTextScale; }
private:
    std::vector<MenuItem> mItems;
    TextRenderer* mTextRenderer;
    int mWindowWidth, mWindowHeight;
    int mHoveredIndex;
    float mTextScale;
    void recalcPositions();
};
