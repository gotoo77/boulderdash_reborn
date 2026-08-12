#include "PlayerSystem.h"

#include "../util/Logger.h"

namespace {

bool canEnter(CellType type, bool exitUnlocked) {
    switch (type) {
    case CellType::Empty:
    case CellType::Dirt:
    case CellType::Diamond:
        return true;
    case CellType::Exit:
        return exitUnlocked;
    default:
        return false;
    }
}

bool tryPushRock(
    Grid& grid,
    int playerX,
    int playerY,
    int dx,
    PlayerEvents& events) {
    if (dx == 0) {
        return false;
    }
    const int rockX = playerX + dx;
    if (!grid.inBounds(rockX, playerY)) {
        return false;
    }
    Cell& player = grid.at(playerX, playerY);
    Cell& rock = grid.at(rockX, playerY);
    if (rock.type != CellType::Rock || rock.falling) {
        return false;
    }
    const int landingX = rockX + dx;
    if (!grid.inBounds(landingX, playerY)) {
        return false;
    }
    Cell& landing = grid.at(landingX, playerY);
    if (landing.type != CellType::Empty) {
        return false;
    }

    landing = rock;
    landing.falling = false;
    landing.willFallNext = false;
    rock = Cell{};
    Cell& destination = grid.at(rockX, playerY);
    destination = Cell{};
    destination.type = CellType::Player;
    destination.playerFacingRight = dx > 0 ? true : dx < 0 ? false : player.playerFacingRight;
    destination.falling = false;
    destination.willFallNext = false;
    player = Cell{};
    events.walked = true;
    events.rockPushed = true;
    return true;
}

} // namespace

void PlayerSystem::update(
    Grid& grid,
    std::optional<Direction>& pendingMove,
    PlayerEvents& events,
    bool exitUnlocked) {
    LOG_T("PlayerSystem::update pending=%s exitUnlocked=%d", pendingMove ? "yes" : "no", exitUnlocked ? 1 : 0);
    if (!pendingMove.has_value()) {
        return;
    }

    int dx = 0;
    int dy = 0;
    switch (*pendingMove) {
    case Direction::Up:
        dy = -1;
        break;
    case Direction::Down:
        dy = 1;
        break;
    case Direction::Left:
        dx = -1;
        break;
    case Direction::Right:
        dx = 1;
        break;
    }

    bool moved = false;
    for (int y = 0; y < grid.height() && !moved; ++y) {
        for (int x = 0; x < grid.width(); ++x) {
            Cell& cell = grid.at(x, y);
            if (cell.type != CellType::Player) {
                continue;
            }
            const int targetX = x + dx;
            const int targetY = y + dy;
            if (!grid.inBounds(targetX, targetY)) {
                moved = true; // consume input even if invalid
                break;
            }

            Cell& target = grid.at(targetX, targetY);
            if (target.type == CellType::Enemy) {
                grid.at(x, y) = Cell{};
                events.playerDied = true;
                events.deathCause = PlayerDeathCause::Enemy;
                events.playerExplosion = true;
                events.explosionX = targetX;
                events.explosionY = targetY;
                pendingMove.reset();
                return;
            }
            if (target.type == CellType::Rock && dy == 0) {
                if (tryPushRock(grid, x, y, dx, events)) {
                    moved = true;
                } else {
                    moved = true;
                }
                break;
            }
            if (!canEnter(target.type, exitUnlocked)) {
                moved = true;
                break;
            }

            const CellType targetType = target.type;
            if (targetType == CellType::Diamond) {
                ++events.diamondsCollected;
            } else if (targetType == CellType::Exit) {
                events.reachedExit = true;
            }

            bool nextFacingRight = cell.playerFacingRight;
            if (dx < 0) {
                nextFacingRight = false;
            } else if (dx > 0) {
                nextFacingRight = true;
            }
            target = Cell{};
            target.type = CellType::Player;
            target.playerFacingRight = nextFacingRight;
            target.falling = false;
            target.willFallNext = false;
            grid.at(x, y) = Cell{};
            if (targetType == CellType::Dirt) {
                events.dug = true;
            } else if (targetType == CellType::Empty || targetType == CellType::Exit) {
                events.walked = true;
            }
            moved = true;
            break;
        }
    }

    pendingMove.reset();
}
