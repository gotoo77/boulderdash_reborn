#include "SDLInputProvider.h"

namespace menu {

void SDLInputProvider::handleEvent(const SDL_Event& event) {
    if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
        switch (event.key.keysym.sym) {
        case SDLK_UP:
        case SDLK_w:
            m_up = true;
            break;
        case SDLK_DOWN:
        case SDLK_s:
            m_down = true;
            break;
        case SDLK_RETURN:
        case SDLK_SPACE:
            m_validate = true;
            break;
        default:
            break;
        }
    }
}

void SDLInputProvider::endFrame() {
    m_up = false;
    m_down = false;
    m_validate = false;
}

} // namespace menu
