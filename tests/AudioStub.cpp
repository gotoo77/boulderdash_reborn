#include "audio/Audio.h"

#include <array>
#include <cstddef>

namespace {

std::array<int, static_cast<std::size_t>(SoundId::Count)> playCounts{};

} // namespace

void Audio::play(SoundId id) {
    ++playCounts[static_cast<std::size_t>(id)];
}

void resetTestAudio() {
    playCounts.fill(0);
}

int testAudioPlayCount(SoundId id) {
    return playCounts[static_cast<std::size_t>(id)];
}
