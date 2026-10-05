param(
    [ValidateSet('Build','Assets','Bootstrap','PackageWindows','PackageAndroid','Tests')]
    [string]$Action = 'Bootstrap',
    [string]$EngineRoot = 'D:\Program Files\Epic Games\UE_4.27',
    [string]$AndroidSDKRoot,
    [string]$AndroidNDKRoot,
    [string]$JavaRoot = 'D:\Program Files\Java\jdk1.8.0_481'
)
$ErrorActionPreference = 'Stop'
# UE4.27's Git working-set parser cannot decode quoted non-ASCII paths.
# Keep Chinese documentation names usable without changing repository/global Git settings.
$env:GIT_CONFIG_PARAMETERS=($env:GIT_CONFIG_PARAMETERS+" 'core.quotepath=false'").Trim()
$taskProjectRoot = Split-Path -Parent $PSScriptRoot
$taskProject = Join-Path $taskProjectRoot 'TPSDuel.uproject'
$taskBuild = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
$taskEditor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UE4Editor-Cmd.exe'
$taskUAT = Join-Path $EngineRoot 'Engine\Build\BatchFiles\RunUAT.bat'
foreach ($taskPath in @($taskProject,$taskBuild,$taskEditor,$taskUAT)) {
    if (-not (Test-Path -LiteralPath $taskPath)) { throw "Required file missing: $taskPath" }
}
$taskVersion = Get-Content -LiteralPath (Join-Path $EngineRoot 'Engine\Build\Build.version') -Raw | ConvertFrom-Json
if ($taskVersion.MajorVersion -ne 4 -or $taskVersion.MinorVersion -ne 27) { throw 'This project targets UE4.27.' }
function Build-Editor {
    & $taskBuild TPSDuelEditor Win64 Development "-Project=$taskProject" -WaitMutex -NoHotReload
    if ($LASTEXITCODE -ne 0) { throw "Editor build failed ($LASTEXITCODE). Check MSVC v142 and Windows SDK installation." }
}
function Initialize-Assets {
    & $taskEditor $taskProject "-ExecutePythonScript=$(Join-Path $PSScriptRoot 'InitializeAssets.py')" -unattended -nosplash -NullRHI -stdout -FullStdOutLogOutput -UTF8Output
    if ($LASTEXITCODE -ne 0) { throw "Asset initialization failed ($LASTEXITCODE)." }
    if (Test-Path -LiteralPath (Join-Path $taskProjectRoot 'Assets\Source\PolyHaven\concrete_floor_worn_001\manifest.json')) {
        $taskImport = Get-Content -LiteralPath (Join-Path $taskProjectRoot 'Saved\FreeAssetsImport.json') -Raw | ConvertFrom-Json
        if ($taskImport.state -ne 'complete' -or $taskImport.assets.Count -ne 3) { throw 'Free asset initialization failed; inspect Saved/import-free-assets.log and Saved/Logs/TPSDuel.log.' }
    }
    foreach ($taskAsset in @('Content\Maps\L_Menu.umap','Content\Maps\L_Arena.umap','Content\Materials\M_DuelColor.uasset','Content\Materials\M_Concrete.uasset','Content\Materials\M_Brick.uasset','Content\Materials\M_Wood.uasset','Content\Materials\M_Metal.uasset','Content\Materials\M_Paint.uasset','Content\Materials\M_Sky.uasset','Content\Mannequin\Character\Mesh\SK_Mannequin.uasset','Content\Audio\S_Fire.uasset')) {
        if (-not (Test-Path -LiteralPath (Join-Path $taskProjectRoot $taskAsset))) { throw "Asset initialization incomplete: $taskAsset" }
    }
    if (Test-Path -LiteralPath (Join-Path $taskProjectRoot 'Assets\Source\Fab\Quantum\RecoveredTextures.json')) {
        $taskQuantum = Get-Content -LiteralPath (Join-Path $taskProjectRoot 'Saved\QuantumPreparation.json') -Raw | ConvertFrom-Json
        if ($taskQuantum.state -ne 'prepared') { throw 'Quantum character preparation failed; inspect Saved/Logs/TPSDuel.log.' }
        $taskCombat = Get-Content -LiteralPath (Join-Path $taskProjectRoot 'Saved\CombatAssets.json') -Raw | ConvertFrom-Json
        if ($taskCombat.state -ne 'prepared') { throw 'Combat animation initialization failed.' }
    }
    if (Test-Path -LiteralPath (Join-Path $taskProjectRoot 'Assets\Source\PolyHaven\modular_factory_facade\manifest.json')) {
        $taskWarehouse = Get-Content -LiteralPath (Join-Path $taskProjectRoot 'Saved\WarehouseImport.json') -Raw | ConvertFrom-Json
        if ($taskWarehouse.state -ne 'complete' -or $taskWarehouse.assets.Count -ne 4) { throw 'Warehouse initialization failed.' }
    }
}
switch ($Action) {
    'Build' { Build-Editor }
    'Assets' { Initialize-Assets }
    'Bootstrap' { Build-Editor; Initialize-Assets }
    'Tests' {
        Build-Editor
        & $taskEditor $taskProject /Game/Maps/L_Arena '-ExecCmds=Automation RunTests TPSDuel' '-TestExit=Automation Test Queue Empty' -unattended -NullRHI -nosplash -stdout -FullStdOutLogOutput "-ReportExportPath=$(Join-Path $taskProjectRoot 'Saved\TestReports')"
        if ($LASTEXITCODE -ne 0) { throw 'UE automation process failed. Inspect Saved/TestReports and the log for test results.' }
        $taskReport = Get-Content (Join-Path $taskProjectRoot 'Saved\TestReports\index.json') -Raw | ConvertFrom-Json
        if ($taskReport.failed -gt 0 -or $taskReport.notRun -gt 0 -or $taskReport.succeeded -lt 1) { throw 'UE automation report contains failed or missing tests.' }
    }
    { $_ -in 'PackageWindows','PackageAndroid' } {
        foreach ($taskMap in @('Content\Maps\L_Menu.umap','Content\Maps\L_Arena.umap')) {
            if (-not (Test-Path -LiteralPath (Join-Path $taskProjectRoot $taskMap))) { throw 'Run Bootstrap before packaging.' }
        }
        $taskPlatform = if ($Action -eq 'PackageWindows') { 'Win64' } else { 'Android' }
        $taskOutput = Join-Path $taskProjectRoot $(if ($taskPlatform -eq 'Win64') { 'Builds\Windows' } else { 'Builds\Android' })
        $taskArguments = @('BuildCookRun',"-project=$taskProject",'-noP4',"-platform=$taskPlatform",'-clientconfig=Development','-build','-cook','-stage','-pak','-package','-archive',"-archivedirectory=$taskOutput",'-utf8output')
        if ($taskPlatform -eq 'Android') {
            if (-not $AndroidSDKRoot) { $AndroidSDKRoot = Join-Path $taskProjectRoot 'LocalToolchain\AndroidSDK' }
            if (-not $AndroidNDKRoot) { $AndroidNDKRoot = Join-Path $taskProjectRoot 'LocalToolchain\android-ndk-r21e' }
            foreach ($taskRequired in @((Join-Path $AndroidSDKRoot 'build-tools\30.0.3\aapt.exe'),(Join-Path $AndroidNDKRoot 'source.properties'),(Join-Path $JavaRoot 'bin\javac.exe'))) {
                if (-not (Test-Path -LiteralPath $taskRequired)) { throw "Android toolchain file missing: $taskRequired. See docs/开发与运行.md." }
            }
            # Process-local settings; preserve the user's global Java/Android environment.
            $env:ANDROID_HOME = $AndroidSDKRoot
            $env:ANDROID_SDK_ROOT = $AndroidSDKRoot
            $env:NDKROOT = $AndroidNDKRoot
            $env:NDK_ROOT = $AndroidNDKRoot
            $env:JAVA_HOME = $JavaRoot
            $taskArguments += '-cookflavor=ETC2'
        }
        & $taskUAT @taskArguments
        if ($LASTEXITCODE -ne 0) { throw "Packaging failed ($LASTEXITCODE). Preserve logs before changing the toolchain." }
        if ($taskPlatform -eq 'Android') {
            $taskAPK = Get-ChildItem $taskOutput -Recurse -Filter '*.apk' | Sort-Object LastWriteTime -Descending | Select-Object -First 1
            if (-not $taskAPK) { throw 'UAT did not archive an Android APK.' }
            Add-Type -AssemblyName System.IO.Compression.FileSystem
            $taskZip = [IO.Compression.ZipFile]::OpenRead($taskAPK.FullName)
            try {
                $taskData = @($taskZip.Entries | Where-Object { $_.FullName -match '^assets/.*\.(obb(\.png)?|pak)$' -and $_.Length -gt 0 })
                if ($taskData.Count -lt 1) { throw 'APK is missing embedded game data. Check the UAT package step and bPackageDataInsideApk.' }
            } finally { $taskZip.Dispose() }
            $taskPreviousAPK=Join-Path $taskProjectRoot 'Builds\Releases\0.3.1\Android\TPSDuel-arm64.apk'
            if(Test-Path -LiteralPath $taskPreviousAPK){
                & (Join-Path $PSScriptRoot 'CheckAndroidUpdate.ps1') -PreviousAPK $taskPreviousAPK -NewAPK $taskAPK.FullName -AndroidSDKRoot $AndroidSDKRoot -JavaRoot $JavaRoot
            }
        }
    }
}
