#pragma once

#include "../core/Grid.h"
#include "../core/Types.h"

struct PlayerEvents;

struct GravitySystem {
    static void update(Grid& grid, PlayerEvents& events);
};
