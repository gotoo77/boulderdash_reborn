#include "TestSuites.h"

#include <optional>

#include "systems/EnemySystem.h"
#include "systems/GravitySystem.h"
#include "systems/PlayerSystem.h"

namespace {

void testPlayerDigsAndCollects() {
    Grid grid(3, 1);
    grid.at(0, 0).type = CellType::Player;
    grid.at(1, 0).type = CellType::Dirt;

    std::optional<Direction> move = Direction::Right;
    PlayerEvents events;
    PlayerSystem::update(grid, move, events, false);

    expect(grid.at(0, 0).type == CellType::Empty, "player origin must be cleared");
    expect(grid.at(1, 0).type == CellType::Player, "player must enter dirt");
    expect(events.dug, "digging must emit an event");
    expect(!move.has_value(), "player input must be consumed");

    grid.at(2, 0).type = CellType::Diamond;
    move = Direction::Right;
    PlayerSystem::update(grid, move, events, false);
    expect(grid.at(2, 0).type == CellType::Player, "player must collect a diamond");
    expect(events.diamondsCollected == 1, "diamond collection must be counted");
}

void testPlayerPushesRock() {
    Grid grid(4, 1);
    grid.at(0, 0).type = CellType::Player;
    grid.at(1, 0).type = CellType::Rock;

    std::optional<Direction> move = Direction::Right;
    PlayerEvents events;
    PlayerSystem::update(grid, move, events, false);

    expect(grid.at(1, 0).type == CellType::Player, "player must occupy the rock origin");
    expect(grid.at(2, 0).type == CellType::Rock, "rock must move into the empty cell");
    expect(events.rockPushed, "rock push must emit an event");
}

void testGravityHasDeterministicSteps() {
    Grid grid(3, 4);
    grid.at(1, 0).type = CellType::Rock;
    PlayerEvents events;

    GravitySystem::update(grid, events);
    expect(grid.at(1, 0).type == CellType::Rock, "rock must be armed before its first fall");
    expect(grid.at(1, 0).falling, "unsupported rock must enter falling state");
    expect(events.rockFallStarted, "fall start must emit an event");

    events = PlayerEvents{};
    GravitySystem::update(grid, events);
    expect(grid.at(1, 0).type == CellType::Empty, "falling rock origin must be cleared");
    expect(grid.at(1, 1).type == CellType::Rock, "falling rock must move by one cell per step");
    expect(events.rockFell, "rock movement must emit an event");
}

void testFallingRockKillsPlayerAndClearsExplosionArea() {
    Grid grid(5, 5);
    grid.at(2, 1).type = CellType::Rock;
    grid.at(2, 1).falling = true;
    grid.at(2, 2).type = CellType::Player;
    grid.at(1, 2).type = CellType::Dirt;
    grid.at(3, 2).type = CellType::WallDestructible;
    grid.at(1, 3).type = CellType::Wall;
    grid.at(3, 3).type = CellType::Exit;
    PlayerEvents events;

    GravitySystem::update(grid, events);

    expect(events.playerDied, "a falling rock must kill the player below it");
    expect(events.deathCause == PlayerDeathCause::Rock,
        "falling rock collision must report the rock death cause");
    expect(grid.at(2, 2).type == CellType::Empty, "player cell must be cleared by explosion");
    expect(grid.at(1, 2).type == CellType::Empty, "explosion must clear dirt");
    expect(grid.at(3, 2).type == CellType::Empty, "explosion must clear destructible walls");
    expect(grid.at(1, 3).type == CellType::Wall, "explosion must preserve solid walls");
    expect(grid.at(3, 3).type == CellType::Exit, "explosion must preserve the exit");
}

void testFallingRockExplodesEnemyIntoDiamonds() {
    Grid grid(5, 5);
    grid.at(2, 1).type = CellType::Rock;
    grid.at(2, 1).falling = true;
    grid.at(2, 2).type = CellType::Enemy;
    grid.at(1, 2).type = CellType::Dirt;
    grid.at(3, 2).type = CellType::WallDestructible;
    grid.at(1, 3).type = CellType::Wall;
    grid.at(3, 3).type = CellType::Exit;
    PlayerEvents events;

    GravitySystem::update(grid, events);

    expect(events.enemyExploded, "a falling rock must emit an enemy explosion event");
    expect(countCells(grid, CellType::Enemy) == 0, "explosion must remove the enemy");
    expect(grid.at(2, 2).type == CellType::Diamond, "enemy cell must become a diamond");
    expect(grid.at(1, 2).type == CellType::Diamond, "explosion must turn dirt into diamonds");
    expect(grid.at(3, 2).type == CellType::Diamond,
        "explosion must turn destructible walls into diamonds");
    expect(grid.at(1, 3).type == CellType::Wall, "diamond explosion must preserve solid walls");
    expect(grid.at(3, 3).type == CellType::Exit, "diamond explosion must preserve the exit");
}

void testEnemyReversesAtBoundary() {
    Grid grid(3, 1);
    grid.at(0, 0).type = CellType::Enemy;
    grid.at(0, 0).enemyMovingRight = false;
    PlayerEvents events;

    EnemySystem::update(grid, events);

    expect(grid.at(0, 0).type == CellType::Empty, "enemy origin must be cleared");
    expect(grid.at(1, 0).type == CellType::Enemy, "enemy must reverse away from a boundary");
    expect(grid.at(1, 0).enemyMovingRight, "enemy direction must reflect the reversal");
}

} // namespace

std::vector<TestCase> systemTests() {
    return {
        { "player digs and collects", testPlayerDigsAndCollects },
        { "player pushes rock", testPlayerPushesRock },
        { "gravity has deterministic steps", testGravityHasDeterministicSteps },
        { "falling rock kills player and clears explosion area", testFallingRockKillsPlayerAndClearsExplosionArea },
        { "falling rock explodes enemy into diamonds", testFallingRockExplodesEnemyIntoDiamonds },
        { "enemy reverses at boundary", testEnemyReversesAtBoundary },
    };
}
