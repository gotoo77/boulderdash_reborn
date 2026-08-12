#include "Renderer.h"

#include "SpriteSheet.h"
#include "PngLoader.h"

#include <algorithm>
#include <array>
#include <exception>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>

#include "../util/Logger.h"

namespace {

struct Color {
    Uint8 r;
    Uint8 g;
    Uint8 b;
    Uint8 a;
};

constexpr Color color(Uint8 r, Uint8 g, Uint8 b, Uint8 a = 255) { return Color{ r, g, b, a }; }

void setColor(SDL_Renderer* renderer, Color c) {
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
}

void fill(SDL_Renderer* renderer, Color c) {
    setColor(renderer, c);
    SDL_RenderClear(renderer);
}

void drawDirtTile(SDL_Renderer* renderer, int tileSize) {
    fill(renderer, color(198, 133, 0));
    setColor(renderer, color(230, 182, 73));
    for (int y = 0; y < tileSize; y += 4) {
        for (int x = (y / 4) % 2 ? 2 : 0; x < tileSize; x += 4) {
            SDL_Rect rect{ x, y, 2, 2 };
            SDL_RenderFillRect(renderer, &rect);
        }
    }
    setColor(renderer, color(110, 70, 0));
    for (int y = 2; y < tileSize; y += 4) {
        for (int x = ((y / 4) % 2) ? 0 : 2; x < tileSize; x += 4) {
            SDL_Rect rect{ x, y, 2, 2 };
            SDL_RenderFillRect(renderer, &rect);
        }
    }
}

void drawWallTile(SDL_Renderer* renderer, int tileSize) {
    fill(renderer, color(30, 30, 30));
    const int brickHeight = std::max(2, tileSize / 4);
    const int brickWidth = std::max(4, tileSize / 4);
    setColor(renderer, color(230, 230, 230));
    for (int y = 0; y < tileSize; y += brickHeight) {
        const int offset = (y / brickHeight) % 2 ? brickWidth / 2 : 0;
        for (int x = offset; x < tileSize; x += brickWidth) {
            SDL_Rect brick{ x, y, brickWidth - 1, brickHeight - 1 };
            SDL_RenderFillRect(renderer, &brick);
        }
    }
    setColor(renderer, color(100, 100, 100));
    for (int y = brickHeight; y < tileSize; y += brickHeight) {
        SDL_RenderDrawLine(renderer, 0, y, tileSize, y);
    }
}

void drawDestructibleWallTile(SDL_Renderer* renderer, int tileSize) {
    fill(renderer, color(60, 30, 20));
    const int crackCount = std::max(1, tileSize / 6);
    setColor(renderer, color(140, 70, 40));
    for (int i = 0; i < crackCount; ++i) {
        const int offset = (i * tileSize) / crackCount;
        SDL_RenderDrawLine(renderer, offset, 0, tileSize - 1, tileSize - 1 - offset / 2);
    }
    setColor(renderer, color(215, 140, 90));
    for (int i = 0; i < crackCount / 2 + 1; ++i) {
        const int x = (i * tileSize) / (crackCount / 2 + 1);
        SDL_RenderDrawLine(renderer, x, tileSize - 1, tileSize - 1 - x / 2, 0);
    }
}

void drawRockTile(SDL_Renderer* renderer, int tileSize) {
    fill(renderer, color(120, 120, 120));
    const int radius = tileSize / 2 - 1;
    const int cx = tileSize / 2;
    const int cy = tileSize / 2;
    for (int y = 0; y < tileSize; ++y) {
        for (int x = 0; x < tileSize; ++x) {
            const int dx = x - cx;
            const int dy = y - cy;
            const int dist2 = dx * dx + dy * dy;
            if (dist2 <= radius * radius) {
                const bool highlight = (dx + dy) < 0;
                const Color shade = highlight ? color(210, 210, 210) : color(90, 90, 90);
                setColor(renderer, shade);
                SDL_RenderDrawPoint(renderer, x, y);
            }
        }
    }
    setColor(renderer, color(235, 235, 235));
    SDL_Rect shine{ cx - radius / 2, cy - radius + 2, radius / 2, radius / 2 };
    SDL_RenderFillRect(renderer, &shine);
}

void drawDiamondTile(SDL_Renderer* renderer, int tileSize) {
    fill(renderer, color(5, 10, 20));
    const int mid = tileSize / 2;
    setColor(renderer, color(0, 200, 255));
    for (int y = 0; y <= mid; ++y) {
        SDL_RenderDrawLine(renderer, mid - y, y, mid + y, y);
        SDL_RenderDrawLine(renderer, mid - y, tileSize - y - 1, mid + y, tileSize - y - 1);
    }
    setColor(renderer, color(255, 255, 255));
    SDL_Rect sparkle{ mid - 1, tileSize / 3, 2, 2 };
    SDL_RenderFillRect(renderer, &sparkle);
}

void drawPlayerTile(SDL_Renderer* renderer, int tileSize) {
    fill(renderer, color(0, 0, 0));
    setColor(renderer, color(255, 235, 59));
    SDL_Rect body{ tileSize / 3, tileSize / 3, tileSize / 3, tileSize / 2 };
    SDL_RenderFillRect(renderer, &body);
    setColor(renderer, color(255, 248, 220));
    SDL_Rect head{ tileSize / 3, tileSize / 6, tileSize / 3, tileSize / 4 };
    SDL_RenderFillRect(renderer, &head);
    setColor(renderer, color(0, 0, 0));
    SDL_Rect visor{ tileSize / 3, tileSize / 6 + tileSize / 12, tileSize / 3, tileSize / 12 };
    SDL_RenderFillRect(renderer, &visor);
    setColor(renderer, color(255, 255, 255));
    SDL_Rect lamp{ tileSize / 2 - tileSize / 12, tileSize / 12, tileSize / 6, tileSize / 12 };
    SDL_RenderFillRect(renderer, &lamp);
}

void drawEnemyTile(SDL_Renderer* renderer, int tileSize) {
    fill(renderer, color(0, 0, 0));
    setColor(renderer, color(200, 40, 40));
    SDL_Rect body{ tileSize / 4, tileSize / 3, tileSize / 2, tileSize / 2 };
    SDL_RenderFillRect(renderer, &body);
    setColor(renderer, color(255, 255, 255));
    SDL_Rect eyeL{ tileSize / 3, tileSize / 2, tileSize / 10, tileSize / 10 };
    SDL_Rect eyeR{ tileSize / 2, tileSize / 2, tileSize / 10, tileSize / 10 };
    SDL_RenderFillRect(renderer, &eyeL);
    SDL_RenderFillRect(renderer, &eyeR);
}

void drawExitTile(SDL_Renderer* renderer, int tileSize) {
    fill(renderer, color(5, 15, 5));
    setColor(renderer, color(0, 200, 120));
    SDL_Rect frame{ tileSize / 6, tileSize / 6, tileSize * 2 / 3, tileSize * 2 / 3 };
    SDL_RenderDrawRect(renderer, &frame);
    setColor(renderer, color(0, 255, 170));
    SDL_Rect door{ tileSize / 3, tileSize / 4, tileSize / 3, tileSize / 2 };
    SDL_RenderFillRect(renderer, &door);
}

void drawExitStateOverlay(SDL_Renderer* renderer, const SDL_Rect& destination, bool unlocked) {
    SDL_BlendMode previousBlendMode = SDL_BLENDMODE_NONE;
    SDL_GetRenderDrawBlendMode(renderer, &previousBlendMode);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    const int inset = std::max(2, destination.w / 8);
    SDL_Rect frame{
        destination.x + inset,
        destination.y + inset,
        destination.w - inset * 2,
        destination.h - inset * 2,
    };
    SDL_Rect passage{
        frame.x + std::max(2, frame.w / 4),
        frame.y + std::max(2, frame.h / 5),
        std::max(2, frame.w / 2),
        std::max(2, frame.h * 4 / 5),
    };

    if (unlocked) {
        setColor(renderer, color(0, 255, 140, 210));
        for (int thickness = 0; thickness < 3; ++thickness) {
            SDL_Rect outline{
                frame.x + thickness,
                frame.y + thickness,
                frame.w - thickness * 2,
                frame.h - thickness * 2,
            };
            SDL_RenderDrawRect(renderer, &outline);
        }
        setColor(renderer, color(0, 20, 10, 210));
        SDL_RenderFillRect(renderer, &passage);
    } else {
        setColor(renderer, color(15, 5, 10, 185));
        SDL_RenderFillRect(renderer, &frame);
        setColor(renderer, color(180, 55, 35, 245));
        SDL_RenderFillRect(renderer, &passage);
        setColor(renderer, color(255, 175, 35, 255));
        SDL_RenderDrawRect(renderer, &frame);
        const int lockSize = std::max(3, destination.w / 6);
        SDL_Rect lock{
            destination.x + (destination.w - lockSize) / 2,
            destination.y + destination.h / 2,
            lockSize,
            lockSize,
        };
        SDL_RenderFillRect(renderer, &lock);
    }

    SDL_SetRenderDrawBlendMode(renderer, previousBlendMode);
}

void drawEmptyTile(SDL_Renderer* renderer) {
    fill(renderer, color(0, 0, 0));
}

SDL_Rect fitPlayerSprite(const SDL_Rect& destination, int sourceWidth, int sourceHeight) {
    if (sourceWidth <= 0 || sourceHeight <= 0) {
        return destination;
    }
    SDL_Rect fitted = destination;
    fitted.w = std::max(1, destination.h * sourceWidth / sourceHeight);
    fitted.w = std::min(fitted.w, destination.w);
    fitted.x += (destination.w - fitted.w) / 2;
    return fitted;
}

} // namespace

