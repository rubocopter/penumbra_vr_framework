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
    & (Join-Path $PSScriptRoot 'New-GameContentFixture.ps1') -Root $redist
    & (Join-Path $PSScriptRoot 'New-GameContentFixture.ps1') -Root $overtureRedist -Games overture
    & (Join-Path $PSScriptRoot 'New-GameRuntimeFixture.ps1') -Root $redist -Game black_plague
    & (Join-Path $PSScriptRoot 'New-GameRuntimeFixture.ps1') -Root $overtureRedist -Game overture
    $steamRoot = Join-Path $temporaryRoot 'Steam'
    New-Item -ItemType Directory -Path (Join-Path $steamRoot 'steamapps') -Force | Out-Null
    $libraryPath = (Join-Path $temporaryRoot 'Library').Replace('\', '\\')
    $vdf = '"libraryfolders"' + "`n{" + "`n" +
        '  "1"' + "`n  {`n" + '    "path" "' + $libraryPath + '"' + "`n  }`n}"
    [System.IO.File]::WriteAllText((Join-Path $steamRoot 'steamapps/libraryfolders.vdf'), $vdf)

    $found = @(& $discover -SteamRoot $steamRoot)
    if ($found.Count -ne 3 -or
        @($found | Where-Object { $_.Game -eq 'Black Plague' -and $_.KnownBuild }).Count -ne 1 -or
        @($found | Where-Object { $_.Game -eq 'Requiem' -and $_.KnownBuild -and $_.Installable }).Count -ne 1 -or
        @($found | Where-Object { $_.Game -eq 'Overture' -and $_.KnownBuild -and $_.Installable }).Count -ne 1) {
        throw 'Steam library discovery did not distinguish all known executable identities.'
    }

    $manual = @(& $discover -SteamRoot (Join-Path $temporaryRoot 'NoSteam') -ManualPaths @($redist, $redist))
    if ($manual.Count -ne 2) {
        throw 'Manual redist path discovery duplicated or omitted executables.'
    }
    Remove-Item -LiteralPath (Join-Path $redist 'expansion01/maps/fixture.dae')
    $incomplete=@(& $discover -SteamRoot $steamRoot | Where-Object {$_.Game -eq 'Requiem'})
    if ($incomplete.Count -ne 1 -or $incomplete[0].Installable -or $incomplete[0].Status -ne 'incomplete') { throw 'EXE-only or damaged Requiem was accepted.' }
    & (Join-Path $PSScriptRoot 'New-GameContentFixture.ps1') -Root $redist
    Remove-Item -LiteralPath (Join-Path $redist 'config/default_settings.cfg')
    $invalidBase=@(& $discover -SteamRoot $steamRoot | Where-Object {$_.Game -eq 'Requiem'})
    if($invalidBase.Count -ne 1 -or $invalidBase[0].Installable){throw 'Requiem accepted an incomplete Black Plague base.'}
    & (Join-Path $PSScriptRoot 'New-GameContentFixture.ps1') -Root $redist
    $custom=Join-Path $temporaryRoot 'Library/steamapps/common/Custom folder with spaces'
    Move-Item -LiteralPath (Split-Path -Parent $overtureRedist) -Destination $custom
    [IO.File]::WriteAllText((Join-Path $temporaryRoot 'Library/steamapps/appmanifest_22180.acf'),'"AppState" { "appid" "22180" "installdir" "Custom folder with spaces" }')
    $renamed=@(& $discover -SteamRoot $steamRoot | Where-Object {$_.Game -eq 'Overture'})
    if($renamed.Count -ne 1 -or -not $renamed[0].Installable) { throw 'Appmanifest installdir was ignored.' }
    $legacy='"libraryfolders" { "1" "'+$libraryPath+'" }'
    [IO.File]::WriteAllText((Join-Path $steamRoot 'steamapps/libraryfolders.vdf'),$legacy)
    if(@(& $discover -SteamRoot $steamRoot).Count -ne 3) { throw 'Legacy library format omitted.' }

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
