#pragma once

#include <optional>

#include "MenuTypes.h"

namespace menu {

class Menu {
public:
    Menu() = default;
    explicit Menu(MenuDefinition def);

    const std::string& id() const { return m_definition.id; }
    const MenuLayout& layout() const { return m_definition.layout; }
    const std::vector<MenuItem>& items() const { return m_definition.items; }
    const std::optional<LogoDefinition>& logo() const { return m_definition.logo; }
    const std::optional<HeaderDefinition>& header() const { return m_definition.header; }
    const MenuColors& colors() const { return m_definition.colors; }
    int itemScale() const { return m_definition.itemScale; }
    const MenuDefinition& definition() const { return m_definition; }
    bool empty() const { return m_definition.items.empty(); }

private:
    MenuDefinition m_definition;
};

} // namespace menu
