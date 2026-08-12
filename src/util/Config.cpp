#include "Config.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>

namespace {

using Json = nlohmann::json;

[[noreturn]] void configError(
    const std::filesystem::path& path,
    const std::string& message) {
    throw std::runtime_error("Invalid config " + path.string() + ": " + message);
}

void readInteger(
    const Json& root,
    const char* key,
    int& target,
    int minimum,
    const std::filesystem::path& path) {
    const auto value = root.find(key);
    if (value == root.end()) {
        return;
    }
    if (!value->is_number_integer()) {
        configError(path, "'" + std::string(key) + "' must be an integer");
    }
    const int parsed = value->get<int>();
    if (parsed < minimum) {
        configError(
            path,
            "'" + std::string(key) + "' must be >= " + std::to_string(minimum));
    }
    target = parsed;
}

void readBoolean(
    const Json& root,
    const char* key,
    bool& target,
    const std::filesystem::path& path) {
    const auto value = root.find(key);
    if (value == root.end()) {
        return;
    }
    if (value->is_boolean()) {
        target = value->get<bool>();
        return;
    }
    if (value->is_number_integer()) {
        const int parsed = value->get<int>();
        if (parsed == 0 || parsed == 1) {
            target = parsed == 1;
            return;
        }
    }
    configError(path, "'" + std::string(key) + "' must be a boolean or 0/1");
}

void readNonEmptyString(
    const Json& root,
    const char* key,
    std::string& target,
    const std::filesystem::path& path) {
    const auto value = root.find(key);
    if (value == root.end()) {
        return;
    }
    if (!value->is_string() || value->get_ref<const std::string&>().empty()) {
        configError(path, "'" + std::string(key) + "' must be a non-empty string");
    }
    target = value->get<std::string>();
}

LogLevel parseLevel(const std::string& value, const std::filesystem::path& path) {
    std::string lowered = value;
    for (char& character : lowered) {
        character = static_cast<char>(
            std::tolower(static_cast<unsigned char>(character)));
    }
    if (lowered == "trace") {
        return LogLevel::Trace;
    }
    if (lowered == "debug") {
        return LogLevel::Debug;
    }
    if (lowered == "info") {
        return LogLevel::Info;
    }
    if (lowered == "warn" || lowered == "warning") {
        return LogLevel::Warn;
    }
    if (lowered == "error") {
        return LogLevel::Error;
    }
    configError(path, "'logLevel' has unsupported value '" + value + "'");
}

} // namespace

Config loadConfig(const std::filesystem::path& configPath) {
    std::ifstream input(configPath);
    if (!input) {
        throw std::runtime_error("Unable to open config file: " + configPath.string());
    }

    Json root;
    try {
        input >> root;
    } catch (const Json::parse_error& error) {
        configError(configPath, "malformed JSON at byte " + std::to_string(error.byte));
    }
    if (!root.is_object()) {
        configError(configPath, "root value must be an object");
    }

    Config config;
    readInteger(root, "tileSize", config.tileSize, 1, configPath);
    readInteger(root, "tickMs", config.tickMs, 1, configPath);
    readInteger(root, "moveRepeatMs", config.moveRepeatMs, 1, configPath);
    readInteger(root, "timeLimitMs", config.timeLimitMs, 1000, configPath);
    readInteger(root, "diamondValue", config.diamondValue, 1, configPath);
    readInteger(root, "exitBonus", config.exitBonus, 0, configPath);
    readInteger(root, "timeBonusPerSecond", config.timeBonusPerSecond, 0, configPath);
    readInteger(root, "enemyMoveIntervalMs", config.enemyMoveIntervalMs, 1, configPath);
    readInteger(root, "gravityStepMs", config.gravityStepMs, 1, configPath);
    readBoolean(root, "tileTestMode", config.tileTestMode, configPath);
    readInteger(root, "gameOverDelayMs", config.gameOverDelayMs, 0, configPath);
    readInteger(root, "respawnDelayMs", config.respawnDelayMs, 0, configPath);
    readNonEmptyString(root, "language", config.language, configPath);
    readInteger(root, "lives", config.startingLives, 1, configPath);
    readBoolean(root, "devMode", config.devMode, configPath);
    readNonEmptyString(
        root, "fullscreenToggleKey", config.fullscreenToggleKey, configPath);

    if (const auto level = root.find("logLevel"); level != root.end()) {
        if (!level->is_string()) {
            configError(configPath, "'logLevel' must be a string");
        }
        config.logLevel = parseLevel(level->get<std::string>(), configPath);
    }

    Logger::info("Config loaded from " + configPath.string(), __func__);
    return config;
}
