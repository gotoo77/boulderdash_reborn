#pragma once

#include <SDL.h>

#include <optional>
#include <string>

#include "core/Types.h"

inline constexpr SDL_Keycode DefaultFullscreenKey = SDLK_f;

void initializeGamepadInput();
void shutdownGamepadInput();
bool translateGamepadEvent(SDL_Event& event);

std::optional<Direction> directionFromKey(SDL_Keycode key);
SDL_Keycode parseKeyFromName(const std::string& name, SDL_Keycode fallback, const char* context);
bool isVolumeIncreaseKey(SDL_Keycode key);
bool isVolumeDecreaseKey(SDL_Keycode key);
