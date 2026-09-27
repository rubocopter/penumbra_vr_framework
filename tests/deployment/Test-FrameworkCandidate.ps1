[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$OvertureExe,
    [Parameter(Mandatory = $true)][string]$BlackPlagueExe,
    [Parameter(Mandatory = $true)][string]$RequiemExe,
    [Parameter(Mandatory = $true)][string]$RetailAlut
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$packager = Join-Path $repoRoot 'tools/Package-FrameworkCandidate.ps1'
$temporaryRoot = [System.IO.Path]::GetFullPath(
    (Join-Path ([System.IO.Path]::GetTempPath()) ('PenumbraVrFrameworkTest-' + [guid]::NewGuid().ToString('N'))))
$tempBase = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath())
if (-not $temporaryRoot.StartsWith($tempBase, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Unsafe Framework fixture path.'
}

try {
    New-Item -ItemType Directory -Path $temporaryRoot | Out-Null
    $archive = Join-Path $temporaryRoot 'Framework.zip'
    $again = Join-Path $temporaryRoot 'FrameworkAgain.zip'
    & $packager -OutputPath $archive
    & $packager -OutputPath $again
    if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne
        (Get-FileHash -LiteralPath $again -Algorithm SHA256).Hash) {
        throw 'Identical Framework inputs produced different ZIP archives.'
    }
    $unpacked = Join-Path $temporaryRoot 'unpacked'
    Expand-Archive -LiteralPath $archive -DestinationPath $unpacked
    $installer = Join-Path $unpacked 'tools/Install-PenumbraFrameworkCandidate.ps1'
    $steam = Join-Path $temporaryRoot 'Steam'
    $common = Join-Path $steam 'steamapps/common'
    $overtureRedist = Join-Path $common 'Penumbra Overture/redist'
    $blackPlagueRedist = Join-Path $common 'Penumbra Black Plague/redist'
    New-Item -ItemType Directory -Path (Join-Path $overtureRedist 'config'), $blackPlagueRedist -Force | Out-Null
    $overtureGame = Join-Path $overtureRedist 'Penumbra.exe'
    $blackPlagueGame = Join-Path $blackPlagueRedist 'Penumbra.exe'
    $requiemGame = Join-Path $blackPlagueRedist 'Requiem.exe'
    $alut = Join-Path $blackPlagueRedist 'alut.dll'
    Copy-Item -LiteralPath $OvertureExe -Destination $overtureGame
    Copy-Item -LiteralPath $BlackPlagueExe -Destination $blackPlagueGame
    Copy-Item -LiteralPath $RequiemExe -Destination $requiemGame
    Copy-Item -LiteralPath $RetailAlut -Destination $alut
    Copy-Item -LiteralPath (Join-Path $repoRoot 'products/overture/data/config/English.lang') -Destination (Join-Path $overtureRedist 'config/English.lang')
    $overtureOriginal = (Get-FileHash -LiteralPath $overtureGame -Algorithm SHA256).Hash
    $alutOriginal = (Get-FileHash -LiteralPath $alut -Algorithm SHA256).Hash

    & $installer -SteamRoot $steam -List 6>$null | Out-Null
    $rejected = $false
    try { & $installer -SteamRoot $steam -Game Requiem -GamePath $requiemGame 6>$null | Out-Null }
    catch { $rejected = $_.Exception.Message -like '*no Framework gameplay VR backend*' }
    if (-not $rejected) { throw 'Requiem was not rejected as unavailable.' }
    $rejected = $false
    try { & $installer -SteamRoot $steam -Game Overture -GamePath $overtureGame -LargeAddressAware 6>$null | Out-Null }
    catch { $rejected = $_.Exception.Message -like '*LAA transform option applies only*' }
    if (-not $rejected -or
        (Get-FileHash -LiteralPath $overtureGame -Algorithm SHA256).Hash -ne $overtureOriginal) {
        throw 'Overture incorrectly accepted the binary-game LAA option.'
    }

    $noSteam = Join-Path $temporaryRoot 'NoSteam'
    & $installer -SteamRoot $noSteam -ManualPaths @($blackPlagueRedist) -Game BlackPlague -LargeAddressAware 6>$null | Out-Null
    if (-not (Test-Path -LiteralPath (Join-Path $blackPlagueRedist 'PenumbraVR.BlackPlague.install.json'))) {
        throw 'Framework candidate did not install Black Plague.'
    }
    if ((Get-FileHash -LiteralPath $blackPlagueGame -Algorithm SHA256).Hash -ne
        'DB086CC7A4C7B10864DE0FEBBE2D71A3E4EFF1EC8D067811A6A59EDC1C617196') {
        throw 'Framework candidate did not apply the exact Black Plague LAA variant.'
    }
    [System.IO.File]::WriteAllText($blackPlagueGame, 'damaged managed LAA executable')
    & $installer -Game BlackPlague -GamePath $blackPlagueRedist -Repair 6>$null | Out-Null
    if ((Get-FileHash -LiteralPath $blackPlagueGame -Algorithm SHA256).Hash -ne
        'DB086CC7A4C7B10864DE0FEBBE2D71A3E4EFF1EC8D067811A6A59EDC1C617196') {
        throw 'Framework candidate did not repair the managed Black Plague LAA executable.'
    }
    Remove-Item -LiteralPath $blackPlagueGame
    & $installer -Game BlackPlague -GamePath $blackPlagueRedist -Repair 6>$null | Out-Null
    if ((Get-FileHash -LiteralPath $blackPlagueGame -Algorithm SHA256).Hash -ne
        'DB086CC7A4C7B10864DE0FEBBE2D71A3E4EFF1EC8D067811A6A59EDC1C617196') {
        throw 'Framework candidate did not reconstruct a missing managed Black Plague LAA executable.'
    }
    $bpProbe = Join-Path $blackPlagueRedist 'PenumbraVR.BlackPlague.Probe.dll'
    $bpProbeHash = (Get-FileHash -LiteralPath $bpProbe -Algorithm SHA256).Hash
    [System.IO.File]::WriteAllText($bpProbe, 'damaged managed probe')
    & $installer -Game BlackPlague -GamePath $blackPlagueRedist -Repair 6>$null | Out-Null
    if ((Get-FileHash -LiteralPath $bpProbe -Algorithm SHA256).Hash -ne $bpProbeHash) {
        throw 'Framework candidate did not repair Black Plague.'
    }
    & $installer -SteamRoot $noSteam -ManualPaths @($blackPlagueRedist) -Game BlackPlague -Restore 6>$null | Out-Null
    if ((Get-FileHash -LiteralPath $alut -Algorithm SHA256).Hash -ne $alutOriginal -or
        (Get-FileHash -LiteralPath $blackPlagueGame -Algorithm SHA256).Hash -ne
            'FD316F7586737A63EBA989ECE2271280FE6A98582A1319FE2151385A3DF97BFF') {
        throw 'Framework candidate did not restore Black Plague.'
    }
    $bpRecoveryDispatched = $false
    try { & $installer -SteamRoot $noSteam -Game BlackPlague -GamePath $blackPlagueGame -Recover 6>$null | Out-Null }
    catch { $bpRecoveryDispatched = $_.Exception.Message -like '*recovery journal is missing or incomplete*' }
    if (-not $bpRecoveryDispatched) { throw 'Framework selector did not dispatch Black Plague recovery.' }

    & $installer -SteamRoot $steam -Game Overture -GamePath $overtureGame 6>$null | Out-Null
    $overtureState = Join-Path (Split-Path -Parent $overtureRedist) '.penumbravr/deploy-state.json'
    if (-not (Test-Path -LiteralPath $overtureState -PathType Leaf)) {
        throw 'Framework candidate did not install Overture.'
    }
    $installedOvertureHash = (Get-FileHash -LiteralPath $overtureGame -Algorithm SHA256).Hash
    $englishLang = Join-Path $overtureRedist 'config/English.lang'
    $installedEnglishHash = (Get-FileHash -LiteralPath $englishLang -Algorithm SHA256).Hash
    Remove-Item -LiteralPath $overtureGame
    Remove-Item -LiteralPath $englishLang
    & $installer -Game Overture -GamePath $overtureGame -Repair 6>$null | Out-Null
    if ((Get-FileHash -LiteralPath $overtureGame -Algorithm SHA256).Hash -ne $installedOvertureHash -or
        (Get-FileHash -LiteralPath $englishLang -Algorithm SHA256).Hash -ne $installedEnglishHash) {
        throw 'Framework candidate did not repair missing Overture executable and language files.'
    }
    & $installer -SteamRoot $steam -Game Overture -GamePath $overtureGame -Restore 6>$null | Out-Null
    if ((Get-FileHash -LiteralPath $overtureGame -Algorithm SHA256).Hash -ne $overtureOriginal -or
        (Test-Path -LiteralPath $overtureState)) {
        throw 'Framework candidate did not restore Overture.'
    }
    $recoveryDispatched = $false
    try { & $installer -Game Overture -GamePath $overtureGame -Recover 6>$null | Out-Null }
    catch { $recoveryDispatched = $_.Exception.Message -like '*recovery journal is missing or incomplete*' }
    if (-not $recoveryDispatched) { throw 'Framework selector did not dispatch Overture recovery.' }

    [System.IO.File]::AppendAllText((Join-Path $unpacked 'PACKAGE-README.md'), 'altered')
    $rejectedTamper = $false
    try { & $installer -SteamRoot $noSteam -List 6>$null | Out-Null }
    catch { $rejectedTamper = $_.Exception.Message -like '*Candidate package hash mismatch*' }
    if (-not $rejectedTamper) {
        throw 'Framework candidate did not reject an altered package before discovery.'
    }
    Write-Host 'One Framework ZIP installs, repairs and restores Overture/Black Plague; Requiem stays fail-closed.'
} finally {
    if (Test-Path -LiteralPath $temporaryRoot -PathType Container) {
        Remove-Item -LiteralPath $temporaryRoot -Recurse -Force
    }
}
