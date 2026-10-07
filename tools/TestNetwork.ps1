param([string]$EngineRoot='D:\Program Files\Epic Games\UE_4.27', [switch]$Packaged, [switch]$CloseRange, [switch]$Crouched)
$ErrorActionPreference='Stop'
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskProject=Join-Path $taskRoot 'TPSDuel.uproject'
$taskEditor=Join-Path $EngineRoot 'Engine\Binaries\Win64\UE4Editor.exe'
$taskRuntimeRoot=$taskRoot
$taskPrefix='"'+$taskProject+'" '
if($Packaged) {
    $taskRuntimeRoot=Join-Path $taskRoot 'Builds\Windows\WindowsNoEditor\TPSDuel'
    $taskEditor=Join-Path $taskRuntimeRoot 'Binaries\Win64\TPSDuel.exe'
    $taskPrefix=''
}
if(-not (Test-Path -LiteralPath $taskEditor)) { throw "Game executable missing: $taskEditor" }
$taskRun=[DateTime]::UtcNow.ToString('yyyyMMddHHmmssfff')
$taskResults=Join-Path $taskRuntimeRoot ('Saved\Smoke\'+$taskRun)
New-Item -ItemType Directory -Path $taskResults -Force | Out-Null
$taskHostLog=Join-Path $taskResults 'host.log'
$taskClientLog=Join-Path $taskResults 'client.log'
$taskCommon=' -game -NullRHI -nosound -unattended -nosplash -DuelShotLog -DuelSmokeRun='+$taskRun
if($CloseRange) { $taskCommon+=' -DuelSmokeClose' }
if($Crouched) { $taskCommon+=' -DuelSmokeCrouch' }
$taskHostArgs=$taskPrefix+'/Game/Maps/L_Arena?listen?duel=1'+$taskCommon+' -DuelSmoke=Host -port=7777 -abslog="'+$taskHostLog+'"'
$taskHost=Start-Process -FilePath $taskEditor -ArgumentList $taskHostArgs -WindowStyle Hidden -PassThru
$taskClient=$null
try {
    $taskDeadline=[DateTime]::UtcNow.AddSeconds(60)
    $taskListening=$false
    while([DateTime]::UtcNow -lt $taskDeadline -and -not $taskHost.HasExited) {
        if(Test-Path -LiteralPath $taskHostLog) {
            $taskLogText=Get-Content -LiteralPath $taskHostLog -Raw
            if($taskLogText -match 'listening on port 7777') { $taskListening=$true; break }
        }
        Start-Sleep -Milliseconds 500
    }
    if(-not $taskListening) { throw "Host did not listen. Inspect $taskHostLog" }
    $taskClientArgs=$taskPrefix+'127.0.0.1:7777?duel=1'+$taskCommon+' -DuelSmoke=Client -abslog="'+$taskClientLog+'"'
    $taskClient=Start-Process -FilePath $taskEditor -ArgumentList $taskClientArgs -WindowStyle Hidden -PassThru
    $taskDeadline=[DateTime]::UtcNow.AddSeconds(120)
    while([DateTime]::UtcNow -lt $taskDeadline) {
        if((Test-Path -LiteralPath (Join-Path $taskResults 'Host.txt')) -and (Test-Path -LiteralPath (Join-Path $taskResults 'Client.txt'))) { break }
        if($taskHost.HasExited -and $taskClient.HasExited) { break }
        Start-Sleep -Milliseconds 500
    }
    foreach($taskRole in @('Host','Client')) {
        $taskResult=Join-Path $taskResults ($taskRole+'.txt')
        if(-not (Test-Path -LiteralPath $taskResult)) { throw "Missing $taskRole result. Inspect $taskResults" }
        $taskText=Get-Content -LiteralPath $taskResult -Raw
        Write-Output ($taskRole+': '+$taskText)
        if(-not $taskText.StartsWith('PASS ')) { throw "Network regression failed: $taskRole" }
    }
    Write-Output ('Verified real host/client shots, death/respawn, BLUE=3 RED=1 and shared winner. Logs: '+$taskResults)
} finally {
    foreach($taskProcess in @($taskClient,$taskHost)) {
        if($taskProcess -and -not $taskProcess.HasExited) {
            if(-not $taskProcess.WaitForExit(6000)) { Stop-Process -Id $taskProcess.Id -Force -ErrorAction SilentlyContinue }
        }
    }
}
