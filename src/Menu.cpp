#include "Menu.h"
#include "TextRenderer.h"
#include "Localization.h"
#include <iostream>

Menu::Menu() : mTextRenderer(nullptr), mWindowWidth(0), mWindowHeight(0), mHoveredIndex(-1), mTextScale(1.0f), mLanguage(Language::Spanish) {
    mItems = {
        {"", 0, 0, 0, 0, false},
        {"", 0, 0, 0, 0, false},
        {"", 0, 0, 0, 0, false},
        {"", 0, 0, 0, 0, false},
        {"", 0, 0, 0, 0, false},
        {"", 0, 0, 0, 0, false}
    };
}

void Menu::init(TextRenderer* textRenderer, int windowWidth, int windowHeight, float textScale) {
    mTextRenderer = textRenderer;
    mWindowWidth = windowWidth;
    mWindowHeight = windowHeight;
    mTextScale = textScale;
    mLanguage = Localization::current();
    refreshLabels();
    recalcPositions();
}

void Menu::setLanguage(Language language) {
    mLanguage = language;
    refreshLabels();
    recalcPositions();
    mHoveredIndex = -1;
}

void Menu::refreshLabels() {
    if (mItems.size() < 6) return;
    mItems[0].text = Localization::t(TextId::MenuNewDream);
    mItems[1].text = Localization::t(TextId::MenuContinueExploring);
    mItems[2].text = Localization::t(TextId::MenuControls);
    mItems[3].text = Localization::t(TextId::MenuCredits);
    mItems[4].text = Localization::t(TextId::MenuLanguage);
    mItems[5].text = Localization::t(TextId::MenuExit);
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
