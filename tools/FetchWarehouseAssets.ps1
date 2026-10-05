$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path -Parent $PSScriptRoot
foreach ($taskAsset in @('modular_factory_facade','concrete_road_barrier','industrial_storage_cart','yellow_plaster_02')) {
    $taskFolder = Join-Path $taskRoot "Assets\Source\PolyHaven\$taskAsset"
    New-Item -ItemType Directory -Path $taskFolder -Force | Out-Null
    $taskMetadata = Invoke-RestMethod "https://api.polyhaven.com/files/$taskAsset"
    $taskMaps = if ($taskAsset -eq 'modular_factory_facade') {
        @('brick','doors','garage','windows','trim_01') | ForEach-Object { "${_}_diff"; "${_}_nor_dx"; "${_}_arm" }
    } else { @('Diffuse','nor_dx','arm') }
    if ($taskAsset -ne 'yellow_plaster_02') { $taskMaps += 'fbx' }
    $taskEntries = foreach ($taskMap in $taskMaps) {
        $taskFile = if ($taskMap -eq 'fbx') { $taskMetadata.fbx.'1k'.fbx } else { $taskMetadata.$taskMap.'1k'.jpg }
        if (-not $taskFile.url -or -not $taskFile.md5) { throw "Missing official metadata: $taskAsset/$taskMap" }
        $taskPath = Join-Path $taskFolder ([IO.Path]::GetFileName(([uri]$taskFile.url).AbsolutePath))
        if (-not (Test-Path -LiteralPath $taskPath) -or (Get-FileHash -LiteralPath $taskPath -Algorithm MD5).Hash -ne $taskFile.md5) {
            Invoke-WebRequest -Uri $taskFile.url -OutFile $taskPath
        }
        if ((Get-FileHash -LiteralPath $taskPath -Algorithm MD5).Hash -ne $taskFile.md5) { throw "Checksum mismatch: $taskPath" }
        [ordered]@{map=$taskMap;file=[IO.Path]::GetFileName($taskPath);url=$taskFile.url;md5=$taskFile.md5;sha256=(Get-FileHash -LiteralPath $taskPath -Algorithm SHA256).Hash;bytes=(Get-Item -LiteralPath $taskPath).Length}
    }
    [ordered]@{asset=$taskAsset;source="https://polyhaven.com/a/$taskAsset";license='CC0';licenseUrl='https://polyhaven.com/license';resolution='1k';files=@($taskEntries)} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $taskFolder 'manifest.json') -Encoding UTF8
    Write-Output "Downloaded and verified: $taskAsset"
}
