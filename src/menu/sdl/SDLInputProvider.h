#pragma once

#include <SDL.h>

#include "../MenuInterfaces.h"

namespace menu {

class SDLInputProvider : public IInputProvider {
public:
    void handleEvent(const SDL_Event& event);
    void endFrame();

    bool isUpPressed() const override { return m_up; }
    bool isDownPressed() const override { return m_down; }
    bool isValidatePressed() const override { return m_validate; }

private:
    bool m_up = false;
    bool m_down = false;
    bool m_validate = false;
};

} // namespace menu
