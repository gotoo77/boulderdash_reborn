#include "SDLMenuRenderer.h"

#include "../../util/Logger.h"

namespace menu {

SDLMenuRenderer::SDLMenuRenderer(SDL_Renderer* renderer, Font& font, int fontScale)
    : m_renderer(renderer),
      m_font(font),
      m_fontScale(fontScale) {
}

SDL_Color SDLMenuRenderer::toSDL(const Color& color) const {
    return SDL_Color{ color.r, color.g, color.b, color.a };
}

int SDLMenuRenderer::textWidth(const std::string& text) const {
    const int width = m_font.textWidth(text, m_fontScale);
    return width > 0 ? width : 0;
}

void SDLMenuRenderer::registerTexture(const std::string& id, SDL_Texture* texture) {
    if (id.empty() || !texture) {
        return;
    }
    m_textures[id] = texture;
}

SDL_Texture* SDLMenuRenderer::textureForId(const std::string& id) const {
    const auto it = m_textures.find(id);
    if (it == m_textures.end()) {
        return nullptr;
    }
    return it->second;
}

void SDLMenuRenderer::drawText(const std::string& text, int x, int y, bool selected, bool enabled) {
    Color color = m_colors.normal;
    if (selected) {
        color = m_colors.selected;
    }
    if (!enabled) {
        color = m_colors.disabled;
    }
    int drawX = x;
    if (m_align == TextAlign::Center) {
        drawX = x - textWidth(text) / 2;
    } else if (m_align == TextAlign::Right) {
        drawX = x - textWidth(text);
    }
    m_font.drawText(drawX, y, text, toSDL(color), m_fontScale);
}

void SDLMenuRenderer::drawImage(const std::string& textureId, int x, int y, int width, int height) {
    SDL_Texture* texture = textureForId(textureId);
    if (!texture) {
        Logger::warn("SDLMenuRenderer missing texture for id '" + textureId + "'", __func__);
        return;
    }
    int tw = 0;
    int th = 0;
    SDL_QueryTexture(texture, nullptr, nullptr, &tw, &th);
    SDL_FRect rect;
    rect.x = static_cast<float>(x) - (width > 0 ? static_cast<float>(width) : static_cast<float>(tw)) * 0.5f;
    rect.y = static_cast<float>(y) - (height > 0 ? static_cast<float>(height) : static_cast<float>(th)) * 0.5f;
    rect.w = (width > 0 ? static_cast<float>(width) : static_cast<float>(tw));
    rect.h = (height > 0 ? static_cast<float>(height) : static_cast<float>(th));
    SDL_RenderCopyF(m_renderer, texture, nullptr, &rect);
}

} // namespace menu
