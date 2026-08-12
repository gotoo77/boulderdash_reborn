#include "Menu.h"

namespace menu {

Menu::Menu(MenuDefinition def)
    : m_definition(std::move(def)) {
}

} // namespace menu
