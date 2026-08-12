#pragma once

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "Grid.h"
#include "Types.h"

struct GameRules {
    int timeLimitMs = 120000;
    int diamondValue = 10;
    int exitBonus = 100;
    int timeBonusPerSecond = 5;
    int enemyMoveIntervalMs = 200;
    int gravityStepMs = 66;
    int maxLives = 3;
    int respawnDelayMs = 5000;
};

class Game {
public:
    static constexpr int TimeWarningThresholdMs = 15000;

    Game(std::vector<std::filesystem::path> levelPaths, GameRules rules);

    void update(std::uint32_t deltaMs);
    void queueMove(Direction dir);
    void setPaused(bool paused);
    bool paused() const { return m_paused; }

    Grid& grid() { return m_grid; }
    const Grid& grid() const { return m_grid; }

    int totalDiamonds() const { return m_totalDiamonds; }
    int collectedDiamonds() const { return m_collectedDiamonds; }
    bool exitReached() const { return m_exitReached; }
    bool exitUnlocked() const { return m_totalDiamonds == 0 || m_collectedDiamonds >= m_totalDiamonds; }
    bool levelComplete() const { return m_exitReached; }
    bool levelFailed() const { return m_levelFailed; }

    int levelScore() const { return m_levelScore; }
    int totalScore() const { return m_totalScore + (m_levelScoreBanked ? 0 : m_levelScore); }
    int timeRemainingMs() const;
    int elapsedMs() const { return m_elapsedMs; }
    int currentLevelNumber() const;
    int levelCount() const { return static_cast<int>(m_levelPaths.size()); }
    std::string currentLevelName() const;
    int remainingLives() const { return m_remainingLives; }
    int maxLives() const { return m_rules.maxLives; }

    bool advanceToNextLevel();
    bool restartLevel();
    bool jumpToLevel(std::size_t index);

private:
    bool anyRockFalling() const;
    bool loadLevelAt(std::size_t index);

    Grid m_grid;
    std::vector<std::filesystem::path> m_levelPaths;
    std::size_t m_currentLevelIndex = 0;
    std::optional<Direction> m_pendingMove;
    GameRules m_rules;
    int m_totalDiamonds = 0;
    int m_collectedDiamonds = 0;
    bool m_exitReached = false;
    bool m_levelFailed = false;
    int m_elapsedMs = 0;
    int m_levelScore = 0;
    int m_totalScore = 0;
    bool m_levelScoreBanked = false;
    int m_enemyMoveAccumulator = 0;
    int m_gravityAccumulator = 0;
    int m_remainingLives = 0;
    bool m_respawnPending = false;
    int m_respawnElapsedMs = 0;
    bool m_rocksFalling = false;
    int m_lastTimeWarningSecond = -1;
    bool m_paused = false;
};
