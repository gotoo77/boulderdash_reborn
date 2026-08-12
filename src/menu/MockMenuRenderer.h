#pragma once

#include <vector>

#include "MenuInterfaces.h"

namespace menu {

struct TextDrawCall {
    std::string text;
    int x = 0;
    int y = 0;
    bool selected = false;
    bool enabled = true;
};

struct ImageDrawCall {
    std::string textureId;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

class MockMenuRenderer : public IMenuRenderer {
public:
    void drawText(const std::string& text, int x, int y, bool selected, bool enabled) override;
    void drawImage(const std::string& textureId, int x, int y, int width, int height) override;

    const std::vector<TextDrawCall>& textCalls() const { return m_textCalls; }
    const std::vector<ImageDrawCall>& imageCalls() const { return m_imageCalls; }
    void reset();

private:
    std::vector<TextDrawCall> m_textCalls;
    std::vector<ImageDrawCall> m_imageCalls;
};

class MockInputProvider : public IInputProvider {
public:
    void setUp(bool state) { m_up = state; }
    void setDown(bool state) { m_down = state; }
    void setValidate(bool state) { m_validate = state; }

    bool isUpPressed() const override { return m_up; }
    bool isDownPressed() const override { return m_down; }
    bool isValidatePressed() const override { return m_validate; }

private:
    bool m_up = false;
    bool m_down = false;
    bool m_validate = false;
};

} // namespace menu
