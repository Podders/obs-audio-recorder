[CmdletBinding()]
param(
    [string] $Version = "0.1.0"
)

$ErrorActionPreference = 'Stop'

$RootDir = Resolve-Path (Join-Path $PSScriptRoot '..')
$ArtifactDir = Join-Path $RootDir 'artifacts'
$ReleaseDir = Join-Path $RootDir 'release'
$IssPath = Join-Path $RootDir 'windows/obs-audio-recorder.iss'

if (-not (Test-Path (Join-Path $ReleaseDir 'obs-audio-recorder'))) {
    throw "Expected staged Windows plugin files in $ReleaseDir"
}

New-Item -ItemType Directory -Force -Path $ArtifactDir | Out-Null

$IsccPath = $null
$Iscc = Get-Command ISCC.exe -ErrorAction SilentlyContinue
if ($Iscc) {
    $IsccPath = $Iscc.Path
} else {
    $Candidates = @(
        'C:\Program Files (x86)\Inno Setup 6\ISCC.exe',
        'C:\Program Files\Inno Setup 6\ISCC.exe'
    )
    foreach ($Candidate in $Candidates) {
        if (Test-Path $Candidate) {
            $IsccPath = $Candidate
            break
        }
    }
}

if (-not $IsccPath) {
    throw 'ISCC.exe was not found. Install Inno Setup before packaging.'
}

Push-Location $RootDir
try {
    & $IsccPath "/O$ArtifactDir" "/DMyAppVersion=$Version" $IssPath
} finally {
    Pop-Location
}

$InstallerName = "obs-audio-recorder-$Version-windows-setup.exe"
$BuiltInstaller = Join-Path $ArtifactDir $InstallerName
$TargetInstaller = Join-Path $ArtifactDir $InstallerName

if (-not (Test-Path $BuiltInstaller)) {
    throw "Expected installer output at $BuiltInstaller"
}
