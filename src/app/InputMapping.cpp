#include "InputMapping.h"

#include "util/Logger.h"

std::optional<Direction> directionFromKey(SDL_Keycode key) {
    switch (key) {
    case SDLK_UP:
        return Direction::Up;
    case SDLK_DOWN:
        return Direction::Down;
    case SDLK_LEFT:
        return Direction::Left;
    case SDLK_RIGHT:
        return Direction::Right;
    default:
        return std::nullopt;
    }
}

SDL_Keycode parseKeyFromName(const std::string& name, SDL_Keycode fallback, const char* context) {
    if (name.empty()) {
        return fallback;
    }
    const SDL_Keycode parsed = SDL_GetKeyFromName(name.c_str());
    if (parsed == SDLK_UNKNOWN) {
        Logger::warn("Invalid key name '" + name + "', falling back to default.", context);
        return fallback;
    }
    return parsed;
}

bool isVolumeIncreaseKey(SDL_Keycode key) {
    return key == SDLK_PLUS || key == SDLK_KP_PLUS || key == SDLK_EQUALS;
}

bool isVolumeDecreaseKey(SDL_Keycode key) {
    return key == SDLK_MINUS || key == SDLK_KP_MINUS;
}