Renderer::Renderer(
    SDL_Renderer* renderer,
    int tileSize,
    int originY,
    const std::filesystem::path& assetsPath)
    : m_renderer(renderer), m_tileSize(tileSize), m_originY(originY) {
    LOG_T("Renderer ctor tileSize=%d originY=%d assets=%s", tileSize, originY, assetsPath.string().c_str());
    try {
        const auto spriteManifest = assetsPath / "sprites" / "index.json";
        const auto indexPath = assetsPath / "tiles_index.json";
        const auto texturePath = assetsPath / "tiles.png";
        const bool filesLoaded = loadSpriteFiles(spriteManifest);
        if (!filesLoaded && std::filesystem::exists(indexPath) && std::filesystem::exists(texturePath)) {
            m_spriteSheet = std::make_unique<SpriteSheet>(renderer, indexPath, texturePath);
            if (!m_spriteSheet->valid()) {
                m_spriteSheet.reset();
            } else {
                Logger::info("Sprite sheet loaded from " + texturePath.string(), __func__);
            }
        } else if (filesLoaded) {
            Logger::info("Sprite files loaded from " + spriteManifest.string(), __func__);
        } else {
            Logger::warn("Sprite assets missing; using fallback textures.", __func__);
        }
    } catch (const std::exception& e) {
        Logger::warn(std::string("Failed to initialize sprite sheet: ") + e.what(), __func__);
    }
    if (!hasSprites()) {
        buildFallbackTextures();
    }
}

