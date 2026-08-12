#pragma once

#include <filesystem>
#include <string>

#include "Logger.h"

struct Config {
    int tileSize = 24;
    int tickMs = 33;
    int moveRepeatMs = 120;
    int timeLimitMs = 120000;
    int diamondValue = 10;
    int exitBonus = 100;
    int timeBonusPerSecond = 5;
    int enemyMoveIntervalMs = 200;
    int gravityStepMs = 66;
    bool tileTestMode = false;
    LogLevel logLevel = LogLevel::Info;
    int gameOverDelayMs = 3000;
    std::string language = "en";
    int startingLives = 3;
    int respawnDelayMs = 5000;
    bool devMode = false;
    std::string fullscreenToggleKey = "f";
};

Config loadConfig(const std::filesystem::path& configPath);
