[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$KnownGameExe,
    [Parameter(Mandatory = $true)][string]$RetailAlut,
    [Parameter(Mandatory = $true)][string]$BuildRoot
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$installer = Join-Path $repoRoot 'tools/Install-BlackPlagueSteamBootstrap.ps1'
$tempBase = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath()).TrimEnd('\') + '\'
$fixture = [System.IO.Path]::GetFullPath((Join-Path $tempBase ('PenumbraVrBpCrash-' + [guid]::NewGuid().ToString('N'))))
if (-not $fixture.StartsWith($tempBase, [StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe Black Plague crash fixture.' }

function Get-Sha256([string]$path) { return (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash }
function Assert-Hash([string]$path, [string]$expected) {
    if ((Get-Sha256 $path) -ne $expected) { throw "Crash recovery changed the wrong file: $path" }
}
function Invoke-CrashedDeployment([string]$phase, [string]$childScript,
    [string]$gameExe, [string]$buildRoot, [string]$localization,
    [string]$loader, [bool]$restore, [bool]$laa) {
    $restoreValue = if ($restore) { 'true' } else { 'false' }
    $laaValue = if ($laa) { 'true' } else { 'false' }
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $childScript `
        -InstallerPath $installer -GameExe $gameExe -BuildRoot $buildRoot `
        -LocalizationPath $localization -LoaderPath $loader `
        -RestoreValue $restoreValue -LaaValue $laaValue 6>$null | Out-Null
    if ($LASTEXITCODE -eq 0) { throw "The $phase deployment did not terminate abruptly." }
    $journal = Join-Path (Split-Path -Parent $gameExe) '.penumbravr-bp-journal'
    if (-not (Test-Path -LiteralPath $journal -PathType Container)) {
        throw "The $phase deployment did not leave a durable recovery journal."
    }
}

try {
    $gameRoot = Join-Path $fixture 'redist'
    if (-not $gameRoot.StartsWith($fixture + [System.IO.Path]::DirectorySeparatorChar,
            [StringComparison]::OrdinalIgnoreCase)) { throw 'Fixture game escapes temporary root.' }
    New-Item -ItemType Directory -Path (Join-Path $gameRoot 'config') -Force | Out-Null
    $exe = Join-Path $gameRoot 'Penumbra.exe'
    $alut = Join-Path $gameRoot 'alut.dll'
    $localization = Join-Path $gameRoot 'config/Espanol.lang'
    $loader = Join-Path $gameRoot 'openvr_api.dll'
    $state = Join-Path $gameRoot 'PenumbraVR.BlackPlague.install.json'
    Copy-Item -LiteralPath $KnownGameExe -Destination $exe
    Copy-Item -LiteralPath $RetailAlut -Destination $alut
    [System.IO.File]::WriteAllText($localization, 'retail localization')
    $originalExe = Get-Sha256 $exe
    $originalAlut = Get-Sha256 $alut
    $originalLocalization = Get-Sha256 $localization
    $childScript = Join-Path $fixture 'crash-child.ps1'
    @'
[CmdletBinding()]
param([string]$InstallerPath, [string]$GameExe, [string]$BuildRoot,
      [string]$LocalizationPath, [string]$LoaderPath,
      [string]$RestoreValue, [string]$LaaValue)
$ErrorActionPreference = 'Stop'
$global:CrashLocalizationPath = $LocalizationPath
$global:CrashLoaderPath = $LoaderPath
$global:CrashOnRestore = $RestoreValue -eq 'true'
function Copy-Item {
    [CmdletBinding()]
    param([string]$LiteralPath, [string]$Destination, [switch]$Recurse, [switch]$Force)
    if (-not $global:CrashOnRestore -and $Destination -ieq $global:CrashLocalizationPath) {
        Stop-Process -Id $PID -Force
    }
    Microsoft.PowerShell.Management\Copy-Item @PSBoundParameters
}
function Remove-Item {
    [CmdletBinding()]
    param([string]$LiteralPath, [switch]$Recurse, [switch]$Force)
    if ($global:CrashOnRestore -and $LiteralPath -ieq $global:CrashLoaderPath) {
        Stop-Process -Id $PID -Force
    }
    Microsoft.PowerShell.Management\Remove-Item @PSBoundParameters
}
if ($global:CrashOnRestore) {
    & $InstallerPath -GamePath $GameExe -Restore 6>$null | Out-Null
} elseif ($LaaValue -eq 'true') {
    & $InstallerPath -GamePath $GameExe -BuildRoot $BuildRoot -LargeAddressAware 6>$null | Out-Null
} else {
    & $InstallerPath -GamePath $GameExe -BuildRoot $BuildRoot 6>$null | Out-Null
}
'@ | Set-Content -LiteralPath $childScript -Encoding UTF8

    Invoke-CrashedDeployment 'install' $childScript $exe $BuildRoot $localization $loader $false $false
    if ((Get-Sha256 $alut) -eq $originalAlut) { throw 'Install did not change ALUT before crash.' }
    $beforePendingRefusal = Get-Sha256 $alut
    $rejectedPending = $false
    try { & $installer -GamePath $exe -BuildRoot $BuildRoot 6>$null | Out-Null }
    catch { $rejectedPending = $_.Exception.Message -like '*needs -Recover*' }
    if (-not $rejectedPending -or (Get-Sha256 $alut) -ne $beforePendingRefusal) {
        throw 'Black Plague started another deployment while recovery was pending.'
    }
    $journalRecord = Get-Content -LiteralPath (Join-Path $gameRoot '.penumbravr-bp-journal/journal.json') -Raw | ConvertFrom-Json
    $alutCopy = @($journalRecord.Entries | Where-Object { $_.Target -ieq $alut })[0].Copy
    [System.IO.File]::AppendAllText($alutCopy, 'altered')
    $rejectedCorruptCopy = $false
    try { & $installer -GamePath $exe -Recover 6>$null | Out-Null }
    catch { $rejectedCorruptCopy = $_.Exception.Message -like '*failed hash verification*' }
    if (-not $rejectedCorruptCopy -or (Get-Sha256 $alut) -ne $beforePendingRefusal) {
        throw 'Black Plague recovery used a corrupt journal copy or wrote before validation.'
    }
    Copy-Item -LiteralPath $RetailAlut -Destination $alutCopy -Force
    Remove-Item -LiteralPath $exe -Force
    & $installer -GamePath $exe -Recover 6>$null | Out-Null
    Assert-Hash $exe $originalExe
    Assert-Hash $alut $originalAlut
    Assert-Hash $localization $originalLocalization
    if (Test-Path -LiteralPath $state) { throw 'Fresh install recovery left deployment state.' }

    & $installer -GamePath $exe -BuildRoot $BuildRoot 6>$null | Out-Null
    $installedAlut = Get-Sha256 $alut
    $installedState = Get-Sha256 $state
    Invoke-CrashedDeployment 'LAA upgrade' $childScript $exe $BuildRoot $localization $loader $false $true
    if ((Get-Sha256 $exe) -ne 'DB086CC7A4C7B10864DE0FEBBE2D71A3E4EFF1EC8D067811A6A59EDC1C617196') {
        throw 'LAA upgrade did not change executable before crash.'
    }
    $journalRecord = Get-Content -LiteralPath (Join-Path $gameRoot '.penumbravr-bp-journal/journal.json') -Raw | ConvertFrom-Json
    $vrCopy = @($journalRecord.Entries | Where-Object { $_.Target -ieq (Join-Path $gameRoot 'vr') })[0].Copy
    $unexpected = Join-Path $vrCopy 'unexpected.txt'
    [System.IO.File]::WriteAllText($unexpected, 'altered')
    $rejectedCorruptDirectory = $false
    try { & $installer -GamePath $exe -Recover 6>$null | Out-Null }
    catch { $rejectedCorruptDirectory = $_.Exception.Message -like '*recovery directory failed verification*' }
    if (-not $rejectedCorruptDirectory -or
        (Get-Sha256 $exe) -ne 'DB086CC7A4C7B10864DE0FEBBE2D71A3E4EFF1EC8D067811A6A59EDC1C617196') {
        throw 'Black Plague recovery used a corrupt OpenVR directory or wrote before validation.'
    }
    Remove-Item -LiteralPath $unexpected -Force
    & $installer -GamePath $exe -Recover 6>$null | Out-Null
    Assert-Hash $exe $originalExe
    Assert-Hash $alut $installedAlut
    Assert-Hash $state $installedState

    & $installer -GamePath $exe -BuildRoot $BuildRoot -LargeAddressAware 6>$null | Out-Null
    $laaExe = Get-Sha256 $exe
    $laaState = Get-Sha256 $state
    Invoke-CrashedDeployment 'restore' $childScript $exe $BuildRoot $localization $loader $true $false
    if ((Get-Sha256 $alut) -ne $originalAlut) { throw 'Restore did not change ALUT before crash.' }
    & $installer -GamePath $exe -Recover 6>$null | Out-Null
    Assert-Hash $exe $laaExe
    Assert-Hash $alut $installedAlut
    Assert-Hash $state $laaState

    & $installer -GamePath $exe -Restore 6>$null | Out-Null
    Assert-Hash $exe $originalExe
    Assert-Hash $alut $originalAlut
    Assert-Hash $localization $originalLocalization
    if ((Test-Path -LiteralPath $state) -or
        (Test-Path -LiteralPath (Join-Path $gameRoot '.penumbravr-bp-journal'))) {
        throw 'Final restore left deployment state or recovery journal.'
    }
    Write-Host 'Black Plague recovered interrupted install, LAA upgrade and restore from durable journals.'
}
finally {
    if (Test-Path -LiteralPath $fixture -PathType Container) {
        $resolved = [System.IO.Path]::GetFullPath($fixture)
        if (-not $resolved.StartsWith($tempBase, [StringComparison]::OrdinalIgnoreCase)) {
            throw "Unsafe Black Plague crash fixture cleanup: $resolved"
        }
        Remove-Item -LiteralPath $resolved -Recurse -Force
    }
}