Renderer::~Renderer() {
    LOG_T("Renderer dtor destroying %zu textures", m_tileTextures.size());
    for (SDL_Texture* texture : m_ownedSpriteTextures) {
        if (texture) {
            SDL_DestroyTexture(texture);
        }
    }
    for (SDL_Texture* texture : m_tileTextures) {
        if (texture) {
            SDL_DestroyTexture(texture);
        }
    }
}

void Renderer::draw(const Grid& grid) const {
    LOG_T("Renderer::draw grid=%dx%d sprites=%s", grid.width(), grid.height(), hasSprites() ? "yes" : "no");
    if (!m_renderer) {
        return;
    }
    if (hasSprites()) {
        drawWithSprites(grid);
    } else {
        drawFallback(grid);
    }
}

void Renderer::drawTestPattern(int windowWidth, int windowHeight) const {
    LOG_T("Renderer::drawTestPattern window=%dx%d", windowWidth, windowHeight);
    if (!m_renderer) {
        return;
    }
    SDL_SetRenderDrawColor(m_renderer, 10, 10, 10, 255);
    SDL_RenderClear(m_renderer);
    if (!hasSprites()) {
        Logger::warn("Sprite sheet unavailable; falling back to procedural tiles for test mode.", __func__);
        return;
    }

    std::vector<std::pair<std::string, SDL_Rect>> frames;
    if (m_useSpriteFiles) {
        frames.reserve(m_spriteFileFrames.size());
        for (const auto& entry : m_spriteFileFrames) {
            if (entry.second.empty()) {
                continue;
            }
            int w = m_tileSize;
            int h = m_tileSize;
            SDL_QueryTexture(entry.second.front(), nullptr, nullptr, &w, &h);
            frames.emplace_back(entry.first, SDL_Rect{ 0, 0, w, h });
        }
    } else if (m_spriteSheet) {
        frames.reserve(m_spriteSheet->sprites().size());
        for (const auto& entry : m_spriteSheet->sprites()) {
            for (const SDL_Rect& rect : entry.second) {
                frames.emplace_back(entry.first, rect);
            }
        }
    }
    std::sort(frames.begin(), frames.end(),
        [](const auto& a, const auto& b) { return a.first < b.first; });

    if (!m_loggedTestPattern) {
        Logger::debug("Rendering tile test pattern for " + std::to_string(frames.size()) + " frames", __func__);
        m_loggedTestPattern = true;
    }

    const int columns = std::max(1, windowWidth / m_tileSize);
    for (std::size_t i = 0; i < frames.size(); ++i) {
        const int col = static_cast<int>(i % columns);
        const int row = static_cast<int>(i / columns);
        SDL_Rect dest{
            col * m_tileSize,
            row * m_tileSize + m_originY,
            m_tileSize,
            m_tileSize };
        if (m_useSpriteFiles) {
            const auto it = m_spriteFileFrames.find(frames[i].first);
            if (it != m_spriteFileFrames.end() && !it->second.empty()) {
                SDL_RenderCopy(m_renderer, it->second.front(), nullptr, &dest);
            }
        } else if (m_spriteSheet) {
            SDL_RenderCopy(m_renderer, m_spriteSheet->texture(), &frames[i].second, &dest);
        }
    }
}

