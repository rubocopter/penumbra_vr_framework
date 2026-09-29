[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$installer = Join-Path (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)) 'products/overture/scripts/deploy.ps1'
$tempBase = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath()).TrimEnd('\') + '\'
$fixture = [System.IO.Path]::GetFullPath((Join-Path $tempBase ('PenumbraVrOvertureRollback-' + [guid]::NewGuid().ToString('N'))))
if (-not $fixture.StartsWith($tempBase, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Unsafe Overture rollback fixture path.'
}

function Write-FixtureFile([string]$path, [string]$value) {
    New-Item -ItemType Directory -Path (Split-Path -Parent $path) -Force | Out-Null
    [System.IO.File]::WriteAllText($path, $value)
}

function Assert-Hash([string]$path, [string]$expected) {
    $actual = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
    if ($actual -ne $expected) { throw "Unexpected file hash: $path" }
}

# The installer inherits this test-only cmdlet shim. Raise one ordinary copy
# failure after earlier files have already changed, then let rollback copy.
function Copy-Item {
    [CmdletBinding()]
    param([string]$LiteralPath, [string]$Destination, [switch]$Recurse, [switch]$Force)
    if ($global:OvertureFailCopyPath -and $Destination -ieq $global:OvertureFailCopyPath) {
        $global:OvertureFailCopyPath = $null
        Assert-Hash $global:OvertureObservedExe $global:OvertureExpectedBeforeFailure
        throw 'Injected late Copy-Item failure'
    }
    Microsoft.PowerShell.Management\Copy-Item @PSBoundParameters
}

function Invoke-ExpectedRollback([scriptblock]$operation, [string]$phase) {
    $failed = $false
    $observed = 'operation succeeded'
    try { & $operation 6>$null | Out-Null }
    catch {
        $observed = $_.Exception.Message
        $failed = $observed -like '*managed files were restored*'
    }
    if (-not $failed) { throw "Overture $phase did not roll back as expected: $observed" }
}

try {
    $package = Join-Path $fixture 'package'
    $game = Join-Path $fixture 'game'
    $redist = Join-Path $game 'redist'
    $exe = Join-Path $redist 'Penumbra.exe'
    $blocked = Join-Path $redist 'z-block.bin'
    $state = Join-Path $game '.penumbravr/deploy-state.json'
    Write-FixtureFile $exe 'original executable'
    Write-FixtureFile (Join-Path $redist 'config/English.lang') 'English'
    New-Item -ItemType Directory -Path (Join-Path $redist 'vr') -Force | Out-Null
    Write-FixtureFile $blocked 'original blocked file'
    Write-FixtureFile (Join-Path $package 'Penumbra_vr.exe') 'candidate executable'
    Write-FixtureFile (Join-Path $package 'release.json') '{"schemaVersion":1,"version":"1.0.0","channel":"release-candidate"}'
    Write-FixtureFile (Join-Path $package 'openvr_api.dll') 'candidate OpenVR'
    Write-FixtureFile (Join-Path $package 'vr/actions.json') '{}'
    Write-FixtureFile (Join-Path $package 'z-block.bin') 'candidate blocked file'
    $originalExe = (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash
    $originalBlocked = (Get-FileHash -LiteralPath $blocked -Algorithm SHA256).Hash
    if (-not $game.StartsWith($fixture + [System.IO.Path]::DirectorySeparatorChar,
            [StringComparison]::OrdinalIgnoreCase)) { throw 'Fixture game path escapes the temporary root.' }

    $rejectedBuildChange = $false
    try {
        & $installer -PackageRoot $package -InstallRoot $game -ExpectedExecutableSha256 ('0' * 64) 6>$null | Out-Null
    } catch { $rejectedBuildChange = $_.Exception.Message -like '*changed after build selection*' }
    if (-not $rejectedBuildChange -or (Test-Path -LiteralPath (Join-Path $game '.penumbravr'))) {
        throw 'Overture changed files after an executable fingerprint mismatch.'
    }
    Assert-Hash $exe $originalExe

    $rejectedFreshRepair = $false
    try { & $installer -PackageRoot $package -InstallRoot $game -Repair 6>$null | Out-Null }
    catch { $rejectedFreshRepair = $_.Exception.Message -like '*requires an existing managed deployment state*' }
    if (-not $rejectedFreshRepair -or (Test-Path -LiteralPath $state)) {
        throw 'Overture repair accepted an unowned installation.'
    }

    $global:OvertureObservedExe = $exe
    $global:OvertureExpectedBeforeFailure = (Get-FileHash -LiteralPath (Join-Path $package 'Penumbra_vr.exe') -Algorithm SHA256).Hash
    $global:OvertureFailCopyPath = $blocked
    Invoke-ExpectedRollback { & $installer -PackageRoot $package -InstallRoot $game -SteamLauncher:$true } 'fresh install'
    Assert-Hash $exe $originalExe
    Assert-Hash $blocked $originalBlocked
    if ((Test-Path -LiteralPath (Join-Path $game '.penumbravr')) -or
        -not (Test-Path -LiteralPath (Join-Path $redist 'vr') -PathType Container) -or
        (Test-Path -LiteralPath (Join-Path $redist 'vr/actions.json')) -or
        (Test-Path -LiteralPath (Join-Path $redist 'Penumbra_vr.exe'))) {
        throw 'Fresh-install rollback left managed files behind.'
    }

    & $installer -PackageRoot $package -InstallRoot $game -SteamLauncher:$true 6>$null | Out-Null
    $installedExe = (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash
    $installedBlocked = (Get-FileHash -LiteralPath $blocked -Algorithm SHA256).Hash
    $installedState = (Get-FileHash -LiteralPath $state -Algorithm SHA256).Hash
    Write-FixtureFile (Join-Path $package 'Penumbra_vr.exe') 'upgraded executable'
    Write-FixtureFile (Join-Path $package 'z-block.bin') 'upgraded blocked file'

    $global:OvertureExpectedBeforeFailure = (Get-FileHash -LiteralPath (Join-Path $package 'Penumbra_vr.exe') -Algorithm SHA256).Hash
    $global:OvertureFailCopyPath = $blocked
    Invoke-ExpectedRollback { & $installer -PackageRoot $package -InstallRoot $game -SteamLauncher:$true } 'upgrade'
    Assert-Hash $exe $installedExe
    Assert-Hash $blocked $installedBlocked
    Assert-Hash $state $installedState

    & $installer -PackageRoot $package -InstallRoot $game -SteamLauncher:$true 6>$null | Out-Null
    $upgradedExe = (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash
    $upgradedBlocked = (Get-FileHash -LiteralPath $blocked -Algorithm SHA256).Hash
    $upgradedState = (Get-FileHash -LiteralPath $state -Algorithm SHA256).Hash
    $action = Join-Path $redist 'vr/actions.json'
    $foreign = Join-Path $redist 'foreign-user-file.txt'
    Write-FixtureFile $blocked 'damaged managed file'
    Remove-Item -LiteralPath $action
    Write-FixtureFile $foreign 'unrelated user data'
    $rejectedUpgrade = $false
    try { & $installer -PackageRoot $package -InstallRoot $game -SteamLauncher:$true 6>$null | Out-Null }
    catch { $rejectedUpgrade = $true }
    if (-not $rejectedUpgrade) { throw 'Ordinary Overture upgrade accepted damaged managed files.' }
    $damagedBlockedHash = (Get-FileHash -LiteralPath $blocked -Algorithm SHA256).Hash
    $global:OvertureExpectedBeforeFailure = $upgradedExe
    $global:OvertureFailCopyPath = $blocked
    Invoke-ExpectedRollback { & $installer -PackageRoot $package -InstallRoot $game -SteamLauncher:$true -Repair } 'repair'
    Assert-Hash $blocked $damagedBlockedHash
    Assert-Hash $state $upgradedState
    if (Test-Path -LiteralPath $action) { throw 'Failed Overture repair recreated a missing managed file.' }
    & $installer -PackageRoot $package -InstallRoot $game -SteamLauncher:$true -Repair 6>$null | Out-Null
    Assert-Hash $blocked $upgradedBlocked
    Assert-Hash $action (Get-FileHash -LiteralPath (Join-Path $package 'vr/actions.json') -Algorithm SHA256).Hash
    if ((Get-Content -LiteralPath $foreign -Raw) -ne 'unrelated user data') {
        throw 'Overture repair changed an unrelated file.'
    }

    $originalBackup = Join-Path $game '.penumbravr/backup/Penumbra.exe'
    Write-FixtureFile $originalBackup 'corrupted original backup'
    Write-FixtureFile $blocked 'second damaged managed file'
    $damagedBlocked = (Get-FileHash -LiteralPath $blocked -Algorithm SHA256).Hash
    $rejectedRepair = $false
    try { & $installer -PackageRoot $package -InstallRoot $game -SteamLauncher:$true -Repair 6>$null | Out-Null }
    catch { $rejectedRepair = $true }
    if (-not $rejectedRepair) { throw 'Overture repair accepted a corrupt original backup.' }
    Assert-Hash $blocked $damagedBlocked
    Write-FixtureFile $originalBackup 'original executable'
    & $installer -PackageRoot $package -InstallRoot $game -SteamLauncher:$true -Repair 6>$null | Out-Null
    Assert-Hash $blocked $upgradedBlocked
    $upgradedState = (Get-FileHash -LiteralPath $state -Algorithm SHA256).Hash
    $global:OvertureExpectedBeforeFailure = $originalExe
    $global:OvertureFailCopyPath = $blocked
    Invoke-ExpectedRollback { & $installer -InstallRoot $game -Restore } 'restore'
    Assert-Hash $exe $upgradedExe
    Assert-Hash $blocked $upgradedBlocked
    Assert-Hash $state $upgradedState

    & $installer -InstallRoot $game -Restore 6>$null | Out-Null
    Assert-Hash $exe $originalExe
    Assert-Hash $blocked $originalBlocked
    if (Test-Path -LiteralPath $state) { throw 'Successful restore left deployment state behind.' }
    Write-Host 'Overture install, upgrade, repair and restore roll back after late copy failures.'
}
finally {
    if (Test-Path -LiteralPath $fixture -PathType Container) {
        $resolved = [System.IO.Path]::GetFullPath($fixture)
        if (-not $resolved.StartsWith($tempBase, [StringComparison]::OrdinalIgnoreCase)) {
            throw "Unsafe Overture fixture cleanup path: $resolved"
        }
        Remove-Item -LiteralPath $resolved -Recurse -Force
    }
}
