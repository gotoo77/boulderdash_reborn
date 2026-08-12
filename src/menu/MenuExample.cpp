#include "MenuLoader.h"
#include "MenuManager.h"
#include "MockMenuRenderer.h"

#ifdef MENU_ENABLE_SELF_TESTS
#include <cassert>
#include <unordered_map>
#endif

namespace menu {

void runMenuExample(
    const std::string& jsonText,
    IMenuRenderer& renderer,
    IInputProvider& input,
    const MenuRenderMetrics& metrics) {
    Menu menu = MenuLoader::loadFromString(jsonText);
    MenuManager manager;
    manager.setMenu(std::move(menu));

    // One simulation frame for documentation/testing purposes.
    manager.update(input);
    manager.render(renderer, metrics);
}

#ifdef MENU_ENABLE_SELF_TESTS
namespace {

class StaticTextProvider : public ITextProvider {
public:
    explicit StaticTextProvider(std::unordered_map<std::string, std::string> entries)
        : m_entries(std::move(entries)) {
    }

    std::string getText(const std::string& id) const override {
        const auto it = m_entries.find(id);
        return it != m_entries.end() ? it->second : std::string{};
    }

private:
    std::unordered_map<std::string, std::string> m_entries;
};

void testLayoutScalingAndPixelAlignment() {
    const std::string json = R"({
        "id": "layout_test",
        "items": [
            { "label": "A", "action": "a" },
            { "label": "B", "action": "b" },
            { "label": "C", "action": "c" }
        ]
    })";
    MockMenuRenderer renderer;
    MockInputProvider input;
    MenuRenderMetrics metrics;
    metrics.viewportWidth = 800;
    metrics.viewportHeight = 600;
    metrics.uiScale = 2.0f;
    Menu menu = MenuLoader::loadFromString(json);
    MenuManager manager;
    manager.setMenu(std::move(menu));
    manager.render(renderer, metrics);
    // anchorY = 300, spacing = 48, topY = 252 -> 252 / 300 / 348
    assert(renderer.textCalls().size() == 3);
    assert(renderer.textCalls()[0].y == 252);
    assert(renderer.textCalls()[1].y == 300);
    assert(renderer.textCalls()[2].y == 348);
}

void testI18NResolution() {
    const std::string json = R"({
        "id": "i18n_test",
        "items": [
            { "label_id": "play_key", "action": "play" },
            { "label": "Raw", "action": "raw" }
        ]
    })";
    MockMenuRenderer renderer;
    MockInputProvider input;
    StaticTextProvider text({ { "play_key", "Jouer" } });
    MenuRenderMetrics metrics;
    metrics.viewportWidth = 640;
    metrics.viewportHeight = 480;
    Menu menu = MenuLoader::loadFromString(json);
    MenuManager manager;
    manager.setMenu(std::move(menu));
    manager.setTextProvider(&text);
    manager.render(renderer, metrics);
    assert(renderer.textCalls().size() == 2);
    assert(renderer.textCalls()[0].text == "Jouer");
    assert(renderer.textCalls()[1].text == "Raw");
}

void testNavigationSkipsDisabled() {
    const std::string json = R"({
        "id": "nav_test",
        "items": [
            { "label": "Disabled", "action": "a", "enabled": false },
            { "label": "FirstEnabled", "action": "b" },
            { "label": "SecondEnabled", "action": "c" }
        ]
    })";
    MockMenuRenderer renderer;
    MockInputProvider input;
    MenuRenderMetrics metrics;
    metrics.viewportWidth = 640;
    metrics.viewportHeight = 480;
    Menu menu = MenuLoader::loadFromString(json);
    MenuManager manager;
    manager.setMenu(std::move(menu));
    // Simulate a single down press to move off the disabled item
    input.setDown(true);
    manager.update(input);
    manager.render(renderer, metrics);
    assert(manager.selectedIndex() == 1);
    auto action = manager.consumeAction();
    assert(!action.has_value());
}

} // namespace

void runMenuSelfTests() {
    testLayoutScalingAndPixelAlignment();
    testI18NResolution();
    testNavigationSkipsDisabled();
}
#endif

} // namespace menu
