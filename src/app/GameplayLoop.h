#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "core/Game.h"

class GameplayLoop {
public:
    GameplayLoop(int tickMs, int moveRepeatMs);

    void reset(std::uint32_t now);
    void clearInput();
    void press(Direction direction, std::uint32_t now, Game& game);
    void release(Direction direction);
    bool update(std::uint32_t now, Game& game);

private:
    std::optional<Direction> heldDirection() const;

    int m_tickMs;
    int m_moveRepeatMs;
    std::vector<Direction> m_heldDirections;
    std::uint32_t m_lastTick = 0;
    std::uint32_t m_lastAutoMove = 0;
    std::optional<Direction> m_lastAutoDirection;
};
