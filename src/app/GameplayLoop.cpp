#include "GameplayLoop.h"

#include <algorithm>

GameplayLoop::GameplayLoop(int tickMs, int moveRepeatMs)
    : m_tickMs(std::max(0, tickMs)),
      m_moveRepeatMs(std::max(0, moveRepeatMs)) {
}

void GameplayLoop::reset(std::uint32_t now) {
    clearInput();
    m_lastTick = now;
    m_lastAutoMove = now;
}

void GameplayLoop::clearInput() {
    m_heldDirections.clear();
    m_lastAutoDirection.reset();
}

void GameplayLoop::press(Direction direction, std::uint32_t now, Game& game) {
    const auto existing = std::find(m_heldDirections.begin(), m_heldDirections.end(), direction);
    if (existing != m_heldDirections.end()) {
        m_heldDirections.erase(existing);
    }
    m_heldDirections.push_back(direction);
    game.queueMove(direction);
    m_lastAutoDirection = direction;
    m_lastAutoMove = now;
}

void GameplayLoop::release(Direction direction) {
    const auto existing = std::find(m_heldDirections.begin(), m_heldDirections.end(), direction);
    if (existing != m_heldDirections.end()) {
        m_heldDirections.erase(existing);
    }
    if (m_lastAutoDirection && *m_lastAutoDirection == direction) {
        m_lastAutoDirection.reset();
    }
}

std::optional<Direction> GameplayLoop::heldDirection() const {
    if (m_heldDirections.empty()) {
        return std::nullopt;
    }
    return m_heldDirections.back();
}

bool GameplayLoop::update(std::uint32_t now, Game& game) {
    if (const auto held = heldDirection()) {
        if (!m_lastAutoDirection || *m_lastAutoDirection != *held) {
            m_lastAutoDirection = held;
            m_lastAutoMove = now;
        } else if (now - m_lastAutoMove >= static_cast<std::uint32_t>(m_moveRepeatMs)) {
            game.queueMove(*held);
            m_lastAutoMove = now;
        }
    } else {
        m_lastAutoDirection.reset();
    }

    if (now - m_lastTick < static_cast<std::uint32_t>(m_tickMs)) {
        return false;
    }
    game.update(now - m_lastTick);
    m_lastTick = now;
    return true;
}
