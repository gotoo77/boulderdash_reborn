Place the following WAV (or compatible) files here for runtime playback:

- `walk.wav`
- `dig.wav`
- `rock_fall.wav`
- `diamond.wav`
- `death.wav`
- `exit_unlock.wav`
- `diamond_fall.wav`
- `explosion.wav`
- `time_warning.wav`

All files should be short SFX (no looping required). Replace them at will; the filenames are referenced directly by the audio system.

Runtime event-to-file associations and per-sound volumes are configured in
`cfg/audio.json`; filenames are not hard-coded in the audio loader.

`exit_unlock.wav` is an original procedural three-note chime generated for this project.
`explosion.wav` is an original procedural noise-and-bass effect generated for this project.
`time_warning.wav` is an original procedural alert beep generated for this project.
