#include "GravitySystem.h"

#include "PlayerSystem.h"
#include "../util/Logger.h"

namespace {

bool isHeavy(const Cell& cell) {
    return cell.type == CellType::Rock || cell.type == CellType::Diamond;
}

bool lacksSupport(CellType type) {
    return type == CellType::Empty || type == CellType::Enemy;
}

bool isDestructible(CellType type) {
    return type != CellType::Wall && type != CellType::Exit;
}

void markDeath(PlayerEvents& events, PlayerDeathCause cause) {
    if (!events.playerDied) {
        events.playerDied = true;
        events.deathCause = cause;
    }
}

bool tryRoll(Grid& grid, int x, int y, int direction) {
    const int sideX = x + direction;
    const int sideY = y;
    const int diagX = x + direction;
    const int diagY = y + 1;
    if (!grid.inBounds(sideX, sideY) || !grid.inBounds(diagX, diagY)) {
        return false;
    }
    Cell& side = grid.at(sideX, sideY);
    Cell& diag = grid.at(diagX, diagY);
    if (side.type != CellType::Empty || diag.type != CellType::Empty) {
        return false;
    }
    diag = grid.at(x, y);
    diag.falling = false;
    diag.willFallNext = false;
    grid.at(x, y) = Cell{};
    return true;
}

void explodeArea(Grid& grid, PlayerEvents& events, int centerX, int centerY, bool spawnDiamonds) {
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            const int px = centerX + dx;
            const int py = centerY + dy;
            if (!grid.inBounds(px, py)) {
                continue;
            }
            Cell& cell = grid.at(px, py);
            if (!isDestructible(cell.type)) {
                continue;
            }
            if (cell.type == CellType::Player) {
                markDeath(events, PlayerDeathCause::Rock);
            }
            cell.type = spawnDiamonds ? CellType::Diamond : CellType::Empty;
            cell.falling = false;
            cell.willFallNext = false;
        }
    }
}

void explodeAndClear(Grid& grid, PlayerEvents& events, int centerX, int centerY) {
    explodeArea(grid, events, centerX, centerY, false);
}

void explodeIntoDiamonds(Grid& grid, PlayerEvents& events, int centerX, int centerY) {
    explodeArea(grid, events, centerX, centerY, true);
}

void applyRolls(Grid& grid) {
    if (grid.height() < 2) {
        return;
    }
    for (int y = grid.height() - 2; y >= 0; --y) {
        for (int x = 0; x < grid.width(); ++x) {
            Cell& cell = grid.at(x, y);
            if (!isHeavy(cell) || cell.falling) {
                continue;
            }
            const CellType belowType = grid.at(x, y + 1).type;
            if (lacksSupport(belowType)) {
                continue;
            }
            if (tryRoll(grid, x, y, -1)) {
                continue;
            }
            tryRoll(grid, x, y, 1);
        }
    }
}

void markFalling(Grid& grid) {
    for (int y = 0; y < grid.height(); ++y) {
        for (int x = 0; x < grid.width(); ++x) {
            Cell& cell = grid.at(x, y);
            if (!isHeavy(cell)) {
                cell.willFallNext = false;
                continue;
            }
            if (y + 1 >= grid.height()) {
                cell.willFallNext = false;
                continue;
            }
            const Cell& below = grid.at(x, y + 1);
            cell.willFallNext = lacksSupport(below.type);
        }
    }
}

void moveFalling(
    Grid& grid,
    PlayerEvents& events) {
    if (grid.height() < 2) {
        return;
    }
    for (int y = grid.height() - 2; y >= 0; --y) {
        for (int x = 0; x < grid.width(); ++x) {
            Cell& cell = grid.at(x, y);
            if (!isHeavy(cell) || !cell.falling) {
                continue;
            }
            Cell& below = grid.at(x, y + 1);
            if (below.type == CellType::Empty) {
                below = cell;
                grid.at(x, y) = Cell{};
                events.rockFell = true;
                continue;
            }
            if (below.type == CellType::Player) {
                markDeath(events, PlayerDeathCause::Rock);
                explodeAndClear(grid, events, x, y + 1);
                grid.at(x, y) = Cell{};
                events.rockFell = true;
                continue;
            }
            if (below.type == CellType::Enemy) {
                explodeIntoDiamonds(grid, events, x, y + 1);
                grid.at(x, y) = Cell{};
                events.rockFell = true;
                continue;
            }
            if (cell.type == CellType::Rock) {
                events.rockFallLanded = true;
            }
            cell.falling = false;
        }
    }
}

void finalizeStates(Grid& grid, PlayerEvents& events) {
    for (int y = 0; y < grid.height(); ++y) {
        for (int x = 0; x < grid.width(); ++x) {
            Cell& cell = grid.at(x, y);
            if (!isHeavy(cell)) {
                cell.falling = false;
                cell.willFallNext = false;
            } else {
                const bool wasFalling = cell.falling;
                const bool willFall = cell.willFallNext;
                if (cell.type == CellType::Rock) {
                    if (!wasFalling && willFall) {
                        events.rockFallStarted = true;
                    } else if (wasFalling && !willFall) {
                        events.rockFallLanded = true;
                    }
                }
                cell.falling = willFall;
                cell.willFallNext = false;
            }
        }
    }
}

} // namespace

void GravitySystem::update(Grid& grid, PlayerEvents& events) {
    LOG_T("GravitySystem::update grid=%dx%d", grid.width(), grid.height());
    applyRolls(grid);
    markFalling(grid);
    moveFalling(grid, events);
    finalizeStates(grid, events);
}
