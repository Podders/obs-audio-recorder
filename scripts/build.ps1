param(
  [string]$RootDir = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path,
  [string]$BuildDir = (Join-Path $RootDir "build"),
  [string]$QtPrefix = $env:QT_PREFIX,
  [string]$ObsLibobsLibrary = $env:OBS_LIBOBS_LIBRARY,
  [string]$ObsFrontendApiLibrary = $env:OBS_FRONTEND_API_LIBRARY
)

$configureArgs = @(
  "-S", $RootDir,
  "-B", $BuildDir,
  "-DOBS_STUDIO_SOURCE_DIR=$($RootDir)/third_party/obs-studio"
)

if ($QtPrefix) {
  $configureArgs += "-DCMAKE_PREFIX_PATH=$QtPrefix"
}

if ($ObsLibobsLibrary) {
  $configureArgs += "-DOBS_LIBOBS_LIBRARY=$ObsLibobsLibrary"
}

if ($ObsFrontendApiLibrary) {
  $configureArgs += "-DOBS_FRONTEND_API_LIBRARY=$ObsFrontendApiLibrary"
}

cmake @configureArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

cmake --build $BuildDir --config Release --target obs-audio-recorder
exit $LASTEXITCODE
