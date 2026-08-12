#include "AudioEventRouter.h"

#include "audio/Audio.h"

void playGameEvents(const std::vector<GameEvent>& events) {
    for (const GameEvent event : events) {
        switch (event) {
        case GameEvent::PlayerWalked:
            Audio::play(SoundId::Walk);
            break;
        case GameEvent::DirtDug:
            Audio::play(SoundId::Dig);
            break;
        case GameEvent::RockMoved:
            Audio::play(SoundId::RockFall);
            break;
        case GameEvent::DiamondCollected:
            Audio::play(SoundId::Diamond);
            break;
        case GameEvent::PlayerDied:
            Audio::play(SoundId::Death);
            break;
        case GameEvent::ExitUnlocked:
            Audio::play(SoundId::ExitUnlock);
            break;
        case GameEvent::DiamondFallStarted:
            Audio::play(SoundId::DiamondFall);
            break;
        case GameEvent::EnemyExploded:
            Audio::play(SoundId::Explosion);
            break;
        case GameEvent::TimeWarning:
            Audio::play(SoundId::TimeWarning);
            break;
        }
    }
}
