#include "MenuFactory.h"

#include <algorithm>
#include <exception>
#include <utility>

#include "menu/MenuLoader.h"
#include "menu/MenuTypes.h"
#include "util/Logger.h"
#include "util/Translator.h"

namespace {

menu::Menu addDevMenuEntry(menu::Menu menu, const Translator& translator, bool devMode) {
    if (!devMode) {
        return menu;
    }
    menu::MenuDefinition definition = menu.definition();
    const auto existing = std::find_if(
        definition.items.begin(),
        definition.items.end(),
        [](const menu::MenuItem& item) { return item.action == "open_dev_level_select"; });
    if (existing == definition.items.end()) {
        definition.items.push_back(
            menu::MenuItem{ translator.tr("menu.devMode"), std::string{}, "open_dev_level_select", true });
    } else {
        existing->label = translator.tr("menu.devMode");
        existing->enabled = true;
    }
    return menu::Menu(std::move(definition));
}

menu::Menu buildFallbackMenu(const Translator& translator, bool devMode) {
    menu::MenuDefinition def;
    def.id = "fallback_main_menu";
    def.layout.anchor = menu::AnchorPoint::TopCenter;
    def.layout.spacing = 26.0f;
    def.layout.offsetY = 200.0f;
    def.itemScale = 2;
    def.header = menu::HeaderDefinition{};
    def.items.push_back(menu::MenuItem{ translator.tr("menu.newGame"), std::string{}, "start_game", true });
    def.items.push_back(menu::MenuItem{ translator.tr("menu.options"), std::string{}, "open_options", true });
    def.items.push_back(menu::MenuItem{ translator.tr("menu.exit"), std::string{}, "quit_game", true });
    return addDevMenuEntry(menu::Menu(def), translator, devMode);
}

menu::Menu buildFallbackPauseMenu() {
    menu::MenuDefinition definition;
    definition.id = "fallback_pause_menu";
    definition.layout.anchor = menu::AnchorPoint::Center;
    definition.layout.spacing = 52.0f;
    definition.layout.offsetY = 20.0f;
    definition.itemScale = 1;
    definition.colors.normal = menu::Color{ 220, 235, 255, 255 };
    definition.colors.selected = menu::Color{ 255, 209, 128, 255 };
    definition.items.push_back(menu::MenuItem{ {}, "pause.resume", "resume_game", true });
    definition.items.push_back(menu::MenuItem{ {}, "pause.options", "open_options", true });
    definition.items.push_back(menu::MenuItem{ {}, "pause.mainMenu", "return_main_menu", true });
    return menu::Menu(std::move(definition));
}

} // namespace

TranslatorTextProvider::TranslatorTextProvider(const Translator& translator)
    : m_translator(translator) {
}

std::string TranslatorTextProvider::getText(const std::string& id) const {
    return m_translator.tr(id);
}

menu::Menu loadPauseMenuDefinition(const std::filesystem::path& menuPath) {
    try {
        return menu::MenuLoader::loadFromFile(menuPath);
    } catch (const std::exception& error) {
        Logger::warn(std::string("Failed to load pause menu '") + menuPath.string() + "': " + error.what(), __func__);
        return buildFallbackPauseMenu();
    }
}

menu::Menu loadMainMenuDefinition(
    const std::filesystem::path& menuPath,
    const Translator& translator,
    bool devMode) {
    try {
        menu::Menu menu = menu::MenuLoader::loadFromFile(menuPath);
        return addDevMenuEntry(std::move(menu), translator, devMode);
    } catch (const std::exception& error) {
        Logger::warn(std::string("Failed to load menu '") + menuPath.string() + "': " + error.what(), __func__);
        return buildFallbackMenu(translator, devMode);
    }
}

menu::Menu buildLevelSelectMenu(
    const Translator& translator,
    const std::vector<std::filesystem::path>& levelPaths) {
    menu::MenuDefinition def;
    def.id = "dev_level_select";
    def.layout.anchor = menu::AnchorPoint::Center;
    def.layout.spacing = 28.0f;
    def.layout.offsetY = 32.0f;
    if (levelPaths.empty()) {
        def.items.push_back(menu::MenuItem{ translator.tr("dev.noLevels"), std::string{}, "dev_no_levels", false });
    } else {
        for (std::size_t i = 0; i < levelPaths.size(); ++i) {
            std::string displayName = levelPaths[i].filename().string();
            if (displayName.empty()) {
                displayName = levelPaths[i].string();
            }
            const std::string label =
                translator.tr("dev.levelPrefix") + " " + std::to_string(i + 1) + " - " + displayName;
            def.items.push_back(
                menu::MenuItem{
                    label,
                    std::string{},
                    std::string(DevLevelActionPrefix) + std::to_string(i),
                    true });
        }
    }
    return menu::Menu(def);
}
