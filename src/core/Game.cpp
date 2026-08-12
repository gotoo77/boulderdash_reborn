#include "Game.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "../audio/Audio.h"
#include "../systems/EnemySystem.h"
#include "../systems/GravitySystem.h"
#include "../systems/PlayerSystem.h"
#include "../util/Logger.h"

namespace {

constexpr int kDefaultWidth = 40;
constexpr int kDefaultHeight = 22;

CellType cellFromChar(char c) {
    switch (c) {
    case '#':
        return CellType::Wall;
    case 'b':
    case 'B':
        return CellType::WallDestructible;
    case '.':
        return CellType::Empty;
    case '*':
        return CellType::Rock;
    case 'd':
    case 'D':
        return CellType::Dirt;
    case 'o':
    case 'O':
        return CellType::Diamond;
    case 'P':
        return CellType::Player;
    case 'E':
        return CellType::Exit;
    case 'X':
    case 'x':
        return CellType::Enemy;
    default:
        return CellType::Dirt;
    }
}

Grid buildFallbackGrid() {
    LOG_T("Building fallback grid");
    Grid grid(kDefaultWidth, kDefaultHeight);
    for (int y = 0; y < grid.height(); ++y) {
        for (int x = 0; x < grid.width(); ++x) {
            Cell& cell = grid.at(x, y);
            if (y == 0 || x == 0 || y == grid.height() - 1 || x == grid.width() - 1) {
                cell.type = CellType::Wall;
            } else {
                cell.type = CellType::Dirt;
            }
        }
    }
    grid.at(1, 1).type = CellType::Player;
    grid.at(grid.width() - 2, grid.height() - 2).type = CellType::Exit;
    return grid;
}

int countDiamonds(const Grid& grid) {
    LOG_T("Counting diamonds in grid %dx%d", grid.width(), grid.height());
    int total = 0;
    for (int y = 0; y < grid.height(); ++y) {
        for (int x = 0; x < grid.width(); ++x) {
            if (grid.at(x, y).type == CellType::Diamond) {
                ++total;
            }
        }
    }
    return total;
}

void updateExitState(Grid& grid, bool unlocked) {
    for (int y = 0; y < grid.height(); ++y) {
        for (int x = 0; x < grid.width(); ++x) {
            Cell& cell = grid.at(x, y);
            if (cell.type == CellType::Exit) {
                cell.exitUnlocked = unlocked;
            }
        }
    }
}

std::filesystem::path defaultLevelPath() {
    LOG_T("Resolving default level path");
#ifdef ASSETS_DIR
    return std::filesystem::path(ASSETS_DIR) / "levels/level01.txt";
#else
    return std::filesystem::path("assets/levels/level01.txt");
#endif
}

Grid loadLevel(const std::filesystem::path& path) {
    LOG_T("Loading level file %s", path.string().c_str());
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Unable to open level file: " + path.string());
    }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            throw std::runtime_error("Level contains an empty row: " + path.string());
        }
        lines.push_back(line);
    }

    if (lines.empty()) {
        throw std::runtime_error("Level file is empty: " + path.string());
    }

    const int width = static_cast<int>(lines.front().size());
    const int height = static_cast<int>(lines.size());
    if (width < 3 || height < 3) {
        throw std::runtime_error("Level is too small: " + path.string());
    }
    for (const auto& row : lines) {
        if (static_cast<int>(row.size()) != width) {
            throw std::runtime_error("Level has inconsistent row widths: " + path.string());
        }
    }
    Grid grid(width, height);

    int playerCount = 0;
    int exitCount = 0;
    for (int y = 0; y < height; ++y) {
        const auto& row = lines[y];
        for (int x = 0; x < width; ++x) {
            Cell& cell = grid.at(x, y);
            cell.type = cellFromChar(row[x]);
            if (cell.type == CellType::Player) {
                ++playerCount;
            } else if (cell.type == CellType::Exit) {
                ++exitCount;
            }
        }
    }

    if (playerCount != 1 || exitCount != 1) {
        throw std::runtime_error(
            "Invalid level " + path.string() + ": found " +
            std::to_string(playerCount) + " player(s) and " +
            std::to_string(exitCount) +
            " exit(s); exactly one of each is required");
    }

    return grid;
}

} // namespace

bool Game::anyRockFalling() const {
    for (int y = 0; y < m_grid.height(); ++y) {
        for (int x = 0; x < m_grid.width(); ++x) {
            const Cell& cell = m_grid.at(x, y);
            if (cell.type == CellType::Rock && cell.falling) {
                return true;
            }
        }
    }
    return false;
}

