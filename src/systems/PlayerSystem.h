#pragma once

#include <optional>

#include "../core/Grid.h"
#include "../core/Types.h"

enum class PlayerDeathCause {
    None,
    Enemy,
    Rock
};

struct PlayerEvents {
    int diamondsCollected = 0;
    bool reachedExit = false;
    bool playerDied = false;
    PlayerDeathCause deathCause = PlayerDeathCause::None;
    bool walked = false;
    bool dug = false;
    bool rockFell = false;
    bool rockPushed = false;
    bool rockFallStarted = false;
    bool rockFallLanded = false;
    bool diamondFell = false;
    bool diamondFallStarted = false;
    bool enemyExploded = false;
    bool playerExplosion = false;
    int explosionX = 0;
    int explosionY = 0;
};

struct PlayerSystem {
    static void update(
        Grid& grid,
        std::optional<Direction>& pendingMove,
        PlayerEvents& events,
        bool exitUnlocked);
};
