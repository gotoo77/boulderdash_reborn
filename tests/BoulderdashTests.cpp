#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "core/Grid.h"
#include "menu/MenuLoader.h"
#include "menu/MenuManager.h"
#include "menu/MockMenuRenderer.h"
#include "systems/EnemySystem.h"
#include "systems/GravitySystem.h"
#include "systems/PlayerSystem.h"
#include "util/Logger.h"

namespace {

void expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void testPlayerDigsAndCollects() {
    Grid grid(3, 1);
    grid.at(0, 0).type = CellType::Player;
    grid.at(1, 0).type = CellType::Dirt;

    std::optional<Direction> move = Direction::Right;
    PlayerEvents events;
    PlayerSystem::update(grid, move, events, false);

    expect(grid.at(0, 0).type == CellType::Empty, "player origin must be cleared");
    expect(grid.at(1, 0).type == CellType::Player, "player must enter dirt");
    expect(events.dug, "digging must emit an event");
    expect(!move.has_value(), "player input must be consumed");

    grid.at(2, 0).type = CellType::Diamond;
    move = Direction::Right;
    PlayerSystem::update(grid, move, events, false);
    expect(grid.at(2, 0).type == CellType::Player, "player must collect a diamond");
    expect(events.diamondsCollected == 1, "diamond collection must be counted");
}

void testPlayerPushesRock() {
    Grid grid(4, 1);
    grid.at(0, 0).type = CellType::Player;
    grid.at(1, 0).type = CellType::Rock;

    std::optional<Direction> move = Direction::Right;
    PlayerEvents events;
    PlayerSystem::update(grid, move, events, false);

    expect(grid.at(1, 0).type == CellType::Player, "player must occupy the rock origin");
    expect(grid.at(2, 0).type == CellType::Rock, "rock must move into the empty cell");
    expect(events.rockPushed, "rock push must emit an event");
}

void testGravityHasDeterministicSteps() {
    Grid grid(3, 4);
    grid.at(1, 0).type = CellType::Rock;
    PlayerEvents events;

    GravitySystem::update(grid, events);
    expect(grid.at(1, 0).type == CellType::Rock, "rock must be armed before its first fall");
    expect(grid.at(1, 0).falling, "unsupported rock must enter falling state");
    expect(events.rockFallStarted, "fall start must emit an event");

    events = PlayerEvents{};
    GravitySystem::update(grid, events);
    expect(grid.at(1, 0).type == CellType::Empty, "falling rock origin must be cleared");
    expect(grid.at(1, 1).type == CellType::Rock, "falling rock must move by one cell per step");
    expect(events.rockFell, "rock movement must emit an event");
}

void testEnemyReversesAtBoundary() {
    Grid grid(3, 1);
    grid.at(0, 0).type = CellType::Enemy;
    grid.at(0, 0).enemyMovingRight = false;
    PlayerEvents events;

    EnemySystem::update(grid, events);

    expect(grid.at(0, 0).type == CellType::Empty, "enemy origin must be cleared");
    expect(grid.at(1, 0).type == CellType::Enemy, "enemy must reverse away from a boundary");
    expect(grid.at(1, 0).enemyMovingRight, "enemy direction must reflect the reversal");
}

void testMenuNavigationSkipsDisabledItems() {
    const std::string json = R"({
        "id": "test",
        "items": [
            { "label": "Disabled", "action": "disabled", "enabled": false },
            { "label": "Play", "action": "play" }
        ]
    })";
    menu::MenuManager manager;
    manager.setMenu(menu::MenuLoader::loadFromString(json));
    menu::MockInputProvider input;

    input.setDown(true);
    manager.update(input);
    expect(manager.selectedIndex() == 1, "navigation must skip disabled menu entries");

    input.setDown(false);
    manager.update(input);
    input.setValidate(true);
    manager.update(input);
    expect(manager.consumeAction() == std::optional<std::string>("play"), "validation must emit the selected action");
}

