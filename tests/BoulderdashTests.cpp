#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "core/Game.h"
#include "core/Grid.h"
#include "menu/MenuLoader.h"
#include "menu/MenuManager.h"
#include "menu/MockMenuRenderer.h"
#include "systems/EnemySystem.h"
#include "systems/GravitySystem.h"
#include "systems/PlayerSystem.h"
#include "util/Config.h"
#include "util/Logger.h"

#include "AudioStub.h"

namespace {

void expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
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

int countCells(const Grid& grid, CellType type) {
    int count = 0;
    for (int y = 0; y < grid.height(); ++y) {
        for (int x = 0; x < grid.width(); ++x) {
            count += grid.at(x, y).type == type ? 1 : 0;
        }
    }
    return count;
}

bool exitCellIsUnlocked(const Grid& grid) {
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

GameRules deterministicRules() {
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

void testGameLoadsLevelAndExpiresTimer() {
    TemporaryLevels levels;
    const auto level = levels.write("timer.txt", "#####\n#P.E#\n#####\n");
    auto rules = deterministicRules();
    rules.timeLimitMs = 1000;
    Game game({ level }, rules);

    expect(game.currentLevelNumber() == 1, "game must load the first level");
    expect(game.currentLevelName() == "timer.txt", "game must expose the loaded level name");
    expect(game.levelCount() == 1, "game must expose the number of levels");
    expect(game.remainingLives() == 3, "game must start with the configured lives");
    expect(game.timeRemainingMs() == 1000, "timer must start at its configured limit");
    expect(exitCellIsUnlocked(game.grid()), "door must start open when no diamond is required");

    game.update(250);
    expect(game.elapsedMs() == 250, "timer must accumulate deterministic updates");
    expect(game.timeRemainingMs() == 750, "remaining time must decrease with updates");
    expect(!game.levelFailed(), "level must remain active before the time limit");

    game.update(750);
    expect(game.elapsedMs() == 1000, "timer must stop at its configured limit");
    expect(game.timeRemainingMs() == 0, "timer must not become negative");
    expect(game.levelFailed(), "level must fail when the timer expires");
}

void testGameRejectsMultipleExits() {
    TemporaryLevels levels;
    const auto level = levels.write("two-exits.txt", "#####\n#PEE#\n#####\n");
    bool rejected = false;
    try {
        Game game({ level }, deterministicRules());
        (void)game;
    } catch (const std::runtime_error& error) {
        rejected = std::string(error.what()).find("2 exit(s)") != std::string::npos;
    }
    expect(rejected, "runtime loader must reject a level containing two exits");
}

void testGameScoresAndAdvancesLevels() {
    TemporaryLevels levels;
    const auto first = levels.write("first.txt", "#####\n#PoE#\n#####\n");
    const auto second = levels.write("second.txt", "#####\n#PE##\n#####\n");
    const auto third = levels.write("third.txt", "#####\n#PE##\n#####\n");
    const auto fourth = levels.write("fourth.txt", "#####\n#PE##\n#####\n");
    const auto rules = deterministicRules();
    resetTestAudio();
    Game game({ first, second, third, fourth }, rules);

    expect(game.totalDiamonds() == 1, "first level must count its diamond");
    expect(!exitCellIsUnlocked(game.grid()), "door must start locked while diamonds remain");
    game.queueMove(Direction::Right);
    game.update(1000);
    expect(game.collectedDiamonds() == 1, "diamond collection must update the game state");
    expect(game.levelScore() == 10, "diamond value must be added to the level score");
    expect(game.totalScore() == 10, "unbanked level score must be visible in total score");
    expect(game.exitUnlocked(), "collecting every diamond must unlock the exit");
    expect(exitCellIsUnlocked(game.grid()), "door cell must expose its unlocked visual state");
    expect(
        testAudioPlayCount(SoundId::ExitUnlock) == 1,
        "collecting the final diamond must play the exit unlock sound once");

    game.update(0);
    expect(
        testAudioPlayCount(SoundId::ExitUnlock) == 1,
        "unlocked exit sound must not repeat on later updates");

    game.queueMove(Direction::Right);
    game.update(0);
    expect(game.levelComplete(), "entering an unlocked exit must complete the level");
    expect(game.levelScore() == 155, "exit and remaining-time bonuses must be deterministic");
    expect(game.totalScore() == 155, "completed level score must be banked exactly once");

    expect(game.advanceToNextLevel(), "a completed level must advance when another exists");
    expect(game.currentLevelNumber() == 2, "next level index must be loaded");
    expect(game.currentLevelName() == "second.txt", "next level name must be exposed");
    expect(game.levelScore() == 0, "level score must reset after advancing");
    expect(game.totalScore() == 155, "banked score must survive a level change");

    const std::vector<std::string> remainingLevelNames = {
        "second.txt", "third.txt", "fourth.txt"
    };
    int expectedScore = 155;
    for (std::size_t index = 0; index < remainingLevelNames.size(); ++index) {
        expect(
            game.currentLevelName() == remainingLevelNames[index],
            "four-level progression must load levels in order");
        game.queueMove(Direction::Right);
        game.update(0);
        expect(game.levelComplete(), "each remaining level exit must be reachable");
        expectedScore += 150;
        expect(
            game.totalScore() == expectedScore,
            "score must accumulate through all four levels");
        if (index + 1 < remainingLevelNames.size()) {
            expect(game.advanceToNextLevel(), "intermediate levels must advance");
        }
    }
    expect(game.currentLevelNumber() == 4, "progression must reach the fourth level");
    expect(!game.advanceToNextLevel(), "advancing past the last level must signal victory");
    expect(game.totalScore() == 605, "victory probing must not bank the score twice");
}

void testGameConsumesLivesAndRespawns() {
    TemporaryLevels levels;
    const auto level = levels.write("respawn.txt", "#####\n#PXE#\n#####\n");
    const auto rules = deterministicRules();
    Game game({ level }, rules);

    auto collideWithEnemy = [&]() {
        game.queueMove(Direction::Right);
        game.update(1);
    };

    collideWithEnemy();
    expect(game.remainingLives() == 2, "first death must consume one life");
    expect(!game.levelFailed(), "game must wait for respawn while lives remain");
    expect(countCells(game.grid(), CellType::Player) == 0, "dead player must leave the grid");

    game.update(99);
    expect(countCells(game.grid(), CellType::Player) == 0, "respawn must respect its delay");
    game.update(1);
    expect(countCells(game.grid(), CellType::Player) == 1, "player must respawn after the delay");
    expect(game.remainingLives() == 2, "respawn must preserve remaining lives");
    expect(game.elapsedMs() == 0, "respawn must restart the level timer");

    collideWithEnemy();
    expect(game.remainingLives() == 1, "second death must consume another life");
    game.update(100);
    collideWithEnemy();
    expect(game.remainingLives() == 0, "last death must consume the final life");
    expect(game.levelFailed(), "game must fail when no lives remain");
}

void testFallingDiamondAndEnemyExplosionPlaySounds() {
    TemporaryLevels levels;
    const auto level = levels.write(
        "diamond-explosion.txt",
        "#####\n#.o.#\n#.X.#\n#P.E#\n#####\n");
    auto rules = deterministicRules();
    rules.gravityStepMs = 0;
    resetTestAudio();
    Game game({ level }, rules);

    game.update(0);
    expect(
        testAudioPlayCount(SoundId::DiamondFall) == 1,
        "a diamond starting to fall must play its dedicated sound once");
    expect(
        testAudioPlayCount(SoundId::Explosion) == 0,
        "explosion sound must wait for the enemy collision");

    game.update(0);
    expect(
        testAudioPlayCount(SoundId::DiamondFall) == 1,
        "diamond fall sound must not repeat while the same diamond keeps falling");
    expect(
        testAudioPlayCount(SoundId::Explosion) == 1,
        "a falling diamond hitting an enemy must play the explosion sound once");
    expect(
        countCells(game.grid(), CellType::Enemy) == 0,
        "diamond collision explosion must remove the enemy");
}

void testTimeWarningPlaysOncePerSecond() {
    TemporaryLevels levels;
    const auto level = levels.write("timer-warning.txt", "#####\n#P.E#\n#####\n");
    auto rules = deterministicRules();
    rules.timeLimitMs = 16000;
    resetTestAudio();
    Game game({ level }, rules);

    game.update(999);
    expect(
        testAudioPlayCount(SoundId::TimeWarning) == 0,
        "timer warning must remain silent above fifteen seconds");
    game.update(1);
    expect(
        testAudioPlayCount(SoundId::TimeWarning) == 1,
        "timer warning must start at fifteen seconds remaining");
    game.update(400);
    expect(
        testAudioPlayCount(SoundId::TimeWarning) == 1,
        "timer warning must not repeat within the same second");
    game.update(600);
    expect(
        testAudioPlayCount(SoundId::TimeWarning) == 2,
        "timer warning must play again on the next countdown second");
    game.update(14000);
    expect(game.levelFailed(), "timer warning scenario must still expire normally");
    expect(
        testAudioPlayCount(SoundId::TimeWarning) == 2,
        "time expiration must not add a late warning sound");
}

void testPauseFreezesTimerAndDiscardsInput() {
    TemporaryLevels levels;
    const auto level = levels.write("pause.txt", "#####\n#P.E#\n#####\n");
    auto rules = deterministicRules();
    rules.timeLimitMs = 20000;
    Game game({ level }, rules);

    game.update(1000);
    expect(game.elapsedMs() == 1000, "timer must advance before pause");
    game.setPaused(true);
    expect(game.paused(), "game must expose its paused state");
    game.queueMove(Direction::Right);
    game.update(5000);
    expect(game.elapsedMs() == 1000, "timer must remain frozen while paused");
    expect(game.grid().at(1, 1).type == CellType::Player, "paused input must be discarded");

    game.setPaused(false);
    game.update(1000);
    expect(!game.paused(), "game must leave its paused state");
    expect(game.elapsedMs() == 2000, "timer must resume without counting paused time");
    expect(
        game.grid().at(1, 1).type == CellType::Player,
        "discarded paused input must not execute after resume");
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

void testFallingRockKillsPlayerAndClearsExplosionArea() {
    Grid grid(5, 5);
    grid.at(2, 1).type = CellType::Rock;
    grid.at(2, 1).falling = true;
    grid.at(2, 2).type = CellType::Player;
    grid.at(1, 2).type = CellType::Dirt;
    grid.at(3, 2).type = CellType::WallDestructible;
    grid.at(1, 3).type = CellType::Wall;
    grid.at(3, 3).type = CellType::Exit;
    PlayerEvents events;

    GravitySystem::update(grid, events);

    expect(events.playerDied, "a falling rock must kill the player below it");
    expect(
        events.deathCause == PlayerDeathCause::Rock,
        "falling rock collision must report the rock death cause");
    expect(grid.at(2, 2).type == CellType::Empty, "player cell must be cleared by explosion");
    expect(grid.at(1, 2).type == CellType::Empty, "explosion must clear dirt");
    expect(
        grid.at(3, 2).type == CellType::Empty,
        "explosion must clear destructible walls");
    expect(grid.at(1, 3).type == CellType::Wall, "explosion must preserve solid walls");
    expect(grid.at(3, 3).type == CellType::Exit, "explosion must preserve the exit");
}

void testFallingRockExplodesEnemyIntoDiamonds() {
    Grid grid(5, 5);
    grid.at(2, 1).type = CellType::Rock;
    grid.at(2, 1).falling = true;
    grid.at(2, 2).type = CellType::Enemy;
    grid.at(1, 2).type = CellType::Dirt;
    grid.at(3, 2).type = CellType::WallDestructible;
    grid.at(1, 3).type = CellType::Wall;
    grid.at(3, 3).type = CellType::Exit;
    PlayerEvents events;

    GravitySystem::update(grid, events);

    expect(events.enemyExploded, "a falling rock must emit an enemy explosion event");
    expect(countCells(grid, CellType::Enemy) == 0, "explosion must remove the enemy");
    expect(grid.at(2, 2).type == CellType::Diamond, "enemy cell must become a diamond");
    expect(grid.at(1, 2).type == CellType::Diamond, "explosion must turn dirt into diamonds");
    expect(
        grid.at(3, 2).type == CellType::Diamond,
        "explosion must turn destructible walls into diamonds");
    expect(grid.at(1, 3).type == CellType::Wall, "diamond explosion must preserve solid walls");
    expect(grid.at(3, 3).type == CellType::Exit, "diamond explosion must preserve the exit");
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
    constexpr std::size_t expectedWidth = 40;
    constexpr std::size_t expectedHeight = 22;
    const std::string allowedCells = "#.bB*dDoOPEXx";
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
            expect(!row.empty(), "level must not contain blank rows: " + entry.path().string());
            rows.push_back(row);
        }
        expect(
            rows.size() == expectedHeight,
            "level must contain exactly " + std::to_string(expectedHeight) +
                " rows: " + entry.path().string());
        const std::size_t width = rows.front().size();
        expect(
            width == expectedWidth,
            "level rows must contain exactly " + std::to_string(expectedWidth) +
                " columns: " + entry.path().string());

        int players = 0;
        int exits = 0;
        for (std::size_t y = 0; y < rows.size(); ++y) {
            expect(rows[y].size() == width, "level rows must have equal width: " + entry.path().string());
            for (std::size_t x = 0; x < width; ++x) {
                const char cell = rows[y][x];
                expect(
                    allowedCells.find(cell) != std::string::npos,
                    "invalid level symbol at row " + std::to_string(y + 1) +
                        ", column " + std::to_string(x + 1) + ": " +
                        entry.path().string());
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

void testAudioConfigurationMapsEveryEventToAnAsset() {
    const std::filesystem::path configPath =
        std::filesystem::path(TEST_CONFIG_DIR) / "audio.json";
    std::ifstream input(configPath);
    expect(static_cast<bool>(input), "audio configuration must be readable");
    nlohmann::json config;
    input >> config;
    expect(config.contains("sounds") && config["sounds"].is_object(),
        "audio configuration must contain a sounds object");
    expect(config.contains("volume") && config["volume"].is_object(),
        "audio configuration must contain a volume object");
    for (const char* channel : { "music", "effects" }) {
        expect(config["volume"].contains(channel) && config["volume"][channel].is_number_integer(),
            "audio volume must contain integer channel: " + std::string(channel));
        const int volume = config["volume"][channel].get<int>();
        expect(volume >= 0 && volume <= 100,
            "audio volume must be between 0 and 100: " + std::string(channel));
    }

    constexpr const char* requiredEvents[] = {
        "walk",
        "dig",
        "rock_fall",
        "diamond_collect",
        "death",
        "exit_unlock",
        "diamond_fall",
        "explosion",
        "time_warning",
        "game_over",
    };
    for (const char* event : requiredEvents) {
        expect(config["sounds"].contains(event),
            "audio configuration is missing event: " + std::string(event));
        const auto& entry = config["sounds"][event];
        const std::string file = entry.is_string() ?
            entry.get<std::string>() : entry.value("file", std::string{});
        expect(!file.empty(), "audio event has no file: " + std::string(event));
        expect(
            std::filesystem::is_regular_file(std::filesystem::path(TEST_ASSET_DIR) / file),
            "audio asset is missing for event " + std::string(event) + ": " + file);
    }
}

void testValidConfigurationLoadsEveryValue() {
    TemporaryLevels files;
    const auto path = files.write("valid.json", R"({
        "tileSize": 32,
        "tickMs": 20,
        "moveRepeatMs": 80,
        "timeLimitMs": 90000,
        "diamondValue": 30,
        "exitBonus": 250,
        "timeBonusPerSecond": 7,
        "enemyMoveIntervalMs": 150,
        "gravityStepMs": 75,
        "tileTestMode": true,
        "logLevel": "warn",
        "gameOverDelayMs": 4000,
        "respawnDelayMs": 2000,
        "language": "fr",
        "lives": 5,
        "devMode": 1,
        "fullscreenToggleKey": "F11"
    })");

    const Config config = loadConfig(path);
    expect(config.tileSize == 32, "valid config must load tileSize");
    expect(config.tickMs == 20, "valid config must load tickMs");
    expect(config.moveRepeatMs == 80, "valid config must load moveRepeatMs");
    expect(config.timeLimitMs == 90000, "valid config must load timeLimitMs");
    expect(config.diamondValue == 30, "valid config must load diamondValue");
    expect(config.exitBonus == 250, "valid config must load exitBonus");
    expect(config.timeBonusPerSecond == 7, "valid config must load time bonus");
    expect(config.enemyMoveIntervalMs == 150, "valid config must load enemy interval");
    expect(config.gravityStepMs == 75, "valid config must load gravity step");
    expect(config.tileTestMode, "valid config must load boolean values");
    expect(config.logLevel == LogLevel::Warn, "valid config must load log level");
    expect(config.gameOverDelayMs == 4000, "valid config must load game over delay");
    expect(config.respawnDelayMs == 2000, "valid config must load respawn delay");
    expect(config.language == "fr", "valid config must load language");
    expect(config.startingLives == 5, "valid config must load lives");
    expect(config.devMode, "valid config must accept 0/1 booleans");
    expect(config.fullscreenToggleKey == "F11", "valid config must load fullscreen key");
}

void testIncompleteConfigurationKeepsDefaults() {
    TemporaryLevels files;
    const auto path = files.write("incomplete.json", R"({"language":"fr","lives":4})");
    const Config defaults;
    const Config config = loadConfig(path);

    expect(config.language == "fr", "incomplete config must apply provided values");
    expect(config.startingLives == 4, "incomplete config must apply provided lives");
    expect(config.tileSize == defaults.tileSize, "missing tileSize must keep its default");
    expect(config.tickMs == defaults.tickMs, "missing tickMs must keep its default");
    expect(
        config.timeLimitMs == defaults.timeLimitMs,
        "missing timeLimitMs must keep its default");
    expect(config.devMode == defaults.devMode, "missing devMode must keep its default");
}

void testCorruptedConfigurationIsRejected() {
    TemporaryLevels files;
    const auto malformed = files.write("malformed.json", R"({"tileSize":24, broken})");
    bool malformedRejected = false;
    try {
        (void)loadConfig(malformed);
    } catch (const std::runtime_error& error) {
        malformedRejected = std::string(error.what()).find("malformed JSON") != std::string::npos;
    }
    expect(malformedRejected, "malformed JSON must be rejected with a precise error");

    const auto wrongType = files.write("wrong-type.json", R"({"lives":"three"})");
    bool typeRejected = false;
    try {
        (void)loadConfig(wrongType);
    } catch (const std::runtime_error& error) {
        typeRejected = std::string(error.what()).find("'lives' must be an integer") !=
            std::string::npos;
    }
    expect(typeRejected, "invalid config types must identify the offending key");
}

} // namespace

int main() {
    Logger::setEnabled(false);
    const std::vector<std::pair<std::string, std::function<void()>>> tests = {
        { "game loads level and expires timer", testGameLoadsLevelAndExpiresTimer },
        { "game rejects multiple exits", testGameRejectsMultipleExits },
        { "game scores and advances levels", testGameScoresAndAdvancesLevels },
        { "game consumes lives and respawns", testGameConsumesLivesAndRespawns },
        { "falling diamond and enemy explosion play sounds", testFallingDiamondAndEnemyExplosionPlaySounds },
        { "time warning plays once per second", testTimeWarningPlaysOncePerSecond },
        { "pause freezes timer and discards input", testPauseFreezesTimerAndDiscardsInput },
        { "player digs and collects", testPlayerDigsAndCollects },
        { "player pushes rock", testPlayerPushesRock },
        { "gravity has deterministic steps", testGravityHasDeterministicSteps },
        { "falling rock kills player and clears explosion area", testFallingRockKillsPlayerAndClearsExplosionArea },
        { "falling rock explodes enemy into diamonds", testFallingRockExplodesEnemyIntoDiamonds },
        { "enemy reverses at boundary", testEnemyReversesAtBoundary },
        { "menu navigation skips disabled items", testMenuNavigationSkipsDisabledItems },
        { "level files are structurally valid", testLevelFilesAreStructurallyValid },
        { "translation catalogs have matching keys", testTranslationCatalogsHaveMatchingKeys },
        { "audio configuration maps every event to an asset", testAudioConfigurationMapsEveryEventToAnAsset },
        { "valid configuration loads every value", testValidConfigurationLoadsEveryValue },
        { "incomplete configuration keeps defaults", testIncompleteConfigurationKeepsDefaults },
        { "corrupted configuration is rejected", testCorruptedConfigurationIsRejected },
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
