# OBS Audio Recorder

This project is the replacement for the earlier vinyl-reel plugin.

## Scope discussed so far

- Add a `Tools -> Audio Record` menu entry in OBS.
- Open a settings panel from that entry.
- Store default recording settings in the plugin UI.
- Add a control in OBS for starting audio-only recording.
- Record from selected OBS internal audio channels.
- Optionally combine selected channels into one output file.
- Export audio to file formats such as FLAC, MP3, and AAC.

## Notes

- The OBS frontend API supports adding Tools menu items and docks.
- A custom button directly inside the built-in Controls panel is not the likely path.
- A practical first version should focus on a clean settings UI and one solid encoding path, then expand formats after that.

## Development

- Build: `bash scripts/build.sh`
- Install into OBS: `bash scripts/install-to-obs.sh`
- Build, install, and launch OBS: `bash scripts/dev-obs.sh`

The current plugin skeleton registers `Tools -> Audio Record`, opens a docked settings panel, persists the selected audio channels, output folder, and combine mode, and records the chosen OBS internal audio channels to WAV.
