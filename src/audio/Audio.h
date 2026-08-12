#pragma once

#include <cstdint>

enum class SoundId : std::uint8_t {
    Walk,
    Dig,
    RockFall,
    Diamond,
    Death,
    ExitUnlock,
    DiamondFall,
    Explosion,
    TimeWarning,
    GameOver,
    Count
};

struct Audio {
    static bool init();
    static void resume();
    static void shutdown();
    static void play(SoundId id);
    static void playMenuMusic();
    static void stopMenuMusic();
    static int musicVolume();
    static int effectsVolume();
    static void setMusicVolume(int percent);
    static void setEffectsVolume(int percent);
};