void testLevelFilesAreStructurallyValid() {
    const std::filesystem::path levelDirectory =
        std::filesystem::path(TEST_ASSET_DIR) / "levels";
    std::size_t levelCount = 0;

    for (const auto& entry : std::filesystem::directory_iterator(levelDirectory)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".txt") {
            continue;
        }
        ++levelCount;
        std::ifstream input(entry.path());
        expect(static_cast<bool>(input), "level must be readable: " + entry.path().string());

        std::vector<std::string> rows;
        std::string row;
        while (std::getline(input, row)) {
            if (!row.empty() && row.back() == '\r') {
                row.pop_back();
            }
            if (!row.empty()) {
                rows.push_back(row);
            }
        }
        expect(rows.size() >= 3, "level must contain at least three rows: " + entry.path().string());
        const std::size_t width = rows.front().size();
        expect(width >= 3, "level must contain at least three columns: " + entry.path().string());

        int players = 0;
        int exits = 0;
        for (std::size_t y = 0; y < rows.size(); ++y) {
            expect(rows[y].size() == width, "level rows must have equal width: " + entry.path().string());
            for (std::size_t x = 0; x < width; ++x) {
                const char cell = rows[y][x];
                players += cell == 'P' ? 1 : 0;
                exits += cell == 'E' ? 1 : 0;
                if (y == 0 || y + 1 == rows.size() || x == 0 || x + 1 == width) {
                    expect(
                        cell == '#' || cell == 'E',
                        "level boundary must be solid except for its exit: " + entry.path().string());
                }
            }
        }
        expect(players == 1, "level must contain exactly one player: " + entry.path().string());
        expect(exits == 1, "level must contain exactly one exit: " + entry.path().string());
    }
    expect(levelCount > 0, "at least one level must be available");
}

void testTranslationCatalogsHaveMatchingKeys() {
    const std::filesystem::path i18nDirectory =
        std::filesystem::path(TEST_ASSET_DIR) / "i18n";
    std::ifstream englishInput(i18nDirectory / "en.json");
    expect(static_cast<bool>(englishInput), "English translation catalog must be readable");
    nlohmann::json english;
    englishInput >> english;
    expect(english.is_object() && !english.empty(), "English translation catalog must be a non-empty object");

    std::size_t catalogCount = 0;
    for (const auto& entry : std::filesystem::directory_iterator(i18nDirectory)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".json") {
            continue;
        }
        ++catalogCount;
        std::ifstream input(entry.path());
        nlohmann::json catalog;
        input >> catalog;
        expect(catalog.is_object(), "translation catalog must be an object: " + entry.path().string());
        for (const auto& [key, value] : english.items()) {
            (void)value;
            expect(catalog.contains(key), "missing translation key '" + key + "' in " + entry.path().string());
            expect(catalog.at(key).is_string(), "translation value must be text for key '" + key + "'");
        }
        expect(catalog.size() == english.size(), "translation catalog has unexpected keys: " + entry.path().string());
    }
    expect(catalogCount >= 2, "at least two translation catalogs must be available");
}

} // namespace

int main() {
    Logger::setEnabled(false);
    const std::vector<std::pair<std::string, std::function<void()>>> tests = {
        { "player digs and collects", testPlayerDigsAndCollects },
        { "player pushes rock", testPlayerPushesRock },
        { "gravity has deterministic steps", testGravityHasDeterministicSteps },
        { "enemy reverses at boundary", testEnemyReversesAtBoundary },
        { "menu navigation skips disabled items", testMenuNavigationSkipsDisabledItems },
        { "level files are structurally valid", testLevelFilesAreStructurallyValid },
        { "translation catalogs have matching keys", testTranslationCatalogsHaveMatchingKeys },
    };

    int failures = 0;
    for (const auto& [name, test] : tests) {
        try {
            test();
            std::cout << "[PASS] " << name << '\n';
        } catch (const std::exception& error) {
            ++failures;
            std::cerr << "[FAIL] " << name << ": " << error.what() << '\n';
        }
    }

    std::cout << tests.size() - static_cast<std::size_t>(failures) << "/"
              << tests.size() << " tests passed\n";
    return failures == 0 ? 0 : 1;
}
