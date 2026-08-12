#include "OptionsStyle.h"

#include <algorithm>
#include <cstdint>
#include <exception>
#include <fstream>
#include <string>

#include <nlohmann/json.hpp>

#include "util/Logger.h"

namespace {

SDL_Color parseColor(const nlohmann::json& value, SDL_Color fallback) {
    auto clamp = [](int v) {
        return static_cast<std::uint8_t>(std::max(0, std::min(255, v)));
    };
    if (value.is_string()) {
        std::string hex = value.get<std::string>();
        if (!hex.empty() && hex[0] == '#') {
            hex.erase(0, 1);
        }
        if (hex.size() == 6 || hex.size() == 8) {
            try {
                const int r = std::stoi(hex.substr(0, 2), nullptr, 16);
                const int g = std::stoi(hex.substr(2, 2), nullptr, 16);
                const int b = std::stoi(hex.substr(4, 2), nullptr, 16);
                const int a = (hex.size() == 8) ? std::stoi(hex.substr(6, 2), nullptr, 16) : 255;
                return SDL_Color{ clamp(r), clamp(g), clamp(b), clamp(a) };
            } catch (...) {
                return fallback;
            }
        }
    } else if (value.is_array() && value.size() >= 3) {
        const int r = static_cast<int>(value[0].get<double>());
        const int g = static_cast<int>(value[1].get<double>());
        const int b = static_cast<int>(value[2].get<double>());
        const int a = value.size() > 3 ? static_cast<int>(value[3].get<double>()) : 255;
        return SDL_Color{ clamp(r), clamp(g), clamp(b), clamp(a) };
    } else if (value.is_object()) {
        const int r = value.value("r", static_cast<int>(fallback.r));
        const int g = value.value("g", static_cast<int>(fallback.g));
        const int b = value.value("b", static_cast<int>(fallback.b));
        const int a = value.value("a", static_cast<int>(fallback.a));
        return SDL_Color{ clamp(r), clamp(g), clamp(b), clamp(a) };
    }
    return fallback;
}

} // namespace

OptionsScreenStyle loadOptionsStyle(const std::filesystem::path& path) {
    OptionsScreenStyle style;
    if (!std::filesystem::exists(path)) {
        return style;
    }
    try {
        std::ifstream input(path);
        nlohmann::json root;
        input >> root;
        if (auto title = root.find("title"); title != root.end() && title->is_object()) {
            style.title.y = title->value("y", style.title.y);
            style.title.scale = title->value("scale", style.title.scale);
            style.title.maxWidth = title->value("maxWidth", style.title.maxWidth);
            if (auto color = title->find("color"); color != title->end()) {
                style.title.color = parseColor(*color, style.title.color);
            }
        }
        if (auto lines = root.find("lines"); lines != root.end() && lines->is_object()) {
            style.lines.y = lines->value("startY", style.lines.y);
            style.lineSpacing = lines->value("spacing", style.lineSpacing);
            style.lines.scale = lines->value("scale", style.lines.scale);
            style.lines.maxWidth = lines->value("maxWidth", style.lines.maxWidth);
            if (auto color = lines->find("color"); color != lines->end()) {
                style.lines.color = parseColor(*color, style.lines.color);
            }
        }
        if (auto hints = root.find("hints"); hints != root.end() && hints->is_object()) {
            style.hints.y = hints->value("startY", style.hints.y);
            style.hintSpacing = hints->value("spacing", style.hintSpacing);
            style.hints.scale = hints->value("scale", style.hints.scale);
            style.hints.maxWidth = hints->value("maxWidth", style.hints.maxWidth);
            if (auto color = hints->find("color"); color != hints->end()) {
                style.hints.color = parseColor(*color, style.hints.color);
            }
        }
    } catch (const std::exception& e) {
        Logger::warn(std::string("Failed to load options menu style: ") + e.what(), __func__);
    }
    return style;
}
