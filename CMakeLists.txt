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

set(BOULDERDASH_ASSET_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets")
set(BOULDERDASH_CONFIG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/cfg")

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
        pkg_check_modules(SDL2_TTF QUIET SDL2_ttf)
        if (SDL2_TTF_FOUND)
            set(SDL2_TTF_TARGET PkgConfig::SDL2_TTF)
        endif()
    else()
        set(SDL2_TTF_TARGET SDL2_ttf::SDL2_ttf)
    endif()

endif()

# --------------------------------------------------
# Target principale
# --------------------------------------------------
add_executable(${BD_BIN}
    src/main.cpp
    src/core/Game.cpp
    src/core/Grid.cpp
    src/audio/Audio.cpp
    src/systems/GravitySystem.cpp
    src/systems/EnemySystem.cpp
    src/systems/PlayerSystem.cpp
    src/render/Renderer.cpp
    src/render/SpriteSheet.cpp
    src/render/HudRenderer.cpp
    src/render/BitmapFont.cpp
    src/render/TextNormalizer.cpp
    src/render/PngLoader.cpp
    src/menu/Menu.cpp
    src/menu/MenuLoader.cpp
    src/menu/MenuManager.cpp
    src/menu/MockMenuRenderer.cpp
    src/menu/MenuExample.cpp
    src/menu/sdl/SDLMenuRenderer.cpp
    src/menu/sdl/SDLInputProvider.cpp
    src/util/Logger.cpp
    src/util/Config.cpp
    src/util/Translator.cpp
)

target_include_directories(${BD_BIN} PRIVATE src)
boulderdash_enable_warnings(${BD_BIN})

# --------------------------------------------------
# Configuration EMSCRIPTEN (WEB)
# --------------------------------------------------
if (EMSCRIPTEN)
    message(STATUS "Configuring Boulder Dash for Emscripten")

    # Génère boulderdash.html + js + wasm
    set(CMAKE_EXECUTABLE_SUFFIX ".html")

    # Les assets sont montés dans /assets (FS virtuel)
    target_compile_definitions(${BD_BIN} PRIVATE ASSETS_DIR="/assets")

    # ⚠️ FLAGS OBLIGATOIRES À LA COMPILATION
    #target_compile_options(boulderdash PRIVATE
    #    "-sUSE_SDL=2"
    #    "-sUSE_SDL_MIXER=2"
    #    "-sUSE_LIBPNG=1"
    #)

    target_compile_options(${BD_BIN} PRIVATE
        "-sUSE_SDL=2"
        "-sUSE_SDL_MIXER=2"
        "-sUSE_LIBPNG=1"
    )

    # ⚠️ FLAGS OBLIGATOIRES AU LINK
    target_link_options(${BD_BIN} PRIVATE
        "-sUSE_SDL=2"
        "-sUSE_SDL_MIXER=2"
        "-sUSE_LIBPNG=1"
        "-sSDL2_MIXER_FORMATS=['ogg','wav']"
        "-sALLOW_MEMORY_GROWTH=1"
        "-sFORCE_FILESYSTEM=1"
        "-sINITIAL_MEMORY=256MB"
        "-sASYNCIFY=1"
        "-sASSERTIONS=2"
        "-sSAFE_HEAP=1"
        "-sSTACK_OVERFLOW_CHECK=2"
        "SHELL:--preload-file ${BOULDERDASH_ASSET_DIR}@/assets"
        "SHELL:--preload-file ${BOULDERDASH_CONFIG_DIR}@/cfg"
    )
# --------------------------------------------------
# Configuration DESKTOP
# --------------------------------------------------
else()
    target_compile_definitions(${BD_BIN} PRIVATE ASSETS_DIR="${BOULDERDASH_ASSET_DIR}")
    target_link_libraries(${BD_BIN}
        PRIVATE SDL2::SDL2 PNG::PNG ${SDL2_MIXER_TARGET}
    )
endif()

if (SDL2_TTF_TARGET)
    target_sources(${BD_BIN} PRIVATE src/render/TtfFont.cpp)
    target_link_libraries(${BD_BIN} PRIVATE ${SDL2_TTF_TARGET})
    target_compile_definitions(${BD_BIN} PRIVATE HAS_SDL_TTF=1)
endif()

target_link_libraries(${BD_BIN} PRIVATE nlohmann_json::nlohmann_json)

if (BUILD_TESTING)
    add_executable(boulderdash_tests
        tests/BoulderdashTests.cpp
        src/core/Grid.cpp
        src/systems/GravitySystem.cpp
        src/systems/EnemySystem.cpp
        src/systems/PlayerSystem.cpp
        src/menu/Menu.cpp
        src/menu/MenuLoader.cpp
        src/menu/MenuManager.cpp
        src/menu/MockMenuRenderer.cpp
        src/util/Logger.cpp
    )
    target_include_directories(boulderdash_tests PRIVATE src)
    boulderdash_enable_warnings(boulderdash_tests)
    target_compile_definitions(boulderdash_tests PRIVATE
        TEST_ASSET_DIR="${BOULDERDASH_ASSET_DIR}"
    )
    target_link_libraries(boulderdash_tests PRIVATE nlohmann_json::nlohmann_json)
    add_test(NAME boulderdash_tests COMMAND boulderdash_tests)
endif()
