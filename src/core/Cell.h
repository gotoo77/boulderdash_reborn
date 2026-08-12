#pragma once

#include "Types.h"

struct Cell {
    CellType type = CellType::Empty;
    bool falling = false;
    bool willFallNext = false;
    bool enemyMovingRight = true;
    bool playerFacingRight = true;
    bool exitUnlocked = false;
};