void Renderer::drawWithSprites(const Grid& grid) const {
    LOG_T("Renderer::drawWithSprites grid=%dx%d", grid.width(), grid.height());
    for (int y = 0; y < grid.height(); ++y) {
        for (int x = 0; x < grid.width(); ++x) {
            const Cell& cell = grid.at(x, y);
            SDL_Rect destination{
                x * m_tileSize,
                y * m_tileSize + m_originY,
                m_tileSize,
                m_tileSize };
            SDL_RendererFlip flip = SDL_FLIP_NONE;
            if (cell.type == CellType::Player && !cell.playerFacingRight) {
                flip = SDL_FLIP_HORIZONTAL;
            }
            const std::string& spriteId = spriteIdForCell(cell);
            bool drawn = false;
            if (m_useSpriteFiles) {
                const auto filesIt = m_spriteFileFrames.find(spriteId);
                if (filesIt != m_spriteFileFrames.end() && !filesIt->second.empty()) {
                    const Uint32 duration = m_spriteFrameDurations.count(spriteId) ?
                        m_spriteFrameDurations.at(spriteId) :
                        m_defaultFrameDurationMs;
                    const Uint32 ticks = SDL_GetTicks();
                    const std::size_t frameIndex = (duration > 0 && filesIt->second.size() > 1) ?
                        (ticks / duration) % filesIt->second.size() :
                        0;
                    SDL_Texture* tex = filesIt->second[frameIndex];
                    if (tex) {
                        SDL_Rect spriteDestination = destination;
                        if (cell.type == CellType::Player) {
                            int sourceWidth = 0;
                            int sourceHeight = 0;
                            SDL_QueryTexture(tex, nullptr, nullptr, &sourceWidth, &sourceHeight);
                            spriteDestination = fitPlayerSprite(
                                destination, sourceWidth, sourceHeight);
                        }
                        if (flip == SDL_FLIP_NONE) {
                            SDL_RenderCopy(m_renderer, tex, nullptr, &spriteDestination);
                        } else {
                            SDL_RenderCopyEx(
                                m_renderer, tex, nullptr, &spriteDestination, 0.0, nullptr, flip);
                        }
                        drawn = true;
                    }
                }
            } else if (const SDL_Rect* frame = sheetFrameForId(spriteId, SDL_GetTicks())) {
                const SDL_Rect spriteDestination = cell.type == CellType::Player ?
                    fitPlayerSprite(destination, frame->w, frame->h) :
                    destination;
                if (flip == SDL_FLIP_NONE) {
                    SDL_RenderCopy(m_renderer, m_spriteSheet->texture(), frame, &spriteDestination);
                } else {
                    SDL_RenderCopyEx(
                        m_renderer,
                        m_spriteSheet->texture(),
                        frame,
                        &spriteDestination,
                        0.0,
                        nullptr,
                        flip);
                }
                drawn = true;
            }

            if (!drawn) {
                SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 255);
                SDL_RenderFillRect(m_renderer, &destination);
            }
            if (cell.type == CellType::Exit) {
                drawExitStateOverlay(m_renderer, destination, cell.exitUnlocked);
            }
        }
    }
}

