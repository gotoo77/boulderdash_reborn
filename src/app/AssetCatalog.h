#pragma once

#include <filesystem>
#include <string>
#include <vector>

struct LanguageEntry {
    std::string code;
    std::string label;
};

std::filesystem::path assetsBasePath();
std::filesystem::path configBasePath();
std::filesystem::path resolveConfigPath();
std::vector<std::filesystem::path> discoverLevels();
std::vector<LanguageEntry> discoverLanguages(const std::filesystem::path& i18nDir);
std::filesystem::path resolveMenuTexturePath(
    const std::filesystem::path& assetsRoot,
    const std::string& textureId);
