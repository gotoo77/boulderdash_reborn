#pragma once

#include <SDL.h>

#include <array>
#include <cstdint>
#include <string_view>

#include "Font.h"

class BitmapFont : public Font {
public:
    struct Glyph {
        int width;
        std::array<std::uint8_t, 7> rows;
    };

    explicit BitmapFont(SDL_Renderer* renderer);

    void drawText(int x, int y, std::string_view text, SDL_Color color, int scale = 1) const override;
    int textWidth(std::string_view text, int scale = 1) const override;
    int lineHeight(int scale = 1) const override;

private:
    void drawGlyph(int x, int y, const Glyph& glyph, SDL_Color color, int scale) const;
    static const Glyph* glyphFor(char c);

    SDL_Renderer* m_renderer;
};
