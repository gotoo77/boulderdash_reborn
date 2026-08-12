#include "MenuLoader.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>

#include <nlohmann/json.hpp> // external JSON dependency kept local to this file

#include "../util/Logger.h"

namespace menu {
namespace {

Color parseColor(const nlohmann::json& value, Color fallback) {
    auto clamp = [](int v) {
        return static_cast<uint8_t>(std::max(0, std::min(255, v)));
    };
    if (value.is_string()) {
        const std::string text = value.get<std::string>();
        std::string hex = text;
        if (!hex.empty() && hex[0] == '#') {
            hex.erase(0, 1);
        }
        if (hex.size() == 6 || hex.size() == 8) {
            int r = 0;
            int g = 0;
            int b = 0;
            int a = 255;
            try {
                r = std::stoi(hex.substr(0, 2), nullptr, 16);
                g = std::stoi(hex.substr(2, 2), nullptr, 16);
                b = std::stoi(hex.substr(4, 2), nullptr, 16);
                if (hex.size() == 8) {
                    a = std::stoi(hex.substr(6, 2), nullptr, 16);
                }
                return Color{ clamp(r), clamp(g), clamp(b), clamp(a) };
            } catch (const std::exception&) {
                return fallback;
            }
        }
    } else if (value.is_array()) {
        const auto arr = value;
        if (arr.size() >= 3) {
            const int r = static_cast<int>(arr[0].get<double>());
            const int g = static_cast<int>(arr[1].get<double>());
            const int b = static_cast<int>(arr[2].get<double>());
            const int a = (arr.size() > 3) ? static_cast<int>(arr[3].get<double>()) : 255;
            return Color{ clamp(r), clamp(g), clamp(b), clamp(a) };
        }
    } else if (value.is_object()) {
        const int r = clamp(value.value("r", static_cast<int>(fallback.r)));
        const int g = clamp(value.value("g", static_cast<int>(fallback.g)));
        const int b = clamp(value.value("b", static_cast<int>(fallback.b)));
        const int a = clamp(value.value("a", static_cast<int>(fallback.a)));
        return Color{ clamp(r), clamp(g), clamp(b), clamp(a) };
    }
    return fallback;
}

AnchorPoint anchorFromString(const std::string& text) {
    std::string lowered = text;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    if (lowered == "center") {
        return AnchorPoint::Center;
    }
    if (lowered == "top_left") {
        return AnchorPoint::TopLeft;
    }
    if (lowered == "top_center") {
        return AnchorPoint::TopCenter;
    }
    if (lowered == "top_right") {
        return AnchorPoint::TopRight;
    }
    if (lowered == "bottom_left") {
        return AnchorPoint::BottomLeft;
    }
    if (lowered == "bottom_center") {
        return AnchorPoint::BottomCenter;
    }
    if (lowered == "bottom_right") {
        return AnchorPoint::BottomRight;
    }
    return AnchorPoint::Center;
}

LayoutDirection layoutDirectionFromString(const std::string& text) {
    std::string lowered = text;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    if (lowered == "horizontal") {
        return LayoutDirection::Horizontal;
    }
    return LayoutDirection::Vertical;
}

TextAlign textAlignFromString(const std::string& text) {
    std::string lowered = text;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    if (lowered == "left") {
        return TextAlign::Left;
    }
    if (lowered == "right") {
        return TextAlign::Right;
    }
    return TextAlign::Center;
}

TransitionDefinition parseTransition(const nlohmann::json& object) {
    TransitionDefinition transition;
    if (!object.is_object()) {
        return transition;
    }
    if (auto it = object.find("type"); it != object.end() && it->is_string()) {
        transition.type = it->get<std::string>();
    }
    if (auto it = object.find("duration"); it != object.end() && it->is_number()) {
        transition.duration = static_cast<float>(it->get<double>());
    }
    if (auto it = object.find("delay"); it != object.end() && it->is_number()) {
        transition.delay = static_cast<float>(it->get<double>());
    }
    return transition;
}

MenuLayout parseLayout(const nlohmann::json& object) {
    MenuLayout layout;
    if (!object.is_object()) {
        return layout;
    }
    if (auto anchorValue = object.find("anchor"); anchorValue != object.end() && anchorValue->is_string()) {
        layout.anchor = anchorFromString(anchorValue->get<std::string>());
    }
    if (auto direction = object.find("direction"); direction != object.end() && direction->is_string()) {
        layout.direction = layoutDirectionFromString(direction->get<std::string>());
    }
    if (auto align = object.find("align"); align != object.end() && align->is_string()) {
        layout.align = textAlignFromString(align->get<std::string>());
    }
    if (auto spacingValue = object.find("spacing"); spacingValue != object.end() && spacingValue->is_number()) {
        layout.spacing = static_cast<float>(spacingValue->get<double>());
    }
    if (auto offset = object.find("offset"); offset != object.end() && offset->is_object()) {
        if (auto ox = offset->find("x"); ox != offset->end() && ox->is_number()) {
            layout.offsetX = static_cast<float>(ox->get<double>());
        }
        if (auto oy = offset->find("y"); oy != offset->end() && oy->is_number()) {
            layout.offsetY = static_cast<float>(oy->get<double>());
        }
    }
    return layout;
}

LogoDefinition parseLogo(const nlohmann::json& object) {
    LogoDefinition logo;
    if (!object.is_object()) {
        return logo;
    }
    if (auto texture = object.find("texture"); texture != object.end() && texture->is_string()) {
        logo.textureId = texture->get<std::string>();
    }
    if (auto pos = object.find("position"); pos != object.end() && pos->is_object()) {
        if (auto px = pos->find("x"); px != pos->end() && px->is_number()) {
            logo.normalizedX = static_cast<float>(px->get<double>());
        }
        if (auto py = pos->find("y"); py != pos->end() && py->is_number()) {
            logo.normalizedY = static_cast<float>(py->get<double>());
        }
    }
    if (auto size = object.find("size"); size != object.end() && size->is_object()) {
        if (auto w = size->find("width"); w != size->end() && w->is_number()) {
            logo.width = static_cast<float>(w->get<double>());
        }
        if (auto h = size->find("height"); h != size->end() && h->is_number()) {
            logo.height = static_cast<float>(h->get<double>());
        }
    }
    logo.visible = !logo.textureId.empty();
    return logo;
}

HeaderTextStyle parseHeaderTextStyle(const nlohmann::json& object, const HeaderTextStyle& defaults) {
    HeaderTextStyle style = defaults;
    if (!object.is_object()) {
        return style;
    }
    if (auto color = object.find("color"); color != object.end()) {
        style.color = parseColor(*color, style.color);
    }
    if (auto scale = object.find("scale"); scale != object.end() && scale->is_number()) {
        style.scale = std::max(1, static_cast<int>(scale->get<double>()));
    }
    if (auto y = object.find("y"); y != object.end() && y->is_number()) {
        style.normalizedY = static_cast<float>(y->get<double>());
    }
    if (auto mwr = object.find("maxWidth"); mwr != object.end() && mwr->is_number()) {
        style.maxWidthRatio = static_cast<float>(mwr->get<double>());
    }
    return style;
}

HeaderDefinition parseHeader(const nlohmann::json& object) {
    HeaderDefinition header;
    if (!object.is_object()) {
        return header;
    }
    if (auto title = object.find("title"); title != object.end()) {
        header.title = parseHeaderTextStyle(*title, header.title);
    }
    if (auto subtitle = object.find("subtitle"); subtitle != object.end()) {
        header.subtitle = parseHeaderTextStyle(*subtitle, header.subtitle);
    }
    if (auto spacing = object.find("spacing"); spacing != object.end() && spacing->is_number()) {
        header.spacing = static_cast<float>(spacing->get<double>());
    }
    return header;
}

} // namespace

MenuDefinition MenuLoader::parseDefinition(const std::string& jsonText) {
    const nlohmann::json root = nlohmann::json::parse(jsonText, nullptr, true, true);
    if (!root.is_object()) {
        throw std::runtime_error("Menu definition root must be an object");
    }

    MenuDefinition definition;
    definition.id = root.value("id", "menu");
    if (definition.id.empty()) {
        definition.id = "menu";
    }

    if (auto layoutValue = root.find("layout"); layoutValue != root.end()) {
        definition.layout = parseLayout(*layoutValue);
    }

    if (auto logoValue = root.find("logo"); logoValue != root.end()) {
        LogoDefinition logo = parseLogo(*logoValue);
        if (logo.visible) {
            definition.logo = logo;
        }
    }

    if (auto headerValue = root.find("header"); headerValue != root.end()) {
        HeaderDefinition header = parseHeader(*headerValue);
        definition.header = header;
    }

    if (auto colorsValue = root.find("colors"); colorsValue != root.end() && colorsValue->is_object()) {
        const auto& colorsObj = *colorsValue;
        if (auto normal = colorsObj.find("normal"); normal != colorsObj.end()) {
            definition.colors.normal = parseColor(*normal, definition.colors.normal);
        }
        if (auto selected = colorsObj.find("selected"); selected != colorsObj.end()) {
            definition.colors.selected = parseColor(*selected, definition.colors.selected);
        }
        if (auto disabled = colorsObj.find("disabled"); disabled != colorsObj.end()) {
            definition.colors.disabled = parseColor(*disabled, definition.colors.disabled);
        }
    }

    if (auto transitionValue = root.find("transition"); transitionValue != root.end()) {
        definition.transition = parseTransition(*transitionValue);
    }

    auto itemsValue = root.find("items");
    if (itemsValue == root.end() || !itemsValue->is_array()) {
        throw std::runtime_error("Menu definition missing 'items' array");
    }
    for (const auto& entry : *itemsValue) {
        if (!entry.is_object()) {
            throw std::runtime_error("Menu item must be an object");
        }
        MenuItem item;
        if (auto it = entry.find("label"); it != entry.end() && it->is_string()) {
            item.label = it->get<std::string>();
        }
        if (auto it = entry.find("label_id"); it != entry.end() && it->is_string()) {
            item.labelId = it->get<std::string>();
        }
        if (auto it = entry.find("action"); it != entry.end() && it->is_string()) {
            item.action = it->get<std::string>();
        }
        if (auto it = entry.find("enabled"); it != entry.end() && it->is_boolean()) {
            item.enabled = it->get<bool>();
        }
        if (item.label.empty() && item.labelId.empty()) {
            throw std::runtime_error("Menu item missing label or label_id");
        }
        if (item.action.empty()) {
            item.action = !item.label.empty() ? item.label : item.labelId;
        }
        definition.items.push_back(std::move(item));
    }
    if (auto itemScale = root.find("item_scale"); itemScale != root.end() && itemScale->is_number_integer()) {
        definition.itemScale = itemScale->get<int>();
    }
    if (definition.items.empty()) {
        throw std::runtime_error("Menu definition contains an empty 'items' array");
    }

    return definition;
}

Menu MenuLoader::loadFromString(const std::string& jsonText) {
    try {
        return Menu(parseDefinition(jsonText));
    } catch (const std::exception& e) {
        Logger::error(std::string("Failed to parse menu JSON: ") + e.what(), __func__);
        throw;
    }
}

Menu MenuLoader::loadFromFile(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Unable to open menu file: " + path.string());
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return loadFromString(buffer.str());
}

} // namespace menu
