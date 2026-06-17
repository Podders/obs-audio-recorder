[CmdletBinding()]
param(
    [ValidateSet('RelWithDebInfo', 'Release', 'Debug', 'MinSizeRel')]
    [string] $Configuration = 'RelWithDebInfo'
)

$ErrorActionPreference = 'Stop'

$RootDir = Resolve-Path (Join-Path $PSScriptRoot '..')
$BuildDir = Join-Path $RootDir 'build_windows'
$InstallPrefix = if ($env:OBS_AUDIO_RECORDER_INSTALL_PREFIX) {
    $env:OBS_AUDIO_RECORDER_INSTALL_PREFIX
} else {
    Join-Path $RootDir 'release'
}

cmake -S (Join-Path $RootDir 'ci') -B $BuildDir `
  -DCMAKE_INSTALL_PREFIX="$InstallPrefix"

cmake --build $BuildDir --config $Configuration --parallel
cmake --install $BuildDir --config $Configuration
exit $LASTEXITCODE
