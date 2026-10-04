param([string]$EngineRoot = 'D:\Program Files\Epic Games\UE_4.27', [switch]$TouchClient)
$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path -Parent $PSScriptRoot
$taskProject = Join-Path $taskRoot 'TPSDuel.uproject'
$taskEditor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UE4Editor.exe'
if (-not (Test-Path -LiteralPath (Join-Path $taskRoot 'Content\Maps\L_Arena.umap'))) { throw 'Run tools/Build.ps1 -Action Bootstrap first.' }
# Independent processes; no virtual machines. Standard GUI windows are created by the game.
$taskHostArgs = '"' + $taskProject + '" /Game/Maps/L_Arena?listen?duel=1 -game -windowed -ResX=960 -ResY=640 -WinX=20 -WinY=40 -port=7777'
Start-Process -FilePath $taskEditor -ArgumentList $taskHostArgs -WindowStyle Hidden | Out-Null
$taskClientArgs = '"' + $taskProject + '" /Game/Maps/L_Menu -game -windowed -ResX=960 -ResY=640 -WinX=1000 -WinY=40'
if ($TouchClient) { $taskClientArgs += ' -DuelTouch -faketouches' }
Start-Process -FilePath $taskEditor -ArgumentList $taskClientArgs -WindowStyle Hidden | Out-Null
Write-Output 'Host starts on UDP 7777. In the client menu join 127.0.0.1:7777.'
