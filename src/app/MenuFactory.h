#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "menu/Menu.h"
#include "menu/MenuInterfaces.h"

class Translator;

inline constexpr const char* DevLevelActionPrefix = "dev_select_level_";

class TranslatorTextProvider : public menu::ITextProvider {
public:
    explicit TranslatorTextProvider(const Translator& translator);
    std::string getText(const std::string& id) const override;

private:
    const Translator& m_translator;
};

menu::Menu loadMainMenuDefinition(
    const std::filesystem::path& menuPath,
    const Translator& translator,
    bool devMode);
menu::Menu loadPauseMenuDefinition(const std::filesystem::path& menuPath);
menu::Menu buildLevelSelectMenu(
    const Translator& translator,
    const std::vector<std::filesystem::path>& levelPaths);
