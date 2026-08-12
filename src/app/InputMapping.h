#pragma once

#include <SDL.h>

#include <optional>
#include <string>

#include "core/Types.h"

inline constexpr SDL_Keycode DefaultFullscreenKey = SDLK_f;

void initializeGamepadInput();
void shutdownGamepadInput();
bool translateGamepadEvent(SDL_Event& event);
int pollInputEvent(SDL_Event* event);

// Application.cpp already includes SDL before this header. Redirect only its
// subsequent SDL_PollEvent calls through our input bridge, so controller events
// are normalized after SDL has safely dequeued them.
#ifndef BOULDERDASH_INPUT_MAPPING_IMPLEMENTATION
#define SDL_PollEvent pollInputEvent
#endif

std::optional<Direction> directionFromKey(SDL_Keycode key);
SDL_Keycode parseKeyFromName(const std::string& name, SDL_Keycode fallback, const char* context);
bool isVolumeIncreaseKey(SDL_Keycode key);
bool isVolumeDecreaseKey(SDL_Keycode key);
