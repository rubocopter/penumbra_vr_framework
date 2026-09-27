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
    $logPath = Join-Path $temporaryRoot 'installer.jsonl'
    $logArgs = @{ LogPath = $logPath }

    & $installer -SteamRoot $steam -List 6>$null | Out-Null
    if (Test-Path -LiteralPath $logPath) { throw 'Read-only discovery created an installer log.' }
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
    & $installer -SteamRoot $noSteam -GamePath $blackPlagueRedist -Game BlackPlague -LargeAddressAware @logArgs 6>$null | Out-Null
    if (-not (Test-Path -LiteralPath (Join-Path $blackPlagueRedist 'PenumbraVR.BlackPlague.install.json'))) {
        throw 'Framework candidate did not install Black Plague.'
    }
    if ((Get-FileHash -LiteralPath $blackPlagueGame -Algorithm SHA256).Hash -ne
        'DB086CC7A4C7B10864DE0FEBBE2D71A3E4EFF1EC8D067811A6A59EDC1C617196') {
        throw 'Framework candidate did not apply the exact Black Plague LAA variant.'
    }
    [System.IO.File]::WriteAllText($blackPlagueGame, 'damaged managed LAA executable')
    & $installer -Game BlackPlague -GamePath $blackPlagueRedist -Repair @logArgs 6>$null | Out-Null
    if ((Get-FileHash -LiteralPath $blackPlagueGame -Algorithm SHA256).Hash -ne
        'DB086CC7A4C7B10864DE0FEBBE2D71A3E4EFF1EC8D067811A6A59EDC1C617196') {
        throw 'Framework candidate did not repair the managed Black Plague LAA executable.'
    }
    Remove-Item -LiteralPath $blackPlagueGame
    & $installer -Game BlackPlague -GamePath $blackPlagueRedist -Repair @logArgs 6>$null | Out-Null
    if ((Get-FileHash -LiteralPath $blackPlagueGame -Algorithm SHA256).Hash -ne
        'DB086CC7A4C7B10864DE0FEBBE2D71A3E4EFF1EC8D067811A6A59EDC1C617196') {
        throw 'Framework candidate did not reconstruct a missing managed Black Plague LAA executable.'
    }
    $bpProbe = Join-Path $blackPlagueRedist 'PenumbraVR.BlackPlague.Probe.dll'
    $bpProbeHash = (Get-FileHash -LiteralPath $bpProbe -Algorithm SHA256).Hash
    [System.IO.File]::WriteAllText($bpProbe, 'damaged managed probe')
    & $installer -Game BlackPlague -GamePath $blackPlagueRedist -Repair @logArgs 6>$null | Out-Null
    if ((Get-FileHash -LiteralPath $bpProbe -Algorithm SHA256).Hash -ne $bpProbeHash) {
        throw 'Framework candidate did not repair Black Plague.'
    }
    & $installer -SteamRoot $noSteam -ManualPaths @($blackPlagueRedist) -Game BlackPlague -Restore @logArgs 6>$null | Out-Null
    if ((Get-FileHash -LiteralPath $alut -Algorithm SHA256).Hash -ne $alutOriginal -or
        (Get-FileHash -LiteralPath $blackPlagueGame -Algorithm SHA256).Hash -ne
            'FD316F7586737A63EBA989ECE2271280FE6A98582A1319FE2151385A3DF97BFF') {
        throw 'Framework candidate did not restore Black Plague.'
    }
    $bpRecoveryDispatched = $false
    try { & $installer -SteamRoot $noSteam -Game BlackPlague -GamePath $blackPlagueGame -Recover @logArgs 6>$null | Out-Null }
    catch { $bpRecoveryDispatched = $_.Exception.Message -like '*recovery journal is missing or incomplete*' }
    if (-not $bpRecoveryDispatched) { throw 'Framework selector did not dispatch Black Plague recovery.' }

    & $installer -SteamRoot $steam -Game Overture -GamePath (Split-Path -Parent $overtureRedist) @logArgs 6>$null | Out-Null
    $overtureState = Join-Path (Split-Path -Parent $overtureRedist) '.penumbravr/deploy-state.json'
    if (-not (Test-Path -LiteralPath $overtureState -PathType Leaf)) {
        throw 'Framework candidate did not install Overture.'
    }
    $installedOvertureHash = (Get-FileHash -LiteralPath $overtureGame -Algorithm SHA256).Hash
    $englishLang = Join-Path $overtureRedist 'config/English.lang'
    $installedEnglishHash = (Get-FileHash -LiteralPath $englishLang -Algorithm SHA256).Hash
    Remove-Item -LiteralPath $overtureGame
    Remove-Item -LiteralPath $englishLang
    & $installer -Game Overture -GamePath $overtureGame -Repair @logArgs 6>$null | Out-Null
    if ((Get-FileHash -LiteralPath $overtureGame -Algorithm SHA256).Hash -ne $installedOvertureHash -or
        (Get-FileHash -LiteralPath $englishLang -Algorithm SHA256).Hash -ne $installedEnglishHash) {
        throw 'Framework candidate did not repair missing Overture executable and language files.'
    }
    & $installer -SteamRoot $steam -Game Overture -GamePath $overtureGame -Restore @logArgs 6>$null | Out-Null
    if ((Get-FileHash -LiteralPath $overtureGame -Algorithm SHA256).Hash -ne $overtureOriginal -or
        (Test-Path -LiteralPath $overtureState)) {
        throw 'Framework candidate did not restore Overture.'
    }
    $recoveryDispatched = $false
    try { & $installer -Game Overture -GamePath $overtureGame -Recover @logArgs 6>$null | Out-Null }
    catch { $recoveryDispatched = $_.Exception.Message -like '*recovery journal is missing or incomplete*' }
    if (-not $recoveryDispatched) { throw 'Framework selector did not dispatch Overture recovery.' }

    $discovered = @(& (Join-Path $unpacked 'tools/Get-PenumbraInstallations.ps1') -SteamRoot $steam)
    $blackPlagueIndex = 0
    $overtureIndex = 0
    $requiemIndex = 0
    for ($i = 0; $i -lt $discovered.Count; $i++) {
        if ($discovered[$i].Path -ieq $blackPlagueGame) { $blackPlagueIndex = $i + 1 }
        if ($discovered[$i].Path -ieq $overtureGame) { $overtureIndex = $i + 1 }
        if ($discovered[$i].Path -ieq $requiemGame) { $requiemIndex = $i + 1 }
    }
    if (-not $blackPlagueIndex -or -not $overtureIndex -or -not $requiemIndex) {
        throw 'Multi-game fixture discovery did not find all three executables.'
    }
    $both = @($blackPlagueIndex, $overtureIndex)
    $rejectedMixedRequiem = $false
    try { & $installer -SteamRoot $steam -Selections @($blackPlagueIndex, $requiemIndex) 6>$null | Out-Null }
    catch { $rejectedMixedRequiem = $_.Exception.Message -like '*not compatible*' }
    if (-not $rejectedMixedRequiem -or
        (Test-Path -LiteralPath (Join-Path $blackPlagueRedist 'PenumbraVR.BlackPlague.install.json'))) {
        throw 'Multi-game selection did not reject Requiem before changing Black Plague.'
    }
    $rejectedMixedLaa = $false
    try { & $installer -SteamRoot $steam -Selections $both -LargeAddressAware 6>$null | Out-Null }
    catch { $rejectedMixedLaa = $_.Exception.Message -like '*LAA transform option applies only*' }
    if (-not $rejectedMixedLaa) {
        throw 'Multi-game selection did not reject incompatible LAA before dispatch.'
    }
    if ((Test-Path -LiteralPath $overtureState) -or
        (Test-Path -LiteralPath (Join-Path $blackPlagueRedist 'PenumbraVR.BlackPlague.install.json'))) {
        throw 'Multi-game selection modified a game before rejecting incompatible LAA.'
    }
    $selectionArgument = '{0},{1}' -f $blackPlagueIndex, $overtureIndex
    & powershell -NoProfile -ExecutionPolicy Bypass -File $installer -SteamRoot $steam `
        -Selections $selectionArgument -LogPath $logPath 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw 'Multi-game selection failed through the packaged command-line entry point.'
    }
    if (-not (Test-Path -LiteralPath $overtureState) -or
        -not (Test-Path -LiteralPath (Join-Path $blackPlagueRedist 'PenumbraVR.BlackPlague.install.json'))) {
        throw 'Multi-game selection did not install both compatible games.'
    }
    & $installer -SteamRoot $steam -Selections $both -Restore @logArgs 6>$null | Out-Null
    if ((Test-Path -LiteralPath $overtureState) -or
        (Test-Path -LiteralPath (Join-Path $blackPlagueRedist 'PenumbraVR.BlackPlague.install.json')) -or
        (Get-FileHash -LiteralPath $overtureGame -Algorithm SHA256).Hash -ne $overtureOriginal -or
        (Get-FileHash -LiteralPath $blackPlagueGame -Algorithm SHA256).Hash -ne
            'FD316F7586737A63EBA989ECE2271280FE6A98582A1319FE2151385A3DF97BFF') {
        throw 'Multi-game selection did not restore both original installations.'
    }
    $logRecords = @(Get-Content -LiteralPath $logPath | ConvertFrom-Json)
    foreach ($gameName in @('Overture', 'Black Plague')) {
        foreach ($operation in @('install', 'restore')) {
            foreach ($result in @('started', 'completed')) {
                if (-not @($logRecords | Where-Object {
                    $_.game -eq $gameName -and $_.operation -eq $operation -and
                    $_.result -eq $result -and $_.packageManifestSha256 -match '^[A-F0-9]{64}$'
                }).Count) {
                    throw "Installer log omitted $gameName $operation $result."
                }
            }
        }
    }
    if (@($logRecords | Where-Object { $_.operation -eq 'recover' -and $_.result -eq 'failed' }).Count -ne 2) {
        throw 'Installer log omitted recovery failures.'
    }

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
