param(
  [string]$RootDir = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path,
  [string]$BuildDir = (Join-Path $RootDir "build"),
  [string]$ObsInstallDir = $env:OBS_INSTALL_DIR
)

if (-not $ObsInstallDir) {
  throw "Set OBS_INSTALL_DIR to the OBS installation root."
}

$dllCandidates = @(
  (Join-Path $BuildDir "Release\obs-audio-recorder.dll"),
  (Join-Path $BuildDir "obs-audio-recorder.dll")
)

$sourceDll = $dllCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $sourceDll) {
  throw "Build the plugin first. No obs-audio-recorder.dll found under $BuildDir."
}

$pluginBinDir = Join-Path $ObsInstallDir "obs-plugins\64bit"
$pluginDataDir = Join-Path $ObsInstallDir "data\obs-plugins\obs-audio-recorder"

New-Item -ItemType Directory -Force -Path $pluginBinDir | Out-Null
New-Item -ItemType Directory -Force -Path $pluginDataDir | Out-Null
Copy-Item $sourceDll $pluginBinDir -Force

Write-Host "Installed to $pluginBinDir"
