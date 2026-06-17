# OBS Audio Recorder

This project is the OBS audio recorder plugin.

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

## CI Build

The GitHub release pipeline now mirrors the sibling project:

- `ci/CMakeLists.txt` is the build entrypoint for CI.
- `ci/buildspec.json` pins OBS Studio `31.1.2` and the matching `obs-deps` / Qt6 bundles.
- `.github/workflows/release.yml` stages the plugin into a release prefix and publishes Windows artifacts on tagged builds.
- `scripts/build-ci.ps1` reproduces the Windows CI build locally from the same `ci/` tree.

## Windows

Windows support is now being wired in alongside the macOS build.

- For local one-off builds, `scripts/build.ps1` still uses the lean root-level CMake path.
- For CI-style Windows builds, use the `ci/` tree; it bootstraps OBS sources and deps automatically from the pinned release assets.
- Build with `pwsh scripts/build.ps1`.
- Build the CI-style Windows path with `pwsh scripts/build-ci.ps1`.
- Install into OBS with `pwsh scripts/install-to-obs.ps1` after setting `OBS_INSTALL_DIR` to the OBS install root.

The Windows install layout uses the standard OBS plugin locations:

- `obs-plugins/64bit/obs-audio-recorder.dll`
- `data/obs-plugins/obs-audio-recorder/`

The current plugin skeleton registers `Tools -> Audio Record`, opens a docked settings panel, persists the selected audio channels, output folder, and combine mode, and records the chosen OBS internal audio channels to WAV.
