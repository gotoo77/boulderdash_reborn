#pragma once

#include <SDL.h>

#include <filesystem>

struct TextStyle {
    SDL_Color color{ 255, 255, 255, 255 };
    int scale = 2;
    float maxWidth = 0.9f;
    float y = 0.0f;
};

struct OptionsScreenStyle {
    TextStyle title{ SDL_Color{ 255, 112, 67, 255 }, 2, 0.8f, 0.08f };
    TextStyle lines{ SDL_Color{ 0, 255, 247, 255 }, 1, 0.88f, 0.28f };
    TextStyle hints{ SDL_Color{ 111, 195, 247, 255 }, 1, 0.88f, 0.70f };
    float lineSpacing = 0.09f;
    float hintSpacing = 0.06f;
};

OptionsScreenStyle loadOptionsStyle(const std::filesystem::path& path);
