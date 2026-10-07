$ErrorActionPreference='Stop'
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskRuntime=Join-Path $taskRoot 'Builds\Windows\WindowsNoEditor\TPSDuel'
$taskExe=Join-Path $taskRuntime 'Binaries\Win64\TPSDuel.exe'
$taskLog=Join-Path $taskRoot 'Saved\controls-v051.log'
$taskResult=Join-Path $taskRuntime 'Saved\InputTest.txt'
if(-not(Test-Path -LiteralPath $taskExe)){throw 'Package Windows first.'}
if(Test-Path -LiteralPath $taskResult){Remove-Item -LiteralPath $taskResult}
$taskArgs='/Game/Maps/L_Arena?duel=1 -DuelInputTest -DuelCadenceLog -windowed -ResX=960 -ResY=640 -unattended -nosplash -nosound -abslog="'+$taskLog+'"'
$taskProcess=Start-Process -FilePath $taskExe -ArgumentList $taskArgs -WindowStyle Hidden -PassThru
try {
    if(-not $taskProcess.WaitForExit(60000)){throw 'Control test timed out.'}
    if($taskProcess.ExitCode -ne 0){throw "Control process exited $($taskProcess.ExitCode)."}
    if(-not(Test-Path -LiteralPath $taskResult)){throw "Missing input result; inspect $taskLog"}
    $taskText=Get-Content -LiteralPath $taskResult -Raw
    Write-Output $taskText
    if(-not $taskText.StartsWith('PASS ')){throw 'Mouse/action regression failed.'}
    Select-String -LiteralPath $taskLog -Pattern 'CADENCE_CHECK' | ForEach-Object {Write-Output $_.Line}
} finally {
    if(-not $taskProcess.HasExited){Stop-Process -Id $taskProcess.Id -Force}
}
