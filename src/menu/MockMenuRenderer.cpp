#include "MockMenuRenderer.h"

namespace menu {

void MockMenuRenderer::drawText(const std::string& text, int x, int y, bool selected, bool enabled) {
    m_textCalls.push_back(TextDrawCall{ text, x, y, selected, enabled });
}

void MockMenuRenderer::drawImage(const std::string& textureId, int x, int y, int width, int height) {
    m_imageCalls.push_back(ImageDrawCall{ textureId, x, y, width, height });
}

void MockMenuRenderer::reset() {
    m_textCalls.clear();
    m_imageCalls.clear();
}

} // namespace menu
