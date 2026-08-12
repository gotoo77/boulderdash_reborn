#!/usr/bin/env python3
"""Generate original retro SFX for Boulderdash Reborn.

The generated files are intentionally synthetic and do not sample or decode
audio from Boulder Dash or any other game. They use simple oscillators,
deterministic pseudo-random noise and envelopes from Python's standard library.
"""
from __future__ import annotations

import argparse
import math
import random
import struct
import wave
from pathlib import Path

SAMPLE_RATE = 22_050
MASTER_PEAK = 0.82


def clamp(value: float, low: float = -1.0, high: float = 1.0) -> float:
    return max(low, min(high, value))


def envelope(
    t: float,
    duration: float,
    attack: float = 0.003,
    release: float = 0.04,
) -> float:
    if duration <= 0:
        return 0.0
    attack_gain = 1.0 if attack <= 0 else min(1.0, t / attack)
    release_start = max(0.0, duration - release)
    if t <= release_start or release <= 0:
        release_gain = 1.0
    else:
        release_gain = max(0.0, (duration - t) / release)
    return attack_gain * release_gain


def square(phase: float, duty: float = 0.5) -> float:
    return 1.0 if (phase % 1.0) < duty else -1.0


def triangle(phase: float) -> float:
    p = phase % 1.0
    return 4.0 * abs(p - 0.5) - 1.0


def lowpass(samples: list[float], cutoff_hz: float) -> list[float]:
    if not samples:
        return []
    dt = 1.0 / SAMPLE_RATE
    rc = 1.0 / (2.0 * math.pi * cutoff_hz)
    alpha = dt / (rc + dt)
    out: list[float] = []
    y = samples[0]
    for sample in samples:
        y += alpha * (sample - y)
        out.append(y)
    return out


def highpass(samples: list[float], cutoff_hz: float) -> list[float]:
    if not samples:
        return []
    dt = 1.0 / SAMPLE_RATE
    rc = 1.0 / (2.0 * math.pi * cutoff_hz)
    alpha = rc / (rc + dt)
    out: list[float] = []
    y = 0.0
    previous = samples[0]
    for sample in samples:
        y = alpha * (y + sample - previous)
        previous = sample
        out.append(y)
    return out


def normalize(samples: list[float], peak: float = MASTER_PEAK) -> list[float]:
    maximum = max((abs(sample) for sample in samples), default=0.0)
    if maximum <= 0.0:
        return samples
    gain = peak / maximum
    return [sample * gain for sample in samples]


