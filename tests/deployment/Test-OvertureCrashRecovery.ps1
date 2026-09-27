[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$installer = Join-Path (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)) 'products/overture/scripts/deploy.ps1'
$tempBase = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath()).TrimEnd('\') + '\'
$fixture = [System.IO.Path]::GetFullPath((Join-Path $tempBase ('PenumbraVrOvertureCrash-' + [guid]::NewGuid().ToString('N'))))
if (-not $fixture.StartsWith($tempBase, [StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe crash fixture path.' }

function Write-FixtureFile([string]$path, [string]$value) {
    New-Item -ItemType Directory -Path (Split-Path -Parent $path) -Force | Out-Null
    [System.IO.File]::WriteAllText($path, $value)
}

function Get-Sha256([string]$path) {
    return (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
}

function Assert-Hash([string]$path, [string]$expected) {
    if ((Get-Sha256 $path) -ne $expected) { throw "Crash recovery changed the wrong file: $path" }
}

function Invoke-CrashedDeployment([string]$phase, [string]$childScript,
    [string]$installerPath, [string]$packagePath, [string]$gamePath,
    [string]$blockedPath, [bool]$restore) {
    $restoreValue = if ($restore) { 'true' } else { 'false' }
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $childScript `
        -InstallerPath $installerPath -PackagePath $packagePath -GamePath $gamePath `
        -BlockedPath $blockedPath -RestoreValue $restoreValue 6>$null | Out-Null
    if ($LASTEXITCODE -eq 0) { throw "The $phase child deployment did not terminate abruptly." }
    $journal = Join-Path $gamePath '.penumbravr-journal'
    if (-not (Test-Path -LiteralPath $journal -PathType Container)) {
        throw "The $phase deployment did not leave a durable recovery journal."
    }
}

try {
    $package = Join-Path $fixture 'package'
    $game = Join-Path $fixture 'game'
    $redist = Join-Path $game 'redist'
    $exe = Join-Path $redist 'Penumbra.exe'
    $blocked = Join-Path $redist 'z-block.bin'
    $state = Join-Path $game '.penumbravr/deploy-state.json'
    if (-not $game.StartsWith($fixture + [System.IO.Path]::DirectorySeparatorChar,
            [StringComparison]::OrdinalIgnoreCase)) { throw 'Fixture game escapes temporary root.' }
    Write-FixtureFile $exe 'retail executable'
    Write-FixtureFile (Join-Path $redist 'config/English.lang') 'English'
    Write-FixtureFile $blocked 'retail blocked file'
    Write-FixtureFile (Join-Path $package 'Penumbra_vr.exe') 'candidate executable'
    Write-FixtureFile (Join-Path $package 'openvr_api.dll') 'candidate OpenVR'
    Write-FixtureFile (Join-Path $package 'vr/actions.json') '{}'
    Write-FixtureFile (Join-Path $package 'z-block.bin') 'candidate blocked file'
    $originalExe = Get-Sha256 $exe
    $originalBlocked = Get-Sha256 $blocked
    $childScript = Join-Path $fixture 'crash-child.ps1'
    @'
[CmdletBinding()]
param([string]$InstallerPath, [string]$PackagePath, [string]$GamePath,
      [string]$BlockedPath, [string]$RestoreValue)
$ErrorActionPreference = 'Stop'
$global:CrashBlockedPath = $BlockedPath
function Copy-Item {
    [CmdletBinding()]
    param([string]$LiteralPath, [string]$Destination, [switch]$Recurse, [switch]$Force)
    if ($Destination -ieq $global:CrashBlockedPath) {
        Stop-Process -Id $PID -Force
    }
    Microsoft.PowerShell.Management\Copy-Item @PSBoundParameters
}
if ($RestoreValue -eq 'true') {
    & $InstallerPath -InstallRoot $GamePath -Restore 6>$null | Out-Null
} else {
    & $InstallerPath -PackageRoot $PackagePath -InstallRoot $GamePath -SteamLauncher:$true 6>$null | Out-Null
}
'@ | Set-Content -LiteralPath $childScript -Encoding UTF8

    Invoke-CrashedDeployment 'install' $childScript $installer $package $game $blocked $false
    if ((Get-Sha256 $exe) -eq $originalExe) { throw 'Fresh install did not change an earlier file before crash.' }
    $beforePendingRefusal = Get-Sha256 $exe
    $rejectedPending = $false
    try { & $installer -PackageRoot $package -InstallRoot $game 6>$null | Out-Null }
    catch { $rejectedPending = $_.Exception.Message -like '*needs -Recover*' }
    if (-not $rejectedPending -or (Get-Sha256 $exe) -ne $beforePendingRefusal) {
        throw 'Overture started another deployment while a recovery journal was pending.'
    }
    $journalRecord = Get-Content -LiteralPath (Join-Path $game '.penumbravr-journal/journal.json') -Raw | ConvertFrom-Json
    $exeCopy = @($journalRecord.Entries | Where-Object { $_.Target -ieq $exe })[0].Copy
    [System.IO.File]::AppendAllText($exeCopy, 'altered')
    $beforeRejectedRecovery = Get-Sha256 $exe
    $rejectedCorruptCopy = $false
    try { & $installer -InstallRoot $game -Recover 6>$null | Out-Null }
    catch { $rejectedCorruptCopy = $_.Exception.Message -like '*failed hash verification*' }
    if (-not $rejectedCorruptCopy -or (Get-Sha256 $exe) -ne $beforeRejectedRecovery) {
        throw 'Overture recovery used a corrupt journal copy or wrote before validation.'
    }
    Write-FixtureFile $exeCopy 'retail executable'
    Remove-Item -LiteralPath $exe -Force
    & $installer -InstallRoot $game -Recover 6>$null | Out-Null
    Assert-Hash $exe $originalExe
    Assert-Hash $blocked $originalBlocked
    if (Test-Path -LiteralPath (Join-Path $game '.penumbravr')) { throw 'Fresh install recovery left deployment state.' }

    & $installer -PackageRoot $package -InstallRoot $game -SteamLauncher:$true 6>$null | Out-Null
    $installedExe = Get-Sha256 $exe
    $installedBlocked = Get-Sha256 $blocked
    $installedState = Get-Sha256 $state
    Write-FixtureFile (Join-Path $package 'Penumbra_vr.exe') 'upgraded executable'
    Write-FixtureFile (Join-Path $package 'z-block.bin') 'upgraded blocked file'
    Invoke-CrashedDeployment 'upgrade' $childScript $installer $package $game $blocked $false
    if ((Get-Sha256 $exe) -eq $installedExe) { throw 'Upgrade did not change an earlier file before crash.' }
    & $installer -InstallRoot $game -Recover 6>$null | Out-Null
    Assert-Hash $exe $installedExe
    Assert-Hash $blocked $installedBlocked
    Assert-Hash $state $installedState

    & $installer -PackageRoot $package -InstallRoot $game -SteamLauncher:$true 6>$null | Out-Null
    $upgradedExe = Get-Sha256 $exe
    $upgradedBlocked = Get-Sha256 $blocked
    $upgradedState = Get-Sha256 $state
    Invoke-CrashedDeployment 'restore' $childScript $installer $package $game $blocked $true
    if ((Get-Sha256 $exe) -ne $originalExe) { throw 'Restore did not change an earlier file before crash.' }
    & $installer -InstallRoot $game -Recover 6>$null | Out-Null
    Assert-Hash $exe $upgradedExe
    Assert-Hash $blocked $upgradedBlocked
    Assert-Hash $state $upgradedState

    & $installer -InstallRoot $game -Restore 6>$null | Out-Null
    Assert-Hash $exe $originalExe
    Assert-Hash $blocked $originalBlocked
    if ((Test-Path -LiteralPath $state) -or (Test-Path -LiteralPath (Join-Path $game '.penumbravr-journal'))) {
        throw 'Final restore left deployment state or recovery journal.'
    }
    Write-Host 'Overture recovered interrupted install, upgrade and restore from durable journals.'
}
finally {
    if (Test-Path -LiteralPath $fixture -PathType Container) {
        $resolved = [System.IO.Path]::GetFullPath($fixture)
        if (-not $resolved.StartsWith($tempBase, [StringComparison]::OrdinalIgnoreCase)) {
            throw "Unsafe crash fixture cleanup path: $resolved"
        }
        Remove-Item -LiteralPath $resolved -Recurse -Force
    }
}
