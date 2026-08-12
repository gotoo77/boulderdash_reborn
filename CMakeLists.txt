cmake_minimum_required(VERSION 3.16)
project(boulderdash LANGUAGES CXX)

set(BD_BIN ${PROJECT_NAME})

# Permet à CMake de trouver les packages générés par Conan (CMakeDeps)
list(PREPEND CMAKE_PREFIX_PATH "${CMAKE_BINARY_DIR}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
include(CTest)

option(BOULDERDASH_STRICT_WARNINGS "Treat project compiler warnings as errors" OFF)
option(BOULDERDASH_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(BOULDERDASH_BUILD_EXAMPLES "Build standalone example code" OFF)

function(boulderdash_enable_warnings target)
    if (MSVC)
        target_compile_options(${target} PRIVATE /W4)
        if (BOULDERDASH_STRICT_WARNINGS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    elseif (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        if (BOULDERDASH_STRICT_WARNINGS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

function(boulderdash_enable_sanitizers target)
    if (NOT BOULDERDASH_SANITIZERS)
        return()
    endif()
    if (EMSCRIPTEN OR NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(FATAL_ERROR "BOULDERDASH_SANITIZERS requires a native GCC or Clang build")
    endif()
    target_compile_options(${target} PRIVATE
        -fsanitize=address,undefined
        -fno-omit-frame-pointer
    )
    target_link_options(${target} PRIVATE -fsanitize=address,undefined)
endfunction()

set(BOULDERDASH_ASSET_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets")
set(BOULDERDASH_CONFIG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/cfg")

# Les six SFX rétro synthétisés sont des artefacts générés. Leur source de
# vérité est le synthétiseur Python versionné ; les WAV ne sont volontairement
# plus suivis par Git.
find_package(Python3 COMPONENTS Interpreter REQUIRED)
set(BOULDERDASH_RETRO_SFX_GENERATOR
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_retro_sfx.py"
)
set(BOULDERDASH_GENERATED_SFX
    "${BOULDERDASH_ASSET_DIR}/sfx/walk.wav"
    "${BOULDERDASH_ASSET_DIR}/sfx/dig.wav"
    "${BOULDERDASH_ASSET_DIR}/sfx/rock_fall.wav"
    "${BOULDERDASH_ASSET_DIR}/sfx/diamond.wav"
    "${BOULDERDASH_ASSET_DIR}/sfx/death.wav"
    "${BOULDERDASH_ASSET_DIR}/sfx/diamond_fall.wav"
)
add_custom_command(
    OUTPUT ${BOULDERDASH_GENERATED_SFX}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${BOULDERDASH_ASSET_DIR}/sfx"
    COMMAND ${Python3_EXECUTABLE} ${BOULDERDASH_RETRO_SFX_GENERATOR}
        --output-dir "${BOULDERDASH_ASSET_DIR}/sfx"
    DEPENDS ${BOULDERDASH_RETRO_SFX_GENERATOR}
    COMMENT "Generating original retro SFX"
    VERBATIM
)
add_custom_target(boulderdash_retro_sfx DEPENDS ${BOULDERDASH_GENERATED_SFX})

# --------------------------------------------------
# Dépendances desktop UNIQUEMENT
# --------------------------------------------------
find_package(nlohmann_json CONFIG REQUIRED)

set(SDL2_TTF_TARGET "")

if (NOT EMSCRIPTEN)
    # Fedora / zlib-ng compat :
    # Forcer l'utilisation de FindZLIB.cmake (le Config est cassé)
    set(ZLIB_NO_CONFIG ON)
    set(ZLIB_USE_STATIC_LIBS OFF)

    find_package(SDL2 REQUIRED)
    # Le package CONFIG de certaines versions de Fedora référence
    # PNG::png_static même lorsque libpng16.a n'est pas installé.
    # Le module CMake officiel trouve correctement la bibliothèque partagée.
    find_package(PNG MODULE REQUIRED)
    find_package(SDL2_mixer QUIET)

    if (NOT SDL2_mixer_FOUND)
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(SDL2_MIXER REQUIRED IMPORTED_TARGET SDL2_mixer)
        set(SDL2_MIXER_TARGET PkgConfig::SDL2_MIXER)
    else()
        set(SDL2_MIXER_TARGET SDL2_mixer::SDL2_mixer)
    endif()

    find_package(SDL2_ttf QUIET)
    if (NOT SDL2_ttf_FOUND)
        if (NOT PkgConfig_FOUND)
            find_package(PkgConfig REQUIRED)
        endif()
        pkg_check_modules(SDL2_TTF REQUIRED IMPORTED_TARGET SDL2_ttf)
        set(SDL2_TTF_TARGET PkgConfig::SDL2_TTF)
    else()
        set(SDL2_TTF_TARGET SDL2_ttf::SDL2_ttf)
    endif()

endif()

# --------------------------------------------------
# Cœur de jeu indépendant de SDL et de l'audio
# --------------------------------------------------
add_library(boulderdash_core STATIC
    src/core/Game.cpp
    src/core/Grid.cpp
    src/systems/GravitySystem.cpp
    src/systems/EnemySystem.cpp
    src/systems/PlayerSystem.cpp
    src/util/Logger.cpp
)
target_include_directories(boulderdash_core PUBLIC src)
boulderdash_enable_warnings(boulderdash_core)
boulderdash_enable_sanitizers(boulderdash_core)

# --------------------------------------------------
# Application SDL
# --------------------------------------------------
add_executable(${BD_BIN}
    src/main.cpp
    src/app/Application.cpp
    src/app/AssetCatalog.cpp
    src/app/AudioEventRouter.cpp
    src/app/GameplayLoop.cpp
    src/app/InputMapping.cpp
    src/app/MenuFactory.cpp
    src/app/OptionsStyle.cpp
    src/app/ScreenState.cpp
    src/audio/Audio.cpp
    src/render/Renderer.cpp
    src/render/SpriteSheet.cpp
    src/render/HudRenderer.cpp
    src/render/BitmapFont.cpp
    src/render/TextNormalizer.cpp
    src/render/PngLoader.cpp
    src/menu/Menu.cpp
    src/menu/MenuLoader.cpp
    src/menu/MenuManager.cpp
    src/menu/sdl/SDLMenuRenderer.cpp
    src/menu/sdl/SDLInputProvider.cpp
    src/util/Config.cpp
    src/util/Translator.cpp
)

add_dependencies(${BD_BIN} boulderdash_retro_sfx)
target_include_directories(${BD_BIN} PRIVATE src)
boulderdash_enable_warnings(${BD_BIN})
boulderdash_enable_sanitizers(${BD_BIN})
target_link_libraries(${BD_BIN} PRIVATE boulderdash_core)

# --------------------------------------------------
# Configuration EMSCRIPTEN (WEB)
# --------------------------------------------------
if (EMSCRIPTEN)
    message(STATUS "Configuring Boulder Dash for Emscripten")

    set(BOULDERDASH_WEB_SHELL "${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html")

    # Les répertoires préchargés ne sont pas suivis automatiquement par le
    # linker Emscripten. Toute modification d'un asset ou d'un JSON doit donc
    # invalider boulderdash.data et provoquer une nouvelle édition de liens.
    file(GLOB_RECURSE BOULDERDASH_PRELOAD_FILES CONFIGURE_DEPENDS
        LIST_DIRECTORIES false
        "${BOULDERDASH_ASSET_DIR}/*"
        "${BOULDERDASH_CONFIG_DIR}/*"
    )
    list(APPEND BOULDERDASH_PRELOAD_FILES ${BOULDERDASH_GENERATED_SFX})
    set_property(TARGET ${BD_BIN} APPEND PROPERTY LINK_DEPENDS
        "${BOULDERDASH_PRELOAD_FILES}"
        "${BOULDERDASH_WEB_SHELL}"
    )

    # Génère boulderdash.html + js + wasm
    set(CMAKE_EXECUTABLE_SUFFIX ".html")

    # Les assets sont montés dans /assets (FS virtuel)
    target_compile_definitions(${BD_BIN} PRIVATE ASSETS_DIR="/assets" CONFIG_DIR="/cfg")

    target_compile_options(${BD_BIN} PRIVATE
        "-sUSE_SDL=2"
        "-sUSE_SDL_MIXER=2"
        "-sUSE_SDL_TTF=2"
        "-sUSE_LIBPNG=1"
    )

    target_link_options(${BD_BIN} PRIVATE
        "-sUSE_SDL=2"
        "-sUSE_SDL_MIXER=2"
        "-sUSE_SDL_TTF=2"
        "-sUSE_LIBPNG=1"
        "-sSDL2_MIXER_FORMATS=['ogg','mp3']"
        "-sALLOW_MEMORY_GROWTH=1"
        "-sFORCE_FILESYSTEM=1"
        "-sINITIAL_MEMORY=256MB"
        "-sASYNCIFY=1"
        "-sASSERTIONS=2"
        "-sSAFE_HEAP=1"
        "-sSTACK_OVERFLOW_CHECK=2"
        "SHELL:--shell-file ${BOULDERDASH_WEB_SHELL}"
        "SHELL:--preload-file ${BOULDERDASH_ASSET_DIR}@/assets"
        "SHELL:--preload-file ${BOULDERDASH_CONFIG_DIR}@/cfg"
    )
# --------------------------------------------------
# Configuration DESKTOP
# --------------------------------------------------
else()
    target_compile_definitions(${BD_BIN} PRIVATE
        ASSETS_DIR="${BOULDERDASH_ASSET_DIR}"
        CONFIG_DIR="${BOULDERDASH_CONFIG_DIR}"
    )
    target_link_libraries(${BD_BIN}
        PRIVATE SDL2::SDL2 PNG::PNG ${SDL2_MIXER_TARGET}
    )
endif()

if (EMSCRIPTEN)
    target_sources(${BD_BIN} PRIVATE src/render/TtfFont.cpp)
    target_compile_definitions(${BD_BIN} PRIVATE HAS_SDL_TTF=1)
elseif (SDL2_TTF_TARGET)
    target_sources(${BD_BIN} PRIVATE src/render/TtfFont.cpp)
    target_link_libraries(${BD_BIN} PRIVATE ${SDL2_TTF_TARGET})
    target_compile_definitions(${BD_BIN} PRIVATE HAS_SDL_TTF=1)
endif()

target_link_libraries(${BD_BIN} PRIVATE nlohmann_json::nlohmann_json)

if (BUILD_TESTING)
    add_executable(boulderdash_tests
        tests/BoulderdashTests.cpp
        tests/GameTests.cpp
        tests/SystemTests.cpp
        tests/DataTests.cpp
        src/app/GameplayLoop.cpp
        src/menu/Menu.cpp
        src/menu/MenuLoader.cpp
        src/menu/MenuManager.cpp
        src/menu/MockMenuRenderer.cpp
        src/util/Config.cpp
    )
    add_dependencies(boulderdash_tests boulderdash_retro_sfx)
    target_include_directories(boulderdash_tests PRIVATE src tests)
    boulderdash_enable_warnings(boulderdash_tests)
    boulderdash_enable_sanitizers(boulderdash_tests)
    target_compile_definitions(boulderdash_tests PRIVATE
        TEST_ASSET_DIR="${BOULDERDASH_ASSET_DIR}"
        TEST_CONFIG_DIR="${BOULDERDASH_CONFIG_DIR}"
    )
    target_link_libraries(boulderdash_tests PRIVATE
        boulderdash_core
        nlohmann_json::nlohmann_json
    )
    add_test(NAME boulderdash_tests COMMAND boulderdash_tests)
endif()

if (BOULDERDASH_BUILD_EXAMPLES)
    add_library(boulderdash_menu_example STATIC
        src/menu/MenuExample.cpp
        src/menu/Menu.cpp
        src/menu/MenuLoader.cpp
        src/menu/MenuManager.cpp
        src/menu/MockMenuRenderer.cpp
    )
    target_include_directories(boulderdash_menu_example PRIVATE src)
    target_link_libraries(boulderdash_menu_example PRIVATE nlohmann_json::nlohmann_json)
    boulderdash_enable_warnings(boulderdash_menu_example)
    boulderdash_enable_sanitizers(boulderdash_menu_example)
endif()
