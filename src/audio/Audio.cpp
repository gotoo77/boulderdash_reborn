#include "Audio.h"

#include <SDL_mixer.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include <array>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>
#include <utility>
#include <vector>

#include "../util/Logger.h"

namespace {

#ifdef __EMSCRIPTEN__
EM_JS(void, resumeBrowserAudioContext, (), {
    const sdl = Module['SDL2'];
    if (!sdl || !sdl.audioContext) {
        Module['boulderdashAudioState'] = 'unavailable';
        return;
    }
    const context = sdl.audioContext;
    Module['boulderdashAudioState'] = context.state;
    if (context.state === 'suspended') {
        context.resume().then(() => {
            Module['boulderdashAudioState'] = context.state;
        }).catch((error) => {
            Module['boulderdashAudioState'] = 'error';
            console.warn('Unable to resume Web Audio context:', error);
        });
    }
});
#endif

using SoundEntry = std::pair<SoundId, const char*>;

std::array<Mix_Chunk*, static_cast<std::size_t>(SoundId::Count)> g_chunks{};
std::array<int, static_cast<std::size_t>(SoundId::Count)> g_chunkBaseVolumes{};
bool g_initialized = false;
Mix_Music* g_menuMusic = nullptr;
bool g_menuMusicActive = false;
int g_musicBaseVolume = MIX_MAX_VOLUME;
int g_musicVolumePercent = 100;
int g_effectsVolumePercent = 100;

int scaledVolume(int baseVolume, int percent) {
    return std::clamp((baseVolume * std::clamp(percent, 0, 100) + 50) / 100, 0, MIX_MAX_VOLUME);
}

void applyEffectsVolume() {
    for (std::size_t index = 0; index < g_chunks.size(); ++index) {
        if (g_chunks[index]) {
            Mix_VolumeChunk(g_chunks[index], scaledVolume(g_chunkBaseVolumes[index], g_effectsVolumePercent));
        }
    }
}

std::filesystem::path assetsPath() {
#ifdef ASSETS_DIR
    return std::filesystem::path(ASSETS_DIR);
#else
    return std::filesystem::path("assets");
#endif
}

std::filesystem::path audioConfigPath() {
#ifdef CONFIG_DIR
    return std::filesystem::path(CONFIG_DIR) / "audio.json";
#else
    return std::filesystem::path("cfg/audio.json");
#endif
}

std::filesystem::path resolveAssetPath(const std::string& configuredPath) {
    const std::filesystem::path path(configuredPath);
    return path.is_absolute() ? path : assetsPath() / path;
}

void loadSound(SoundId id, const char* eventName, const nlohmann::json& sounds) {
    const auto entry = sounds.find(eventName);
    if (entry == sounds.end()) {
        Logger::warn("Audio event missing from cfg/audio.json: " + std::string(eventName), __func__);
        return;
    }

    std::string filename;
    int volume = MIX_MAX_VOLUME;
    if (entry->is_string()) {
        filename = entry->get<std::string>();
    } else if (entry->is_object()) {
        filename = entry->value("file", std::string{});
        volume = std::clamp(entry->value("volume", MIX_MAX_VOLUME), 0, MIX_MAX_VOLUME);
    }
    if (filename.empty()) {
        Logger::warn("Audio event has no file: " + std::string(eventName), __func__);
        return;
    }

    const auto path = resolveAssetPath(filename);
    LOG_T("Loading audio event=%s path=%s", eventName, path.string().c_str());
    Mix_Chunk* chunk = Mix_LoadWAV(path.string().c_str());
    if (!chunk) {
        Logger::warn("Failed to load sound \"" + path.string() + "\": " + Mix_GetError(), __func__);
        return;
    }
    const auto index = static_cast<std::size_t>(id);
    g_chunkBaseVolumes[index] = volume;
    Mix_VolumeChunk(chunk, scaledVolume(volume, g_effectsVolumePercent));
    g_chunks[index] = chunk;
}

void loadMenuMusic(const nlohmann::json& root) {
    const auto music = root.find("music");
    if (music == root.end() || !music->is_object()) {
        Logger::debug("No music section in cfg/audio.json", __func__);
        return;
    }
    const auto menu = music->find("menu");
    if (menu == music->end()) {
        return;
    }

    std::vector<std::string> candidates;
    int volume = MIX_MAX_VOLUME;
    if (menu->is_string()) {
        candidates.push_back(menu->get<std::string>());
    } else if (menu->is_object()) {
        volume = std::clamp(menu->value("volume", MIX_MAX_VOLUME), 0, MIX_MAX_VOLUME);
        if (const auto files = menu->find("files"); files != menu->end() && files->is_array()) {
            for (const auto& file : *files) {
                if (file.is_string()) {
                    candidates.push_back(file.get<std::string>());
                }
            }
        }
    }

    for (const auto& filename : candidates) {
        const auto path = resolveAssetPath(filename);
        if (!std::filesystem::exists(path)) {
            continue;
        }
        Mix_Music* music = Mix_LoadMUS(path.string().c_str());
        if (!music) {
            Logger::warn("Failed to load menu music \"" + std::string(filename) + "\": " + Mix_GetError(), __func__);
            continue;
        }
        g_menuMusic = music;
        g_musicBaseVolume = volume;
        Mix_VolumeMusic(scaledVolume(g_musicBaseVolume, g_musicVolumePercent));
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

    nlohmann::json config;
    try {
        std::ifstream input(audioConfigPath());
        if (!input) {
            throw std::runtime_error("unable to open " + audioConfigPath().string());
        }
        input >> config;
    } catch (const std::exception& error) {
        Logger::warn(std::string("Audio configuration failed: ") + error.what(), __func__);
        return false;
    }

    const auto device = config.value("device", nlohmann::json::object());
    const int frequency = std::max(8000, device.value("frequency", 44100));
    const int outputChannels = std::clamp(device.value("outputChannels", 2), 1, 2);
    const int chunkSize = std::max(256, device.value("chunkSize", 2048));
    const int mixingChannels = std::max(1, device.value("mixingChannels", 8));
    if (Mix_OpenAudio(frequency, MIX_DEFAULT_FORMAT, outputChannels, chunkSize) < 0) {
        Logger::warn(std::string("Mix_OpenAudio failed: ") + Mix_GetError(), __func__);
        return false;
    }
    Mix_AllocateChannels(mixingChannels);

    const auto volumes = config.value("volume", nlohmann::json::object());
    g_musicVolumePercent = std::clamp(volumes.value("music", 100), 0, 100);
    g_effectsVolumePercent = std::clamp(volumes.value("effects", 100), 0, 100);

    constexpr SoundEntry kSounds[] = {
        { SoundId::Walk, "walk" },
        { SoundId::Dig, "dig" },
        { SoundId::RockFall, "rock_fall" },
        { SoundId::Diamond, "diamond_collect" },
        { SoundId::Death, "death" },
        { SoundId::ExitUnlock, "exit_unlock" },
        { SoundId::DiamondFall, "diamond_fall" },
        { SoundId::Explosion, "explosion" },
        { SoundId::TimeWarning, "time_warning" },
        { SoundId::GameOver, "game_over" },
    };

    const auto sounds = config.value("sounds", nlohmann::json::object());
    for (const auto& [id, eventName] : kSounds) {
        loadSound(id, eventName, sounds);
    }

    loadMenuMusic(config);

    g_initialized = true;
    return true;
}

void Audio::resume() {
#ifdef __EMSCRIPTEN__
    resumeBrowserAudioContext();
    if (g_initialized && g_menuMusicActive && g_menuMusic) {
        Mix_ResumeMusic();
    }
#endif
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
    g_chunkBaseVolumes.fill(0);
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

int Audio::musicVolume() {
    return g_musicVolumePercent;
}

int Audio::effectsVolume() {
    return g_effectsVolumePercent;
}

void Audio::setMusicVolume(int percent) {
    g_musicVolumePercent = std::clamp(percent, 0, 100);
    if (g_initialized) {
        Mix_VolumeMusic(scaledVolume(g_musicBaseVolume, g_musicVolumePercent));
    }
}

void Audio::setEffectsVolume(int percent) {
    g_effectsVolumePercent = std::clamp(percent, 0, 100);
    if (g_initialized) {
        applyEffectsVolume();
    }
}
