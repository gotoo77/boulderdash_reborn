#pragma once

#include <optional>

#include "MenuInterfaces.h"
#include "MenuTypes.h"
#include "Menu.h"

namespace menu {

struct MenuResult {
    bool consumed = false;
    std::string action;
};

class MenuManager {
public:
    MenuManager() = default;

    void setMenu(Menu menu);
    void setTextProvider(const ITextProvider* provider) { m_textProvider = provider; }
    const Menu* currentMenu() const { return m_menu ? &*m_menu : nullptr; }
    int selectedIndex() const { return m_selectedIndex; }

    void update(const IInputProvider& input);
    std::optional<std::string> consumeAction();
    void render(IMenuRenderer& renderer, const MenuRenderMetrics& metrics) const;

private:
    void moveSelection(int delta);

    std::optional<Menu> m_menu;
    const ITextProvider* m_textProvider = nullptr;
    int m_selectedIndex = 0;
    bool m_prevUp = false;
    bool m_prevDown = false;
    bool m_prevValidate = false;
    std::optional<std::string> m_pendingAction;
};

} // namespace menu
