#include "TtfFont.h"

#include <string>

#include "../util/Logger.h"

TtfFont::TtfFont(SDL_Renderer* renderer, const std::string& fontPath, int pointSize)
    : m_renderer(renderer) {
    m_font = TTF_OpenFont(fontPath.c_str(), pointSize);
    if (!m_font) {
        Logger::warn(std::string("Failed to load TTF font '") + fontPath + "': " + TTF_GetError(), __func__);
    } else {
        TTF_SetFontHinting(m_font, TTF_HINTING_LIGHT);
    }
}

TtfFont::~TtfFont() {
    if (m_font) {
        TTF_CloseFont(m_font);
        m_font = nullptr;
    }
}

void TtfFont::drawText(int x, int y, std::string_view text, SDL_Color color, int scale) const {
    if (!m_font || !m_renderer) {
        return;
    }
    SDL_Surface* surface = TTF_RenderUTF8_Blended(m_font, std::string(text).c_str(), color);
    if (!surface) {
        Logger::warn(std::string("TTF_RenderUTF8_Blended failed: ") + TTF_GetError(), __func__);
        return;
    }
    SDL_Texture* texture = SDL_CreateTextureFromSurface(m_renderer, surface);
    if (!texture) {
        Logger::warn(std::string("SDL_CreateTextureFromSurface failed: ") + SDL_GetError(), __func__);
        SDL_FreeSurface(surface);
        return;
    }
    SDL_Rect dest{
        x,
        y,
        surface->w * scale,
        surface->h * scale
    };
    SDL_RenderCopy(m_renderer, texture, nullptr, &dest);
    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
}

int TtfFont::textWidth(std::string_view text, int scale) const {
    if (!m_font) {
        return 0;
    }
    int w = 0;
    int h = 0;
    if (TTF_SizeUTF8(m_font, std::string(text).c_str(), &w, &h) != 0) {
        return 0;
    }
    return w * scale;
}

int TtfFont::lineHeight(int scale) const {
    if (!m_font) {
        return 0;
    }
    return TTF_FontHeight(m_font) * scale;
}
