#include "TestSuites.h"

#include "app/GameplayLoop.h"

namespace {

void testGameLoadsLevelAndExpiresTimer() {
    TemporaryLevels levels;
    const auto level = levels.write("timer.txt", "#####\n#P.E#\n#####\n");
    auto rules = deterministicRules();
    rules.timeLimitMs = 1000;
    Game game({ level }, rules);

    expect(game.currentLevelNumber() == 1, "game must load the first level");
    expect(game.currentLevelName() == "timer.txt", "game must expose the loaded level name");
    expect(game.levelCount() == 1, "game must expose the number of levels");
    expect(game.remainingLives() == 3, "game must start with the configured lives");
    expect(game.timeRemainingMs() == 1000, "timer must start at its configured limit");
    expect(exitCellIsUnlocked(game.grid()), "door must start open when no diamond is required");

    game.update(250);
    expect(game.elapsedMs() == 250, "timer must accumulate deterministic updates");
    expect(game.timeRemainingMs() == 750, "remaining time must decrease with updates");
    expect(!game.levelFailed(), "level must remain active before the time limit");

    game.update(750);
    expect(game.elapsedMs() == 1000, "timer must stop at its configured limit");
    expect(game.timeRemainingMs() == 0, "timer must not become negative");
    expect(game.levelFailed(), "level must fail when the timer expires");
}

void testGameRejectsMultipleExits() {
    TemporaryLevels levels;
    const auto level = levels.write("two-exits.txt", "#####\n#PEE#\n#####\n");
    bool rejected = false;
    try {
        Game game({ level }, deterministicRules());
        (void)game;
    } catch (const std::runtime_error& error) {
        rejected = std::string(error.what()).find("2 exit(s)") != std::string::npos;
    }
    expect(rejected, "runtime loader must reject a level containing two exits");
}

void testGameScoresAndAdvancesLevels() {
    TemporaryLevels levels;
    const auto first = levels.write("first.txt", "#####\n#PoE#\n#####\n");
    const auto second = levels.write("second.txt", "#####\n#PE##\n#####\n");
    const auto third = levels.write("third.txt", "#####\n#PE##\n#####\n");
    const auto fourth = levels.write("fourth.txt", "#####\n#PE##\n#####\n");
    const auto rules = deterministicRules();
    Game game({ first, second, third, fourth }, rules);

    expect(game.totalDiamonds() == 1, "first level must count its diamond");
    expect(!exitCellIsUnlocked(game.grid()), "door must start locked while diamonds remain");
    game.queueMove(Direction::Right);
    game.update(1000);
    expect(game.collectedDiamonds() == 1, "diamond collection must update the game state");
    expect(game.levelScore() == 10, "diamond value must be added to the level score");
    expect(game.totalScore() == 10, "unbanked level score must be visible in total score");
    expect(game.exitUnlocked(), "collecting every diamond must unlock the exit");
    expect(exitCellIsUnlocked(game.grid()), "door cell must expose its unlocked visual state");
    expect(countEvent(game.consumeEvents(), GameEvent::ExitUnlocked) == 1,
        "collecting the final diamond must emit one exit unlock event");

    game.update(0);
    expect(countEvent(game.consumeEvents(), GameEvent::ExitUnlocked) == 0,
        "exit unlock event must not repeat on later updates");

    game.queueMove(Direction::Right);
    game.update(0);
    expect(game.levelComplete(), "entering an unlocked exit must complete the level");
    expect(game.levelScore() == 155, "exit and remaining-time bonuses must be deterministic");
    expect(game.totalScore() == 155, "completed level score must be banked exactly once");

    expect(game.advanceToNextLevel(), "a completed level must advance when another exists");
    expect(game.currentLevelNumber() == 2, "next level index must be loaded");
    expect(game.currentLevelName() == "second.txt", "next level name must be exposed");
    expect(game.levelScore() == 0, "level score must reset after advancing");
    expect(game.totalScore() == 155, "banked score must survive a level change");

    const std::vector<std::string> remainingLevelNames = {
        "second.txt", "third.txt", "fourth.txt"
    };
    int expectedScore = 155;
    for (std::size_t index = 0; index < remainingLevelNames.size(); ++index) {
        expect(
            game.currentLevelName() == remainingLevelNames[index],
            "four-level progression must load levels in order");
        game.queueMove(Direction::Right);
        game.update(0);
        expect(game.levelComplete(), "each remaining level exit must be reachable");
        expectedScore += 150;
        expect(game.totalScore() == expectedScore, "score must accumulate through all four levels");
        if (index + 1 < remainingLevelNames.size()) {
            expect(game.advanceToNextLevel(), "intermediate levels must advance");
        }
    }
    expect(game.currentLevelNumber() == 4, "progression must reach the fourth level");
    expect(!game.advanceToNextLevel(), "advancing past the last level must signal victory");
    expect(game.totalScore() == 605, "victory probing must not bank the score twice");
}

void testGameConsumesLivesAndRespawns() {
    TemporaryLevels levels;
    const auto level = levels.write("respawn.txt", "#####\n#PXE#\n#####\n");
    const auto rules = deterministicRules();
    Game game({ level }, rules);

    auto collideWithEnemy = [&]() {
        game.queueMove(Direction::Right);
        game.update(1);
    };

    collideWithEnemy();
    expect(game.remainingLives() == 2, "first death must consume one life");
    expect(!game.levelFailed(), "game must wait for respawn while lives remain");
    expect(countCells(game.grid(), CellType::Player) == 0, "dead player must leave the grid");

    game.update(99);
    expect(countCells(game.grid(), CellType::Player) == 0, "respawn must respect its delay");
    game.update(1);
    expect(countCells(game.grid(), CellType::Player) == 1, "player must respawn after the delay");
    expect(game.remainingLives() == 2, "respawn must preserve remaining lives");
    expect(game.elapsedMs() == 0, "respawn must restart the level timer");

    collideWithEnemy();
    expect(game.remainingLives() == 1, "second death must consume another life");
    game.update(100);
    collideWithEnemy();
    expect(game.remainingLives() == 0, "last death must consume the final life");
    expect(game.levelFailed(), "game must fail when no lives remain");
}

void testFallingDiamondAndEnemyExplosionEmitEvents() {
    TemporaryLevels levels;
    const auto level = levels.write(
        "diamond-explosion.txt",
        "#####\n#.o.#\n#.X.#\n#P.E#\n#####\n");
    auto rules = deterministicRules();
    rules.gravityStepMs = 0;
    Game game({ level }, rules);

    game.update(0);
    const auto firstEvents = game.consumeEvents();
    expect(countEvent(firstEvents, GameEvent::DiamondFallStarted) == 1,
        "a diamond starting to fall must emit its dedicated event once");
    expect(countEvent(firstEvents, GameEvent::EnemyExploded) == 0,
        "explosion event must wait for the enemy collision");

    game.update(0);
    const auto secondEvents = game.consumeEvents();
    expect(countEvent(secondEvents, GameEvent::DiamondFallStarted) == 0,
        "diamond fall event must not repeat while the same diamond keeps falling");
    expect(countEvent(secondEvents, GameEvent::EnemyExploded) == 1,
        "a falling diamond hitting an enemy must emit one explosion event");
    expect(countCells(game.grid(), CellType::Enemy) == 0,
        "diamond collision explosion must remove the enemy");
}

void testTimeWarningEmitsOncePerSecond() {
    TemporaryLevels levels;
    const auto level = levels.write("timer-warning.txt", "#####\n#P.E#\n#####\n");
    auto rules = deterministicRules();
    rules.timeLimitMs = 16000;
    Game game({ level }, rules);

    game.update(999);
    expect(countEvent(game.consumeEvents(), GameEvent::TimeWarning) == 0,
        "timer warning event must remain absent above fifteen seconds");
    game.update(1);
    expect(countEvent(game.consumeEvents(), GameEvent::TimeWarning) == 1,
        "timer warning event must start at fifteen seconds remaining");
    game.update(400);
    expect(countEvent(game.consumeEvents(), GameEvent::TimeWarning) == 0,
        "timer warning event must not repeat within the same second");
    game.update(600);
    expect(countEvent(game.consumeEvents(), GameEvent::TimeWarning) == 1,
        "timer warning event must repeat on the next countdown second");
    game.update(14000);
    expect(game.levelFailed(), "timer warning scenario must still expire normally");
    expect(countEvent(game.consumeEvents(), GameEvent::TimeWarning) == 0,
        "time expiration must not add a late warning event");
}

void testPauseFreezesTimerAndDiscardsInput() {
    TemporaryLevels levels;
    const auto level = levels.write("pause.txt", "#####\n#P.E#\n#####\n");
    auto rules = deterministicRules();
    rules.timeLimitMs = 20000;
    Game game({ level }, rules);

    game.update(1000);
    expect(game.elapsedMs() == 1000, "timer must advance before pause");
    game.setPaused(true);
    expect(game.paused(), "game must expose its paused state");
    game.queueMove(Direction::Right);
    game.update(5000);
    expect(game.elapsedMs() == 1000, "timer must remain frozen while paused");
    expect(game.grid().at(1, 1).type == CellType::Player, "paused input must be discarded");

    game.setPaused(false);
    game.update(1000);
    expect(!game.paused(), "game must leave its paused state");
    expect(game.elapsedMs() == 2000, "timer must resume without counting paused time");
    expect(game.grid().at(1, 1).type == CellType::Player,
        "discarded paused input must not execute after resume");
}

void testGameplayLoopOwnsTickAndInputRepeat() {
    TemporaryLevels levels;
    const auto level = levels.write("gameplay-loop.txt", "######\n#P..E#\n######\n");
    Game game({ level }, deterministicRules());
    GameplayLoop loop(10, 20);
    loop.reset(100);

    loop.press(Direction::Right, 100, game);
    expect(!loop.update(109, game), "gameplay loop must wait until the configured tick");
    expect(game.grid().at(1, 1).type == CellType::Player, "input must wait for a game tick");
    expect(loop.update(110, game), "gameplay loop must run at the configured tick");
    expect(game.grid().at(2, 1).type == CellType::Player, "pressed direction must reach Game");

    expect(loop.update(130, game), "gameplay loop must keep ticking while input is held");
    expect(game.grid().at(3, 1).type == CellType::Player, "held direction must repeat after its delay");
    loop.release(Direction::Right);
    loop.update(150, game);
    expect(game.grid().at(3, 1).type == CellType::Player, "released direction must stop repeating");
}

} // namespace

std::vector<TestCase> gameTests() {
    return {
        { "game loads level and expires timer", testGameLoadsLevelAndExpiresTimer },
        { "game rejects multiple exits", testGameRejectsMultipleExits },
        { "game scores and advances levels", testGameScoresAndAdvancesLevels },
        { "game consumes lives and respawns", testGameConsumesLivesAndRespawns },
        { "falling diamond and enemy explosion emit events", testFallingDiamondAndEnemyExplosionEmitEvents },
        { "time warning emits once per second", testTimeWarningEmitsOncePerSecond },
        { "pause freezes timer and discards input", testPauseFreezesTimerAndDiscardsInput },
        { "gameplay loop owns tick and input repeat", testGameplayLoopOwnsTickAndInputRepeat },
    };
}
