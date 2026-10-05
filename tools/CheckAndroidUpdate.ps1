param(
    [string]$PreviousAPK,
    [string]$NewAPK,
    [string]$AndroidSDKRoot,
    [string]$JavaRoot='D:\Program Files\Java\jdk1.8.0_481'
)
$ErrorActionPreference='Stop'
$taskRoot=Split-Path -Parent $PSScriptRoot
if(-not $AndroidSDKRoot){$AndroidSDKRoot=Join-Path $taskRoot 'LocalToolchain\AndroidSDK'}
if(-not $PreviousAPK){$PreviousAPK=Join-Path $taskRoot 'Builds\Releases\0.4.1\Android\TPSDuel-arm64.apk'}
if(-not $NewAPK){
    $taskLatest=Get-ChildItem (Join-Path $taskRoot 'Builds\Android') -Recurse -Filter '*.apk' | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if(-not $taskLatest){throw 'No new APK found.'}
    $NewAPK=$taskLatest.FullName
}
$taskAapt=Join-Path $AndroidSDKRoot 'build-tools\30.0.3\aapt.exe'
$taskSigner=Join-Path $AndroidSDKRoot 'build-tools\30.0.3\apksigner.bat'
foreach($taskFile in @($PreviousAPK,$NewAPK,$taskAapt,$taskSigner,(Join-Path $JavaRoot 'bin\java.exe'))){
    if(-not (Test-Path -LiteralPath $taskFile)){throw "Required update check file missing: $taskFile"}
}
$env:JAVA_HOME=$JavaRoot
$env:Path=(Join-Path $JavaRoot 'bin')+';C:\Windows\System32;C:\Windows;'+$env:Path
function Read-APK([string]$Path){
    $taskBadging=& $taskAapt dump badging $Path
    if($LASTEXITCODE -ne 0){throw "Cannot read APK metadata: $Path"}
    $taskPackageLine=$taskBadging | Where-Object {$_ -like 'package:*'} | Select-Object -First 1
    $taskMatch=[regex]::Match($taskPackageLine,'package: name=''(?<package>[^'']+)'' versionCode=''(?<code>\d+)'' versionName=''(?<version>[^'']+)''')
    if(-not $taskMatch.Success){throw "Cannot parse APK package/version: $Path"}
    $taskCert=& $taskSigner verify --print-certs $Path
    if($LASTEXITCODE -ne 0){throw "APK signature verification failed: $Path"}
    $taskFingerprints=@($taskCert | Where-Object {$_ -match 'certificate SHA-256 digest: ([0-9a-fA-F]+)'} | ForEach-Object {($_ -split ': ')[-1].ToLowerInvariant()} | Sort-Object)
    if($taskFingerprints.Count -lt 1){throw "No signing certificate: $Path"}
    [pscustomobject]@{APK=$Path; Package=$taskMatch.Groups['package'].Value; VersionCode=[long]$taskMatch.Groups['code'].Value; VersionName=$taskMatch.Groups['version'].Value; CertificateSHA256=($taskFingerprints -join ',')}
}
$taskOld=Read-APK $PreviousAPK
$taskNew=Read-APK $NewAPK
if($taskOld.Package -ne $taskNew.Package){throw 'Package names differ: this would install a separate application.'}
if($taskOld.CertificateSHA256 -ne $taskNew.CertificateSHA256){throw 'Signing certificates differ: cannot replace the installed APK without an intentional signing migration.'}
if($taskNew.VersionCode -le $taskOld.VersionCode){throw 'Increment StoreVersion before issuing a new update.'}
Write-Output ('PASS APK UPDATE: '+$taskOld.Package+' versionCode '+$taskOld.VersionCode+' -> '+$taskNew.VersionCode+'; signing certificate unchanged.')
$taskReport=Join-Path $taskRoot 'Saved\AndroidUpdateCheck.json'
New-Item -ItemType Directory -Path (Split-Path -Parent $taskReport) -Force | Out-Null
@{Previous=$taskOld;New=$taskNew;CheckedAt=[DateTime]::UtcNow.ToString('o');Result='PASS'} | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $taskReport -Encoding UTF8
Write-Output ('Report: '+$taskReport)