void Renderer::drawFallback(const Grid& grid) const {
    LOG_T("Renderer::drawFallback grid=%dx%d", grid.width(), grid.height());
    for (int y = 0; y < grid.height(); ++y) {
        for (int x = 0; x < grid.width(); ++x) {
            const Cell& cell = grid.at(x, y);
            const int index = static_cast<int>(cell.type);
            SDL_Texture* texture = (index >= 0 && index < static_cast<int>(m_tileTextures.size())) ?
                m_tileTextures[index] :
                nullptr;
            SDL_Rect rect{
                x * m_tileSize,
                y * m_tileSize + m_originY,
                m_tileSize,
                m_tileSize };
            SDL_RendererFlip flip = SDL_FLIP_NONE;
            if (cell.type == CellType::Player && !cell.playerFacingRight) {
                flip = SDL_FLIP_HORIZONTAL;
            }
            if (texture) {
                if (flip == SDL_FLIP_NONE) {
                    SDL_RenderCopy(m_renderer, texture, nullptr, &rect);
                } else {
                    SDL_RenderCopyEx(m_renderer, texture, nullptr, &rect, 0.0, nullptr, flip);
                }
            } else {
                SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 255);
                SDL_RenderFillRect(m_renderer, &rect);
            }
            if (cell.type == CellType::Exit) {
                drawExitStateOverlay(m_renderer, rect, cell.exitUnlocked);
            }
        }
    }
}

void Renderer::buildFallbackTextures() {
    LOG_T("Renderer::buildFallbackTextures cells=%d", static_cast<int>(CellType::Exit) + 1);
    const int cellCount = static_cast<int>(CellType::Exit) + 1;
    m_tileTextures.resize(cellCount, nullptr);
    for (int i = 0; i < cellCount; ++i) {
        m_tileTextures[i] = createTileTexture(static_cast<CellType>(i));
        if (!m_tileTextures[i]) {
            Logger::warn("Failed to build fallback texture for cell " + std::to_string(i), __func__);
        }
    }
}

