#include "TestSuites.h"

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "menu/MenuLoader.h"
#include "menu/MenuManager.h"
#include "menu/MockMenuRenderer.h"
#include "util/Config.h"

namespace {

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
    expect(manager.consumeAction() == std::optional<std::string>("play"),
        "validation must emit the selected action");
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
        expect(rows.size() == expectedHeight,
            "level must contain exactly " + std::to_string(expectedHeight) +
                " rows: " + entry.path().string());
        const std::size_t width = rows.front().size();
        expect(width == expectedWidth,
            "level rows must contain exactly " + std::to_string(expectedWidth) +
                " columns: " + entry.path().string());

        int players = 0;
        int exits = 0;
        for (std::size_t y = 0; y < rows.size(); ++y) {
            expect(rows[y].size() == width,
                "level rows must have equal width: " + entry.path().string());
            for (std::size_t x = 0; x < width; ++x) {
                const char cell = rows[y][x];
                expect(allowedCells.find(cell) != std::string::npos,
                    "invalid level symbol at row " + std::to_string(y + 1) +
                        ", column " + std::to_string(x + 1) + ": " +
                        entry.path().string());
                players += cell == 'P' ? 1 : 0;
                exits += cell == 'E' ? 1 : 0;
                if (y == 0 || y + 1 == rows.size() || x == 0 || x + 1 == width) {
                    expect(cell == '#' || cell == 'E',
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
    expect(english.is_object() && !english.empty(),
        "English translation catalog must be a non-empty object");

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
            expect(catalog.contains(key),
                "missing translation key '" + key + "' in " + entry.path().string());
            expect(catalog.at(key).is_string(),
                "translation value must be text for key '" + key + "'");
        }
        expect(catalog.size() == english.size(),
            "translation catalog has unexpected keys: " + entry.path().string());
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
        expect(std::filesystem::is_regular_file(std::filesystem::path(TEST_ASSET_DIR) / file),
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
    expect(config.timeLimitMs == defaults.timeLimitMs,
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

std::vector<TestCase> dataTests() {
    return {
        { "menu navigation skips disabled items", testMenuNavigationSkipsDisabledItems },
        { "level files are structurally valid", testLevelFilesAreStructurallyValid },
        { "translation catalogs have matching keys", testTranslationCatalogsHaveMatchingKeys },
        { "audio configuration maps every event to an asset", testAudioConfigurationMapsEveryEventToAnAsset },
        { "valid configuration loads every value", testValidConfigurationLoadsEveryValue },
        { "incomplete configuration keeps defaults", testIncompleteConfigurationKeepsDefaults },
        { "corrupted configuration is rejected", testCorruptedConfigurationIsRejected },
    };
}
