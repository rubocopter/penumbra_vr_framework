[CmdletBinding()]
param([switch]$Built)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$temp = Join-Path ([IO.Path]::GetTempPath()) ('PvrVersion-' + [guid]::NewGuid().ToString('N'))
try {
    New-Item -ItemType Directory -Path $temp | Out-Null
    $release = Get-Content -LiteralPath (Join-Path $repo 'release.json') -Raw | ConvertFrom-Json
    if ($release.version -cne '1.0.0' -or $release.channel -cne 'release-candidate') { throw 'Wrong release identity.' }
    $generator = Join-Path $repo 'tools/Write-PenumbraVrVersion.ps1'
    & $generator -OutputDirectory $temp
    & powershell -NoProfile -ExecutionPolicy Bypass -File $generator -OutputDirectory $temp
    if ($LASTEXITCODE -ne 0) { throw 'PowerShell 5.1 file entry point failed.' }
    $header = Get-Content -LiteralPath (Join-Path $temp 'pvr_version.hpp') -Raw
    if ($header -notmatch 'PVR_VERSION_TEXT "1.0.0"' -or $header -notmatch 'PVR_RELEASE_CHANNEL "release-candidate"') { throw 'Header identity differs.' }
    $before = (Get-Item (Join-Path $temp 'pvr_version.hpp')).LastWriteTimeUtc
    & $generator -OutputDirectory $temp
    if ((Get-Item (Join-Path $temp 'pvr_version.hpp')).LastWriteTimeUtc -ne $before) { throw 'Unchanged identity unnecessarily rebuilt inputs.' }
    $bad = Join-Path $temp 'bad.json'
    '{"schemaVersion":1,"version":"1.0.0\"bad","channel":"release-candidate"}' | Set-Content $bad
    $rejected = $false
    try { & $generator -OutputDirectory $temp -ReleaseManifestPath $bad } catch { $rejected = $true }
    if (-not $rejected -or (Get-Content (Join-Path $temp 'pvr_version.hpp') -Raw) -cne $header) { throw 'Invalid identity was not rejected before writes.' }
    if ($Built) {
        foreach ($relative in @('build/bin/Release/PenumbraVR.BlackPlague.Probe.dll','build/bin/Release/PenumbraVR.Requiem.Probe.dll','build/bin/Release/PenumbraVR.BlackPlague.Bootstrap.dll','build/bin/Release/PenumbraVR.LaaTransform.exe','products/overture/build/bin/Release/Penumbra_vr.exe')) {
            $info = (Get-Item -LiteralPath (Join-Path $repo $relative)).VersionInfo
            if ($info.ProductVersion -cne '1.0.0' -or $info.FileVersion -cne '1.0.0.0') { throw "Wrong PE version: $relative" }
        }
    }
    Write-Host 'Release identity checks passed.'
} finally { if (Test-Path $temp) { Remove-Item -LiteralPath $temp -Recurse -Force } }
