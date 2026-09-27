[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$KnownGameExe
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$packager = Join-Path $repoRoot 'tools/Package-OvertureCandidate.ps1'
$temporaryRoot = [System.IO.Path]::GetFullPath(
    (Join-Path ([System.IO.Path]::GetTempPath()) ('PenumbraVrOverturePackageTest-' + [guid]::NewGuid().ToString('N'))))
$tempBase = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath())
if (-not $temporaryRoot.StartsWith($tempBase, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Unsafe Overture package fixture path.'
}

try {
    New-Item -ItemType Directory -Path $temporaryRoot | Out-Null
    $archive = Join-Path $temporaryRoot 'OvertureCandidate.zip'
    $again = Join-Path $temporaryRoot 'OvertureCandidateAgain.zip'
    & $packager -OutputPath $archive
    & $packager -OutputPath $again
    if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne
        (Get-FileHash -LiteralPath $again -Algorithm SHA256).Hash) {
        throw 'Identical Overture package inputs produced different ZIP archives.'
    }
    $unpacked = Join-Path $temporaryRoot 'unpacked'
    Expand-Archive -LiteralPath $archive -DestinationPath $unpacked
    $installer = Join-Path $unpacked 'Install-PenumbraVR.ps1'
    $gameRoot = Join-Path $temporaryRoot 'game'
    $redist = Join-Path $gameRoot 'redist'
    New-Item -ItemType Directory -Path (Join-Path $redist 'config') -Force | Out-Null
    $gameExe = Join-Path $redist 'Penumbra.exe'
    Copy-Item -LiteralPath $KnownGameExe -Destination $gameExe
    Copy-Item -LiteralPath (Join-Path $repoRoot 'products/overture/data/config/English.lang') -Destination (Join-Path $redist 'config/English.lang')
    $originalHash = (Get-FileHash -LiteralPath $gameExe -Algorithm SHA256).Hash
    & $installer -PackageRoot $unpacked -InstallRoot $gameRoot -SteamLauncher:$true 6>$null | Out-Null
    $state = Join-Path $gameRoot '.penumbravr/deploy-state.json'
    if (-not (Test-Path -LiteralPath $state -PathType Leaf) -or
        (Get-FileHash -LiteralPath $gameExe -Algorithm SHA256).Hash -ne
            (Get-FileHash -LiteralPath (Join-Path $unpacked 'Penumbra_vr.exe') -Algorithm SHA256).Hash) {
        throw 'Standalone Overture ZIP did not install its packaged executable.'
    }
    $installedExeHash = (Get-FileHash -LiteralPath $gameExe -Algorithm SHA256).Hash
    $binding = Join-Path $redist 'vr/bindings/vive_controller.json'
    $bindingSource = Join-Path $unpacked 'vr/bindings/vive_controller.json'
    [System.IO.File]::AppendAllText($binding, 'external edit')
    $rejectedUpgrade = $false
    try { & $installer -PackageRoot $unpacked -InstallRoot $gameRoot 6>$null | Out-Null }
    catch { $rejectedUpgrade = $_.Exception.Message -like '*externally modified managed file*' }
    if (-not $rejectedUpgrade -or
        (Get-FileHash -LiteralPath $gameExe -Algorithm SHA256).Hash -ne $installedExeHash -or
        -not (Test-Path -LiteralPath $state -PathType Leaf)) {
        throw 'Overture upgrade changed files before rejecting an external edit.'
    }
    $rejectedRestore = $false
    try { & $installer -InstallRoot $gameRoot -Restore 6>$null | Out-Null }
    catch { $rejectedRestore = $_.Exception.Message -like '*externally modified managed file*' }
    if (-not $rejectedRestore -or
        (Get-FileHash -LiteralPath $gameExe -Algorithm SHA256).Hash -ne $installedExeHash -or
        -not (Test-Path -LiteralPath $state -PathType Leaf)) {
        throw 'Overture restore changed earlier files before rejecting an external edit.'
    }
    Copy-Item -LiteralPath $bindingSource -Destination $binding -Force
    & $installer -InstallRoot $gameRoot -Restore 6>$null | Out-Null
    if ((Get-FileHash -LiteralPath $gameExe -Algorithm SHA256).Hash -ne $originalHash -or
        (Test-Path -LiteralPath $state)) {
        throw 'Standalone Overture ZIP did not restore the original executable.'
    }
    [System.IO.File]::AppendAllText((Join-Path $unpacked 'README.md'), 'tampered')
    $tamperedArchive = Join-Path $temporaryRoot 'Tampered.zip'
    $rejected = $false
    try { & $packager -PackageRoot $unpacked -OutputPath $tamperedArchive 6>$null | Out-Null }
    catch { $rejected = $_.Exception.Message -like '*hash mismatch*' }
    if (-not $rejected -or (Test-Path -LiteralPath $tamperedArchive)) {
        throw 'Overture packager did not reject a modified package before writing.'
    }
    Write-Host 'Overture ZIP is deterministic and installs/restores independently of the source checkout.'
} finally {
    if (Test-Path -LiteralPath $temporaryRoot -PathType Container) {
        Remove-Item -LiteralPath $temporaryRoot -Recurse -Force
    }
}
