$ErrorActionPreference = 'Stop'

Write-Host "Locating uproject..."
$uproject = Join-Path $PSScriptRoot 'ThirdPersonCameraMan.uproject'
if (!(Test-Path $uproject)) {
  throw "Missing uproject at $uproject"
}

$engineRoot = 'C:\Program Files\Epic Games\UE_5.6'
$buildBat = Join-Path $engineRoot 'Engine\Build\BatchFiles\Build.bat'
if (!(Test-Path $buildBat)) {
  throw "Build.bat not found at $buildBat"
}

$argsList = @(
  'ThirdPersonCameraManEditor',
  'Win64',
  'Development',
  $uproject,
  '-WaitMutex',
  '-NoHotReload'
)

Write-Host "Running: $buildBat $($argsList -join ' ')"
& $buildBat @argsList
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "Build finished."

