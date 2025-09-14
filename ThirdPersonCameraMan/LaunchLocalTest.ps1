$ErrorActionPreference = 'Stop'

function Find-UnrealEditor {
  $cands = @(
    'C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor.exe',
    'C:\Program Files\Epic Games\UE_5.5\Engine\Binaries\Win64\UnrealEditor.exe',
    'C:\Program Files\Epic Games\UE_5.4\Engine\Binaries\Win64\UnrealEditor.exe',
    'C:\Program Files\Epic Games\UE_5.3\Engine\Binaries\Win64\UnrealEditor.exe'
  )
  foreach ($p in $cands) { if (Test-Path $p) { return $p } }
  throw 'Could not find UnrealEditor.exe in common locations.'
}

$uproject = (Resolve-Path 'ThirdPersonCameraMan.uproject').Path
$editor = Find-UnrealEditor

Write-Host "Starting operator (listen server)..."
$serverArgs = @(
  '"' + $uproject + '"',
  '-game',
  '-log',
  '-windowed', '-ResX=1280', '-ResY=720',
  '-listen'
)
Start-Process -FilePath $editor -ArgumentList $serverArgs | Out-Null

Start-Sleep -Seconds 2

Write-Host "Starting viewer (client)..."
$clientArgs = @(
  '"' + $uproject + '"',
  '-game',
  '-log',
  '-windowed', '-ResX=1280', '-ResY=720',
  '-ExecCmds="open 127.0.0.1"'
)
Start-Process -FilePath $editor -ArgumentList $clientArgs | Out-Null

Write-Host "Launched two instances. The first is operator, second is viewer."