Game::Game(std::vector<std::filesystem::path> levelPaths, GameRules rules)
    : m_grid(buildFallbackGrid()),
      m_levelPaths(std::move(levelPaths)),
      m_rules(rules),
      m_remainingLives(rules.maxLives) {
    LOG_T("Game constructed with %zu level paths", m_levelPaths.size());
    if (m_levelPaths.empty()) {
        m_levelPaths.push_back(defaultLevelPath());
    }
    loadLevelAt(0);
}

void Game::update(std::uint32_t deltaMs) {
    LOG_T("Game::update deltaMs=%u", deltaMs);
    if (m_paused || m_exitReached || m_levelFailed) {
        return;
    }

    const int delta = static_cast<int>(deltaMs);
    const bool rocksWereFalling = m_rocksFalling;

    if (m_respawnPending) {
        m_respawnElapsedMs += delta;
        const int respawnDelay = std::max(0, m_rules.respawnDelayMs);
        if (m_respawnElapsedMs >= respawnDelay) {
            m_respawnPending = false;
            m_respawnElapsedMs = 0;
            restartLevel();
        }
        return;
    }

    if (m_rules.timeLimitMs > 0) {
        m_elapsedMs = std::min(m_elapsedMs + delta, m_rules.timeLimitMs);
        const int remainingMs = timeRemainingMs();
        if (remainingMs > 0 && remainingMs <= TimeWarningThresholdMs) {
            const int warningSecond = (remainingMs + 999) / 1000;
            if (warningSecond != m_lastTimeWarningSecond) {
                Audio::play(SoundId::TimeWarning);
                m_lastTimeWarningSecond = warningSecond;
            }
        }
        if (m_elapsedMs >= m_rules.timeLimitMs) {
            m_levelFailed = true;
            Logger::warn("Time limit reached for level " + currentLevelName(), __func__);
            return;
        }
    }

    PlayerEvents events;
    const bool exitWasUnlocked = exitUnlocked();
    auto explodeAround = [&](int centerX, int centerY) {
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                const int px = centerX + dx;
                const int py = centerY + dy;
                if (!m_grid.inBounds(px, py)) {
                    continue;
                }
                Cell& target = m_grid.at(px, py);
                if (target.type == CellType::Wall || target.type == CellType::Exit) {
                    continue;
                }
                target = Cell{};
            }
        }
    };
    PlayerSystem::update(m_grid, m_pendingMove, events, exitUnlocked());
    auto queueRespawn = [&]() {
        if (m_rules.respawnDelayMs <= 0) {
            restartLevel();
        } else {
            m_respawnPending = true;
            m_respawnElapsedMs = 0;
        }
    };
    auto handlePlayerDeath = [&]() {
        if (!events.playerDied) {
            return false;
        }
        if (events.playerExplosion) {
            explodeAround(events.explosionX, events.explosionY);
        }
        Audio::play(SoundId::Death);
        std::string reason = "Player died";
        if (events.deathCause == PlayerDeathCause::Enemy) {
            reason += " to an enemy";
        } else if (events.deathCause == PlayerDeathCause::Rock) {
            reason += " crushed by a boulder";
        }
        if (m_remainingLives > 1) {
            --m_remainingLives;
            Logger::warn(
                reason + " on level " + currentLevelName() + ". Lives remaining: " + std::to_string(m_remainingLives),
                __func__);
            queueRespawn();
        } else {
            m_remainingLives = 0;
            m_levelFailed = true;
            Logger::warn(reason + " on level " + currentLevelName(), __func__);
        }
        return true;
    };
    if (handlePlayerDeath()) {
        return;
    }
    if (events.rockPushed) {
        Audio::play(SoundId::RockFall);
    } else if (events.diamondsCollected > 0) {
        Audio::play(SoundId::Diamond);
    } else if (events.dug) {
        Audio::play(SoundId::Dig);
    } else if (events.walked) {
        Audio::play(SoundId::Walk);
    }
    if (events.diamondsCollected > 0) {
        m_collectedDiamonds += events.diamondsCollected;
        if (m_collectedDiamonds > m_totalDiamonds) {
            m_collectedDiamonds = m_totalDiamonds;
        }
        updateExitState(m_grid, exitUnlocked());
        if (!exitWasUnlocked && exitUnlocked()) {
            Audio::play(SoundId::ExitUnlock);
        }
        m_levelScore += events.diamondsCollected * m_rules.diamondValue;
        Logger::info("Diamonds collected: " + std::to_string(m_collectedDiamonds) + "/" +
            std::to_string(m_totalDiamonds),
            __func__);
    }
    if (events.reachedExit && exitUnlocked()) {
        m_exitReached = true;
        const int remainingSeconds = timeRemainingMs() / 1000;
        m_levelScore += m_rules.exitBonus;
        m_levelScore += remainingSeconds * m_rules.timeBonusPerSecond;
        m_totalScore += m_levelScore;
        m_levelScoreBanked = true;
        Logger::info("Exit reached! Level complete.", __func__);
        return;
    }

    auto processEnemyStep = [&]() -> bool {
        EnemySystem::update(m_grid, events);
        return handlePlayerDeath();
    };

    if (m_rules.enemyMoveIntervalMs <= 0) {
        if (processEnemyStep()) {
            return;
        }
    } else {
        m_enemyMoveAccumulator += delta;
        while (m_enemyMoveAccumulator >= m_rules.enemyMoveIntervalMs) {
            m_enemyMoveAccumulator -= m_rules.enemyMoveIntervalMs;
            if (processEnemyStep()) {
                return;
            }
        }
    }

    bool gravityApplied = false;
    auto runGravityStep = [&]() {
        gravityApplied = true;
        GravitySystem::update(m_grid, events);
    };
    if (m_rules.gravityStepMs <= 0) {
        runGravityStep();
    } else {
        m_gravityAccumulator += delta;
        while (m_gravityAccumulator >= m_rules.gravityStepMs && !events.playerDied) {
            m_gravityAccumulator -= m_rules.gravityStepMs;
            runGravityStep();
        }
    }
    if (gravityApplied) {
        const bool rocksFallingNow = anyRockFalling();
        if (events.rockFallStarted && !rocksWereFalling) {
            Audio::play(SoundId::RockFall);
        }
        if (rocksWereFalling && !rocksFallingNow && events.rockFallLanded) {
            Audio::play(SoundId::RockFall);
        }
        if (events.diamondFallStarted) {
            Audio::play(SoundId::DiamondFall);
        }
        if (events.enemyExploded) {
            Audio::play(SoundId::Explosion);
        }
        m_rocksFalling = rocksFallingNow;
    }
    if (handlePlayerDeath()) {
        return;
    }
}

