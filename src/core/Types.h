#pragma once

#include <cstdint>

enum class CellType : std::uint8_t {
    Empty,
    Dirt,
    Wall,
    WallDestructible,
    Rock,
    Diamond,
    Player,
    Enemy,
    Exit
};

enum class Direction : std::uint8_t {
    Up,
    Down,
    Left,
    Right
};
