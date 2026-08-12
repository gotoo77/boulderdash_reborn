#include <SDL.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include <algorithm>
#include <clocale>
#include <cstdio>
#include <exception>
#ifndef __EMSCRIPTEN__
#include <execinfo.h>
#include <unistd.h>
#endif
#include <csignal>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "app/Application.h"
#include "app/AssetCatalog.h"
#include "app/AudioEventRouter.h"
#include "app/GameplayLoop.h"
#include "app/InputMapping.h"
#include "app/MenuFactory.h"
#include "app/OptionsStyle.h"
#include "app/ScreenState.h"
#include "audio/Audio.h"
#include "core/Game.h"
#include "menu/MenuManager.h"
#include "menu/MenuTypes.h"
#include "menu/sdl/SDLInputProvider.h"
#include "menu/sdl/SDLMenuRenderer.h"
#include "render/BitmapFont.h"
#include "render/HudRenderer.h"
#include "render/PngLoader.h"
#include "render/Renderer.h"
#ifdef HAS_SDL_TTF
#include <SDL_ttf.h>
#include "render/TtfFont.h"
#endif
#include "util/Config.h"
#include "util/Logger.h"
#include "util/Translator.h"

namespace {

constexpr int kHudHeight = 72;
constexpr int kOptionCount = 3;
constexpr int kVolumeStep = 10;

void installCrashHandler() {
#ifndef __EMSCRIPTEN__
    auto handler = [](int sig) {
        void* trace[64];
        const int count = backtrace(trace, 64);
        fprintf(stderr, "Caught signal %d, backtrace:\n", sig);
        backtrace_symbols_fd(trace, count, STDERR_FILENO);
        _exit(128 + sig);
    };
    std::signal(SIGSEGV, handler);
    std::signal(SIGABRT, handler);
#endif
}

void delayFrame(Uint32 milliseconds) {
#ifdef __EMSCRIPTEN__
    emscripten_sleep(milliseconds);
#else
    SDL_Delay(milliseconds);
#endif
}

std::string formatTitleTime(int ms) {
    if (ms < 0) {
        ms = 0;
    }
    const int totalSeconds = ms / 1000;
    const int minutes = totalSeconds / 60;
    const int seconds = totalSeconds % 60;
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d", minutes, seconds);
    return buffer;
}

SDL_Texture* loadTextureFromPng(SDL_Renderer* renderer, const std::filesystem::path& path) {
    std::vector<unsigned char> pixels;
    int width = 0;
    int height = 0;
    int pitch = 0;
    if (!gfx::loadPngRGBA(path, pixels, width, height, pitch)) {
        Logger::warn("Menu texture load failed: " + path.string(), __func__);
        return nullptr;
    }
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(
        pixels.data(),
        width,
        height,
        32,
        pitch,
        SDL_PIXELFORMAT_RGBA32);
    if (!surface) {
        Logger::warn(std::string("SDL_CreateRGBSurfaceWithFormatFrom failed: ") + SDL_GetError(), __func__);
        return nullptr;
    }
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    if (!texture) {
        Logger::warn(std::string("SDL_CreateTextureFromSurface failed: ") + SDL_GetError(), __func__);
        return nullptr;
    }
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    return texture;
}

} // namespace

int runApplication() {
    // Ensure console and SDL use a UTF-8 locale to avoid mojibake in logs/titles.
    const char* localesToTry[] = { "C.UTF-8", "en_US.UTF-8", "fr_FR.UTF-8", nullptr };
    for (const char** loc = localesToTry; *loc; ++loc) {
        if (std::setlocale(LC_ALL, *loc)) {
            break;
        }
    }

    installCrashHandler();

    const Config config = loadConfig(resolveConfigPath());
    Logger::setLevel(config.logLevel);
    if (config.tileTestMode) {
        Logger::info("Tile test mode enabled: gameplay disabled, displaying sprite sheet.", __func__);
    }
    const bool tileTestMode = config.tileTestMode;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
        return 1;
    }

#ifdef HAS_SDL_TTF
    bool ttfInitialized = true;
    if (TTF_Init() != 0) {
        Logger::warn(std::string("TTF_Init failed: ") + TTF_GetError(), __func__);
        ttfInitialized = false;
    }
#endif

    const bool audioInitialized = Audio::init();
    if (!audioInitialized) {
        Logger::warn("Audio disabled; SDL_mixer initialization failed.", __func__);
    }

    GameRules rules{
        config.timeLimitMs,
        config.diamondValue,
        config.exitBonus,
        config.timeBonusPerSecond,
        config.enemyMoveIntervalMs,
        config.gravityStepMs,
        config.startingLives,
        config.respawnDelayMs,
    };

    const auto levelPaths = discoverLevels();
    Game game(levelPaths, rules);
    int windowWidth = game.grid().width() * config.tileSize;
    int windowHeight = game.grid().height() * config.tileSize + kHudHeight;

    SDL_Window* window = SDL_CreateWindow(
        "Boulder Dash",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        windowWidth,
        windowHeight,
        SDL_WINDOW_SHOWN);

    if (!window) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << "\n";
        Audio::shutdown();
        SDL_Quit();
        return 1;
    }

