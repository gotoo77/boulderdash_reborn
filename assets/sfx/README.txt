Runtime event-to-file associations and per-sound volumes are configured in
`cfg/audio.json`; filenames are not hard-coded in the audio loader.

Asset provenance and redistribution status are tracked in `SOURCES.md`.

The following retro SFX are generated automatically by CMake from
`tools/generate_retro_sfx.py` and are intentionally not tracked by Git:

- `walk.wav`
- `dig.wav`
- `rock_fall.wav`
- `diamond.wav`
- `death.wav`
- `diamond_fall.wav`

They can also be regenerated manually with:

    python tools/generate_retro_sfx.py

The generator uses no source audio or ROM data. It synthesizes deterministic
22.05 kHz / 16-bit mono WAV files from simple oscillators, seeded noise,
envelopes and filters.

`walk.wav` deliberately shares the scrape/grit family of `dig.wav`, with a
shorter and more heavily low-pass-filtered recipe for a softer footstep. Their
relative playback levels are tuned separately in `cfg/audio.json`.

`death.wav` is an original procedural retro collapse/boom with a descending
pitch body and deterministic noise burst.

`exit_unlock.wav` is an original procedural three-note chime generated for this project.
`explosion.wav` is an original procedural noise-and-bass effect generated for this project.
`time_warning.wav` is an original procedural alert beep generated for this project.
