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

    $rejectedFreshRepair = $false
    try { & $installer -GamePath $gameExe -BuildRoot $BuildRoot -CommunityTranslations -Repair | Out-Null }
    catch { $rejectedFreshRepair = $_.Exception.Message -like '*requires an existing managed installation record*' }
    if (-not $rejectedFreshRepair) { throw 'Black Plague repair accepted an unowned installation.' }

    & $installer -GamePath $gameExe -BuildRoot $BuildRoot -CommunityTranslations | Out-Null
    $probe = Join-Path $fixture 'PenumbraVR.BlackPlague.Probe.dll'
    $localization = Join-Path $fixture 'config/Espanol.lang'
    if (-not (Test-Path -LiteralPath $probe -PathType Leaf) -or
        -not (Test-Path -LiteralPath $localization -PathType Leaf)) {
        throw 'Fixture installation did not create its expected payloads.'
    }
    & $installer -GamePath $gameExe -BuildRoot $BuildRoot -CommunityTranslations | Out-Null
    Write-Host 'Black Plague clean upgrade completed.'

    $audioConfig = Join-Path $fixture 'alsoft.ini'
    $audioConfigBytes = [System.IO.File]::ReadAllBytes($audioConfig)
    $audioConfigHash = (Get-FileHash -LiteralPath $audioConfig -Algorithm SHA256).Hash
    Remove-Item -LiteralPath $audioConfig
    $rejectedAudioUpgrade = $false
    try { & $installer -GamePath $gameExe -BuildRoot $BuildRoot -CommunityTranslations | Out-Null }
    catch { $rejectedAudioUpgrade = $true }
    if (-not $rejectedAudioUpgrade) { throw 'Ordinary upgrade accepted a missing managed OpenAL configuration.' }
    & $installer -GamePath $gameExe -BuildRoot $BuildRoot -CommunityTranslations -Repair | Out-Null
    if ((Get-FileHash -LiteralPath $audioConfig -Algorithm SHA256).Hash -ne $audioConfigHash) {
        throw 'Black Plague repair did not regenerate the recorded OpenAL configuration.'
    }
    [System.IO.File]::WriteAllText($audioConfig, 'personal audio settings')
    $rejectedAudioRepair = $false
    try { & $installer -GamePath $gameExe -BuildRoot $BuildRoot -CommunityTranslations -Repair | Out-Null }
    catch { $rejectedAudioRepair = $true }
    if (-not $rejectedAudioRepair -or (Get-Content -LiteralPath $audioConfig -Raw) -ne 'personal audio settings') {
        throw 'Black Plague repair accepted or changed personal audio settings.'
    }
    [System.IO.File]::WriteAllBytes($audioConfig, $audioConfigBytes)

    $probeHash = (Get-FileHash -LiteralPath $probe -Algorithm SHA256).Hash
    $openVrBinding = Join-Path $fixture 'vr/actions.json'
    $bindingHash = (Get-FileHash -LiteralPath $openVrBinding -Algorithm SHA256).Hash
    [System.IO.File]::WriteAllText($probe, 'damaged managed probe')
    Remove-Item -LiteralPath $openVrBinding
    $rejectedUpgrade = $false
    try { & $installer -GamePath $gameExe -BuildRoot $BuildRoot -CommunityTranslations | Out-Null }
    catch { $rejectedUpgrade = $true }
    if (-not $rejectedUpgrade) { throw 'Ordinary Black Plague upgrade accepted damaged managed payload.' }
    & $installer -GamePath $gameExe -BuildRoot $BuildRoot -CommunityTranslations -Repair | Out-Null
    if ((Get-FileHash -LiteralPath $probe -Algorithm SHA256).Hash -ne $probeHash -or
        (Get-FileHash -LiteralPath $openVrBinding -Algorithm SHA256).Hash -ne $bindingHash) {
        throw 'Black Plague repair did not restore the managed payload.'
    }

    $alutBackup = Join-Path $fixture 'PenumbraVR_alut_original.dll'
    [System.IO.File]::WriteAllText($alutBackup, 'corrupted retail backup')
    [System.IO.File]::WriteAllText($probe, 'second damaged probe')
    $damagedProbeHash = (Get-FileHash -LiteralPath $probe -Algorithm SHA256).Hash
    $rejectedBackup = $false
    try { & $installer -GamePath $gameExe -BuildRoot $BuildRoot -CommunityTranslations -Repair | Out-Null }
    catch { $rejectedBackup = $true }
    if (-not $rejectedBackup -or
        (Get-FileHash -LiteralPath $probe -Algorithm SHA256).Hash -ne $damagedProbeHash) {
        throw 'Black Plague repair accepted a corrupt retail backup or modified the probe before rejection.'
    }
    Copy-Item -LiteralPath $RetailAlut -Destination $alutBackup -Force
    & $installer -GamePath $gameExe -BuildRoot $BuildRoot -CommunityTranslations -Repair | Out-Null

    $foreignVrFile = Join-Path $fixture 'vr/foreign-user-file.txt'
    [System.IO.File]::WriteAllText($foreignVrFile, 'unrelated data')
    $rejectedRepair = $false
    try { & $installer -GamePath $gameExe -BuildRoot $BuildRoot -CommunityTranslations -Repair | Out-Null }
    catch { $rejectedRepair = $true }
    if (-not $rejectedRepair -or -not (Test-Path -LiteralPath $foreignVrFile -PathType Leaf)) {
        throw 'Black Plague repair accepted or deleted an unrelated OpenVR file.'
    }
    Remove-Item -LiteralPath $foreignVrFile

    [System.IO.File]::WriteAllText($probe, 'third-party probe change')
    [System.IO.File]::WriteAllText($localization, 'user localization change')
    $probeBefore = (Get-FileHash -LiteralPath $probe -Algorithm SHA256).Hash
    $localizationBefore = (Get-FileHash -LiteralPath $localization -Algorithm SHA256).Hash
    $state = Join-Path $fixture 'PenumbraVR.BlackPlague.install.json'
    $stateBefore = (Get-FileHash -LiteralPath $state -Algorithm SHA256).Hash

    $rejected = $false
    try {
        & $installer -GamePath $gameExe -BuildRoot $BuildRoot -CommunityTranslations | Out-Null
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
        & powershell -NoProfile -ExecutionPolicy Bypass -File $installer -GamePath $gameExe -BuildRoot $BuildRoot -CommunityTranslations -Restore 2>&1 | Out-Null
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
        & powershell -NoProfile -ExecutionPolicy Bypass -File $installer -GamePath $gameExe -BuildRoot $BuildRoot -CommunityTranslations -Restore 2>&1 | Out-Null
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
        & powershell -NoProfile -ExecutionPolicy Bypass -File $installer -GamePath $gameExe -BuildRoot $BuildRoot -CommunityTranslations -Restore 2>&1 | Out-Null
    } catch {
        # Windows PowerShell promotes native stderr to an error under Stop.
    }
    if ($LASTEXITCODE -eq 0 -or
        (Get-FileHash -LiteralPath $alut -Algorithm SHA256).Hash -ne $alutBefore -or
        (Get-FileHash -LiteralPath $state -Algorithm SHA256).Hash -ne $stateBefore) {
        throw 'Restore changed the installation before rejecting a modified localization backup.'
    }
    [System.IO.File]::WriteAllText($localizationBackup, 'retail localization fixture')

    & powershell -NoProfile -ExecutionPolicy Bypass -File $installer -GamePath $gameExe -BuildRoot $BuildRoot -CommunityTranslations -Restore | Out-Null
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

    [System.IO.File]::WriteAllText($audioConfig, 'user-owned OpenAL settings')
    & $installer -GamePath $gameExe -BuildRoot $BuildRoot -CommunityTranslations | Out-Null
    Remove-Item -LiteralPath $audioConfig
    $userAudioStateHash = (Get-FileHash -LiteralPath $state -Algorithm SHA256).Hash
    $rejectedUserAudioRepair = $false
    try { & $installer -GamePath $gameExe -BuildRoot $BuildRoot -CommunityTranslations -Repair | Out-Null }
    catch { $rejectedUserAudioRepair = $_.Exception.Message -like '*user-owned OpenAL configuration*' }
    if (-not $rejectedUserAudioRepair -or (Test-Path -LiteralPath $audioConfig) -or
        (Get-FileHash -LiteralPath $state -Algorithm SHA256).Hash -ne $userAudioStateHash) {
        throw 'Black Plague repair replaced a missing user-owned OpenAL configuration.'
    }
    & $installer -GamePath $gameExe -BuildRoot $BuildRoot -CommunityTranslations -Restore | Out-Null

    $foreignVrRoot = Join-Path $fixture 'vr'
    New-Item -ItemType Directory -Path $foreignVrRoot | Out-Null
    $foreignFile = Join-Path $foreignVrRoot 'user-actions.json'
    [System.IO.File]::WriteAllText($foreignFile, 'unrelated actions')
    $rejected = $false
    try {
        & $installer -GamePath $gameExe -BuildRoot $BuildRoot -CommunityTranslations | Out-Null
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