#ifdef __EMSCRIPTEN__
    constexpr Uint32 preferredRenderer = SDL_RENDERER_SOFTWARE;
    constexpr Uint32 fallbackRenderer = SDL_RENDERER_ACCELERATED;
#else
    constexpr Uint32 preferredRenderer = SDL_RENDERER_ACCELERATED;
    constexpr Uint32 fallbackRenderer = SDL_RENDERER_SOFTWARE;
#endif
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, preferredRenderer);
    if (!renderer) {
        Logger::warn(
            std::string("Preferred SDL renderer unavailable, trying fallback renderer: ") + SDL_GetError(),
            __func__);
        renderer = SDL_CreateRenderer(window, -1, fallbackRenderer);
    }
    if (!renderer) {
        std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << "\n";
        Audio::shutdown();
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    const auto assetsPath = assetsBasePath();
    auto availableLanguages = discoverLanguages(assetsPath / "i18n");
    Renderer gridRenderer(renderer, config.tileSize, kHudHeight, assetsPath);
    HudRenderer hud(renderer, kHudHeight);
    const SDL_Keycode fullscreenToggleKey =
        parseKeyFromName(config.fullscreenToggleKey, DefaultFullscreenKey, __func__);
    const std::string fullscreenKeyName = SDL_GetKeyName(fullscreenToggleKey);
    const std::filesystem::path uiFontPath = "assets/fonts/ui_font.ttf";

#ifdef HAS_SDL_TTF
    std::unique_ptr<Font> uiFont;
    std::vector<std::filesystem::path> bundledPaths = {
        assetsPath / "ui" / "ui_font.ttf",
        assetsPath / "fonts" / "ui_font.ttf"
    };
    for (const auto& bundledFontPath : bundledPaths) {
        if (!ttfInitialized || !std::filesystem::exists(bundledFontPath)) {
            continue;
        }
        auto ttfFont = std::make_unique<TtfFont>(renderer, bundledFontPath.string(), 32);
        if (ttfFont->valid()) {
            uiFont = std::move(ttfFont);
            Logger::info("Loaded UI font: " + bundledFontPath.string(), __func__);
            break;
        }
    }
    // Try common system fonts if no bundled font is present.
    if (!uiFont && ttfInitialized) {
        const std::vector<std::filesystem::path> fallbacks = {
#if defined(_WIN32)
            "C:/Windows/Fonts/msgothic.ttc",
            "C:/Windows/Fonts/meiryo.ttc",
            "C:/Windows/Fonts/segoeui.ttf",
#elif defined(__APPLE__)
            "/System/Library/Fonts/ヒラギノ角ゴシック W3.ttc",
            "/System/Library/Fonts/Helvetica.ttc",
            "/System/Library/Fonts/Supplemental/Arial Unicode.ttf",
#else
            "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc",
            "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
            "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
#endif
        };
        for (const auto& path : fallbacks) {
            if (!std::filesystem::exists(path)) {
                continue;
            }
            auto ttfFont = std::make_unique<TtfFont>(renderer, path.string(), 32);
            if (ttfFont->valid()) {
                uiFont = std::move(ttfFont);
                Logger::info("Loaded system UI font: " + path.string(), __func__);
                break;
            }
        }
    }
    if (!uiFont) {
        uiFont = std::make_unique<BitmapFont>(renderer);
        if (!ttfInitialized) {
            Logger::warn("Falling back to bitmap font because SDL_ttf is unavailable.", __func__);
        } else if (!std::filesystem::exists(uiFontPath)) {
            Logger::warn("Falling back to bitmap font; missing TTF file at " + uiFontPath.string(), __func__);
        } else {
            Logger::warn("Falling back to bitmap font; TTF font failed to load.", __func__);
        }
    }
#else
    std::unique_ptr<Font> uiFont = std::make_unique<BitmapFont>(renderer);
#endif
    Translator translator(assetsPath / "i18n", config.language);
    Font& uiFontRef = *uiFont;
    const bool uiFontIsBitmap = dynamic_cast<BitmapFont*>(uiFont.get()) != nullptr;
    auto reportUiState = [&]() {
        reportWebUiState(
            translator.activeLanguage(),
            uiFontIsBitmap,
            Audio::musicVolume(),
            Audio::effectsVolume());
    };
    reportUiState();
    if (availableLanguages.empty()) {
        availableLanguages.push_back(LanguageEntry{ translator.activeLanguage(), translator.tr("language.name") });
    } else if (std::find_if(
                   availableLanguages.begin(),
                   availableLanguages.end(),
                   [&](const LanguageEntry& entry) { return entry.code == translator.activeLanguage(); }) ==
        availableLanguages.end()) {
        availableLanguages.push_back(LanguageEntry{ translator.activeLanguage(), translator.tr("language.name") });
    }
    int languageSelection = 0;
    int optionSelection = 0;
    for (std::size_t i = 0; i < availableLanguages.size(); ++i) {
        if (availableLanguages[i].code == translator.activeLanguage()) {
            languageSelection = static_cast<int>(i);
            break;
        }
    }
    const auto menuFile = configBasePath() / "main_menu.json";
    const auto pauseMenuFile = configBasePath() / "pause_menu.json";
    const auto optionsMenuFile = configBasePath() / "options_menu.json";
    OptionsScreenStyle optionsStyle = loadOptionsStyle(optionsMenuFile);
    std::unordered_map<std::string, SDL_Texture*> menuTextures;
    auto loadMenuTextureIfNeeded = [&](const std::string& textureId) -> SDL_Texture* {
        if (textureId.empty()) {
            return nullptr;
        }
        const auto existing = menuTextures.find(textureId);
        if (existing != menuTextures.end()) {
            return existing->second;
        }
        const std::filesystem::path texturePath = resolveMenuTexturePath(assetsPath, textureId);
        if (!std::filesystem::exists(texturePath)) {
            Logger::warn("Menu texture not found: " + texturePath.string(), __func__);
            return nullptr;
        }
        SDL_Texture* texture = loadTextureFromPng(renderer, texturePath);
        if (texture) {
            menuTextures[textureId] = texture;
        }
        return texture;
    };

    menu::Menu mainMenu;
    menu::Menu pauseMenu;
    menu::Menu levelSelectMenu;
    menu::MenuManager mainMenuManager;
    menu::MenuManager pauseMenuManager;
    menu::MenuManager levelSelectMenuManager;
    TranslatorTextProvider textProvider(translator);
    mainMenuManager.setTextProvider(&textProvider);
    pauseMenuManager.setTextProvider(&textProvider);
    levelSelectMenuManager.setTextProvider(&textProvider);
    const int menuFontScale = uiFontIsBitmap ? 3 : 1;
    menu::SDLMenuRenderer sdlMenuRenderer(renderer, uiFontRef, menuFontScale);
    menu::SDLInputProvider menuInput;
    menu::SDLInputProvider pauseInput;
    menu::SDLInputProvider levelSelectInput;
    menu::MenuRenderMetrics menuMetrics{ windowWidth, windowHeight, 1.0f, std::nullopt };
    auto applyMenuStyleFor = [&](const menu::Menu& menuDef) {
        sdlMenuRenderer.setColors(menuDef.colors());
        const int scale = menuDef.itemScale() > 0 ? menuDef.itemScale() : menuFontScale;
        sdlMenuRenderer.setFontScale(scale);
        sdlMenuRenderer.setTextAlign(menuDef.layout().align);
    };
    auto toSDLColor = [](const menu::Color& color) {
        return SDL_Color{ color.r, color.g, color.b, color.a };
    };
    auto refreshWindowSize = [&]() {
        SDL_RenderSetLogicalSize(renderer, windowWidth, windowHeight);
        menuMetrics.viewportWidth = windowWidth;
        menuMetrics.viewportHeight = windowHeight;
    };
    const Uint32 fullscreenMask = SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP;
    bool isFullscreen = (SDL_GetWindowFlags(window) & fullscreenMask) != 0;
    auto setFullscreen = [&](bool enable) {
        const Uint32 flags = SDL_WINDOW_FULLSCREEN_DESKTOP;
        if (enable) {
            if (SDL_SetWindowFullscreen(window, flags) != 0) {
                Logger::warn(std::string("Failed to enter fullscreen: ") + SDL_GetError(), __func__);
                return;
            }
        } else {
            if (SDL_SetWindowFullscreen(window, 0) != 0) {
                Logger::warn(std::string("Failed to exit fullscreen: ") + SDL_GetError(), __func__);
                return;
            }
        }
        isFullscreen = (SDL_GetWindowFlags(window) & fullscreenMask) != 0;
        refreshWindowSize();
    };
    auto toggleFullscreen = [&]() { setFullscreen(!isFullscreen); };
    refreshWindowSize();
    auto registerMenuAssets = [&](const menu::Menu& menuDef) {
        if (const auto& logo = menuDef.logo()) {
            if (SDL_Texture* texture = loadMenuTextureIfNeeded(logo->textureId)) {
                sdlMenuRenderer.registerTexture(logo->textureId, texture);
            }
        }
    };
    auto rebuildMenus = [&]() {
        mainMenu = loadMainMenuDefinition(menuFile, translator, config.devMode);
        mainMenuManager.setMenu(mainMenu);
        registerMenuAssets(mainMenu);
        pauseMenu = loadPauseMenuDefinition(pauseMenuFile);
        pauseMenuManager.setMenu(pauseMenu);
        if (config.devMode) {
            levelSelectMenu = buildLevelSelectMenu(translator, levelPaths);
            levelSelectMenuManager.setMenu(levelSelectMenu);
        }
    };
    rebuildMenus();

    bool running = true;
    GameplayLoop gameplayLoop(config.tickMs, config.moveRepeatMs);
    gameplayLoop.reset(SDL_GetTicks());
    auto clearInputState = [&]() {
        gameplayLoop.clearInput();
    };
    ScreenState screen = ScreenState::Menu;
    ScreenState optionsReturnScreen = ScreenState::Menu;
    GameOverState gameOverState{};
    const Uint32 gameOverDelay = static_cast<Uint32>(config.gameOverDelayMs);
    auto updateMenuMusic = [&]() {
        const bool shouldPlay = (screen == ScreenState::Menu) || (screen == ScreenState::Options) ||
            (screen == ScreenState::LevelSelect);
        if (shouldPlay) {
            Audio::playMenuMusic();
        } else {
            Audio::stopMenuMusic();
        }
    };
    updateMenuMusic();
    auto rebuildGame = [&]() {
        game = Game(levelPaths, rules);
        clearInputState();
        gameplayLoop.reset(SDL_GetTicks());
    };
    auto startNewGame = [&]() {
        rebuildGame();
        screen = ScreenState::Playing;
        updateMenuMusic();
    };
    auto startLevelFromSelection = [&](std::size_t index) -> bool {
        if (index >= levelPaths.size()) {
            Logger::warn("Invalid level selection: " + std::to_string(index), __func__);
            return false;
        }
        rebuildGame();
        if (!game.jumpToLevel(index)) {
            Logger::warn("Failed to jump to level index " + std::to_string(index), __func__);
            return false;
        }
        screen = ScreenState::Playing;
        updateMenuMusic();
        return true;
    };
    auto pauseGame = [&]() {
        game.setPaused(true);
        screen = ScreenState::Paused;
        pauseMenuManager.setMenu(pauseMenu);
        clearInputState();
        updateMenuMusic();
    };
    auto resumeGame = [&]() {
        game.setPaused(false);
        screen = ScreenState::Playing;
        clearInputState();
        gameplayLoop.reset(SDL_GetTicks());
        updateMenuMusic();
    };
    auto returnToMainMenu = [&]() {
        rebuildGame();
        screen = ScreenState::Menu;
        mainMenuManager.setMenu(mainMenu);
        updateMenuMusic();
    };
    auto leaveOptions = [&]() {
        screen = optionsReturnScreen;
        if (screen == ScreenState::Menu) {
            mainMenuManager.setMenu(mainMenu);
        } else if (screen == ScreenState::Paused) {
            pauseMenuManager.setMenu(pauseMenu);
        }
        updateMenuMusic();
    };
    auto fitTextScale = [&](const std::string& text, int desiredScale, float maxWidthRatio) {
        const int baseWidth = uiFontRef.textWidth(text, 1);
        const int maxWidth = static_cast<int>(static_cast<float>(windowWidth) * maxWidthRatio);
        if (baseWidth <= 0 || maxWidth <= 0) {
            return desiredScale;
        }
        const int maxScale = std::max(1, maxWidth / baseWidth);
        return std::max(1, std::min(desiredScale, maxScale));
    };
    auto drawCenteredText = [&](int y, const std::string& text, int desiredScale, SDL_Color color, float maxWidthRatio = 0.9f) {
        const int scale = fitTextScale(text, desiredScale, maxWidthRatio);
        const int width = uiFontRef.textWidth(text, scale);
        const int x = std::max(0, (windowWidth - width) / 2);
        uiFontRef.drawText(x, y, text, color, scale);
    };
    auto adjustBothVolumes = [&](int delta) {
        Audio::setMusicVolume(Audio::musicVolume() + delta);
        Audio::setEffectsVolume(Audio::effectsVolume() + delta);
    };
    auto adjustSelectedOption = [&](int delta) {
        if (optionSelection == 0 && !availableLanguages.empty()) {
            const int total = static_cast<int>(availableLanguages.size());
            const int direction = delta < 0 ? -1 : 1;
            languageSelection = (languageSelection + direction + total) % total;
            translator.setLanguage(availableLanguages[languageSelection].code);
            reportUiState();
            rebuildMenus();
        } else if (optionSelection == 1) {
            Audio::setMusicVolume(Audio::musicVolume() + delta);
            reportUiState();
        } else if (optionSelection == 2) {
            Audio::setEffectsVolume(Audio::effectsVolume() + delta);
            reportUiState();
        }
    };
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_KEYDOWN || event.type == SDL_MOUSEBUTTONDOWN ||
                event.type == SDL_FINGERDOWN) {
                Audio::resume();
            }
            if (event.type == SDL_QUIT) {
                running = false;
                continue;
            }
            if (event.type == SDL_WINDOWEVENT &&
                (event.window.event == SDL_WINDOWEVENT_RESIZED ||
                    event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)) {
                refreshWindowSize();
                continue;
            }
            if (event.type == SDL_KEYDOWN && event.key.repeat == 0 &&
                event.key.keysym.sym == fullscreenToggleKey) {
                toggleFullscreen();
                continue;
            }
            if (event.type == SDL_KEYDOWN && event.key.repeat == 0 &&
                screen != ScreenState::Options &&
                (isVolumeIncreaseKey(event.key.keysym.sym) || isVolumeDecreaseKey(event.key.keysym.sym))) {
                adjustBothVolumes(isVolumeIncreaseKey(event.key.keysym.sym) ? kVolumeStep : -kVolumeStep);
                continue;
            }
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
                switch (screen) {
                case ScreenState::Menu:
                    running = false;
                    break;
                case ScreenState::Options:
                    leaveOptions();
                    break;
                case ScreenState::LevelSelect:
                    screen = ScreenState::Menu;
                    mainMenuManager.setMenu(mainMenu);
                    updateMenuMusic();
                    break;
                case ScreenState::Playing:
                    pauseGame();
                    break;
                case ScreenState::Paused:
                    resumeGame();
                    break;
                case ScreenState::GameOver:
                    rebuildGame();
                    screen = ScreenState::Menu;
                    mainMenuManager.setMenu(mainMenu);
                    updateMenuMusic();
                    break;
                case ScreenState::Victory:
                    rebuildGame();
                    screen = ScreenState::Menu;
                    mainMenuManager.setMenu(mainMenu);
                    updateMenuMusic();
                    break;
                }
                continue;
            }
            if (tileTestMode) {
                continue;
            }
            if (screen == ScreenState::Menu) {
                if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
                    menuInput.handleEvent(event);
                }
            } else if (screen == ScreenState::Options) {
                if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
                    if (event.key.keysym.sym == SDLK_UP || event.key.keysym.sym == SDLK_DOWN) {
                        const int delta = event.key.keysym.sym == SDLK_UP ? -1 : 1;
                        optionSelection = (optionSelection + delta + kOptionCount) % kOptionCount;
                    } else if (event.key.keysym.sym == SDLK_LEFT ||
                        event.key.keysym.sym == SDLK_RIGHT ||
                        isVolumeIncreaseKey(event.key.keysym.sym) ||
                        isVolumeDecreaseKey(event.key.keysym.sym)) {
                        const bool increase = event.key.keysym.sym == SDLK_RIGHT ||
                            isVolumeIncreaseKey(event.key.keysym.sym);
                        adjustSelectedOption(increase ? kVolumeStep : -kVolumeStep);
                    } else if (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_SPACE) {
                        leaveOptions();
                    }
                }
            } else if (screen == ScreenState::LevelSelect) {
                if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
                    levelSelectInput.handleEvent(event);
                }
            } else if (screen == ScreenState::GameOver) {
                if (event.type == SDL_KEYDOWN && event.key.repeat == 0 &&
                    (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_SPACE)) {
                    rebuildGame();
                    screen = ScreenState::Menu;
                    mainMenuManager.setMenu(mainMenu);
                    updateMenuMusic();
                }
            } else if (screen == ScreenState::Victory) {
                if (event.type == SDL_KEYDOWN && event.key.repeat == 0 &&
                    (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_SPACE)) {
                    rebuildGame();
                    screen = ScreenState::Menu;
                    mainMenuManager.setMenu(mainMenu);
                    updateMenuMusic();
                }
            } else if (screen == ScreenState::Paused) {
                if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
                    if (event.key.keysym.sym == SDLK_p) {
                        resumeGame();
                    } else {
                        pauseInput.handleEvent(event);
                    }
                }
            } else if (screen == ScreenState::Playing) {
                if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
                    if (event.key.keysym.sym == SDLK_p) {
                        pauseGame();
                    } else if (auto dir = directionFromKey(event.key.keysym.sym)) {
                        gameplayLoop.press(*dir, SDL_GetTicks(), game);
                    }
                } else if (event.type == SDL_KEYUP) {
                    if (auto dir = directionFromKey(event.key.keysym.sym)) {
                        gameplayLoop.release(*dir);
                    }
                }
            }
        }

        if (tileTestMode) {
            gridRenderer.drawTestPattern(windowWidth, windowHeight);
            SDL_RenderPresent(renderer);
            delayFrame(16);
            continue;
        }

        const Uint32 now = SDL_GetTicks();
        reportWebState(screen);

        if (screen == ScreenState::Playing) {
            if (gameplayLoop.update(now, game)) {
                playGameEvents(game.consumeEvents());
            }

            if (game.levelComplete()) {
                clearInputState();
                if (!game.advanceToNextLevel()) {
                    screen = ScreenState::Victory;
                    updateMenuMusic();
                }
                continue;
            }

            if (game.levelFailed()) {
                clearInputState();
                gameOverState.startedAt = now;
                gameOverState.level = game.currentLevelNumber();
                gameOverState.score = game.totalScore();
                screen = ScreenState::GameOver;
                Audio::play(SoundId::GameOver);
                updateMenuMusic();
                continue;
            }

            std::string title = "Boulder Dash - Niveau " + std::to_string(game.currentLevelNumber()) + "/" +
                std::to_string(game.levelCount()) + " Score " + std::to_string(game.totalScore()) +
                " Temps " + formatTitleTime(game.timeRemainingMs());
            SDL_SetWindowTitle(window, title.c_str());

            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);
            gridRenderer.draw(game.grid());
            hud.draw(game, windowWidth);
            SDL_RenderPresent(renderer);
        } else if (screen == ScreenState::Paused) {
            pauseMenuManager.update(pauseInput);
            if (auto action = pauseMenuManager.consumeAction()) {
                if (*action == "resume_game") {
                    resumeGame();
                } else if (*action == "open_options") {
                    optionsReturnScreen = ScreenState::Paused;
                    screen = ScreenState::Options;
                    updateMenuMusic();
                } else if (*action == "return_main_menu") {
                    returnToMainMenu();
                } else {
                    Logger::warn("Unhandled pause menu action: " + *action, __func__);
                }
            }
            if (screen != ScreenState::Paused) {
                pauseInput.endFrame();
                continue;
            }
            SDL_SetWindowTitle(window, translator.tr("pause.title").c_str());
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);
            gridRenderer.draw(game.grid());
            hud.draw(game, windowWidth);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_Rect overlay{ 0, 0, windowWidth, windowHeight };
            SDL_SetRenderDrawColor(renderer, 0, 0, 20, 190);
            SDL_RenderFillRect(renderer, &overlay);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
            const SDL_Color titleColor{ 100, 200, 255, 255 };
            const SDL_Color secondary{ 220, 235, 255, 255 };
            drawCenteredText(windowHeight / 6, translator.tr("pause.title"), 2, titleColor);
            applyMenuStyleFor(pauseMenu);
            pauseMenuManager.render(sdlMenuRenderer, menuMetrics);
            drawCenteredText(
                windowHeight - 2 * uiFontRef.lineHeight(1),
                translator.tr("pause.hint"),
                1,
                secondary);
            SDL_RenderPresent(renderer);
            pauseInput.endFrame();
        } else if (screen == ScreenState::Menu) {
            mainMenuManager.update(menuInput);
            if (auto action = mainMenuManager.consumeAction()) {
                if (*action == "start_game") {
                    startNewGame();
                } else if (*action == "open_options") {
                    optionsReturnScreen = ScreenState::Menu;
                    screen = ScreenState::Options;
                    updateMenuMusic();
                } else if (*action == "open_dev_level_select") {
                    if (config.devMode) {
                        screen = ScreenState::LevelSelect;
                        levelSelectMenuManager.setMenu(levelSelectMenu);
                        updateMenuMusic();
                    } else {
                        Logger::warn("Dev menu requested while disabled", __func__);
                    }
                } else if (*action == "quit_game") {
                    running = false;
                } else {
                    Logger::warn("Unhandled menu action: " + *action, __func__);
                }
            }
            if (!running) {
                menuInput.endFrame();
                continue;
            }
            if (screen != ScreenState::Menu) {
                menuInput.endFrame();
                continue;
            }

            applyMenuStyleFor(mainMenu);
            const std::string title = translator.tr("menu.title");
            SDL_SetWindowTitle(window, title.c_str());
            SDL_SetRenderDrawColor(renderer, 5, 5, 20, 255);
            SDL_RenderClear(renderer);
            const auto headerDef = mainMenu.header();
            const SDL_Color titleColor = headerDef ? toSDLColor(headerDef->title.color) : SDL_Color{ 255, 255, 255, 255 };
            const SDL_Color subtitleColor =
                headerDef ? toSDLColor(headerDef->subtitle.color) : SDL_Color{ 180, 180, 180, 255 };
            const int titleY = headerDef ?
                static_cast<int>(headerDef->title.normalizedY * static_cast<float>(windowHeight)) :
                windowHeight / 5;
            const int titleScale = headerDef ? headerDef->title.scale : 5;
            drawCenteredText(titleY, translator.tr("menu.title"), titleScale, titleColor, headerDef ? headerDef->title.maxWidthRatio : 0.9f);
            int subtitleY = headerDef ?
                static_cast<int>(headerDef->subtitle.normalizedY * static_cast<float>(windowHeight)) :
                titleY + uiFontRef.lineHeight(3) + 24;
            if (headerDef && headerDef->subtitle.normalizedY <= 0.0f) {
                subtitleY = titleY + uiFontRef.lineHeight(headerDef->title.scale) +
                    static_cast<int>(headerDef->spacing);
            }
            const int subtitleScale = headerDef ? headerDef->subtitle.scale : 3;
            drawCenteredText(subtitleY, translator.tr("menu.subtitle"), subtitleScale, subtitleColor, headerDef ? headerDef->subtitle.maxWidthRatio : 0.9f);
            mainMenuManager.render(sdlMenuRenderer, menuMetrics);
            const int hintLineHeight = uiFontRef.lineHeight(2);
            const int hintBottomY = windowHeight - hintLineHeight - 16;
            const std::string fullscreenHint = translator.tr("menu.fullscreenHint") + " " + fullscreenKeyName;
            drawCenteredText(hintBottomY - hintLineHeight - 6, fullscreenHint, 1, subtitleColor);
            drawCenteredText(hintBottomY, translator.tr("menu.hint"), 1, subtitleColor);
            SDL_RenderPresent(renderer);
            menuInput.endFrame();
        } else if (screen == ScreenState::Options) {
            const std::string title = translator.tr("menu.title");
            SDL_SetWindowTitle(window, title.c_str());
            SDL_SetRenderDrawColor(renderer, 5, 10, 5, 255);
            SDL_RenderClear(renderer);
            const int titleY = static_cast<int>(optionsStyle.title.y * static_cast<float>(windowHeight));
            const SDL_Color titleColor = optionsStyle.title.color;
            const SDL_Color secondary = optionsStyle.lines.color;
            drawCenteredText(titleY, translator.tr("options.title"), optionsStyle.title.scale, titleColor, optionsStyle.title.maxWidth);
            std::ostringstream delayText;
            delayText << std::fixed << std::setprecision(1) << (config.gameOverDelayMs / 1000.0f);
            LanguageEntry currentLanguage{ translator.activeLanguage(), translator.tr("language.name") };
            if (!availableLanguages.empty()) {
                const int safeIndex = std::max(0, std::min(languageSelection, static_cast<int>(availableLanguages.size()) - 1));
                currentLanguage = availableLanguages[safeIndex];
            }
            std::vector<std::string> lines{
                translator.tr("options.languageLabel") + ": " + currentLanguage.label + " (" + currentLanguage.code + ")",
                translator.tr("options.musicVolumeLabel") + ": " + std::to_string(Audio::musicVolume()) + "%",
                translator.tr("options.effectsVolumeLabel") + ": " + std::to_string(Audio::effectsVolume()) + "%",
            };
            std::vector<std::string> hints{
                translator.tr("options.controlsHint"),
                translator.tr("options.delayLabel") + ": " + delayText.str() + translator.tr("duration.secondsSuffix"),
                translator.tr("menu.fullscreenHint") + " " + fullscreenKeyName,
                translator.tr("options.backHint"),
            };
            int y = static_cast<int>(optionsStyle.lines.y * static_cast<float>(windowHeight));
            const int lineSpacingPx = static_cast<int>(optionsStyle.lineSpacing * static_cast<float>(windowHeight));
            drawCenteredText(y, translator.tr("options.description"), optionsStyle.lines.scale, secondary, optionsStyle.lines.maxWidth);
            y += lineSpacingPx;
            for (std::size_t index = 0; index < lines.size(); ++index) {
                const bool selected = static_cast<int>(index) == optionSelection;
                const std::string row = std::string(selected ? "> " : "  ") + lines[index] + (selected ? " <" : "  ");
                drawCenteredText(y, row, optionsStyle.lines.scale,
                    selected ? titleColor : secondary, optionsStyle.lines.maxWidth);
                y += lineSpacingPx;
            }
            int hintY = static_cast<int>(optionsStyle.hints.y * static_cast<float>(windowHeight));
            const int hintSpacingPx = static_cast<int>(optionsStyle.hintSpacing * static_cast<float>(windowHeight));
            for (const auto& hint : hints) {
                drawCenteredText(hintY, hint, optionsStyle.hints.scale, optionsStyle.hints.color, optionsStyle.hints.maxWidth);
                hintY += hintSpacingPx;
            }
            SDL_RenderPresent(renderer);
        } else if (screen == ScreenState::LevelSelect) {
            levelSelectMenuManager.update(levelSelectInput);
            if (auto action = levelSelectMenuManager.consumeAction()) {
                const std::string prefix(DevLevelActionPrefix);
                if (action->rfind(prefix, 0) == 0) {
                    const std::string indexStr = action->substr(prefix.size());
                    try {
                        const int parsedIndex = std::stoi(indexStr);
                        if (parsedIndex >= 0 && startLevelFromSelection(static_cast<std::size_t>(parsedIndex))) {
                            levelSelectInput.endFrame();
                            continue;
                        }
                    } catch (const std::exception&) {
                        Logger::warn("Invalid dev level action: " + *action, __func__);
                    }
                }
            }
            const std::string title = translator.tr("dev.title");
            SDL_SetWindowTitle(window, title.c_str());
            SDL_SetRenderDrawColor(renderer, 10, 5, 20, 255);
            SDL_RenderClear(renderer);
            applyMenuStyleFor(levelSelectMenu);
            const SDL_Color titleColor{ 255, 255, 255, 255 };
            const SDL_Color secondary{ 180, 180, 180, 255 };
            drawCenteredText(windowHeight / 6, translator.tr("dev.title"), 4, titleColor);
            drawCenteredText(windowHeight / 6 + uiFontRef.lineHeight(3), translator.tr("dev.subtitle"), 2, secondary);
            levelSelectMenuManager.render(sdlMenuRenderer, menuMetrics);
            const int hintLineHeight = uiFontRef.lineHeight(2);
            const int hintBottomY = windowHeight - hintLineHeight - 16;
            const std::string fullscreenHint = translator.tr("menu.fullscreenHint") + " " + fullscreenKeyName;
            drawCenteredText(hintBottomY - hintLineHeight - 6, fullscreenHint, 2, secondary);
            drawCenteredText(hintBottomY, translator.tr("dev.hint"), 2, secondary);
            SDL_RenderPresent(renderer);
            levelSelectInput.endFrame();
        } else if (screen == ScreenState::GameOver) {
            const Uint32 elapsed = now - gameOverState.startedAt;
            if (gameOverDelay == 0 || elapsed >= gameOverDelay) {
                rebuildGame();
                screen = ScreenState::Menu;
                mainMenuManager.setMenu(mainMenu);
                updateMenuMusic();
                continue;
            }
            const std::string title = translator.tr("menu.title");
            SDL_SetWindowTitle(window, title.c_str());
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);
            gridRenderer.draw(game.grid());
            hud.draw(game, windowWidth);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_Rect overlay{ 0, 0, windowWidth, windowHeight };
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
            SDL_RenderFillRect(renderer, &overlay);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
            const SDL_Color titleColor{ 255, 255, 255, 255 };
            const SDL_Color secondary{ 200, 200, 200, 255 };
            drawCenteredText(windowHeight / 3, translator.tr("gameOver.title"), 4, titleColor);
            const std::string levelLine =
                translator.tr("gameOver.levelLabel") + " " + std::to_string(gameOverState.level);
            drawCenteredText(windowHeight / 3 + uiFontRef.lineHeight(3), levelLine, 3, secondary);
            const std::string scoreLine =
                translator.tr("gameOver.scoreLabel") + " " + std::to_string(gameOverState.score);
            drawCenteredText(windowHeight / 3 + 2 * uiFontRef.lineHeight(3), scoreLine, 3, secondary);
            const Uint32 remainingMs = gameOverDelay - elapsed;
            const Uint32 remainingSeconds = (remainingMs + 999) / 1000;
            const std::string countdown =
                translator.tr("gameOver.returning") + " " + std::to_string(remainingSeconds) +
                translator.tr("duration.secondsSuffix");
            drawCenteredText(windowHeight - 2 * uiFontRef.lineHeight(2), countdown, 2, secondary);
            SDL_RenderPresent(renderer);
        } else if (screen == ScreenState::Victory) {
            SDL_SetWindowTitle(window, translator.tr("victory.title").c_str());
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);
            gridRenderer.draw(game.grid());
            hud.draw(game, windowWidth);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_Rect overlay{ 0, 0, windowWidth, windowHeight };
            SDL_SetRenderDrawColor(renderer, 0, 20, 10, 205);
            SDL_RenderFillRect(renderer, &overlay);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
            const SDL_Color titleColor{ 0, 255, 127, 255 };
            const SDL_Color secondary{ 230, 255, 235, 255 };
            drawCenteredText(windowHeight / 3, translator.tr("victory.title"), 4, titleColor);
            const std::string scoreLine =
                translator.tr("victory.scoreLabel") + " " + std::to_string(game.totalScore());
            drawCenteredText(
                windowHeight / 3 + uiFontRef.lineHeight(4), scoreLine, 3, secondary);
            drawCenteredText(
                windowHeight - 2 * uiFontRef.lineHeight(2),
                translator.tr("victory.returnHint"),
                2,
                secondary);
            SDL_RenderPresent(renderer);
        }

        delayFrame(screen == ScreenState::Playing ? 1 : 16);
    }

    for (auto& entry : menuTextures) {
        if (entry.second) {
            SDL_DestroyTexture(entry.second);
        }
    }
    uiFont.reset();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    Audio::shutdown();
#ifdef HAS_SDL_TTF
    if (ttfInitialized) {
        TTF_Quit();
    }
#endif
    SDL_Quit();
    return 0;
}
