[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$OutputDirectory,
    [string]$ReleaseManifestPath
)
$ErrorActionPreference = 'Stop'
if (-not $ReleaseManifestPath) { $ReleaseManifestPath = Join-Path (Split-Path -Parent $PSScriptRoot) 'release.json' }
$identity = Get-Content -LiteralPath $ReleaseManifestPath -Raw | ConvertFrom-Json
if ($identity.schemaVersion -ne 1 -or $identity.version -cnotmatch '^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$' -or
    $identity.channel -notin @('release-candidate','stable')) { throw 'Invalid release identity.' }
$numbers = @($identity.version.Split('.') | ForEach-Object { [int]$_ })
if (@($numbers | Where-Object { $_ -gt 65535 }).Count) { throw 'Release version exceeds PE version range.' }
$template = Get-Content -LiteralPath (Join-Path (Split-Path -Parent $PSScriptRoot) 'src/common/version.rc.in') -Raw
$resource = $template.Replace('@VERSION_TUPLE@',(($numbers + @(0)) -join ',')).Replace('@VERSION@',[string]$identity.version)
$header = "#pragma once`n#define PVR_VERSION_TEXT `"$($identity.version)`"`n#define PVR_VERSION_WIDE L`"$($identity.version)`"`n#define PVR_RELEASE_CHANNEL `"$($identity.channel)`"`n"
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
foreach ($item in @(@('pvr_version.hpp',$header),@('pvr_version.rc',$resource))) {
    $path = Join-Path $OutputDirectory $item[0]
    if (-not (Test-Path -LiteralPath $path) -or [IO.File]::ReadAllText($path) -cne $item[1]) {
        [IO.File]::WriteAllText($path,$item[1],[Text.UTF8Encoding]::new($false))
    }
}
