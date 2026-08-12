#pragma once

#include <SDL.h>

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "../core/Grid.h"
#include "SpriteSheet.h"

class Renderer {
public:
    Renderer(
        SDL_Renderer* renderer,
        int tileSize,
        int originY,
        const std::filesystem::path& assetsPath);
    ~Renderer();

    void draw(const Grid& grid) const;
    void drawTestPattern(int windowWidth, int windowHeight) const;
    bool hasSprites() const { return m_spriteSheet || m_useSpriteFiles; }

private:
    void drawWithSprites(const Grid& grid) const;
    void drawFallback(const Grid& grid) const;
    void buildFallbackTextures();
    const std::string& spriteIdForCell(const Cell& cell) const;
    const SDL_Rect* sheetFrameForId(const std::string& id) const;
    const SDL_Rect* sheetFrameForId(const std::string& id, Uint32 ticks) const;
    SDL_Texture* loadTextureFromPng(const std::filesystem::path& path);
    bool loadSpriteFiles(const std::filesystem::path& manifestPath);

    SDL_Texture* createTileTexture(CellType type);

    SDL_Renderer* m_renderer;
    int m_tileSize;
    int m_originY;
    std::unique_ptr<SpriteSheet> m_spriteSheet;
    bool m_useSpriteFiles = false;
    Uint32 m_defaultFrameDurationMs = 150;
    std::unordered_map<std::string, std::vector<SDL_Texture*>> m_spriteFileFrames;
    std::unordered_map<std::string, Uint32> m_spriteFrameDurations;
    std::vector<SDL_Texture*> m_ownedSpriteTextures;
    std::vector<SDL_Texture*> m_tileTextures;
    mutable bool m_loggedTestPattern = false;
};
