#include "ScreenState.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

EM_JS(void, reportStateToBrowser, (const char* state), {
    Module['boulderdashState'] = UTF8ToString(state);
});
EM_JS(void, reportUiToBrowser, (const char* language, const char* fontBackend, int musicVolume, int effectsVolume), {
    Module['boulderdashLanguage'] = UTF8ToString(language);
    Module['boulderdashFontBackend'] = UTF8ToString(fontBackend);
    Module['boulderdashMusicVolume'] = musicVolume;
    Module['boulderdashEffectsVolume'] = effectsVolume;
});
#else
void reportStateToBrowser(const char*) {
}
void reportUiToBrowser(const char*, const char*, int, int) {
}
#endif

void reportWebState(ScreenState state) {
    switch (state) {
    case ScreenState::Menu:
        reportStateToBrowser("menu");
        break;
    case ScreenState::Options:
        reportStateToBrowser("options");
        break;
    case ScreenState::LevelSelect:
        reportStateToBrowser("level-select");
        break;
    case ScreenState::Playing:
        reportStateToBrowser("playing");
        break;
    case ScreenState::Paused:
        reportStateToBrowser("paused");
        break;
    case ScreenState::GameOver:
        reportStateToBrowser("game-over");
        break;
    case ScreenState::Victory:
        reportStateToBrowser("victory");
        break;
    }
}

void reportWebUiState(
    const std::string& language,
    bool usesBitmapFont,
    int musicVolume,
    int effectsVolume) {
    reportUiToBrowser(
        language.c_str(),
        usesBitmapFont ? "bitmap" : "ttf",
        musicVolume,
        effectsVolume);
}