void Game::queueMove(Direction dir) {
    LOG_T("Game::queueMove dir=%d", static_cast<int>(dir));
    if (m_paused) {
        return;
    }
    m_pendingMove = dir;
}

void Game::setPaused(bool paused) {
    m_paused = paused;
    if (paused) {
        m_pendingMove.reset();
    }
}

int Game::timeRemainingMs() const {
    if (m_rules.timeLimitMs <= 0) {
        return 0;
    }
    return std::max(0, m_rules.timeLimitMs - m_elapsedMs);
}

int Game::currentLevelNumber() const {
    return static_cast<int>(m_currentLevelIndex + 1);
}

std::string Game::currentLevelName() const {
    if (m_levelPaths.empty()) {
        return "N/A";
    }
    return m_levelPaths[m_currentLevelIndex].filename().string();
}

bool Game::advanceToNextLevel() {
    LOG_D("Game::advanceToNextLevel");
    if (!m_exitReached) {
        return true;
    }
    const std::size_t next = m_currentLevelIndex + 1;
    if (next >= m_levelPaths.size()) {
        Logger::info("All levels completed!", __func__);
        return false;
    }
    return loadLevelAt(next);
}

bool Game::restartLevel() {
    LOG_D("Game::restartLevel");
    return loadLevelAt(m_currentLevelIndex);
}

bool Game::jumpToLevel(std::size_t index) {
    LOG_D("Game::jumpToLevel index=%zu", index);
    return loadLevelAt(index);
}

bool Game::loadLevelAt(std::size_t index) {
    LOG_D("Game::loadLevelAt index=%zu", index);
    if (index >= m_levelPaths.size()) {
        return false;
    }
    const auto& path = m_levelPaths[index];
    m_grid = loadLevel(path);
    m_currentLevelIndex = index;
    m_totalDiamonds = countDiamonds(m_grid);
    m_collectedDiamonds = 0;
    updateExitState(m_grid, exitUnlocked());
    m_exitReached = false;
    m_levelFailed = false;
    m_pendingMove.reset();
    m_elapsedMs = 0;
    m_levelScore = 0;
    m_levelScoreBanked = false;
    m_enemyMoveAccumulator = 0;
    m_gravityAccumulator = 0;
    m_respawnPending = false;
    m_respawnElapsedMs = 0;
    m_rocksFalling = anyRockFalling();
    m_lastTimeWarningSecond = -1;
    m_paused = false;
    Logger::info("Loaded level " + std::to_string(currentLevelNumber()) + "/" +
        std::to_string(levelCount()) + " - " + currentLevelName(),
        __func__);
    Logger::info("Diamonds to collect: " + std::to_string(m_totalDiamonds), __func__);
    return true;
}
