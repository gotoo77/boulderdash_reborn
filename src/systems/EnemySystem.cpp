#include "EnemySystem.h"

#include "../util/Logger.h"

namespace {

bool canOccupy(const Cell& cell) {
    return cell.type == CellType::Empty || cell.type == CellType::Player;
}

void markDeath(PlayerEvents& events, int x, int y) {
    if (!events.playerDied) {
        events.playerDied = true;
        events.deathCause = PlayerDeathCause::Enemy;
    }
    events.playerExplosion = true;
    events.explosionX = x;
    events.explosionY = y;
}

} // namespace

void EnemySystem::update(Grid& grid, PlayerEvents& events) {
    LOG_T("EnemySystem::update grid=%dx%d", grid.width(), grid.height());
    if (grid.width() <= 0) {
        return;
    }

    for (int y = 0; y < grid.height(); ++y) {
        for (int x = 0; x < grid.width(); ++x) {
            Cell& cell = grid.at(x, y);
            if (cell.type != CellType::Enemy) {
                continue;
            }

            auto attemptMove = [&](int step) -> bool {
                const int targetX = x + step;
                if (!grid.inBounds(targetX, y)) {
                    return false;
                }
                Cell& target = grid.at(targetX, y);
                if (!canOccupy(target)) {
                    return false;
                }

                if (target.type == CellType::Player) {
                    markDeath(events, targetX, y);
                }

                Cell movedEnemy = cell;
                movedEnemy.enemyMovingRight = step > 0;
                grid.at(targetX, y) = movedEnemy;
                grid.at(x, y) = Cell{};
                if (step > 0) {
                    ++x;
                }
                return true;
            };

            const int preferredStep = cell.enemyMovingRight ? 1 : -1;
            if (attemptMove(preferredStep)) {
                continue;
            }

            cell.enemyMovingRight = !cell.enemyMovingRight;
            attemptMove(cell.enemyMovingRight ? 1 : -1);
        }
    }
}