SDL_Texture* Renderer::loadTextureFromPng(const std::filesystem::path& path) {
    std::vector<unsigned char> pixels;
    int width = 0;
    int height = 0;
    int pitch = 0;
    if (!gfx::loadPngRGBA(path, pixels, width, height, pitch)) {
        Logger::warn("Failed to load sprite: " + path.string(), __func__);
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
    SDL_Texture* texture = SDL_CreateTextureFromSurface(m_renderer, surface);
    SDL_FreeSurface(surface);
    if (!texture) {
        Logger::warn(std::string("SDL_CreateTextureFromSurface failed: ") + SDL_GetError(), __func__);
        return nullptr;
    }
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    return texture;
}

bool Renderer::loadSpriteFiles(const std::filesystem::path& manifestPath) {
    if (!std::filesystem::exists(manifestPath)) {
        return false;
    }
    try {
        std::ifstream input(manifestPath);
        nlohmann::json root;
        input >> root;
        const bool enabled = root.value("enabled", true);
        if (!enabled) {
            return false;
        }
        m_defaultFrameDurationMs = root.value("defaultFrameDurationMs", 150);
        auto spritesIt = root.find("sprites");
        if (spritesIt == root.end() || !spritesIt->is_object()) {
            return false;
        }
        const std::filesystem::path baseDir = manifestPath.parent_path();
        for (const auto& entry : spritesIt->items()) {
            const std::string& id = entry.key();
            const auto& obj = entry.value();
            std::vector<std::filesystem::path> files;
            Uint32 frameDuration = obj.value("frameDurationMs", m_defaultFrameDurationMs);
            if (obj.contains("file") && obj["file"].is_string()) {
                files.push_back(obj["file"].get<std::string>());
            } else if (obj.contains("frames") && obj["frames"].is_array()) {
                for (const auto& frame : obj["frames"]) {
                    if (frame.is_string()) {
                        files.push_back(frame.get<std::string>());
                    }
                }
            }
            if (files.empty()) {
                Logger::warn("Sprite entry '" + id + "' missing 'file' or 'frames'", __func__);
                continue;
            }
            std::vector<SDL_Texture*> textures;
            for (auto path : files) {
                if (!path.is_absolute()) {
                    path = baseDir / path;
                }
                SDL_Texture* tex = loadTextureFromPng(path);
                if (tex) {
                    m_ownedSpriteTextures.push_back(tex);
                    textures.push_back(tex);
                } else {
                    Logger::warn("Failed to load sprite frame: " + path.string(), __func__);
                }
            }
            if (!textures.empty()) {
                m_spriteFileFrames[id] = std::move(textures);
                m_spriteFrameDurations[id] = frameDuration;
            }
        }
        m_useSpriteFiles = !m_spriteFileFrames.empty();
        return m_useSpriteFiles;
    } catch (const std::exception& e) {
        Logger::warn(std::string("Failed to load sprite manifest: ") + e.what(), __func__);
        return false;
    }
}

const std::string& Renderer::spriteIdForCell(const Cell& cell) const {
    static const std::string kEmpty = "EMPTY";
    static const std::string kDirt = "DIRT";
    static const std::string kWallSolid = "WALL_SOLID";
    static const std::string kWallDestruct = "WALL_DESTRUCT";
    static const std::string kRockStable = "ROCK_STABLE";
    static const std::string kRockFalling = "ROCK_FALLING";
    static const std::string kDiamondStable = "DIAMOND";
    static const std::string kDiamondFalling = "DIAMOND_FALLING";
    static const std::string kPlayerIdle = "PLAYER";
    static const std::string kEnemy = "ENEMY";
    static const std::string kExit = "EXIT";

    switch (cell.type) {
    case CellType::Empty:
        return kEmpty;
    case CellType::Dirt:
        return kDirt;
    case CellType::Wall:
        return kWallSolid;
    case CellType::WallDestructible:
        return kWallDestruct;
    case CellType::Rock:
        return cell.falling ? kRockFalling : kRockStable;
    case CellType::Diamond:
        return cell.falling ? kDiamondFalling : kDiamondStable;
    case CellType::Player:
        return kPlayerIdle;
    case CellType::Enemy:
        return kEnemy;
    case CellType::Exit:
        return kExit;
    default:
        return kEmpty;
    }
}

const SDL_Rect* Renderer::sheetFrameForId(const std::string& id) const {
    if (!m_spriteSheet) {
        return nullptr;
    }
    const auto& sprites = m_spriteSheet->sprites();
    const auto it = sprites.find(id);
    if (it == sprites.end() || it->second.empty()) {
        return nullptr;
    }
    return &it->second.front();
}

const SDL_Rect* Renderer::sheetFrameForId(const std::string& id, Uint32 ticks) const {
    if (!m_spriteSheet) {
        return nullptr;
    }
    const auto& sprites = m_spriteSheet->sprites();
    const auto it = sprites.find(id);
    if (it == sprites.end() || it->second.empty()) {
        return nullptr;
    }
    if (it->second.size() == 1) {
        return &it->second.front();
    }
    const std::size_t frameIndex = (ticks / 150) % it->second.size();
    return &it->second[frameIndex];
}

SDL_Texture* Renderer::createTileTexture(CellType type) {
    LOG_T("Renderer::createTileTexture type=%d", static_cast<int>(type));
    if (!m_renderer) {
        return nullptr;
    }
    SDL_Texture* texture = SDL_CreateTexture(
        m_renderer,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,
        m_tileSize,
        m_tileSize);
    if (!texture) {
        Logger::warn(std::string("Failed to create tile texture: ") + SDL_GetError(), __func__);
        return nullptr;
    }
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);

    SDL_Texture* previousTarget = SDL_GetRenderTarget(m_renderer);
    if (SDL_SetRenderTarget(m_renderer, texture) != 0) {
        Logger::warn(std::string("SDL_SetRenderTarget failed: ") + SDL_GetError(), __func__);
        SDL_DestroyTexture(texture);
        SDL_SetRenderTarget(m_renderer, previousTarget);
        return nullptr;
    }

    switch (type) {
    case CellType::Wall:
        drawWallTile(m_renderer, m_tileSize);
        break;
    case CellType::WallDestructible:
        drawDestructibleWallTile(m_renderer, m_tileSize);
        break;
    case CellType::Dirt:
        drawDirtTile(m_renderer, m_tileSize);
        break;
    case CellType::Rock:
        drawRockTile(m_renderer, m_tileSize);
        break;
    case CellType::Diamond:
        drawDiamondTile(m_renderer, m_tileSize);
        break;
    case CellType::Player:
        drawPlayerTile(m_renderer, m_tileSize);
        break;
    case CellType::Enemy:
        drawEnemyTile(m_renderer, m_tileSize);
        break;
    case CellType::Exit:
        drawExitTile(m_renderer, m_tileSize);
        break;
    case CellType::Empty:
    default:
        drawEmptyTile(m_renderer);
        break;
    }

    SDL_SetRenderTarget(m_renderer, previousTarget);
    return texture;
}
