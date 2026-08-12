#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace menu {

struct Color {
    uint8_t r = 255;
    uint8_t g = 255;
    uint8_t b = 255;
    uint8_t a = 255;
};

struct MenuColors {
    Color normal{ 180, 180, 180, 255 };
    Color selected{ 255, 240, 160, 255 };
    Color disabled{ 110, 110, 110, 255 };
};

enum class AnchorPoint {
    TopLeft,
    TopCenter,
    TopRight,
    Center,
    BottomLeft,
    BottomCenter,
    BottomRight
};

enum class LayoutDirection {
    Vertical,
    Horizontal
};

enum class TextAlign {
    Left,
    Center,
    Right
};

struct MenuLayout {
    AnchorPoint anchor = AnchorPoint::Center;
    LayoutDirection direction = LayoutDirection::Vertical;
    TextAlign align = TextAlign::Center;
    float spacing = 24.0f;
    float offsetX = 0.0f;
    float offsetY = 0.0f;
};

struct LogoDefinition {
    std::string textureId;
    float normalizedX = 0.5f;
    float normalizedY = 0.2f;
    float width = 0.0f;
    float height = 0.0f;
    bool visible = false;
};

struct MenuItem {
    std::string label;
    std::string labelId;
    std::string action;
    bool enabled = true;
};

struct HeaderTextStyle {
    Color color{};
    int scale = 4;
    float normalizedY = 0.12f;
    float maxWidthRatio = 0.9f;
};

struct HeaderDefinition {
    HeaderTextStyle title{};
    HeaderTextStyle subtitle{ Color{ 180, 180, 180, 255 }, 2, 0.20f, 0.9f };
    float spacing = 10.0f;
};

struct TransitionDefinition {
    std::string type;
    float duration = 0.0f;
    float delay = 0.0f;
};

struct MenuDefinition {
    std::string id;
    MenuLayout layout;
    std::optional<LogoDefinition> logo;
    std::optional<TransitionDefinition> transition;
    std::optional<HeaderDefinition> header;
    MenuColors colors{};
    int itemScale = 0;
    std::vector<MenuItem> items;
};

struct MenuRenderMetrics {
    int viewportWidth = 0;
    int viewportHeight = 0;
    float uiScale = 1.0f;
    std::optional<float> pixelRatio;
};

} // namespace menu
