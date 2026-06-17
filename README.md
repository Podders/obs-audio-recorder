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

## Windows

Windows support is now being wired in alongside the macOS build.

- By default CMake fetches the OBS Studio `31.1.2` source tree at configure time so the build stays pinned to the target OBS release without vendoring the repo.
- You can override that with `-DOBS_STUDIO_SOURCE_DIR=/path/to/obs-studio` if you already have a local checkout or SDK tree.
- Set `OBS_LIBOBS_LIBRARY` and `OBS_FRONTEND_API_LIBRARY` to the import libraries from an OBS build or SDK.
- Set `QT_PREFIX` to your Qt installation prefix if CMake does not find it automatically.
- Build with `pwsh scripts/build.ps1`.
- Install into OBS with `pwsh scripts/install-to-obs.ps1` after setting `OBS_INSTALL_DIR` to the OBS install root.

The Windows install layout uses the standard OBS plugin locations:

- `obs-plugins/64bit/obs-audio-recorder.dll`
- `data/obs-plugins/obs-audio-recorder/`

The current plugin skeleton registers `Tools -> Audio Record`, opens a docked settings panel, persists the selected audio channels, output folder, and combine mode, and records the chosen OBS internal audio channels to WAV.
