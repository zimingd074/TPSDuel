$ErrorActionPreference='Stop'
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskRuntime=Join-Path $taskRoot 'Builds\Windows\WindowsNoEditor\TPSDuel'
$taskExe=Join-Path $taskRuntime 'Binaries\Win64\TPSDuel.exe'
$taskLog=Join-Path $taskRoot 'Saved\weapon-pose-v054.log'
$taskResult=Join-Path $taskRuntime 'Saved\WeaponTest.txt'
if(-not(Test-Path -LiteralPath $taskExe)){throw 'Package Windows first.'}
if(Test-Path -LiteralPath $taskResult){Remove-Item -LiteralPath $taskResult}
$taskArgs='/Game/Maps/L_Arena?duel=1 -DuelWeaponTest -DuelShotLog -windowed -ResX=1280 -ResY=720 -unattended -nosplash -nosound -abslog="'+$taskLog+'"'
$taskProcess=Start-Process -FilePath $taskExe -ArgumentList $taskArgs -WindowStyle Hidden -PassThru
try {
    if(-not $taskProcess.WaitForExit(60000)){throw 'Weapon pose test timed out.'}
    if($taskProcess.ExitCode -ne 0){throw "Weapon test process exited $($taskProcess.ExitCode)."}
    if(-not(Test-Path -LiteralPath $taskResult)){throw "Missing weapon result; inspect $taskLog"}
    $taskText=Get-Content -LiteralPath $taskResult -Raw
    Write-Output $taskText
    if(-not $taskText.StartsWith('PASS ')){throw 'Weapon pose regression failed.'}
    Select-String -LiteralPath $taskLog -Pattern 'WEAPON_POSE|WEAPON_COVER' | ForEach-Object {Write-Output $_.Line}
    if(-not(Select-String -LiteralPath $taskLog -Pattern 'muzzleBlocked=1 hit=Actor_' -Quiet)){throw 'Cover shot did not stop on the blocking test wall.'}
} finally {
    if(-not $taskProcess.HasExited){Stop-Process -Id $taskProcess.Id -Force}
}
