param([switch]$NullRHI)
$ErrorActionPreference='Stop'
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskRuntime=Join-Path $taskRoot 'Builds\Windows\WindowsNoEditor\TPSDuel'
$taskExe=Join-Path $taskRuntime 'Binaries\Win64\TPSDuel.exe'
$taskLog=Join-Path $taskRoot 'Saved\locomotion-v054.log'
$taskResult=Join-Path $taskRuntime 'Saved\MotionTest.txt'
if(-not(Test-Path -LiteralPath $taskExe)){throw 'Package Windows first.'}
if(Test-Path -LiteralPath $taskResult){Remove-Item -LiteralPath $taskResult}
$taskArgs='/Game/Maps/L_Arena?duel=1 -DuelMotionTest -windowed -ResX=1560 -ResY=720 -unattended -nosplash -nosound -abslog="'+$taskLog+'"'
if($NullRHI){$taskArgs+=' -NullRHI'}
$taskProcess=Start-Process -FilePath $taskExe -ArgumentList $taskArgs -WindowStyle Hidden -PassThru
try {
    if(-not $taskProcess.WaitForExit(120000)){throw 'Movement test timed out.'}
    if($taskProcess.ExitCode -ne 0){throw "Movement process exited $($taskProcess.ExitCode)."}
    if(-not(Test-Path -LiteralPath $taskResult)){throw "Missing movement result; inspect $taskLog"}
    $taskText=Get-Content -LiteralPath $taskResult -Raw
    Write-Output $taskText
    if(-not $taskText.StartsWith('PASS ')){throw 'Leg movement or jump check failed.'}
    $taskSamples=@(Select-String -LiteralPath $taskLog -Pattern 'MOTION_SAMPLE')
    if($taskSamples.Count -ne 22){throw 'Expected twenty-two sampled poses.'}
    foreach($taskSample in $taskSamples){
        Write-Output $taskSample.Line
        if($taskSample.Line -notmatch 'frame=(\d+) speed=([\d.]+) animSpeed=([\d.]+).*gripError=([\d.]+)'){throw 'Could not parse pose sample.'}
        $taskIndex=[int]$Matches[1]
        $taskSpeed=[double]::Parse($Matches[2],[Globalization.CultureInfo]::InvariantCulture)
        $taskAnimSpeed=[double]::Parse($Matches[3],[Globalization.CultureInfo]::InvariantCulture)
        $taskGripError=[double]::Parse($Matches[4],[Globalization.CultureInfo]::InvariantCulture)
        if($taskIndex -lt 12 -and [Math]::Abs($taskSpeed-$taskAnimSpeed) -gt 20){throw 'Animation speed does not follow movement.'}
        if($taskIndex -eq 8 -and $taskSpeed -gt 1){throw 'Character did not brake to idle.'}
        if($taskGripError -gt 1){throw 'Moving hand no longer follows the rifle.'}
        if($taskSample.Line -notmatch 'carry=([\d.]+) rate=([-\d.]+).*ammo=(\d+) reload=([-\d.]+)'){throw 'Missing stance/weapon sample.'}
        $taskCarry=[double]::Parse($Matches[1],[Globalization.CultureInfo]::InvariantCulture)
        $taskRate=[double]::Parse($Matches[2],[Globalization.CultureInfo]::InvariantCulture)
        $taskAmmo=[int]$Matches[3]
        $taskReload=[double]::Parse($Matches[4],[Globalization.CultureInfo]::InvariantCulture)
        if($taskIndex -eq 0 -and $taskCarry -lt .9){throw 'Running rifle did not reach low ready.'}
        if($taskIndex -in @(0,9,10,11,18,19,20,21) -and [Math]::Abs($taskSpeed-375) -gt 5){throw 'Cardinal/diagonal movement speeds differ.'}
        if($taskCarry -gt .9 -and $taskSample.Line -notmatch 'gunDirection=.*Z=-(0\.[4-9]|1\.)'){throw 'Moving rifle did not point down.'}
        if($taskIndex -eq 9 -and $taskRate -ne 1){throw 'Dedicated backward clip must play forward.'}
        if($taskIndex -eq 13 -and ($taskCarry -gt .05 -or $taskSpeed -gt 125)){throw 'Aiming did not raise rifle and slow movement.'}
        if($taskIndex -eq 15 -and ($taskCarry -gt .05 -or $taskAmmo -ge 30)){throw 'Running fire did not raise rifle or consume ammo.'}
        if($taskIndex -eq 16 -and ($taskCarry -gt .05 -or $taskReload -lt 0)){throw 'Reload did not leave carry stance.'}
    }
    $taskCombatSamples=@(Select-String -LiteralPath $taskLog -Pattern 'COMBAT_SAMPLE')
    if($taskCombatSamples.Count -ne 22){throw 'Missing combat animation evidence.'}
    foreach($taskSample in $taskCombatSamples){
        if($taskSample.Line -notmatch 'frame=(\d+) dedicated=1 direction=([-\d.]+) action=(.*)$'){throw 'Dedicated animation graph was not active.'}
        $taskIndex=[int]$Matches[1]; $taskDirection=[double]::Parse($Matches[2],[Globalization.CultureInfo]::InvariantCulture); $taskAction=$Matches[3]
        if($taskIndex -eq 9 -and [Math]::Abs($taskDirection) -lt 170){throw 'Backward sample did not select backward direction.'}
        if($taskIndex -eq 10 -and [Math]::Abs($taskDirection-90) -gt 10){throw 'Right strafe direction incorrect.'}
        if($taskIndex -eq 11 -and [Math]::Abs($taskDirection+90) -gt 10){throw 'Left strafe direction incorrect.'}
        if($taskIndex -ge 18){
            $taskExpectedDirection=@(45,-45,135,-135)[$taskIndex-18]
            if([Math]::Abs($taskDirection-$taskExpectedDirection) -gt 5){throw 'Diagonal animation direction incorrect.'}
        }
        if($taskIndex -in @(15,16) -and $taskAction -eq 'None'){throw 'Fire/reload animation was not active.'}
    }
    if(-not $NullRHI){
        $taskOutput=Join-Path $taskRoot 'docs\screenshots\locomotion-v0.5.4'
        New-Item -ItemType Directory -Path $taskOutput -Force | Out-Null
        # Windows image previews may map existing PNGs and prevent truncation.
        # Preserve the previous capture, then publish a fresh file at its path.
        $taskPreviousShots=Join-Path $taskRoot ('Saved\PreviousMotionShots\'+[Guid]::NewGuid().ToString('N'))
        New-Item -ItemType Directory -Path $taskPreviousShots -Force | Out-Null
        foreach($taskIndex in 0..21){
            $taskName='Motion-{0:00}.png' -f $taskIndex
            $taskShot=Join-Path $taskRuntime ('Saved\Screenshots\'+$taskName)
            if(-not(Test-Path -LiteralPath $taskShot)){throw "Missing screenshot $taskShot"}
            $taskDestination=Join-Path $taskOutput $taskName
            if(Test-Path -LiteralPath $taskDestination){Move-Item -LiteralPath $taskDestination -Destination (Join-Path $taskPreviousShots $taskName)}
            Copy-Item -LiteralPath $taskShot -Destination $taskDestination
        }
        Write-Output "Screenshots: $taskOutput"
    }
} finally {
    if(-not $taskProcess.HasExited){Stop-Process -Id $taskProcess.Id -Force}
}
