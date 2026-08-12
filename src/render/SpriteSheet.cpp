#include "SpriteSheet.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <optional>
#include <queue>
#include <sstream>

#include "../util/Logger.h"
#include "PngLoader.h"

namespace {

std::string readFile(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        return {};
    }
    std::ostringstream oss;
    oss << input.rdbuf();
    return oss.str();
}

std::optional<int> extractInt(const std::string& text, const std::string& key, std::size_t start, std::size_t end) {
    const std::string needle = '"' + key + '"';
    const auto keyPos = text.find(needle, start);
    if (keyPos == std::string::npos || keyPos > end) {
        return std::nullopt;
    }
    const auto colonPos = text.find(':', keyPos + needle.size());
    if (colonPos == std::string::npos || colonPos > end) {
        return std::nullopt;
    }
    const auto numberStart = text.find_first_of("-0123456789", colonPos + 1);
    if (numberStart == std::string::npos || numberStart > end) {
        return std::nullopt;
    }
    const auto numberEnd = text.find_first_not_of("-0123456789", numberStart);
    const auto length = (numberEnd == std::string::npos) ? std::string::npos : numberEnd - numberStart;
    try {
        return std::stoi(text.substr(numberStart, length));
    } catch (...) {
        return std::nullopt;
    }
}

} // namespace

SpriteSheet::SpriteSheet(
    SDL_Renderer* renderer,
    const std::filesystem::path& indexPath,
    const std::filesystem::path& texturePath) {
    LOG_T("SpriteSheet ctor index=%s texture=%s", indexPath.string().c_str(), texturePath.string().c_str());
    if (!loadIndex(indexPath)) {
        Logger::warn("Failed to parse sprite index at " + indexPath.string(), __func__);
        return;
    }
    if (!loadTexture(renderer, texturePath)) {
        Logger::warn("Failed to load sprite texture at " + texturePath.string(), __func__);
        m_spriteFrames.clear();
        m_tileSize = 0;
    }
}

SpriteSheet::~SpriteSheet() {
    if (m_texture) {
        SDL_DestroyTexture(m_texture);
        m_texture = nullptr;
    }
}

bool SpriteSheet::loadIndex(const std::filesystem::path& path) {
    LOG_T("SpriteSheet::loadIndex %s", path.string().c_str());
    const std::string content = readFile(path);
    if (content.empty()) {
        return false;
    }
    auto tileSizeOpt = extractInt(content, "tileSize", 0, content.size());
    if (!tileSizeOpt.has_value() || *tileSizeOpt <= 0) {
        return false;
    }
    m_tileSize = *tileSizeOpt;
    Logger::debug("SpriteSheet tileSize=" + std::to_string(m_tileSize), __func__);

    const std::string spritesKey = "\"sprites\"";
    const auto spritesPos = content.find(spritesKey);
    if (spritesPos == std::string::npos) {
        return false;
    }
    const auto blockStart = content.find('{', spritesPos + spritesKey.size());
    if (blockStart == std::string::npos) {
        return false;
    }

    std::size_t cursor = blockStart + 1;
    while (cursor < content.size()) {
        cursor = content.find('"', cursor);
        if (cursor == std::string::npos) {
            break;
        }
        if (cursor > 0 && content[cursor - 1] == '\\') {
            ++cursor;
            continue;
        }
        const auto nameEnd = content.find('"', cursor + 1);
        if (nameEnd == std::string::npos) {
            break;
        }
        const std::string spriteName = content.substr(cursor + 1, nameEnd - cursor - 1);
        const auto entryStart = content.find('{', nameEnd);
        if (entryStart == std::string::npos) {
            break;
        }
        const auto entryEnd = content.find('}', entryStart);
        if (entryEnd == std::string::npos) {
            break;
        }
        SpriteDef def;
        def.x = extractInt(content, "x", entryStart, entryEnd).value_or(0);
        def.y = extractInt(content, "y", entryStart, entryEnd).value_or(0);
        def.frames = std::max(1, extractInt(content, "frames", entryStart, entryEnd).value_or(1));
        def.sourceX = extractInt(content, "sourceX", entryStart, entryEnd).value_or(-1);
        def.sourceY = extractInt(content, "sourceY", entryStart, entryEnd).value_or(-1);
        def.sourceWidth = extractInt(content, "sourceWidth", entryStart, entryEnd).value_or(-1);
        def.sourceHeight = extractInt(content, "sourceHeight", entryStart, entryEnd).value_or(-1);
        Logger::debug(
            "Sprite '" + spriteName + "' starts at (" + std::to_string(def.x) + ", " + std::to_string(def.y) +
            ") frames=" + std::to_string(def.frames),
            __func__);
        m_spriteDefs[spriteName] = def;
        cursor = entryEnd + 1;
        while (cursor < content.size() && std::isspace(static_cast<unsigned char>(content[cursor]))) {
            ++cursor;
        }
        if (cursor < content.size() && content[cursor] == ',') {
            ++cursor;
        }
    }

    if (m_spriteDefs.empty()) {
        Logger::warn("SpriteSheet contained no sprite entries.", __func__);
    }
    return !m_spriteDefs.empty();
}

bool SpriteSheet::loadTexture(SDL_Renderer* renderer, const std::filesystem::path& path) {
    LOG_T("SpriteSheet::loadTexture %s", path.string().c_str());
    std::vector<unsigned char> pixels;
    int width = 0;
    int height = 0;
    int pitch = 0;
    if (!gfx::loadPngRGBA(path, pixels, width, height, pitch)) {
        Logger::warn("Failed to load sprite texture: " + path.string(), __func__);
        return false;
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
        return false;
    }
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    if (!texture) {
        Logger::warn(std::string("SDL_CreateTextureFromSurface failed: ") + SDL_GetError(), __func__);
        return false;
    }
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);

    Logger::debug(
        "Loaded sprite texture " + path.string() + " (" + std::to_string(width) + "x" +
        std::to_string(height) + ")",
        __func__);
    m_texture = texture;
    generateSpriteFrames(pixels, width, height);
    return true;
}

