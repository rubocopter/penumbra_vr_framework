[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$BlackPlagueExe,
    [Parameter(Mandatory = $true)][string]$RequiemExe,
    [Parameter(Mandatory = $true)][string]$RetailAlut,
    [Parameter(Mandatory = $true)][string]$BuildRoot
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$installer = Join-Path $repoRoot 'tools/Install-BlackPlagueSteamBootstrap.ps1'
$temporaryRoot = [System.IO.Path]::GetFullPath(
    (Join-Path ([System.IO.Path]::GetTempPath()) ('PenumbraVrSharedInstallTest-' + [guid]::NewGuid().ToString('N'))))
$tempBase = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath()).TrimEnd('\') + '\'
if (-not $temporaryRoot.StartsWith($tempBase, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Unsafe shared-install fixture path.'
}

try {
    $redist = Join-Path $temporaryRoot 'redist'
    $blackPlagueConfig = Join-Path $redist 'config'
    $requiemConfig = Join-Path $redist 'expansion01/config'
    New-Item -ItemType Directory -Path $blackPlagueConfig -Force | Out-Null
    New-Item -ItemType Directory -Path $requiemConfig -Force | Out-Null
    $bpTarget = Join-Path $redist 'Penumbra.exe'
    $requiemTarget = Join-Path $redist 'Requiem.exe'
    $alut = Join-Path $redist 'alut.dll'
    $blackPlagueLanguage = Join-Path $blackPlagueConfig 'Espanol.lang'
    $language = Join-Path $requiemConfig 'Espanol_exp.lang'
    Copy-Item -LiteralPath $BlackPlagueExe -Destination $bpTarget
    Copy-Item -LiteralPath $RequiemExe -Destination $requiemTarget
    Copy-Item -LiteralPath $RetailAlut -Destination $alut
    & (Join-Path $PSScriptRoot 'New-GameContentFixture.ps1') -Root $redist
    [System.IO.File]::WriteAllText($blackPlagueLanguage, 'original Black Plague language')
    [System.IO.File]::WriteAllText($language, 'original Requiem language')
    $originalAlut = (Get-FileHash -LiteralPath $alut -Algorithm SHA256).Hash
    $originalBlackPlagueLanguage = (Get-FileHash -LiteralPath $blackPlagueLanguage -Algorithm SHA256).Hash
    $originalLanguage = (Get-FileHash -LiteralPath $language -Algorithm SHA256).Hash
    $originalRequiemExe = (Get-FileHash -LiteralPath $requiemTarget -Algorithm SHA256).Hash

    $blackPlagueBackup = Join-Path $redist 'PenumbraVR_Espanol_original.lang'
    $backup = Join-Path $redist 'PenumbraVR_Espanol_exp_original.lang'
    $statePath = Join-Path $redist 'PenumbraVR.BlackPlague.install.json'

    & $installer -GamePath $bpTarget -BuildRoot $BuildRoot | Out-Null
    $englishState = Get-Content -LiteralPath $statePath -Raw | ConvertFrom-Json
    if ($englishState.spanishLocalizationSha256 -or
        $englishState.requiemSpanishLocalizationSha256 -or
        (Get-FileHash -LiteralPath $blackPlagueLanguage -Algorithm SHA256).Hash -ne $originalBlackPlagueLanguage -or
        (Get-FileHash -LiteralPath $language -Algorithm SHA256).Hash -ne $originalLanguage -or
        (Test-Path -LiteralPath $blackPlagueBackup) -or
        (Test-Path -LiteralPath $backup)) {
        throw 'Default install unexpectedly claimed or changed a Spanish localization.'
    }
    & $installer -GamePath $bpTarget -BuildRoot $BuildRoot -Repair | Out-Null
    $englishRepairState = Get-Content -LiteralPath $statePath -Raw | ConvertFrom-Json
    if ($englishRepairState.spanishLocalizationSha256 -or
        $englishRepairState.requiemSpanishLocalizationSha256 -or
        (Get-FileHash -LiteralPath $blackPlagueLanguage -Algorithm SHA256).Hash -ne $originalBlackPlagueLanguage -or
        (Get-FileHash -LiteralPath $language -Algorithm SHA256).Hash -ne $originalLanguage -or
        (Test-Path -LiteralPath $blackPlagueBackup) -or
        (Test-Path -LiteralPath $backup)) {
        throw 'Repair unexpectedly changed translation ownership for an English/default install.'
    }
    & $installer -GamePath $bpTarget -Restore | Out-Null

    $lock = [System.IO.File]::Open($language, [System.IO.FileMode]::Open,
        [System.IO.FileAccess]::Read, [System.IO.FileShare]::Read)
    $failure = $null
    try { & $installer -GamePath $bpTarget -BuildRoot $BuildRoot -InstallSpanishBlackPlague -InstallSpanishRequiem | Out-Null }
    catch { $failure = $_.Exception.Message }
    finally { $lock.Dispose() }
    if (-not $failure -or
        $failure -notlike 'Deployment failed and original managed files were restored:*' -or
        (Get-FileHash -LiteralPath $alut -Algorithm SHA256).Hash -ne $originalAlut -or
        (Get-FileHash -LiteralPath $language -Algorithm SHA256).Hash -ne $originalLanguage) {
        throw "Shared transaction did not roll back a locked Requiem localization: $failure"
    }

    & $installer -GamePath $bpTarget -BuildRoot $BuildRoot -InstallSpanishBlackPlague -InstallSpanishRequiem | Out-Null
    $probe = Join-Path $redist 'PenumbraVR.Requiem.Probe.dll'
    $state = Get-Content -LiteralPath $statePath -Raw | ConvertFrom-Json
    if ((Get-FileHash -LiteralPath $probe -Algorithm SHA256).Hash -ne $state.requiemProbeSha256 -or
        (Get-FileHash -LiteralPath $blackPlagueBackup -Algorithm SHA256).Hash -ne $originalBlackPlagueLanguage -or
        (Get-FileHash -LiteralPath $backup -Algorithm SHA256).Hash -ne $originalLanguage -or
        (Get-FileHash -LiteralPath $blackPlagueLanguage -Algorithm SHA256).Hash -ne
            (Get-FileHash -LiteralPath (Join-Path $repoRoot 'assets/localization/black_plague/redist/config/Espanol.lang') -Algorithm SHA256).Hash -or
        (Get-FileHash -LiteralPath $language -Algorithm SHA256).Hash -ne
            (Get-FileHash -LiteralPath (Join-Path $repoRoot 'assets/localization/requiem/redist/expansion01/config/Espanol_exp.lang') -Algorithm SHA256).Hash) {
        throw 'Shared transaction omitted the verified Requiem payload or backup.'
    }
    [System.IO.File]::WriteAllText($probe, 'damaged Requiem probe')
    & $installer -GamePath $bpTarget -BuildRoot $BuildRoot -Repair | Out-Null
    if ((Get-FileHash -LiteralPath $probe -Algorithm SHA256).Hash -ne $state.requiemProbeSha256) {
        throw 'Shared transaction did not repair the Requiem probe.'
    }
    [System.IO.File]::WriteAllText($language, 'damaged managed Requiem language')
    & $installer -GamePath $bpTarget -BuildRoot $BuildRoot -Repair | Out-Null
    $repairedStateHash = (Get-FileHash -LiteralPath $statePath -Algorithm SHA256).Hash
    & $installer -GamePath $bpTarget -BuildRoot $BuildRoot -Repair | Out-Null
    if ((Get-FileHash -LiteralPath $backup -Algorithm SHA256).Hash -ne $originalLanguage -or
        (Get-FileHash -LiteralPath $statePath -Algorithm SHA256).Hash -ne $repairedStateHash -or
        (Get-FileHash -LiteralPath $language -Algorithm SHA256).Hash -ne $state.requiemSpanishLocalizationSha256) {
        throw 'Repeated Requiem repair replaced a pristine backup or changed deployment ownership.'
    }
    & $installer -GamePath $bpTarget -Restore | Out-Null
    if ((Get-FileHash -LiteralPath $alut -Algorithm SHA256).Hash -ne $originalAlut -or
        (Get-FileHash -LiteralPath $blackPlagueLanguage -Algorithm SHA256).Hash -ne $originalBlackPlagueLanguage -or
        (Get-FileHash -LiteralPath $language -Algorithm SHA256).Hash -ne $originalLanguage -or
        (Get-FileHash -LiteralPath $requiemTarget -Algorithm SHA256).Hash -ne $originalRequiemExe -or
        (Test-Path -LiteralPath $probe) -or (Test-Path -LiteralPath $blackPlagueBackup) -or
        (Test-Path -LiteralPath $backup) -or
        (Test-Path -LiteralPath $statePath)) {
        throw 'Shared transaction failed exact restore.'
    }
    Write-Host 'Shared Black Plague/Requiem rollback, install, repair and exact restore passed.'
} finally {
    if (Test-Path -LiteralPath $temporaryRoot -PathType Container) {
        Remove-Item -LiteralPath $temporaryRoot -Recurse -Force
    }
}
