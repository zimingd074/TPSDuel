param([string]$AdbPath = 'D:\android\AndrioSdk\platform-tools\adb.exe', [string]$Serial)
$ErrorActionPreference = 'Stop'
if (-not (Test-Path -LiteralPath $AdbPath)) { throw "ADB not found: $AdbPath" }
$taskPrefix = @()
if ($Serial) { $taskPrefix = @('-s',$Serial) }
& $AdbPath devices -l
& $AdbPath @taskPrefix get-state
if ($LASTEXITCODE -ne 0) { throw 'Connect and authorize an Android phone; specify -Serial when multiple devices are connected.' }
foreach ($taskProperty in @('ro.product.model','ro.build.version.release','ro.build.version.sdk','ro.product.cpu.abilist')) {
    Write-Output $taskProperty
    & $AdbPath @taskPrefix shell getprop $taskProperty
}
& $AdbPath @taskPrefix shell uname -r
& $AdbPath @taskPrefix shell getconf PAGE_SIZE
