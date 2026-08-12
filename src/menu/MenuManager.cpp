#include "MenuManager.h"

#include <algorithm>
#include <cmath>

namespace menu {
namespace {

std::pair<float, float> anchorNormalized(AnchorPoint anchor) {
    switch (anchor) {
    case AnchorPoint::TopLeft:
        return { 0.0f, 0.0f };
    case AnchorPoint::TopCenter:
        return { 0.5f, 0.0f };
    case AnchorPoint::TopRight:
        return { 1.0f, 0.0f };
    case AnchorPoint::BottomLeft:
        return { 0.0f, 1.0f };
    case AnchorPoint::BottomCenter:
        return { 0.5f, 1.0f };
    case AnchorPoint::BottomRight:
        return { 1.0f, 1.0f };
    case AnchorPoint::Center:
    default:
        return { 0.5f, 0.5f };
    }
}

} // namespace

void MenuManager::setMenu(Menu menu) {
    m_menu = std::move(menu);
    m_selectedIndex = 0;
    m_pendingAction.reset();
}

void MenuManager::moveSelection(int delta) {
    if (!m_menu) {
        return;
    }
    const auto& items = m_menu->items();
    if (items.empty()) {
        m_selectedIndex = 0;
        return;
    }
    const int total = static_cast<int>(items.size());
    int next = m_selectedIndex;
    for (int i = 0; i < total; ++i) {
        next = (next + delta + total) % total;
        if (items[next].enabled) {
            m_selectedIndex = next;
            return;
        }
    }
    m_selectedIndex = next;
}

void MenuManager::update(const IInputProvider& input) {
    if (!m_menu) {
        return;
    }
    const bool upPressed = input.isUpPressed();
    const bool downPressed = input.isDownPressed();
    const bool validatePressed = input.isValidatePressed();

    if (upPressed && !m_prevUp) {
        moveSelection(-1);
    }
    if (downPressed && !m_prevDown) {
        moveSelection(1);
    }
    if (validatePressed && !m_prevValidate) {
        const auto& items = m_menu->items();
        if (!items.empty() && m_selectedIndex >= 0 && m_selectedIndex < static_cast<int>(items.size())) {
            const MenuItem& item = items[m_selectedIndex];
            if (item.enabled) {
                m_pendingAction = item.action;
            }
        }
    }

    m_prevUp = upPressed;
    m_prevDown = downPressed;
    m_prevValidate = validatePressed;
}

std::optional<std::string> MenuManager::consumeAction() {
    if (!m_pendingAction) {
        return std::nullopt;
    }
    auto action = m_pendingAction;
    m_pendingAction.reset();
    return action;
}

void MenuManager::render(IMenuRenderer& renderer, const MenuRenderMetrics& metrics) const {
    if (!m_menu) {
        return;
    }
    const float scale =
        std::max(0.0f, metrics.uiScale) *
        (metrics.pixelRatio.has_value() ? std::max(0.0f, metrics.pixelRatio.value()) : 1.0f);
    const float appliedScale = scale > 0.0f ? scale : 1.0f;
    const auto toInt = [](float value) {
        return static_cast<int>(std::lround(value));
    };
    const auto resolveLabel = [&](const MenuItem& item) -> std::string {
        if (!item.labelId.empty() && m_textProvider) {
            std::string translated = m_textProvider->getText(item.labelId);
            if (!translated.empty()) {
                return translated;
            }
        }
        return item.label;
    };

    const auto& items = m_menu->items();
    if (const auto& logo = m_menu->logo()) {
        const int logoWidth = logo->width > 0.0f ? toInt(logo->width * appliedScale) : 0;
        const int logoHeight = logo->height > 0.0f ? toInt(logo->height * appliedScale) : 0;
        renderer.drawImage(
            logo->textureId,
            toInt(logo->normalizedX * static_cast<float>(metrics.viewportWidth)),
            toInt(logo->normalizedY * static_cast<float>(metrics.viewportHeight)),
            logoWidth,
            logoHeight);
    }

    if (items.empty()) {
        return;
    }

    const auto anchor = anchorNormalized(m_menu->layout().anchor);
    const float anchorX =
        anchor.first * static_cast<float>(metrics.viewportWidth) + m_menu->layout().offsetX * appliedScale;
    const float anchorY =
        anchor.second * static_cast<float>(metrics.viewportHeight) + m_menu->layout().offsetY * appliedScale;
    const float spacing = m_menu->layout().spacing * appliedScale;
    const float totalSpacing = spacing * static_cast<float>(items.size() > 0 ? items.size() - 1 : 0);

    if (m_menu->layout().direction == LayoutDirection::Horizontal) {
        const float leftX = anchorX - totalSpacing * 0.5f;
        for (std::size_t i = 0; i < items.size(); ++i) {
            const float x = leftX + spacing * static_cast<float>(i);
            const bool selected = static_cast<int>(i) == m_selectedIndex;
            renderer.drawText(resolveLabel(items[i]), toInt(x), toInt(anchorY), selected, items[i].enabled);
        }
    } else {
        const float topY = anchorY - totalSpacing * 0.5f;
        for (std::size_t i = 0; i < items.size(); ++i) {
            const float y = topY + spacing * static_cast<float>(i);
            const bool selected = static_cast<int>(i) == m_selectedIndex;
            renderer.drawText(resolveLabel(items[i]), toInt(anchorX), toInt(y), selected, items[i].enabled);
        }
    }
}

} // namespace menu
