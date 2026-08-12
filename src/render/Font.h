#pragma once

#include <SDL.h>

#include <string_view>

// Lightweight text drawing contract implemented by concrete fonts.
class Font {
public:
    virtual ~Font() = default;

    virtual void drawText(int x, int y, std::string_view text, SDL_Color color, int scale = 1) const = 0;
    virtual int textWidth(std::string_view text, int scale = 1) const = 0;
    virtual int lineHeight(int scale = 1) const = 0;
};
