#include "Translator.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>

#include "Logger.h"

namespace {

std::string readFile(const std::filesystem::path& path) {
    std::ifstream stream(path);
    if (!stream) {
        return {};
    }
    return { std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() };
}

} // namespace

Translator::Translator(const std::filesystem::path& basePath, const std::string& language)
    : m_basePath(basePath),
      m_requestedLanguage(language),
      m_activeLanguage("en") {
    LOG_T("Translator ctor base=%s language=%s", basePath.string().c_str(), language.c_str());
    setLanguage(language);
}

std::string Translator::tr(const std::string& key) const {
    const auto it = m_entries.find(key);
    if (it != m_entries.end()) {
        return it->second;
    }
    return key;
}

bool Translator::setLanguage(const std::string& language) {
    LOG_T("Translator::setLanguage language=%s", language.c_str());
    if (language == m_activeLanguage && !m_entries.empty()) {
        m_requestedLanguage = language;
        return true;
    }
    m_entries.clear();
    m_requestedLanguage = language;
    const auto english = m_basePath / "en.json";
    loadLanguageFile(english);
    m_activeLanguage = "en";
    if (language.empty() || language == "en") {
        Logger::info("Using default English translations", __func__);
        return true;
    }
    const auto requested = m_basePath / (language + ".json");
    if (std::filesystem::exists(requested)) {
        loadLanguageFile(requested);
        m_activeLanguage = language;
        Logger::info("Loaded translations for language '" + language + "'", __func__);
        return true;
    }
    Logger::warn("Missing translation file for language '" + language + "', using English fallback", __func__);
    return false;
}

void Translator::loadLanguageFile(const std::filesystem::path& path) {
    LOG_T("Translator::loadLanguageFile %s", path.string().c_str());
    const auto content = readFile(path);
    if (content.empty()) {
        Logger::warn("Translation file empty or missing: " + path.string(), __func__);
        return;
    }
    std::size_t cursor = 0;
    while (cursor < content.size()) {
        const auto keyStart = content.find('"', cursor);
        if (keyStart == std::string::npos) {
            break;
        }
        const auto keyEnd = findStringEnd(content, keyStart + 1);
        if (keyEnd == std::string::npos) {
            break;
        }
        const std::string key = decode(content.substr(keyStart + 1, keyEnd - keyStart - 1));
        const auto colon = content.find(':', keyEnd + 1);
        if (colon == std::string::npos) {
            break;
        }
        const auto valueStart = content.find('"', colon + 1);
        if (valueStart == std::string::npos) {
            break;
        }
        const auto valueEnd = findStringEnd(content, valueStart + 1);
        if (valueEnd == std::string::npos) {
            break;
        }
        const std::string value = decode(content.substr(valueStart + 1, valueEnd - valueStart - 1));
        m_entries[key] = value;
        cursor = valueEnd + 1;
    }
}

std::size_t Translator::findStringEnd(const std::string& text, std::size_t start) {
    bool escaped = false;
    for (std::size_t i = start; i < text.size(); ++i) {
        const char c = text[i];
        if (escaped) {
            escaped = false;
            continue;
        }
        if (c == '\\') {
            escaped = true;
            continue;
        }
        if (c == '"') {
            return i;
        }
    }
    return std::string::npos;
}

std::string Translator::decode(const std::string& value) {
    std::string decoded;
    decoded.reserve(value.size());
    bool escaped = false;
    for (char c : value) {
        if (!escaped) {
            if (c == '\\') {
                escaped = true;
                continue;
            }
            decoded.push_back(c);
            continue;
        }
        switch (c) {
        case 'n':
            decoded.push_back('\n');
            break;
        case 't':
            decoded.push_back('\t');
            break;
        case '\\':
            decoded.push_back('\\');
            break;
        case '"':
            decoded.push_back('"');
            break;
        default:
            decoded.push_back(c);
            break;
        }
        escaped = false;
    }
    return decoded;
}
