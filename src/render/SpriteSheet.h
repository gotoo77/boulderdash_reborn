#pragma once

#include <SDL.h>

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

class SpriteSheet {
public:
    SpriteSheet(
        SDL_Renderer* renderer,
        const std::filesystem::path& indexPath,
        const std::filesystem::path& texturePath);
    ~SpriteSheet();

    bool valid() const { return m_texture != nullptr; }
    SDL_Texture* texture() const { return m_texture; }

    const std::unordered_map<std::string, std::vector<SDL_Rect>>& sprites() const;

private:
    struct SpriteDef {
        int x = 0;
        int y = 0;
        int frames = 1;
    };

    bool loadIndex(const std::filesystem::path& path);
    bool loadTexture(SDL_Renderer* renderer, const std::filesystem::path& path);
    void generateSpriteFrames(const std::vector<unsigned char>& pixels, int width, int height);
    std::vector<SDL_Rect> extractFrames(
        const SpriteDef& def,
        const std::vector<unsigned char>& pixels,
        int textureWidth,
        int textureHeight) const;

    int m_tileSize = 0;
    SDL_Texture* m_texture = nullptr;
    std::unordered_map<std::string, SpriteDef> m_spriteDefs;
    std::unordered_map<std::string, std::vector<SDL_Rect>> m_spriteFrames;
};
