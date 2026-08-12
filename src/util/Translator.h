#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>

class Translator {
public:
    Translator(const std::filesystem::path& basePath, const std::string& language);

    std::string tr(const std::string& key) const;
    const std::string& requestedLanguage() const { return m_requestedLanguage; }
    const std::string& activeLanguage() const { return m_activeLanguage; }
    bool setLanguage(const std::string& language);

private:
    void loadLanguageFile(const std::filesystem::path& path);
    static std::size_t findStringEnd(const std::string& text, std::size_t start);
    static std::string decode(const std::string& value);

    std::filesystem::path m_basePath;
    std::unordered_map<std::string, std::string> m_entries;
    std::string m_requestedLanguage;
    std::string m_activeLanguage;
};
