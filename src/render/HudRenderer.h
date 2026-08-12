#pragma once

#include <SDL.h>

#include <string>

#include "../core/Game.h"
#include "BitmapFont.h"

class HudRenderer {
public:
    HudRenderer(SDL_Renderer* renderer, int height, int scale = 2);

    void draw(const Game& game, int windowWidth) const;

private:
    SDL_Renderer* m_renderer;
    int m_height;
    int m_scale;
    BitmapFont m_font;
};
