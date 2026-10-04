param(
    [string]$ADBPath='D:\android\AndrioSdk\platform-tools\adb.exe',
    [string]$Serial,
    [switch]$Launch
)
$ErrorActionPreference='Stop'
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskAPK=Get-ChildItem (Join-Path $taskRoot 'Builds\Android') -Recurse -Filter '*.apk' | Sort-Object LastWriteTime -Descending | Select-Object -First 1
if(-not $taskAPK){ throw 'No packaged APK. Run Build.ps1 -Action PackageAndroid first.' }
if(-not (Test-Path -LiteralPath $ADBPath)){ throw "ADB missing: $ADBPath" }
$taskDevices=& $ADBPath devices
if($LASTEXITCODE -ne 0){ throw 'Cannot read Android devices.' }
$taskAuthorized=@($taskDevices | Where-Object { $_ -match '^\S+\s+device$' } | ForEach-Object { ($_ -split '\s+')[0] })
if(-not $Serial){
    if($taskAuthorized.Count -ne 1){ throw 'Connect exactly one phone with USB debugging authorized, or pass -Serial.' }
    $Serial=$taskAuthorized[0]
}
if($taskAuthorized -notcontains $Serial){ throw 'Selected device is not authorized. Allow USB debugging on the phone.' }
$taskABI=(& $ADBPath -s $Serial shell getprop ro.product.cpu.abilist).Trim()
if($LASTEXITCODE -ne 0 -or $taskABI -notmatch 'arm64-v8a'){ throw "This APK requires arm64-v8a; device reports $taskABI" }
Write-Output ('Installing test APK: '+$taskAPK.FullName)
& $ADBPath -s $Serial install -r $taskAPK.FullName
if($LASTEXITCODE -ne 0){ throw 'APK installation failed. Preserve the error; do not uninstall existing app data automatically.' }
if($Launch){
    & $ADBPath -s $Serial shell am start -n 'com.tpsduel.prototype/com.epicgames.ue4.SplashActivity'
    if($LASTEXITCODE -ne 0){ throw 'App launch request failed.' }
}
Write-Output 'In the phone menu, enter the PC LAN IPv4:7777 and select JOIN MATCH.'
