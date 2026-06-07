#include "Menu.h"
#include "TextRenderer.h"
#include <iostream>

Menu::Menu() : mTextRenderer(nullptr), mWindowWidth(0), mWindowHeight(0), mHoveredIndex(-1), mTextScale(1.0f) {
    mItems = {
        {"Nuevo Sue\u00f1o", 0, 0, 0, 0, false},
        {"Continuar Exploraci\u00f3n", 0, 0, 0, 0, false},
        {"Ajustes de Casa", 0, 0, 0, 0, false},
        {"Cr\u00e9ditos", 0, 0, 0, 0, false},
        {"Salir", 0, 0, 0, 0, false}
    };
}

void Menu::init(TextRenderer* textRenderer, int windowWidth, int windowHeight, float textScale) {
    mTextRenderer = textRenderer;
    mWindowWidth = windowWidth;
    mWindowHeight = windowHeight;
    mTextScale = textScale;
    recalcPositions();
}

void Menu::recalcPositions() {
    float totalHeight = 0;
    float spacing = 50.0f;
    std::vector<float> heights;
    for (auto& item : mItems) {
        glm::vec2 size = mTextRenderer->getTextSize(item.text, mTextScale);
        item.w = size.x;
        item.h = size.y;
        heights.push_back(size.y);
        totalHeight += size.y;
    }
    totalHeight += spacing * (mItems.size() - 1);

    float startY = (mWindowHeight - totalHeight) / 1.8f;
    float currentY = startY;
    for (size_t i = 0; i < mItems.size(); i++) {
        mItems[i].x = (mWindowWidth - mItems[i].w) / 2.0f;
        mItems[i].y = currentY;
        currentY += heights[i] + spacing;
    }
}

void Menu::update(float mouseX, float mouseY) {
    mHoveredIndex = -1;
    const float padX = 32.0f;
    const float padY = 16.0f;
    for (size_t i = 0; i < mItems.size(); i++) {
        auto& item = mItems[i];
        item.hovered = false;
        if (mouseX >= item.x - padX && mouseX <= item.x + item.w + padX &&
            mouseY >= item.y - padY && mouseY <= item.y + item.h + padY) {
            item.hovered = true;
            mHoveredIndex = static_cast<int>(i);
        }
    }
}
