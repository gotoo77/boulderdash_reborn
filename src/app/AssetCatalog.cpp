#include "AssetCatalog.h"

#include <algorithm>
#include <exception>
#include <fstream>
#include <optional>
#include <sstream>

#include "util/Logger.h"

namespace {

std::optional<std::string> extractJsonString(const std::string& text, const std::string& key) {
    const std::string needle = '"' + key + '"';
    auto keyPos = text.find(needle);
    if (keyPos == std::string::npos) {
        return std::nullopt;
    }
    keyPos += needle.size();
    const auto colonPos = text.find(':', keyPos);
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

std::string readLanguageLabel(const std::filesystem::path& file, const std::string& fallback) {
    std::ifstream input(file);
    if (!input) {
        return fallback;
    }
    std::ostringstream oss;
    oss << input.rdbuf();
    if (auto value = extractJsonString(oss.str(), "language.name")) {
        return *value;
    }
    return fallback;
}

} // namespace

std::filesystem::path assetsBasePath() {
#ifdef ASSETS_DIR
    return std::filesystem::path(ASSETS_DIR);
#else
    return std::filesystem::path("assets");
#endif
}

std::filesystem::path configBasePath() {
#ifdef CONFIG_DIR
    return std::filesystem::path(CONFIG_DIR);
#else
    return assetsBasePath().parent_path() / "cfg";
#endif
}

std::filesystem::path resolveConfigPath() {
    return configBasePath() / "config.json";
}

std::vector<std::filesystem::path> discoverLevels() {
    std::vector<std::filesystem::path> levels;
    const auto levelDir = assetsBasePath() / "levels";
    try {
        if (std::filesystem::exists(levelDir)) {
            for (const auto& entry : std::filesystem::directory_iterator(levelDir)) {
                if (entry.is_regular_file() && entry.path().extension() == ".txt") {
                    levels.push_back(entry.path());
                }
            }
        }
    } catch (const std::exception& e) {
        Logger::warn(std::string("Failed to list levels: ") + e.what(), __func__);
    }
    std::sort(levels.begin(), levels.end());
    if (levels.empty()) {
        levels.push_back(levelDir / "level01.txt");
    }
    return levels;
}

std::vector<LanguageEntry> discoverLanguages(const std::filesystem::path& i18nDir) {
    std::vector<LanguageEntry> languages;
    auto addLanguage = [&](const std::string& code) {
        if (code.empty()) {
            return;
        }
        const auto existing = std::find_if(
            languages.begin(),
            languages.end(),
            [&](const LanguageEntry& entry) { return entry.code == code; });
        if (existing != languages.end()) {
            return;
        }
        languages.push_back(LanguageEntry{ code, readLanguageLabel(i18nDir / (code + ".json"), code) });
    };

    addLanguage("en");
    try {
        if (std::filesystem::exists(i18nDir)) {
            for (const auto& entry : std::filesystem::directory_iterator(i18nDir)) {
                if (entry.is_regular_file() && entry.path().extension() == ".json") {
                    addLanguage(entry.path().stem().string());
                }
            }
        }
    } catch (const std::exception& e) {
        Logger::warn(std::string("Failed to list languages: ") + e.what(), __func__);
    }

    std::sort(languages.begin(), languages.end(), [](const LanguageEntry& a, const LanguageEntry& b) {
        return a.label < b.label;
    });
    return languages;
}

std::filesystem::path resolveMenuTexturePath(
    const std::filesystem::path& assetsRoot,
    const std::string& textureId) {
    if (textureId.empty()) {
        return {};
    }
    std::filesystem::path candidate(textureId);
    if (candidate.is_absolute() && std::filesystem::exists(candidate)) {
        return candidate;
    }
    if (std::filesystem::exists(candidate)) {
        return candidate;
    }
    const std::filesystem::path relativeToAssets = assetsRoot / candidate;
    if (std::filesystem::exists(relativeToAssets)) {
        return relativeToAssets;
    }
    return candidate;
}
