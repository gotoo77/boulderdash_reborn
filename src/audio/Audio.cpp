#include "Audio.h"

#include <SDL_mixer.h>

#include <array>
#include <filesystem>
#include <string>
#include <utility>

#include "../util/Logger.h"

namespace {

using SoundEntry = std::pair<SoundId, const char*>;

std::array<Mix_Chunk*, static_cast<std::size_t>(SoundId::Count)> g_chunks{};
bool g_initialized = false;
Mix_Music* g_menuMusic = nullptr;
bool g_menuMusicActive = false;

std::filesystem::path assetsPath() {
#ifdef ASSETS_DIR
    return std::filesystem::path(ASSETS_DIR);
#else
    return std::filesystem::path("assets");
#endif
}

std::filesystem::path soundPath(const char* filename) {
    return assetsPath() / "sfx" / filename;
}

std::filesystem::path themeDir() {
    return assetsPath() / "theme";
}

void loadSound(SoundId id, const char* filename) {
    LOG_T("Loading sound id=%u name=%s", static_cast<unsigned>(id), filename);
    const auto path = soundPath(filename);
    Mix_Chunk* chunk = Mix_LoadWAV(path.string().c_str());
    if (!chunk) {
        Logger::warn("Failed to load sound \"" + std::string(filename) + "\": " + Mix_GetError(), __func__);
        return;
    }
    g_chunks[static_cast<std::size_t>(id)] = chunk;
}

void loadMenuMusic() {
    const auto dir = themeDir();
    LOG_T("Discovering menu music in %s", dir.string().c_str());
    constexpr const char* kCandidates[] = {
        "bd_theme_menu.ogg",
        "bd_theme_menu.wav",
    };
    for (const char* filename : kCandidates) {
        const auto path = dir / filename;
        if (!std::filesystem::exists(path)) {
            continue;
        }
        Mix_Music* music = Mix_LoadMUS(path.string().c_str());
        if (!music) {
            Logger::warn("Failed to load menu music \"" + std::string(filename) + "\": " + Mix_GetError(), __func__);
            continue;
        }
        g_menuMusic = music;
        Logger::info("Menu music loaded from " + path.string(), __func__);
        return;
    }
    Logger::debug("Menu music assets missing; skipping theme playback", __func__);
}

} // namespace

bool Audio::init() {
    LOG_T("Audio::init");
    if (g_initialized) {
        return true;
    }

    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        Logger::warn(std::string("Mix_OpenAudio failed: ") + Mix_GetError(), __func__);
        return false;
    }
    Mix_AllocateChannels(8);

    constexpr SoundEntry kSounds[] = {
        { SoundId::Walk, "walk.wav" },
        { SoundId::Dig, "dig.wav" },
        { SoundId::RockFall, "rock_fall.wav" },
        { SoundId::Diamond, "diamond.wav" },
        { SoundId::Death, "death.wav" },
    };

    for (const auto& [id, filename] : kSounds) {
        loadSound(id, filename);
    }

    loadMenuMusic();

    g_initialized = true;
    return true;
}

void Audio::shutdown() {
    LOG_T("Audio::shutdown");
    if (!g_initialized) {
        return;
    }

    for (auto& chunk : g_chunks) {
        if (!chunk) {
            continue;
        }
        Mix_FreeChunk(chunk);
        chunk = nullptr;
    }
    if (g_menuMusic) {
        Mix_FreeMusic(g_menuMusic);
        g_menuMusic = nullptr;
    }

    Mix_CloseAudio();
    g_menuMusicActive = false;
    g_initialized = false;
}

void Audio::play(SoundId id) {
    LOG_T("Audio::play id=%u", static_cast<unsigned>(id));
    if (!g_initialized) {
        return;
    }

    Mix_Chunk* chunk = g_chunks[static_cast<std::size_t>(id)];
    if (!chunk) {
        return;
    }
    Mix_PlayChannel(-1, chunk, 0);
}

void Audio::playMenuMusic() {
    LOG_T("Audio::playMenuMusic");
    if (!g_initialized || !g_menuMusic) {
        return;
    }
    if (g_menuMusicActive && Mix_PlayingMusic() != 0) {
        return;
    }
    if (Mix_PlayMusic(g_menuMusic, -1) == -1) {
        Logger::warn(std::string("Failed to play menu music: ") + Mix_GetError(), __func__);
        g_menuMusicActive = false;
        return;
    }
    g_menuMusicActive = true;
}

void Audio::stopMenuMusic() {
    LOG_T("Audio::stopMenuMusic");
    if (!g_initialized) {
        return;
    }
    if (g_menuMusicActive && Mix_PlayingMusic() != 0) {
        Mix_HaltMusic();
    }
    g_menuMusicActive = false;
}
