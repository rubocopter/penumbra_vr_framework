[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$BlackPlagueExe,
    [Parameter(Mandatory = $true)][string]$RequiemExe,
    [Parameter(Mandatory = $true)][string]$OvertureCheckpointExe
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$discover = Join-Path $repoRoot 'tools/Get-PenumbraInstallations.ps1'
$temporaryRoot = Join-Path ([System.IO.Path]::GetTempPath()) ('PenumbraVrDiscoverTest-' + [guid]::NewGuid().ToString('N'))
$temporaryRoot = [System.IO.Path]::GetFullPath($temporaryRoot)
$tempBase = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath())
if (-not $temporaryRoot.StartsWith($tempBase, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Unsafe discovery fixture path.'
}

try {
    $redist = Join-Path $temporaryRoot 'Library/steamapps/common/Penumbra Black Plague/redist'
    New-Item -ItemType Directory -Path $redist -Force | Out-Null
    Copy-Item -LiteralPath $BlackPlagueExe -Destination (Join-Path $redist 'Penumbra.exe')
    Copy-Item -LiteralPath $RequiemExe -Destination (Join-Path $redist 'Requiem.exe')
    $overtureRedist = Join-Path $temporaryRoot 'Library/steamapps/common/Penumbra Overture/redist'
    New-Item -ItemType Directory -Path $overtureRedist -Force | Out-Null
    Copy-Item -LiteralPath $OvertureCheckpointExe -Destination (Join-Path $overtureRedist 'Penumbra.exe')
    $steamRoot = Join-Path $temporaryRoot 'Steam'
    New-Item -ItemType Directory -Path (Join-Path $steamRoot 'steamapps') -Force | Out-Null
    $libraryPath = (Join-Path $temporaryRoot 'Library').Replace('\', '\\')
    $vdf = '"libraryfolders"' + "`n{" + "`n" +
        '  "1"' + "`n  {`n" + '    "path" "' + $libraryPath + '"' + "`n  }`n}"
    [System.IO.File]::WriteAllText((Join-Path $steamRoot 'steamapps/libraryfolders.vdf'), $vdf)

    $found = @(& $discover -SteamRoot $steamRoot)
    if ($found.Count -ne 3 -or
        @($found | Where-Object { $_.Game -eq 'Black Plague' -and $_.KnownBuild }).Count -ne 1 -or
        @($found | Where-Object { $_.Game -eq 'Requiem' -and $_.KnownBuild -and -not $_.Installable }).Count -ne 1 -or
        @($found | Where-Object { $_.Game -eq 'Overture' -and $_.KnownBuild -and -not $_.Installable }).Count -ne 1) {
        throw 'Steam library discovery did not distinguish all known executable identities.'
    }

    $manual = @(& $discover -SteamRoot (Join-Path $temporaryRoot 'NoSteam') -ManualPaths @($redist, $redist))
    if ($manual.Count -ne 2) {
        throw 'Manual redist path discovery duplicated or omitted executables.'
    }

    $unknown = Join-Path $redist 'Unknown.exe'
    Copy-Item -LiteralPath $BlackPlagueExe -Destination $unknown
    $bytes = [System.IO.File]::ReadAllBytes($unknown)
    $bytes[$bytes.Length - 1] = $bytes[$bytes.Length - 1] -bxor 1
    [System.IO.File]::WriteAllBytes($unknown, $bytes)
    $unsupported = @(& $discover -SteamRoot (Join-Path $temporaryRoot 'NoSteam') -ManualPaths @($unknown))
    if ($unsupported.Count -ne 1 -or $unsupported[0].KnownBuild -or $unsupported[0].Installable) {
        throw 'Unknown executable was marked installable or hidden.'
    }
    $invalid = Join-Path $temporaryRoot 'Invalid.exe'
    [System.IO.File]::WriteAllText($invalid, 'not a PE executable')
    $invalidResult = @(& $discover -SteamRoot (Join-Path $temporaryRoot 'NoSteam') -ManualPaths @($invalid))
    if ($invalidResult.Count -ne 1 -or $invalidResult[0].KnownBuild -or
        $invalidResult[0].Installable -or $invalidResult[0].Game -ne 'Unknown') {
        throw 'Invalid executable was not reported as non-installable.'
    }
    Write-Host 'Steam and manual discovery distinguish shared-folder games and reject unknown builds.'
} finally {
    if (Test-Path -LiteralPath $temporaryRoot -PathType Container) {
        Remove-Item -LiteralPath $temporaryRoot -Recurse -Force
    }
}
