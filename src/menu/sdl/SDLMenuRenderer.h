#pragma once

#include <SDL.h>

#include <string>
#include <unordered_map>

#include "../../render/Font.h"
#include "../MenuInterfaces.h"
#include "../MenuTypes.h"

namespace menu {

class SDLMenuRenderer : public IMenuRenderer {
public:
    SDLMenuRenderer(SDL_Renderer* renderer, Font& font, int fontScale = 3);
    ~SDLMenuRenderer() override = default;

    void registerTexture(const std::string& id, SDL_Texture* texture);
    void setColors(const MenuColors& colors) { m_colors = colors; }
    void setFontScale(int scale) { m_fontScale = scale; }
    void drawText(const std::string& text, int x, int y, bool selected, bool enabled) override;
    void drawImage(const std::string& textureId, int x, int y, int width, int height) override;

    void setTextAlign(TextAlign align) { m_align = align; }

private:
    SDL_Texture* textureForId(const std::string& id) const;
    SDL_Color toSDL(const Color& color) const;
    int textWidth(const std::string& text) const;

    SDL_Renderer* m_renderer;
    Font& m_font;
    int m_fontScale = 3;
    TextAlign m_align = TextAlign::Center;
    MenuColors m_colors{};
    std::unordered_map<std::string, SDL_Texture*> m_textures;
};

} // namespace menu