void SpriteSheet::generateSpriteFrames(const std::vector<unsigned char>& pixels, int width, int height) {
    LOG_T("SpriteSheet::generateSpriteFrames size=%dx%d entries=%zu", width, height, m_spriteDefs.size());
    m_spriteFrames.clear();
    for (const auto& entry : m_spriteDefs) {
        m_spriteFrames[entry.first] = extractFrames(entry.second, pixels, width, height);
    }
}

std::vector<SDL_Rect> SpriteSheet::extractFrames(
    const SpriteDef& def,
    const std::vector<unsigned char>& pixels,
    int textureWidth,
    int textureHeight) const {
    LOG_T(
        "SpriteSheet::extractFrames sprite=(%d,%d) frames=%d", def.x, def.y, def.frames);
    const bool hasExplicitSource = def.sourceX >= 0 && def.sourceY >= 0 &&
        def.sourceWidth > 0 && def.sourceHeight > 0;
    if (hasExplicitSource) {
        std::vector<SDL_Rect> frames;
        frames.reserve(def.frames);
        for (int i = 0; i < def.frames; ++i) {
            const int frameX = def.sourceX + i * def.sourceWidth;
            if (frameX + def.sourceWidth > textureWidth ||
                def.sourceY + def.sourceHeight > textureHeight) {
                Logger::warn("Explicit sprite frame exceeds texture bounds", __func__);
                break;
            }
            frames.push_back(SDL_Rect{
                frameX,
                def.sourceY,
                def.sourceWidth,
                def.sourceHeight,
            });
        }
        return frames;
    }

    const int startX = def.x * m_tileSize;
    const int startY = def.y * m_tileSize;
    const int regionWidth = std::min(m_tileSize, textureWidth - startX);
    const int regionHeight = std::min(m_tileSize, textureHeight - startY);
    if (regionWidth <= 0 || regionHeight <= 0) {
        return {};
    }

    const int minDim = std::max(16, m_tileSize / 4);
    std::vector<uint8_t> mask(regionWidth * regionHeight, 0);
    auto idx = [&](int x, int y) { return y * regionWidth + x; };

    for (int y = 0; y < regionHeight; ++y) {
        for (int x = 0; x < regionWidth; ++x) {
            const int globalX = startX + x;
            const int globalY = startY + y;
            const int pixelIndex = (globalY * textureWidth + globalX) * 4;
            const unsigned char r = pixels[pixelIndex];
            const unsigned char g = pixels[pixelIndex + 1];
            const unsigned char b = pixels[pixelIndex + 2];
            const unsigned char a = pixels[pixelIndex + 3];
            const int brightness = r + g + b;
            if (a > 16 && brightness > 80) {
                mask[idx(x, y)] = 1;
            }
        }
    }

    std::vector<uint8_t> visited(regionWidth * regionHeight, 0);
    std::vector<SDL_Rect> regions;
    std::queue<std::pair<int, int>> queue;

    auto emitRegion = [&](int minX, int minY, int maxX, int maxY) {
        SDL_Rect rect{
            startX + minX,
            startY + minY,
            maxX - minX + 1,
            maxY - minY + 1 };
        regions.push_back(rect);
    };

    for (int y = 0; y < regionHeight; ++y) {
        for (int x = 0; x < regionWidth; ++x) {
            const int index = idx(x, y);
            if (!mask[index] || visited[index]) {
                continue;
            }
            visited[index] = 1;
            queue.push({ x, y });
            int minX = x;
            int maxX = x;
            int minY = y;
            int maxY = y;
            while (!queue.empty()) {
                auto [cx, cy] = queue.front();
                queue.pop();
                minX = std::min(minX, cx);
                maxX = std::max(maxX, cx);
                minY = std::min(minY, cy);
                maxY = std::max(maxY, cy);
                for (int ny = std::max(0, cy - 1); ny <= std::min(regionHeight - 1, cy + 1); ++ny) {
                    for (int nx = std::max(0, cx - 1); nx <= std::min(regionWidth - 1, cx + 1); ++nx) {
                        const int neighborIndex = idx(nx, ny);
                        if (!visited[neighborIndex] && mask[neighborIndex]) {
                            visited[neighborIndex] = 1;
                            queue.push({ nx, ny });
                        }
                    }
                }
            }
            const int width = maxX - minX + 1;
            const int height = maxY - minY + 1;
            if (width < minDim || height < minDim) {
                continue;
            }
            emitRegion(minX, minY, maxX, maxY);
        }
    }

    if (regions.empty()) {
        SDL_Rect fallback{
            startX,
            startY,
            regionWidth,
            regionHeight };
        regions.push_back(fallback);
    }

    std::sort(regions.begin(), regions.end(), [](const SDL_Rect& a, const SDL_Rect& b) {
        if (a.y == b.y) {
            return a.x < b.x;
        }
        return a.y < b.y;
    });

    std::vector<SDL_Rect> frames;
    frames.reserve(def.frames);
    for (int i = 0; i < def.frames; ++i) {
        if (i < static_cast<int>(regions.size())) {
            frames.push_back(regions[i]);
        } else {
            frames.push_back(regions.back());
        }
    }
    return frames;
}

const std::unordered_map<std::string, std::vector<SDL_Rect>>& SpriteSheet::sprites() const {
    return m_spriteFrames;
}
