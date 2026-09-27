[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$KnownGameExe,
    [Parameter(Mandatory = $true)][string]$RetailAlut,
    [Parameter(Mandatory = $true)][string]$BuildRoot
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$installer = Join-Path $repoRoot 'tools/Install-BlackPlagueSteamBootstrap.ps1'
$expectedExe = 'FD316F7586737A63EBA989ECE2271280FE6A98582A1319FE2151385A3DF97BFF'
$expectedAlut = 'D81DEA8E88E35C319F7F2D8AAEB14C63A4986131492D3DF860D1F2C18B844590'

if ((Get-FileHash -LiteralPath $KnownGameExe -Algorithm SHA256).Hash -ne $expectedExe) {
    throw 'Test requires the known canonical Black Plague executable.'
}
if ((Get-FileHash -LiteralPath $RetailAlut -Algorithm SHA256).Hash -ne $expectedAlut) {
    throw 'Test requires the known retail alut.dll.'
}

$fixture = Join-Path ([System.IO.Path]::GetTempPath()) ('PenumbraVrInstallerTest-' + [guid]::NewGuid().ToString('N'))
$fixture = [System.IO.Path]::GetFullPath($fixture)
if (-not $fixture.StartsWith([System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath()), [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Unsafe test fixture path.'
}

try {
    New-Item -ItemType Directory -Path $fixture | Out-Null
    $gameExe = Join-Path $fixture 'Penumbra.exe'
    Copy-Item -LiteralPath $KnownGameExe -Destination $gameExe
    Copy-Item -LiteralPath $RetailAlut -Destination (Join-Path $fixture 'alut.dll')
    $configDirectory = Join-Path $fixture 'config'
    New-Item -ItemType Directory -Path $configDirectory | Out-Null
    $originalLocalization = Join-Path $configDirectory 'Espanol.lang'
    [System.IO.File]::WriteAllText($originalLocalization, 'retail localization fixture')
    $originalLocalizationHash = (Get-FileHash -LiteralPath $originalLocalization -Algorithm SHA256).Hash

    & $installer -GamePath $gameExe -BuildRoot $BuildRoot | Out-Null
    $probe = Join-Path $fixture 'PenumbraVR.BlackPlague.Probe.dll'
    $localization = Join-Path $fixture 'config/Espanol.lang'
    if (-not (Test-Path -LiteralPath $probe -PathType Leaf) -or
        -not (Test-Path -LiteralPath $localization -PathType Leaf)) {
        throw 'Fixture installation did not create its expected payloads.'
    }
    & $installer -GamePath $gameExe -BuildRoot $BuildRoot | Out-Null
    Write-Host 'Black Plague clean upgrade completed.'

    [System.IO.File]::WriteAllText($probe, 'third-party probe change')
    [System.IO.File]::WriteAllText($localization, 'user localization change')
    $probeBefore = (Get-FileHash -LiteralPath $probe -Algorithm SHA256).Hash
    $localizationBefore = (Get-FileHash -LiteralPath $localization -Algorithm SHA256).Hash
    $state = Join-Path $fixture 'PenumbraVR.BlackPlague.install.json'
    $stateBefore = (Get-FileHash -LiteralPath $state -Algorithm SHA256).Hash

    $rejected = $false
    try {
        & $installer -GamePath $gameExe -BuildRoot $BuildRoot | Out-Null
    } catch {
        $rejected = $true
    }
    if (-not $rejected) {
        throw 'Upgrade accepted externally modified payloads.'
    }
    if ((Get-FileHash -LiteralPath $probe -Algorithm SHA256).Hash -ne $probeBefore -or
        (Get-FileHash -LiteralPath $localization -Algorithm SHA256).Hash -ne $localizationBefore -or
        (Get-FileHash -LiteralPath $state -Algorithm SHA256).Hash -ne $stateBefore) {
        throw 'Rejected upgrade modified the existing installation.'
    }
    Write-Host 'Black Plague upgrade preflight preserved externally modified files.'

    $localizationSource = Join-Path $repoRoot 'assets/localization/black_plague/redist/config/Espanol.lang'
    Copy-Item -LiteralPath $localizationSource -Destination $localization -Force
    $alut = Join-Path $fixture 'alut.dll'
    $alutBefore = (Get-FileHash -LiteralPath $alut -Algorithm SHA256).Hash
    try {
        & powershell -NoProfile -ExecutionPolicy Bypass -File $installer -GamePath $gameExe -BuildRoot $BuildRoot -Restore 2>&1 | Out-Null
    } catch {
        # Windows PowerShell promotes native stderr to an error under Stop.
    }
    if ($LASTEXITCODE -eq 0) {
        throw 'Restore accepted an externally modified probe.'
    }
    if ((Get-FileHash -LiteralPath $alut -Algorithm SHA256).Hash -ne $alutBefore -or
        (Get-FileHash -LiteralPath $probe -Algorithm SHA256).Hash -ne $probeBefore -or
        (Get-FileHash -LiteralPath $state -Algorithm SHA256).Hash -ne $stateBefore) {
        throw 'Rejected restore modified the existing installation.'
    }
    Write-Host 'Black Plague restore preflight preserved externally modified files.'

    Copy-Item -LiteralPath (Join-Path $BuildRoot 'bin/Release/PenumbraVR.BlackPlague.Probe.dll') -Destination $probe -Force
    $foreignAsset = Join-Path $fixture 'vr/foreign-user-file.txt'
    [System.IO.File]::WriteAllText($foreignAsset, 'unrelated file')
    try {
        & powershell -NoProfile -ExecutionPolicy Bypass -File $installer -GamePath $gameExe -BuildRoot $BuildRoot -Restore 2>&1 | Out-Null
    } catch {
        # Windows PowerShell promotes native stderr to an error under Stop.
    }
    if ($LASTEXITCODE -eq 0 -or
        -not (Test-Path -LiteralPath $foreignAsset -PathType Leaf) -or
        (Get-FileHash -LiteralPath $alut -Algorithm SHA256).Hash -ne $alutBefore -or
        (Get-FileHash -LiteralPath $state -Algorithm SHA256).Hash -ne $stateBefore) {
        throw 'Restore removed an unrelated OpenVR asset or changed the installation before rejection.'
    }
    Write-Host 'Black Plague restore preflight preserved unrelated OpenVR assets.'

    Remove-Item -LiteralPath $foreignAsset
    $localizationBackup = Join-Path $fixture 'PenumbraVR_Espanol_original.lang'
    [System.IO.File]::AppendAllText($localizationBackup, 'external change')
    try {
        & powershell -NoProfile -ExecutionPolicy Bypass -File $installer -GamePath $gameExe -BuildRoot $BuildRoot -Restore 2>&1 | Out-Null
    } catch {
        # Windows PowerShell promotes native stderr to an error under Stop.
    }
    if ($LASTEXITCODE -eq 0 -or
        (Get-FileHash -LiteralPath $alut -Algorithm SHA256).Hash -ne $alutBefore -or
        (Get-FileHash -LiteralPath $state -Algorithm SHA256).Hash -ne $stateBefore) {
        throw 'Restore changed the installation before rejecting a modified localization backup.'
    }
    [System.IO.File]::WriteAllText($localizationBackup, 'retail localization fixture')

    & powershell -NoProfile -ExecutionPolicy Bypass -File $installer -GamePath $gameExe -BuildRoot $BuildRoot -Restore | Out-Null
    if ($LASTEXITCODE -ne 0 -or
        (Get-FileHash -LiteralPath $alut -Algorithm SHA256).Hash -ne $expectedAlut -or
        (Get-FileHash -LiteralPath $originalLocalization -Algorithm SHA256).Hash -ne $originalLocalizationHash -or
        (Test-Path -LiteralPath $probe) -or
        (Test-Path -LiteralPath $state) -or
        (Test-Path -LiteralPath $localizationBackup) -or
        (Test-Path -LiteralPath (Join-Path $fixture 'vr'))) {
        throw 'Clean restore did not return the fixture to its original state.'
    }
    Write-Host 'Black Plague clean restore returned the fixture to its original state.'

    $foreignVrRoot = Join-Path $fixture 'vr'
    New-Item -ItemType Directory -Path $foreignVrRoot | Out-Null
    $foreignFile = Join-Path $foreignVrRoot 'user-actions.json'
    [System.IO.File]::WriteAllText($foreignFile, 'unrelated actions')
    $rejected = $false
    try {
        & $installer -GamePath $gameExe -BuildRoot $BuildRoot | Out-Null
    } catch {
        $rejected = $true
    }
    if (-not $rejected -or
        -not (Test-Path -LiteralPath $foreignFile -PathType Leaf) -or
        (Get-FileHash -LiteralPath $alut -Algorithm SHA256).Hash -ne $expectedAlut -or
        (Test-Path -LiteralPath $state) -or
        (Test-Path -LiteralPath (Join-Path $fixture 'PenumbraVR_alut_original.dll'))) {
        throw 'Fresh install replaced unrelated OpenVR assets or created partial state.'
    }
    Write-Host 'Black Plague fresh-install preflight preserved unrelated OpenVR assets.'
} finally {
    if (Test-Path -LiteralPath $fixture -PathType Container) {
        Remove-Item -LiteralPath $fixture -Recurse -Force
    }
}
