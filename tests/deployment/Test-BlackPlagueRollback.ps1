[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$KnownGameExe,
    [Parameter(Mandatory = $true)][string]$RetailAlut,
    [Parameter(Mandatory = $true)][string]$BuildRoot
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$installer = Join-Path $repoRoot 'tools/Install-BlackPlagueSteamBootstrap.ps1'
$temporaryRoot = [System.IO.Path]::GetFullPath(
    (Join-Path ([System.IO.Path]::GetTempPath()) ('PenumbraVrRollbackTest-' + [guid]::NewGuid().ToString('N'))))
$tempBase = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath())
if (-not $temporaryRoot.StartsWith($tempBase, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Unsafe rollback fixture path.'
}

try {
    $gameRoot = Join-Path $temporaryRoot 'redist'
    $configRoot = Join-Path $gameRoot 'config'
    New-Item -ItemType Directory -Path $configRoot -Force | Out-Null
    $gameExe = Join-Path $gameRoot 'Penumbra.exe'
    $alut = Join-Path $gameRoot 'alut.dll'
    $localization = Join-Path $configRoot 'Espanol.lang'
    Copy-Item -LiteralPath $KnownGameExe -Destination $gameExe
    Copy-Item -LiteralPath $RetailAlut -Destination $alut
    [System.IO.File]::WriteAllText($localization, 'original language')
    $alutHash = (Get-FileHash -LiteralPath $alut -Algorithm SHA256).Hash
    $localizationHash = (Get-FileHash -LiteralPath $localization -Algorithm SHA256).Hash

    # The localization overwrite occurs after proxy, probe, OpenVR and hand
    # writes. A read-shared handle lets preflight read but denies replacement.
    $lock = [System.IO.File]::Open($localization, [System.IO.FileMode]::Open,
        [System.IO.FileAccess]::Read, [System.IO.FileShare]::Read)
    $failure = $null
    try {
        & $installer -GamePath $gameExe -BuildRoot $BuildRoot -CommunityTranslations | Out-Null
    } catch { $failure = $_.Exception.Message } finally { $lock.Dispose() }
    if (-not $failure -or $failure -notlike 'Deployment failed and original managed files were restored:*') {
        throw "Locked localization did not reach verified rollback: $failure"
    }
    if ((Get-FileHash -LiteralPath $alut -Algorithm SHA256).Hash -ne $alutHash -or
        (Get-FileHash -LiteralPath $localization -Algorithm SHA256).Hash -ne $localizationHash) {
        throw 'Failed deployment did not restore the original files.'
    }
    foreach ($relative in @('PenumbraVR_alut_original.dll',
                           'PenumbraVR.BlackPlague.Probe.dll', 'openvr_api.dll',
                           'PenumbraVR_Espanol_original.lang',
                           'PenumbraVR.BlackPlague.install.json',
                           'vr', 'assets/rework/HAND_Low_C.jpg')) {
        if (Test-Path -LiteralPath (Join-Path $gameRoot $relative)) {
            throw "Failed deployment left managed payload: $relative"
        }
    }
    & $installer -GamePath $gameExe -BuildRoot $BuildRoot -CommunityTranslations | Out-Null
    $state = Join-Path $gameRoot 'PenumbraVR.BlackPlague.install.json'
    $beforeUpgrade = (Get-FileHash -LiteralPath $state -Algorithm SHA256).Hash
    $beforeProxy = (Get-FileHash -LiteralPath $alut -Algorithm SHA256).Hash
    $beforeLocalization = (Get-FileHash -LiteralPath $localization -Algorithm SHA256).Hash
    $lock = [System.IO.File]::Open($localization, [System.IO.FileMode]::Open,
        [System.IO.FileAccess]::Read, [System.IO.FileShare]::Read)
    $failure = $null
    try {
        & $installer -GamePath $gameExe -BuildRoot $BuildRoot -CommunityTranslations | Out-Null
    } catch { $failure = $_.Exception.Message } finally { $lock.Dispose() }
    if (-not $failure -or $failure -notlike 'Deployment failed and original managed files were restored:*' -or
        (Get-FileHash -LiteralPath $state -Algorithm SHA256).Hash -ne $beforeUpgrade -or
        (Get-FileHash -LiteralPath $alut -Algorithm SHA256).Hash -ne $beforeProxy -or
        (Get-FileHash -LiteralPath $localization -Algorithm SHA256).Hash -ne $beforeLocalization) {
        throw "Interrupted upgrade did not preserve the installed state: $failure"
    }
    $loader = Join-Path $gameRoot 'openvr_api.dll'
    $beforeLoader = (Get-FileHash -LiteralPath $loader -Algorithm SHA256).Hash
    $lock = [System.IO.File]::Open($loader, [System.IO.FileMode]::Open,
        [System.IO.FileAccess]::Read, [System.IO.FileShare]::Read)
    $failure = $null
    try {
        & $installer -GamePath $gameExe -Restore | Out-Null
    } catch { $failure = $_.Exception.Message } finally { $lock.Dispose() }
    if (-not $failure -or $failure -notlike 'Deployment failed and original managed files were restored:*' -or
        (Get-FileHash -LiteralPath $state -Algorithm SHA256).Hash -ne $beforeUpgrade -or
        (Get-FileHash -LiteralPath $alut -Algorithm SHA256).Hash -ne $beforeProxy -or
        (Get-FileHash -LiteralPath $loader -Algorithm SHA256).Hash -ne $beforeLoader) {
        throw "Interrupted restore did not preserve the installed state: $failure"
    }
    & $installer -GamePath $gameExe -Restore | Out-Null
    if ((Get-FileHash -LiteralPath $alut -Algorithm SHA256).Hash -ne $alutHash -or
        (Get-FileHash -LiteralPath $localization -Algorithm SHA256).Hash -ne $localizationHash -or
        (Test-Path -LiteralPath $state)) {
        throw 'Restore after interrupted upgrade did not return the original files.'
    }
    Write-Host 'Black Plague interrupted install/upgrade/restore rolled back; clean restore recovered originals.'
} finally {
    if (Test-Path -LiteralPath $temporaryRoot -PathType Container) {
        Remove-Item -LiteralPath $temporaryRoot -Recurse -Force
    }
}
