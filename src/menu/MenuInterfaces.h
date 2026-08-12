#pragma once

#include <string>

namespace menu {

class IMenuRenderer {
public:
    virtual ~IMenuRenderer() = default;

    virtual void drawText(
        const std::string& text,
        int x,
        int y,
        bool selected,
        bool enabled) = 0;

    virtual void drawImage(
        const std::string& textureId,
        int x,
        int y,
        int width,
        int height) = 0;
};

class IInputProvider {
public:
    virtual ~IInputProvider() = default;
    virtual bool isUpPressed() const = 0;
    virtual bool isDownPressed() const = 0;
    virtual bool isValidatePressed() const = 0;
};

class ITextProvider {
public:
    virtual ~ITextProvider() = default;
    virtual std::string getText(const std::string& id) const = 0;
};

} // namespace menu
