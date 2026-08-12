#pragma once

#include "../core/Grid.h"
#include "../core/Types.h"

#include "PlayerSystem.h"

struct EnemySystem {
    static void update(Grid& grid, PlayerEvents& events);
};
