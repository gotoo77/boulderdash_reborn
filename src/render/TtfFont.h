#pragma once

#include <SDL.h>
#include <SDL_ttf.h>

#include <string>
#include <string_view>

#include "Font.h"

class TtfFont : public Font {
public:
    TtfFont(SDL_Renderer* renderer, const std::string& fontPath, int pointSize);
    ~TtfFont() override;

    bool valid() const { return m_font != nullptr; }

    void drawText(int x, int y, std::string_view text, SDL_Color color, int scale = 1) const override;
    int textWidth(std::string_view text, int scale = 1) const override;
    int lineHeight(int scale = 1) const override;

private:
    SDL_Renderer* m_renderer = nullptr;
    TTF_Font* m_font = nullptr;
};
