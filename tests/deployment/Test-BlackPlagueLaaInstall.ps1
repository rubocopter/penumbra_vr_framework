[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$KnownGameExe,
    [Parameter(Mandatory = $true)][string]$RetailAlut,
    [Parameter(Mandatory = $true)][string]$BuildRoot
)

$ErrorActionPreference = 'Stop'
# Fixture deployments must never edit the real Documents configuration.
$PSDefaultParameterValues = @{ '*:SettingsScope' = 'DefaultFiles' }
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$installer = Join-Path $repoRoot 'tools/Install-BlackPlagueSteamBootstrap.ps1'
$canonical = 'FD316F7586737A63EBA989ECE2271280FE6A98582A1319FE2151385A3DF97BFF'
$transformed = 'DB086CC7A4C7B10864DE0FEBBE2D71A3E4EFF1EC8D067811A6A59EDC1C617196'
$temporaryRoot = [System.IO.Path]::GetFullPath(
    (Join-Path ([System.IO.Path]::GetTempPath()) ('PenumbraVrLaaInstallTest-' + [guid]::NewGuid().ToString('N'))))
$tempPrefix = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath()).TrimEnd('\') + '\'
if (-not $temporaryRoot.StartsWith($tempPrefix, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Unsafe LAA installation fixture path.'
}
try {
    $gameRoot = Join-Path $temporaryRoot 'redist'
    New-Item -ItemType Directory -Path (Join-Path $gameRoot 'config') -Force | Out-Null
    $gameExe = Join-Path $gameRoot 'Penumbra.exe'
    $alut = Join-Path $gameRoot 'alut.dll'
    $backup = Join-Path $gameRoot 'PenumbraVR_Penumbra_original.exe'
    $state = Join-Path $gameRoot 'PenumbraVR.BlackPlague.install.json'
    $localization = Join-Path $gameRoot 'config/Espanol.lang'
    Copy-Item -LiteralPath $KnownGameExe -Destination $gameExe
    Copy-Item -LiteralPath $RetailAlut -Destination $alut
    [System.IO.File]::WriteAllText($localization, 'original language')
    if ((Get-FileHash -LiteralPath $gameExe -Algorithm SHA256).Hash -ne $canonical) {
        throw 'Test fixture is not the canonical Black Plague build.'
    }

    $lock = [System.IO.File]::Open($localization, [System.IO.FileMode]::Open,
        [System.IO.FileAccess]::Read, [System.IO.FileShare]::Read)
    $failure = $null
    try { & $installer -GamePath $gameExe -BuildRoot $BuildRoot -LargeAddressAware 6>$null | Out-Null }
    catch { $failure = $_.Exception.Message } finally { $lock.Dispose() }
    if (-not $failure -or $failure -notlike 'Deployment failed and original managed files were restored:*' -or
        (Get-FileHash -LiteralPath $gameExe -Algorithm SHA256).Hash -ne $canonical -or
        (Test-Path -LiteralPath $backup) -or (Test-Path -LiteralPath $state)) {
        throw "Interrupted LAA installation did not roll back: $failure"
    }

    & $installer -GamePath $gameExe -BuildRoot $BuildRoot -LargeAddressAware 6>$null | Out-Null
    $record = Get-Content -LiteralPath $state -Raw | ConvertFrom-Json
    if ((Get-FileHash -LiteralPath $gameExe -Algorithm SHA256).Hash -ne $transformed -or
        (Get-FileHash -LiteralPath $backup -Algorithm SHA256).Hash -ne $canonical -or
        -not $record.laaApplied) {
        throw 'LAA installation did not verify the transformed executable and original backup.'
    }
    & $installer -GamePath $gameExe -BuildRoot $BuildRoot 6>$null | Out-Null
    if ((Get-FileHash -LiteralPath $gameExe -Algorithm SHA256).Hash -ne $transformed -or
        -not (Get-Content -LiteralPath $state -Raw | ConvertFrom-Json).laaApplied) {
        throw 'LAA upgrade lost executable ownership.'
    }
    & $installer -GamePath $gameExe -Restore 6>$null | Out-Null
    if ((Get-FileHash -LiteralPath $gameExe -Algorithm SHA256).Hash -ne $canonical -or
        (Test-Path -LiteralPath $backup) -or (Test-Path -LiteralPath $state)) {
        throw 'LAA restore did not return the exact canonical executable.'
    }
    $alreadyLaa = Join-Path $temporaryRoot 'AlreadyLaa.exe'
    & (Join-Path $BuildRoot 'bin/Release/PenumbraVR.LaaTransform.exe') $gameExe $alreadyLaa | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Could not prepare the pre-existing LAA fixture.' }
    Copy-Item -LiteralPath $alreadyLaa -Destination $gameExe -Force
    & $installer -GamePath $gameExe -BuildRoot $BuildRoot -LargeAddressAware 6>$null | Out-Null
    if ((Get-Content -LiteralPath $state -Raw | ConvertFrom-Json).laaApplied -or
        (Test-Path -LiteralPath $backup)) {
        throw 'Installer claimed ownership of a pre-existing LAA executable.'
    }
    & $installer -GamePath $gameExe -Restore 6>$null | Out-Null
    if ((Get-FileHash -LiteralPath $gameExe -Algorithm SHA256).Hash -ne $transformed) {
        throw 'Restore changed a pre-existing LAA executable.'
    }
    Copy-Item -LiteralPath $KnownGameExe -Destination $gameExe -Force
    & $installer -GamePath $gameExe -BuildRoot $BuildRoot 6>$null | Out-Null
    & $installer -GamePath $gameExe -BuildRoot $BuildRoot -LargeAddressAware 6>$null | Out-Null
    if ((Get-FileHash -LiteralPath $gameExe -Algorithm SHA256).Hash -ne $transformed -or
        -not (Get-Content -LiteralPath $state -Raw | ConvertFrom-Json).laaApplied) {
        throw 'Upgrade to managed LAA did not preserve ownership.'
    }
    & $installer -GamePath $gameExe -Restore 6>$null | Out-Null
    if ((Get-FileHash -LiteralPath $gameExe -Algorithm SHA256).Hash -ne $canonical) {
        throw 'Restore after LAA upgrade did not recover the canonical executable.'
    }
    Write-Host 'Black Plague LAA install, rollback, upgrade and restore preserved exact builds.'
} finally {
    if (Test-Path -LiteralPath $temporaryRoot -PathType Container) {
        Remove-Item -LiteralPath $temporaryRoot -Recurse -Force
    }
}
