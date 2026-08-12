#pragma once

#include <filesystem>

#include "Menu.h"

namespace menu {

class MenuLoader {
public:
    static Menu loadFromFile(const std::filesystem::path& path);
    static Menu loadFromString(const std::string& jsonText);

private:
    static MenuDefinition parseDefinition(const std::string& jsonText);
};

} // namespace menu