def write_wav(path: Path, samples: list[float]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    pcm = bytearray()
    for sample in normalize(samples):
        value = int(round(clamp(sample) * 32767.0))
        pcm.extend(struct.pack("<h", value))
    with wave.open(str(path), "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(SAMPLE_RATE)
        wav.writeframes(bytes(pcm))


def generate_walk() -> list[float]:
    """Muted cousin of dig: two soft dirt/gravel micro-scrapes."""
    duration = 0.095
    count = int(duration * SAMPLE_RATE)
    rng = random.Random(0x57414C4B)
    raw = [0.0] * count
    burst_centres = (0.020, 0.060)
    phase = 0.0

    for i in range(count):
        t = i / SAMPLE_RATE
        noise = rng.random() * 2.0 - 1.0
        scrape = 0.0
        for centre in burst_centres:
            distance = abs(t - centre)
            if distance < 0.022:
                shape = (1.0 - distance / 0.022) ** 2
                scrape += noise * shape

        progress = t / duration
        freq = 240.0 - 55.0 * progress
        phase += freq / SAMPLE_RATE
        soft_step = triangle(phase) * math.exp(-35.0 * t)
        raw[i] = (
            0.56 * scrape + 0.18 * soft_step
        ) * envelope(t, duration, 0.0015, 0.028)

    # Lower cutoff than dig makes footsteps more muffled/soft.
    return highpass(lowpass(raw, 1900.0), 100.0)


def generate_dig() -> list[float]:
    duration = 0.145
    count = int(duration * SAMPLE_RATE)
    rng = random.Random(0x444947)
    raw = [0.0] * count
    burst_centres = (0.018, 0.061, 0.105)
    for i in range(count):
        t = i / SAMPLE_RATE
        noise = rng.random() * 2.0 - 1.0
        scrape = 0.0
        for centre in burst_centres:
            distance = abs(t - centre)
            if distance < 0.026:
                shape = (1.0 - distance / 0.026) ** 2
                scrape += noise * shape
        grain_phase = (
            t * (520.0 + 120.0 * math.sin(2.0 * math.pi * 11.0 * t))
        ) % 1.0
        grain = square(grain_phase, 0.18) * 0.18
        raw[i] = (
            0.82 * scrape + grain
        ) * envelope(t, duration, 0.001, 0.025)
    return highpass(lowpass(raw, 4200.0), 180.0)


def generate_rock_fall() -> list[float]:
    duration = 0.38
    count = int(duration * SAMPLE_RATE)
    rng = random.Random(0x524F434B)
    raw: list[float] = []
    phase = 0.0
    impact_time = 0.265
    for i in range(count):
        t = i / SAMPLE_RATE
        progress = min(1.0, t / impact_time)
        freq = 150.0 - 86.0 * progress
        phase += freq / SAMPLE_RATE
        falling = 0.58 * triangle(phase) + 0.20 * square(phase * 0.5, 0.42)
        falling *= max(0.0, 1.0 - t / impact_time)
        wobble = (
            math.sin(2.0 * math.pi * 7.0 * t)
            * 0.08
            * (1.0 - progress)
        )
        impact_dt = t - impact_time
        impact = 0.0
        if impact_dt >= 0.0:
            impact_env = math.exp(-20.0 * impact_dt)
            impact_noise = (
                (rng.random() * 2.0 - 1.0)
                * math.exp(-34.0 * impact_dt)
            )
            impact_tone = (
                math.sin(2.0 * math.pi * 72.0 * impact_dt)
                * impact_env
            )
            impact = 0.62 * impact_tone + 0.58 * impact_noise
        raw.append(
            (falling + wobble + impact)
            * envelope(t, duration, 0.002, 0.055)
        )
    return lowpass(raw, 2600.0)


def generate_diamond() -> list[float]:
    duration = 0.205
    count = int(duration * SAMPLE_RATE)
    # Generic synthesized arcade chime, intentionally composed from scratch.
    notes = (
        (1046.502, 0.000, 0.105),
        (1318.510, 0.045, 0.115),
        (1760.000, 0.090, 0.115),
    )
    samples = [0.0] * count
    for freq, start, length in notes:
        phase = 0.0
        start_i = int(start * SAMPLE_RATE)
        end_i = min(count, int((start + length) * SAMPLE_RATE))
        for i in range(start_i, end_i):
            local_t = (i - start_i) / SAMPLE_RATE
            phase += freq / SAMPLE_RATE
            env = envelope(local_t, length, 0.0015, 0.045)
            tone = (
                0.72 * square(phase, 0.5)
                + 0.28 * square(phase * 2.0, 0.25)
            )
            samples[i] += tone * env * 0.42
    return highpass(lowpass(samples, 7000.0), 450.0)


def generate_death() -> list[float]:
    """Short retro collapse/boom with a descending synthetic body."""
    duration = 0.620
    count = int(duration * SAMPLE_RATE)
    rng = random.Random(0x4445415448)
    raw: list[float] = []
    phase_body = 0.0
    phase_edge = 0.0

    for i in range(count):
        t = i / SAMPLE_RATE
        progress = t / duration

        # Fast falling pitch gives the "everything collapsed" gesture.
        freq = 210.0 * (1.0 - progress) ** 1.8 + 52.0
        freq *= 1.0 + 0.035 * math.sin(2.0 * math.pi * 9.0 * t)
        phase_body += freq / SAMPLE_RATE
        phase_edge += (freq * 1.99) / SAMPLE_RATE

        body = (
            0.62 * triangle(phase_body)
            + 0.22 * square(phase_edge, 0.38)
        ) * math.exp(-3.5 * t)

        noise = rng.random() * 2.0 - 1.0
        blast = noise * math.exp(-10.5 * t)
        crumble = (
            noise
            * max(0.0, 1.0 - progress) ** 2
            * (0.4 + 0.6 * abs(math.sin(2.0 * math.pi * 17.0 * t)))
        )
        sub = (
            math.sin(2.0 * math.pi * (74.0 - 20.0 * progress) * t)
            * math.exp(-4.0 * t)
        )

        raw.append(
            (0.55 * body + 0.48 * blast + 0.24 * crumble + 0.25 * sub)
            * envelope(t, duration, 0.0015, 0.090)
        )

    return lowpass(raw, 3300.0)


def generate_diamond_fall() -> list[float]:
    duration = 0.285
    count = int(duration * SAMPLE_RATE)
    rng = random.Random(0x4446414C4C)
    samples: list[float] = []
    phase_a = 0.0
    phase_b = 0.0
    for i in range(count):
        t = i / SAMPLE_RATE
        progress = t / duration
        freq_a = 1450.0 - 720.0 * progress
        freq_b = freq_a * 1.503
        phase_a += freq_a / SAMPLE_RATE
        phase_b += freq_b / SAMPLE_RATE
        shimmer = (
            0.52 * square(phase_a, 0.33)
            + 0.26 * triangle(phase_b)
        )
        grit = (rng.random() * 2.0 - 1.0) * 0.10
        tremolo = 0.72 + 0.28 * square(t * 24.0, 0.55)
        samples.append(
            (shimmer * tremolo + grit)
            * envelope(t, duration, 0.0015, 0.055)
        )
    return highpass(lowpass(samples, 6200.0), 500.0)


GENERATORS = {
    "walk.wav": generate_walk,
    "dig.wav": generate_dig,
    "rock_fall.wav": generate_rock_fall,
    "diamond.wav": generate_diamond,
    "death.wav": generate_death,
    "diamond_fall.wav": generate_diamond_fall,
}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path(__file__).resolve().parents[1] / "assets" / "sfx",
        help="directory receiving generated WAV files",
    )
    args = parser.parse_args()
    for filename, generator in GENERATORS.items():
        output = args.output_dir / filename
        write_wav(output, generator())
        print(output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
