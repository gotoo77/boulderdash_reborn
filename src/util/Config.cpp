#include "Config.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <optional>
#include <sstream>

namespace {

std::string readFile(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file) {
        return {};
    }
    std::ostringstream oss;
    oss << file.rdbuf();
    return oss.str();
}

std::optional<int> extractInt(const std::string& text, const std::string& key) {
    const std::string needle = '"' + key + '"';
    const auto keyPos = text.find(needle);
    if (keyPos == std::string::npos) {
        return std::nullopt;
    }
    const auto colonPos = text.find(':', keyPos + needle.size());
    if (colonPos == std::string::npos) {
        return std::nullopt;
    }
    const auto numberStart = text.find_first_of("-0123456789", colonPos + 1);
    if (numberStart == std::string::npos) {
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

std::optional<std::string> extractString(const std::string& text, const std::string& key) {
    const std::string needle = '"' + key + '"';
    const auto keyPos = text.find(needle);
    if (keyPos == std::string::npos) {
        return std::nullopt;
    }
    const auto colonPos = text.find(':', keyPos + needle.size());
    if (colonPos == std::string::npos) {
        return std::nullopt;
    }
    const auto quoteStart = text.find('"', colonPos + 1);
    if (quoteStart == std::string::npos) {
        return std::nullopt;
    }
    const auto quoteEnd = text.find('"', quoteStart + 1);
    if (quoteEnd == std::string::npos) {
        return std::nullopt;
    }
    return text.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
}

LogLevel parseLevel(const std::string& value) {

    const std::string lowered = [&]() {
        std::string tmp = value;
        for (char& c : tmp) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return tmp;
    }();

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
    return LogLevel::Info;
}

} // namespace

Config loadConfig(const std::filesystem::path& configPath) {
    Config config;
    const auto content = readFile(configPath);
    if (content.empty()) {
        Logger::warn("Config file missing or empty, using defaults", __func__);
        return config;
    }

    if (auto tileSize = extractInt(content, "tileSize")) {
        config.tileSize = *tileSize;
    }
    if (auto tickMs = extractInt(content, "tickMs")) {
        config.tickMs = std::max(1, *tickMs);
    }
    if (auto moveRepeat = extractInt(content, "moveRepeatMs")) {
        config.moveRepeatMs = std::max(1, *moveRepeat);
    }
    if (auto timeLimit = extractInt(content, "timeLimitMs")) {
        config.timeLimitMs = std::max(1000, *timeLimit);
    }
    if (auto diamondValue = extractInt(content, "diamondValue")) {
        config.diamondValue = std::max(1, *diamondValue);
    }
    if (auto exitBonus = extractInt(content, "exitBonus")) {
        config.exitBonus = std::max(0, *exitBonus);
    }
    if (auto timeBonus = extractInt(content, "timeBonusPerSecond")) {
        config.timeBonusPerSecond = std::max(0, *timeBonus);
    }
    if (auto enemyInterval = extractInt(content, "enemyMoveIntervalMs")) {
        config.enemyMoveIntervalMs = std::max(1, *enemyInterval);
    }
    if (auto gravityStep = extractInt(content, "gravityStepMs")) {
        config.gravityStepMs = std::max(1, *gravityStep);
    }
    if (auto testMode = extractInt(content, "tileTestMode")) {
        config.tileTestMode = (*testMode) != 0;
    }
    if (auto level = extractString(content, "logLevel")) {
        config.logLevel = parseLevel(*level);
    }
    if (auto gameOverDelay = extractInt(content, "gameOverDelayMs")) {
        config.gameOverDelayMs = std::max(0, *gameOverDelay);
    }
    if (auto respawnDelay = extractInt(content, "respawnDelayMs")) {
        config.respawnDelayMs = std::max(0, *respawnDelay);
    }
    if (auto language = extractString(content, "language")) {
        config.language = *language;
    }
    if (auto lives = extractInt(content, "lives")) {
        config.startingLives = std::max(1, *lives);
    }
    if (auto devMode = extractInt(content, "devMode")) {
        config.devMode = (*devMode) != 0;
    }
    if (auto fullscreenKey = extractString(content, "fullscreenToggleKey")) {
        if (!fullscreenKey->empty()) {
            config.fullscreenToggleKey = *fullscreenKey;
        }
    }

    Logger::info("Config loaded from " + configPath.string(), __func__);
    return config;
}
