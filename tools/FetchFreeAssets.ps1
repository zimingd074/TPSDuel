param([string]$Resolution = '1k')
$ErrorActionPreference = 'Stop'
if ($Resolution -ne '1k') { throw 'The initial mobile asset set uses 1k textures.' }
$taskProjectRoot = Split-Path -Parent $PSScriptRoot
foreach ($taskAsset in @('concrete_floor_worn_001','barrel_03','wooden_military_crate')) {
    $taskDestination = Join-Path $taskProjectRoot "Assets\Source\PolyHaven\$taskAsset"
    New-Item -ItemType Directory -Path $taskDestination -Force | Out-Null
    $taskFiles = Invoke-RestMethod -Uri "https://api.polyhaven.com/files/$taskAsset"
    $taskMaps = @('Diffuse','nor_dx','arm')
    if ($taskAsset -ne 'concrete_floor_worn_001') { $taskMaps += 'fbx' }
    $taskManifest = @()
    foreach ($taskMap in $taskMaps) {
        $taskFile = if ($taskMap -eq 'fbx') { $taskFiles.fbx.$Resolution.fbx } else { $taskFiles.$taskMap.$Resolution.jpg }
        if (-not $taskFile.url -or -not $taskFile.md5) { throw "Missing official download metadata: $taskAsset / $taskMap" }
        $taskPath = Join-Path $taskDestination ([IO.Path]::GetFileName(([Uri]$taskFile.url).AbsolutePath))
        if (-not (Test-Path -LiteralPath $taskPath) -or (Get-FileHash -LiteralPath $taskPath -Algorithm MD5).Hash -ne $taskFile.md5) {
            Invoke-WebRequest -Uri $taskFile.url -OutFile $taskPath
        }
        if ((Get-FileHash -LiteralPath $taskPath -Algorithm MD5).Hash -ne $taskFile.md5) { throw "Download checksum mismatch: $taskPath" }
        $taskManifest += [ordered]@{map=$taskMap;file=[IO.Path]::GetFileName($taskPath);url=$taskFile.url;md5=$taskFile.md5;sha256=(Get-FileHash -LiteralPath $taskPath -Algorithm SHA256).Hash;bytes=(Get-Item -LiteralPath $taskPath).Length}
    }
    [ordered]@{asset=$taskAsset;source="https://polyhaven.com/a/$taskAsset";license='CC0';licenseUrl='https://polyhaven.com/license';resolution=$Resolution;files=$taskManifest} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $taskDestination 'manifest.json') -Encoding UTF8
    $taskManifest | ForEach-Object { [pscustomobject]$_ } | Select-Object map,file,bytes
}
