#pragma once

#include <cstdint>
#include <string>

enum class ScreenState {
    Menu,
    Options,
    LevelSelect,
    Playing,
    Paused,
    GameOver,
    Victory,
};

struct GameOverState {
    std::uint32_t startedAt = 0;
    int level = 0;
    int score = 0;
};

void reportWebState(ScreenState state);
void reportWebUiState(
    const std::string& language,
    bool usesBitmapFont,
    int musicVolume,
    int effectsVolume);
