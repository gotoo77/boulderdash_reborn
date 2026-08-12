#pragma once

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "core/Game.h"
#include "core/Grid.h"

inline void expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

inline int countEvent(const std::vector<GameEvent>& events, GameEvent expected) {
    return static_cast<int>(std::count(events.begin(), events.end(), expected));
}

class TemporaryLevels {
public:
    TemporaryLevels() {
        const auto identifier = std::chrono::steady_clock::now().time_since_epoch().count();
        m_directory = std::filesystem::temp_directory_path() /
            ("boulderdash-tests-" + std::to_string(identifier));
        std::filesystem::create_directories(m_directory);
    }

    ~TemporaryLevels() {
        std::error_code error;
        std::filesystem::remove_all(m_directory, error);
    }

    std::filesystem::path write(const std::string& name, const std::string& content) const {
        const auto path = m_directory / name;
        std::ofstream output(path);
        output << content;
        if (!output) {
            throw std::runtime_error("unable to write temporary level: " + path.string());
        }
        return path;
    }

private:
    std::filesystem::path m_directory;
};

inline int countCells(const Grid& grid, CellType type) {
    int count = 0;
    for (int y = 0; y < grid.height(); ++y) {
        for (int x = 0; x < grid.width(); ++x) {
            count += grid.at(x, y).type == type ? 1 : 0;
        }
    }
    return count;
}

inline bool exitCellIsUnlocked(const Grid& grid) {
    for (int y = 0; y < grid.height(); ++y) {
        for (int x = 0; x < grid.width(); ++x) {
            const Cell& cell = grid.at(x, y);
            if (cell.type == CellType::Exit) {
                return cell.exitUnlocked;
            }
        }
    }
    throw std::runtime_error("level must contain an exit cell");
}

inline GameRules deterministicRules() {
    GameRules rules;
    rules.timeLimitMs = 10000;
    rules.diamondValue = 10;
    rules.exitBonus = 100;
    rules.timeBonusPerSecond = 5;
    rules.enemyMoveIntervalMs = 100000;
    rules.gravityStepMs = 100000;
    rules.maxLives = 3;
    rules.respawnDelayMs = 100;
    return rules;
}

using TestCase = std::pair<std::string, std::function<void()>>;
