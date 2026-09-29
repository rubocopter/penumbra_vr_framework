[CmdletBinding()]
param(
    [string]$GamePath,
    [string]$SteamRoot,
    [string]$BuildRoot,
    [switch]$LargeAddressAware,
    [switch]$Restore,
    [switch]$Recover,
    [switch]$Repair,
    [ValidateSet('Auto','Enable','Disable')][string]$RequiemMode='Auto',
    [switch]$CommunityTranslations,
    [switch]$Preflight,
    [switch]$RecommendedSettings,
    [ValidateSet('DefaultFiles','DefaultAndUserFiles')][string]$SettingsScope='DefaultAndUserFiles'
)

$ErrorActionPreference = 'Stop'
if ($Repair -and ($Restore -or $Recover -or $LargeAddressAware)) {
    throw 'Black Plague repair cannot be combined with restore, recovery or a new LAA transform.'
}
if (-not $BuildRoot) {
    $BuildRoot = Join-Path (Split-Path -Parent $PSScriptRoot) 'build'
}

$ExpectedGameHashes = @(
    'FD316F7586737A63EBA989ECE2271280FE6A98582A1319FE2151385A3DF97BFF',
    'DB086CC7A4C7B10864DE0FEBBE2D71A3E4EFF1EC8D067811A6A59EDC1C617196'
)
$ExpectedRequiemHashes = @(
    'B64232D751CEE376E1384CFE5A4A81DBD7DEDDF03983CC11D0D0A34D5825EEA2',
    '577D1D7780872CD6C5B99B45759CDC48FEE486A1CCBF319E8F6CF0EAED54E955'
)
$ExpectedOriginalAlutHash = 'D81DEA8E88E35C319F7F2D8AAEB14C63A4986131492D3DF860D1F2C18B844590'
$GeneratedAudioConfigText = "[general]`nhrtf = auto`n"
$GeneratedAudioConfigHash = 'DEE6201AFD49898B403A322A595841DA9325FDBDE4A88461822172C9C9151553'

$RepoRoot = Split-Path -Parent $PSScriptRoot
if ($Recover -and -not $GamePath) {
    throw 'Pass -GamePath explicitly when recovering an interrupted Black Plague deployment.'
}
if (-not $GamePath) {
    $discovery = Join-Path $PSScriptRoot 'Get-PenumbraInstallations.ps1'
    if (-not (Test-Path -LiteralPath $discovery -PathType Leaf)) {
        throw "Installation discovery script not found: $discovery"
    }
    $discoveryArgs = @{}
    if ($SteamRoot) { $discoveryArgs.SteamRoot = $SteamRoot }
    $matches = @(& $discovery @discoveryArgs | Where-Object { $_.Installable -and $_.Game -eq 'Black Plague' })
    if ($matches.Count -ne 1) {
        $paths = if ($matches.Count) { ($matches.Path -join ', ') } else { 'none' }
        throw "Expected exactly one supported Black Plague installation; found $($matches.Count): $paths. Pass -GamePath explicitly."
    }
    $GamePath = $matches[0].Path
}
$DeploymentManifestPath = Join-Path $RepoRoot 'assets\deployment\manifest.json'
if (-not (Test-Path -LiteralPath $DeploymentManifestPath -PathType Leaf)) {
    throw "Deployment payload manifest not found: $DeploymentManifestPath"
}
$DeploymentManifest = Get-Content -LiteralPath $DeploymentManifestPath -Raw | ConvertFrom-Json
$BlackPlagueDeployment = @($DeploymentManifest.products | Where-Object { $_.game -eq 'black_plague' })
if ($DeploymentManifest.schemaVersion -ne 1 -or $BlackPlagueDeployment.Count -ne 1) {
    throw 'Deployment payload manifest does not contain one supported Black Plague product definition.'
}

function Get-DeploymentPayload([string]$Id) {
    $payload = @($BlackPlagueDeployment[0].payloads | Where-Object { $_.id -eq $Id })
    if ($payload.Count -ne 1) {
        throw "Deployment payload '$Id' is missing or duplicated."
    }
    return $payload[0]
}

function Get-DeploymentDestination([string]$Id, [string]$Root) {
    $payload = Get-DeploymentPayload $Id
    return Join-Path $Root ([string]$payload.destination -replace '/', '\')
}

$GamePath = [System.IO.Path]::GetFullPath($GamePath)
$GameRoot = Split-Path -Parent $GamePath
$AlutPath = Get-DeploymentDestination 'bootstrap_proxy' $GameRoot
$OriginalAlutPath = Join-Path $GameRoot 'PenumbraVR_alut_original.dll'
$OriginalGamePath = Join-Path $GameRoot 'PenumbraVR_Penumbra_original.exe'
$ProbePath = Get-DeploymentDestination 'probe' $GameRoot
$OpenVrPath = Get-DeploymentDestination 'openvr_loader' $GameRoot
$VrAssetsPath = Get-DeploymentDestination 'openvr_actions' $GameRoot
$HandTexturePath = Get-DeploymentDestination 'hand_texture' $GameRoot
$HandAssetsPath = Split-Path -Parent $HandTexturePath
$LocalizationPath = Get-DeploymentDestination 'spanish_localization' $GameRoot
$LocalizationBackupPath = Join-Path $GameRoot 'PenumbraVR_Espanol_original.lang'
$AudioConfigPath = Get-DeploymentDestination 'openal_hrtf_config' $GameRoot
$InstallStatePath = Join-Path $GameRoot 'PenumbraVR.BlackPlague.install.json'
$RequiemGamePath = Join-Path $GameRoot 'Requiem.exe'
$RequiemProbePath = Get-DeploymentDestination 'requiem_probe' $GameRoot
$RequiemLocalizationPath = Get-DeploymentDestination 'requiem_spanish_localization' $GameRoot
$RequiemLocalizationBackupPath = Join-Path $GameRoot 'PenumbraVR_Espanol_exp_original.lang'
$OpenAlPath = Join-Path $GameRoot 'OpenAL32.dll'
$OriginalOpenAlPath = Join-Path $GameRoot 'PenumbraVR_OpenAL_original.dll'
$OpenAlHash = '606CF8B4C22C7AE00813585279AC045115907BC66D1483EB26355907021EA4B0'
Import-Module (Join-Path $PSScriptRoot 'PenumbraVrConfiguration.psm1') -Force
$ConfigurationPaths=@(Get-PvrConfigurationPaths -Game black_plague -RedistRoot $GameRoot)+@(Get-PvrConfigurationPaths -Game requiem -RedistRoot $GameRoot)
$PersonalConfigurationPaths=@($ConfigurationPaths | Where-Object {-not $_.StartsWith($GameRoot.TrimEnd('\')+'\',[StringComparison]::OrdinalIgnoreCase)})
$ActiveConfigurationPaths=@()

function Get-Sha256([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return $null }
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToUpperInvariant()
}

function Get-OptionalProperty($Object, [string]$Name) {
    if ($null -eq $Object) { return $null }
    $property = $Object.PSObject.Properties[$Name]
    if ($null -eq $property) { return $null }
    return $property.Value
}

function Get-DirectorySnapshot([string]$Root) {
    if (-not (Test-Path -LiteralPath $Root -PathType Container)) {
        throw "Managed directory is missing: $Root"
    }
    $items = @(Get-ChildItem -LiteralPath $Root -Recurse -Force)
    foreach ($item in @((Get-Item -LiteralPath $Root)) + $items) {
        if ($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) {
            throw "Managed directory contains a reparse point: $($item.FullName)"
        }
    }
    $files = @($items | Where-Object { -not $_.PSIsContainer } | ForEach-Object {
        [ordered]@{
            path = $_.FullName.Substring($Root.Length).TrimStart('\').Replace('\', '/')
            sha256 = Get-Sha256 $_.FullName
        }
    } | Sort-Object { $_.path })
    $directories = @($items | Where-Object { $_.PSIsContainer } | ForEach-Object {
        $_.FullName.Substring($Root.Length).TrimStart('\').Replace('\', '/')
    } | Sort-Object)
    return [PSCustomObject]@{ files = $files; directories = $directories }
}

function Assert-ManagedDirectory([string]$Root, $Expected) {
    $actual = Get-DirectorySnapshot $Root
    if (($actual | ConvertTo-Json -Depth 5 -Compress) -cne
        ($Expected | ConvertTo-Json -Depth 5 -Compress)) {
        throw "Managed directory changed after installation: $Root"
    }
}

function Assert-RepairableManagedDirectory([string]$Root, $Expected) {
    if (-not (Test-Path -LiteralPath $Root)) { return }
    $actual = Get-DirectorySnapshot $Root
    $expectedFiles = @{}
    $expectedDirectories = @{}
    foreach ($file in @($Expected.files)) { $expectedFiles[[string]$file.path] = $true }
    foreach ($directory in @($Expected.directories)) { $expectedDirectories[[string]$directory] = $true }
    foreach ($file in @($actual.files)) {
        if (-not $expectedFiles.ContainsKey([string]$file.path)) {
            throw "Repair refused an unrecorded OpenVR file: $($file.path)"
        }
    }
    foreach ($directory in @($actual.directories)) {
        if (-not $expectedDirectories.ContainsKey([string]$directory)) {
            throw "Repair refused an unrecorded OpenVR directory: $directory"
        }
    }
}

function Get-ExpectedVrSnapshot($State) {
    $recorded = Get-OptionalProperty $State 'vrAssetsSnapshot'
    if ($recorded) { return $recorded }
    # Legacy development installs did not record the directory. Their contents
    # can only be recognized against the pinned repository payload.
    return Get-DirectorySnapshot (Join-Path $RepoRoot 'assets/openvr')
}

function Get-ExpectedOpenVrHash($State) {
    $recorded = Get-OptionalProperty $State 'openVrSha256'
    if ($recorded) { return [string]$recorded }
    return Get-Sha256 (Join-Path $RepoRoot 'products/overture/dependencies/openvr-2.15.6/bin/win32/openvr_api.dll')
}

function Assert-SupportedGame {
    if (-not (Test-Path -LiteralPath $GamePath -PathType Leaf)) {
        throw "Black Plague executable not found: $GamePath"
    }
    $hash = Get-Sha256 $GamePath
    if ($ExpectedGameHashes -notcontains $hash) {
        throw "Unsupported Black Plague executable SHA-256: $hash"
    }
}

# Requiem shares the native ALUT loader and OpenVR action directory with BP.
# When present, its exact executable must be recognized before the joint
# transaction writes either game's files.
$InstallRequiem = $RequiemMode -eq 'Enable' -or ($RequiemMode -eq 'Auto' -and (Test-Path -LiteralPath $RequiemGamePath -PathType Leaf))
if (-not $Restore -and -not $Recover -and -not $Repair -and $InstallRequiem -and
    $ExpectedRequiemHashes -notcontains (Get-Sha256 $RequiemGamePath)) {
    throw "Unsupported Requiem executable SHA-256: $(Get-Sha256 $RequiemGamePath)"
}
if(-not $Restore -and -not $Recover -and -not $Repair -and $InstallRequiem) {
    Import-Module (Join-Path $PSScriptRoot 'PenumbraVrPrerequisites.psm1') -Force
    $missing=@(Test-PvrGameContent -Game requiem -RedistRoot $GameRoot | Where-Object {$_.Status -ne 'present'})
    if($missing.Count) { throw "Requiem data is incomplete: $($missing.Name -join ', '). Verify the expansion in Steam." }
}

$ManagedTransactionPaths = @(
    $GamePath, $OriginalGamePath, $AlutPath, $OriginalAlutPath,
    $ProbePath, $OpenVrPath, $VrAssetsPath,
    $HandTexturePath, $LocalizationPath, $LocalizationBackupPath,
    $AudioConfigPath, $InstallStatePath,
    $RequiemProbePath, $RequiemLocalizationPath,
    $RequiemLocalizationBackupPath, $OpenAlPath, $OriginalOpenAlPath
) + $ConfigurationPaths

function Assert-ManagedTransactionPath([string]$path) {
    $target = [System.IO.Path]::GetFullPath($path)
    $game = [System.IO.Path]::GetFullPath($GameRoot).TrimEnd('\')
    $prefix = $game + '\'
    if (-not $target.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase) -and $target -notin $PersonalConfigurationPaths) {
        throw "Managed deployment path escapes game root: $target"
    }
    $rootItem = Get-Item -LiteralPath $game -Force -ErrorAction SilentlyContinue
    if ($rootItem -and ($rootItem.Attributes -band [System.IO.FileAttributes]::ReparsePoint)) {
        throw "Black Plague game root is a reparse point: $game"
    }
    $cursor = $target
    while ($cursor -and $cursor -ine $game) {
        $item = Get-Item -LiteralPath $cursor -Force -ErrorAction SilentlyContinue
        if ($item -and ($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint)) {
            throw "Managed deployment path is a reparse point: $cursor"
        }
        $cursor = Split-Path -Parent $cursor
    }
    return $target
}

function New-DeploymentSnapshot {
    $root = Assert-ManagedTransactionPath (Join-Path $GameRoot '.penumbravr-bp-journal')
    $staging = Assert-ManagedTransactionPath (Join-Path $GameRoot ('.penumbravr-bp-journal-staging-' + [guid]::NewGuid().ToString('N')))
    if (Test-Path -LiteralPath $root) {
        throw "An interrupted Black Plague deployment needs recovery before another write: $root"
    }
    $entries = [System.Collections.Generic.List[object]]::new()
    try {
        New-Item -ItemType Directory -Path $staging | Out-Null
        for ($i = 0; $i -lt $ManagedTransactionPaths.Count; $i++) {
            $target = Assert-ManagedTransactionPath $ManagedTransactionPaths[$i]
            $item = Get-Item -LiteralPath $target -Force -ErrorAction SilentlyContinue
            $kind = if ($target -in $ConfigurationPaths -and $target -notin $ActiveConfigurationPaths) {'untouched'} elseif ($null -eq $item) { 'absent' } elseif ($item.PSIsContainer) { 'directory' } else { 'file' }
            $copy = Join-Path $staging ("item-$i")
            $snapshotHash = $null
            $snapshotDirectory = $null
            if ($kind -notin @('absent','untouched')) {
                Copy-Item -LiteralPath $target -Destination $copy -Recurse -Force
                if ($kind -eq 'file') { $snapshotHash = Get-Sha256 $copy }
                if ($kind -eq 'file' -and (Get-Sha256 $target) -ne $snapshotHash) {
                    throw "Deployment snapshot failed hash verification: $target"
                }
                if ($kind -eq 'directory') {
                    $snapshotDirectory = Get-DirectorySnapshot $copy
                    if ((Get-DirectorySnapshot $target | ConvertTo-Json -Depth 5 -Compress) -cne
                        ($snapshotDirectory | ConvertTo-Json -Depth 5 -Compress)) {
                        throw "Deployment snapshot failed directory verification: $target"
                    }
                }
            }
            $entries.Add([PSCustomObject]@{
                Target = $target
                Copy = Join-Path $root ("item-$i")
                Kind = $kind
                SnapshotHash = $snapshotHash
                SnapshotDirectory = $snapshotDirectory
            })
        }
        [ordered]@{
            Version = 1
            GameRoot = [System.IO.Path]::GetFullPath($GameRoot)
            Entries = $entries.ToArray()
        } | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $staging 'journal.json') -Encoding UTF8
        [System.IO.Directory]::Move($staging, $root)
        return [PSCustomObject]@{ Root = $root; Entries = $entries.ToArray() }
    } catch {
        if (Test-Path -LiteralPath $staging -PathType Container) {
            Remove-Item -LiteralPath $staging -Recurse -Force
        }
        throw
    }
}

function Read-DeploymentSnapshot {
    $root = Assert-ManagedTransactionPath (Join-Path $GameRoot '.penumbravr-bp-journal')
    $manifest = Join-Path $root 'journal.json'
    if (-not (Test-Path -LiteralPath $manifest -PathType Leaf)) {
        throw "Black Plague recovery journal is missing or incomplete: $manifest"
    }
    $record = Get-Content -LiteralPath $manifest -Raw | ConvertFrom-Json
    if ([int]$record.Version -ne 1 -or
        [System.IO.Path]::GetFullPath([string]$record.GameRoot).TrimEnd('\') -ine
            [System.IO.Path]::GetFullPath($GameRoot).TrimEnd('\')) {
        throw "Black Plague recovery journal belongs to a different game or version: $manifest"
    }
    $entries = @($record.Entries)
    # Journals created by the earlier BP-only installer contain the original
    # 12 paths. Their recovery remains valid after adding the shared product.
    if ($entries.Count -notin @(12,15,17,21)) {
        throw "Black Plague recovery journal has the wrong managed set: $manifest"
    }
    for ($i = 0; $i -lt $entries.Count; $i++) {
        $entry = $entries[$i]
        $target = Assert-ManagedTransactionPath ([string]$entry.Target)
        $copy = Assert-ManagedTransactionPath ([string]$entry.Copy)
        if ($target -ine [System.IO.Path]::GetFullPath($ManagedTransactionPaths[$i]) -or
            $copy -ine (Join-Path $root ("item-$i")) -or
            $entry.Kind -notin @('absent', 'file', 'directory','untouched')) {
            throw "Invalid Black Plague recovery journal entry: $target"
        }
        if($target -in $PersonalConfigurationPaths -and $entry.Kind -eq 'directory'){throw 'A settings recovery target cannot be a directory.'}
        if($entry.Kind -eq 'untouched' -and $target -notin $ConfigurationPaths){throw 'Only inactive settings can be excluded from a recovery snapshot.'}
        if ($entry.Kind -eq 'file' -and
            ([string]$entry.SnapshotHash -notmatch '^[0-9A-Fa-f]{64}$' -or
             (Get-Sha256 $copy) -ne [string]$entry.SnapshotHash)) {
            throw "Black Plague recovery copy failed hash verification: $copy"
        }
        if ($entry.Kind -eq 'directory' -and
            (-not (Test-Path -LiteralPath $copy -PathType Container) -or
             (Get-DirectorySnapshot $copy | ConvertTo-Json -Depth 5 -Compress) -cne
                ($entry.SnapshotDirectory | ConvertTo-Json -Depth 5 -Compress))) {
            throw "Black Plague recovery directory failed verification: $copy"
        }
        if ($entry.Kind -in @('absent','untouched') -and (Test-Path -LiteralPath $copy)) {
            throw "Unexpected Black Plague recovery copy: $copy"
        }
    }
    return [PSCustomObject]@{ Root = $root; Entries = $entries }
}

function Complete-DeploymentSnapshot($Snapshot) {
    $root = Assert-ManagedTransactionPath $Snapshot.Root
    if ($root -ine (Join-Path ([System.IO.Path]::GetFullPath($GameRoot).TrimEnd('\')) '.penumbravr-bp-journal')) {
        throw "Unsafe Black Plague journal completion path: $root"
    }
    $resolved = Assert-ManagedTransactionPath (Join-Path $GameRoot ('.penumbravr-bp-journal-resolved-' + [guid]::NewGuid().ToString('N')))
    [System.IO.Directory]::Move($root, $resolved)
    try { Remove-Item -LiteralPath $resolved -Recurse -Force }
    catch { Write-Warning "Completed Black Plague journal retained for cleanup: $resolved" }
}

function Restore-DeploymentSnapshot($Snapshot) {
    foreach ($entry in @($Snapshot.Entries)) {
        if($entry.Kind -eq 'untouched'){continue}
        $entry.Target = Assert-ManagedTransactionPath ([string]$entry.Target)
        $entry.Copy = Assert-ManagedTransactionPath ([string]$entry.Copy)
        $current = Get-Item -LiteralPath $entry.Target -Force -ErrorAction SilentlyContinue
        if ($current -and ($current.Attributes -band [System.IO.FileAttributes]::ReparsePoint)) {
            throw "Rollback target became a reparse point: $($entry.Target)"
        }
        if ($entry.Kind -eq 'file' -and $current -and -not $current.PSIsContainer -and
            (Get-Sha256 $entry.Target) -eq (Get-Sha256 $entry.Copy)) { continue }
        if ($entry.Kind -eq 'directory' -and $current -and $current.PSIsContainer -and
            ((Get-DirectorySnapshot $entry.Target | ConvertTo-Json -Depth 5 -Compress) -ceq
             (Get-DirectorySnapshot $entry.Copy | ConvertTo-Json -Depth 5 -Compress))) { continue }
        if ($current) {
            if ($current.PSIsContainer) {
                if ($entry.Kind -ne 'directory' -and $entry.Target -ine $VrAssetsPath) {
                    throw "Rollback target became an unexpected directory: $($entry.Target)"
                }
                $gamePrefix = [System.IO.Path]::GetFullPath($GameRoot).TrimEnd('\') + '\'
                if (-not $entry.Target.StartsWith($gamePrefix, [StringComparison]::OrdinalIgnoreCase)) {
                    throw "Unsafe rollback directory: $($entry.Target)"
                }
                Remove-Item -LiteralPath $entry.Target -Recurse -Force
            } else {
                Remove-Item -LiteralPath $entry.Target -Force
            }
        }
        if ($entry.Kind -ne 'absent') {
            New-Item -ItemType Directory -Path (Split-Path -Parent $entry.Target) -Force | Out-Null
            Copy-Item -LiteralPath $entry.Copy -Destination $entry.Target -Recurse -Force
            if ($entry.Kind -eq 'file' -and (Get-Sha256 $entry.Target) -ne (Get-Sha256 $entry.Copy)) {
                throw "Rollback failed hash verification: $($entry.Target)"
            }
            if ($entry.Kind -eq 'directory' -and
                ((Get-DirectorySnapshot $entry.Target | ConvertTo-Json -Depth 5 -Compress) -cne
                 (Get-DirectorySnapshot $entry.Copy | ConvertTo-Json -Depth 5 -Compress))) {
                throw "Rollback failed directory verification: $($entry.Target)"
            }
        }
    }
}

function Invoke-DeploymentRollback($Snapshot, [string]$Failure) {
    try {
        $verifiedSnapshot = Read-DeploymentSnapshot
        Restore-DeploymentSnapshot $verifiedSnapshot
        Complete-DeploymentSnapshot $verifiedSnapshot
    } catch {
        throw "Deployment failed: $Failure. Rollback also failed: $($_.Exception.Message). Snapshot retained: $($Snapshot.Root)"
    }
    throw "Deployment failed and original managed files were restored: $Failure"
}

if ($Recover) {
    if (-not (Test-Path -LiteralPath $GameRoot -PathType Container)) {
        throw "Black Plague recovery game folder is missing: $GameRoot"
    }
    $recoverySnapshot = Read-DeploymentSnapshot
    Restore-DeploymentSnapshot $recoverySnapshot
    Complete-DeploymentSnapshot $recoverySnapshot
    Write-Host "Interrupted Black Plague deployment recovered at $GameRoot"
    return
}
if (Test-Path -LiteralPath (Join-Path $GameRoot '.penumbravr-bp-journal')) {
    throw "An interrupted Black Plague deployment needs -Recover -GamePath '$GamePath' before another write."
}
$PreviousState = $null
if (Test-Path -LiteralPath $InstallStatePath -PathType Leaf) {
    $PreviousState = Get-Content -LiteralPath $InstallStatePath -Raw | ConvertFrom-Json
}
if($PreviousState -and -not $Restore -and -not $Recover) {
    if(-not $PSBoundParameters.ContainsKey('CommunityTranslations')) { $CommunityTranslations=[bool](Get-OptionalProperty $PreviousState 'spanishLocalizationSha256') }
    if($Repair -and -not $PSBoundParameters.ContainsKey('RequiemMode')) { $InstallRequiem=[bool](Get-OptionalProperty $PreviousState 'requiemProbeSha256') }
}
$PreviousConfiguration=@(Get-OptionalProperty $PreviousState 'configuration' | Where-Object {$null -ne $_})
if($Repair){
    if($RequiemMode -ne 'Auto'){throw 'Repair preserves the recorded component set; use install/update to change Requiem selection.'}
    $InstallRequiem=[bool](Get-OptionalProperty $PreviousState 'requiemProbeSha256')
    if($InstallRequiem -and ($ExpectedRequiemHashes -notcontains (Get-Sha256 $RequiemGamePath) -or
        (Get-Sha256 $RequiemGamePath) -ne [string](Get-OptionalProperty $PreviousState 'requiemExeSha256'))){
        throw 'The managed Requiem executable is missing or changed. Verify expansion files in Steam before repair.'
    }
}
foreach($gameId in @('black_plague','requiem')) {
    Get-PvrConfigurationRecords -Game $gameId -RedistRoot $GameRoot -Previous @($PreviousConfiguration | Where-Object {$_.Game -eq $gameId}) | Out-Null
}
$RepairLaaExecutable = $Repair -and $PreviousState -and
    [bool](Get-OptionalProperty $PreviousState 'laaApplied') -and
    (Get-Sha256 $GamePath) -ne $ExpectedGameHashes[1] -and
    (Get-OptionalProperty $PreviousState 'gameExeSha256') -eq $ExpectedGameHashes[1] -and
    (Get-OptionalProperty $PreviousState 'originalGameExeSha256') -eq $ExpectedGameHashes[0] -and
    (Get-Sha256 $OriginalGamePath) -eq $ExpectedGameHashes[0]
if (-not $RepairLaaExecutable -and -not $Restore) { Assert-SupportedGame }

if ($Restore) {
    if (-not (Test-Path -LiteralPath $OriginalAlutPath -PathType Leaf)) {
        throw "No managed ALUT backup exists at $OriginalAlutPath"
    }
    if ((Get-Sha256 $OriginalAlutPath) -ne $ExpectedOriginalAlutHash) {
        throw 'The managed ALUT backup does not match the known retail DLL; restore aborted.'
    }

    $state = $null
    if (Test-Path -LiteralPath $InstallStatePath -PathType Leaf) {
        $state = Get-Content -LiteralPath $InstallStatePath -Raw | ConvertFrom-Json
        if ((Get-OptionalProperty $state 'schema') -ne 1 -or
            (Get-OptionalProperty $state 'gameExeSha256') -notin $ExpectedGameHashes -or
            ((Get-Sha256 $GamePath) -and (Get-OptionalProperty $state 'gameExeSha256') -ne (Get-Sha256 $GamePath))) {
            throw 'The Black Plague installation record does not match this game; restore aborted.'
        }
        if (-not (Get-OptionalProperty $state 'probeSha256') -or
            (Get-Sha256 $ProbePath) -ne [string]$state.probeSha256) {
            throw 'The managed probe changed after installation; restore aborted.'
        }
        $managedRequiemHash = Get-OptionalProperty $state 'requiemProbeSha256'
        if ($managedRequiemHash -and
            (Get-Sha256 $RequiemProbePath) -ne [string]$managedRequiemHash) {
            throw 'The managed Requiem probe changed after installation; restore aborted.'
        }
        if ((Get-Sha256 $OpenVrPath) -ne (Get-ExpectedOpenVrHash $state)) {
            throw 'The managed OpenVR loader changed after installation; restore aborted.'
        }
        Assert-ManagedDirectory $VrAssetsPath (Get-ExpectedVrSnapshot $state)
        $currentHash = Get-Sha256 $AlutPath
        if ($currentHash -and $state.installedProxySha256 -and
            $currentHash -ne [string]$state.installedProxySha256) {
            throw 'alut.dll changed after Penumbra VR installation; restore aborted to avoid overwriting another modification.'
        }
        if ((Test-Path -LiteralPath $HandTexturePath -PathType Leaf) -and
            (Get-OptionalProperty $state 'handTextureSha256') -and
            (Get-Sha256 $HandTexturePath) -ne [string](Get-OptionalProperty $state 'handTextureSha256')) {
            throw 'The installed Penumbra VR hand texture changed after installation; restore aborted.'
        }
        if ((Test-Path -LiteralPath $LocalizationPath -PathType Leaf) -and
            (Get-OptionalProperty $state 'spanishLocalizationSha256') -and
            (Get-Sha256 $LocalizationPath) -ne [string](Get-OptionalProperty $state 'spanishLocalizationSha256')) {
            throw 'The installed Penumbra VR Spanish localization changed after installation; restore aborted.'
        }
        if ([bool](Get-OptionalProperty $state 'spanishLocalizationHadOriginal')) {
            $originalHash = Get-OptionalProperty $state 'spanishLocalizationOriginalSha256'
            if (-not $originalHash -or
                (Get-Sha256 $LocalizationBackupPath) -ne [string]$originalHash) {
                throw 'The managed Spanish localization backup failed hash validation; restore aborted.'
            }
        }
        $managedRequiemLanguageHash = Get-OptionalProperty $state 'requiemSpanishLocalizationSha256'
        if ($managedRequiemLanguageHash -and
            (Get-Sha256 $RequiemLocalizationPath) -ne [string]$managedRequiemLanguageHash) {
            throw 'The managed Requiem localization changed after installation; restore aborted.'
        }
        if ([bool](Get-OptionalProperty $state 'requiemSpanishLocalizationHadOriginal') -and
            (Get-Sha256 $RequiemLocalizationBackupPath) -ne
                [string](Get-OptionalProperty $state 'requiemSpanishLocalizationOriginalSha256')) {
            throw 'The Requiem localization backup failed hash validation; restore aborted.'
        }
        if ([bool](Get-OptionalProperty $state 'audioConfigCreated') -and
            (Get-Sha256 $AudioConfigPath) -ne [string](Get-OptionalProperty $state 'audioConfigSha256')) {
            throw 'The managed OpenAL configuration changed after installation; restore aborted.'
        }
        if ([bool](Get-OptionalProperty $state 'laaApplied')) {
            if (((Get-Sha256 $GamePath) -and (Get-Sha256 $GamePath) -ne $ExpectedGameHashes[1]) -or
                (Get-Sha256 $OriginalGamePath) -ne $ExpectedGameHashes[0]) {
                throw 'The managed LAA executable or canonical backup changed; restore aborted.'
            }
        } elseif (Test-Path -LiteralPath $OriginalGamePath) {
            throw 'An unowned executable backup exists; restore aborted.'
        }
    } else {
        throw 'No installation record exists; restore aborted.'
    }

    $managedOpenAl=Get-OptionalProperty $state 'openAlSha256'
    if($managedOpenAl -and (Get-Sha256 $OpenAlPath) -ne $managedOpenAl) { throw 'Managed OpenAL changed; repair before removal.' }
    if([bool](Get-OptionalProperty $state 'openAlHadOriginal') -and (Get-Sha256 $OriginalOpenAlPath) -ne [string](Get-OptionalProperty $state 'originalOpenAlSha256')) { throw 'Original OpenAL backup failed verification.' }
    foreach($plan in $PreviousConfiguration){Test-PvrConfigurationPlan -Plan $plan -Restore}
    $ActiveConfigurationPaths=@($PreviousConfiguration | ForEach-Object {$_.Path})
    if($Preflight) { [pscustomobject]@{Files=@($ManagedTransactionPaths | Where-Object {$_ -notin $ConfigurationPaths -or $_ -in $ActiveConfigurationPaths});Configuration=@($PreviousConfiguration);Operation='restore'}; return }
    $deploymentSnapshot = New-DeploymentSnapshot
    try {
    foreach($plan in $PreviousConfiguration){Restore-PvrConfiguration -Plan $plan}
    Copy-Item -LiteralPath $OriginalAlutPath -Destination $AlutPath -Force
    Remove-Item -LiteralPath $OriginalAlutPath -Force
    if($managedOpenAl) {
        if([bool](Get-OptionalProperty $state 'openAlHadOriginal')) { Copy-Item -LiteralPath $OriginalOpenAlPath -Destination $OpenAlPath -Force; Remove-Item -LiteralPath $OriginalOpenAlPath -Force }
        else { Remove-Item -LiteralPath $OpenAlPath -Force }
    }
    foreach ($managedFile in @($ProbePath, $OpenVrPath, $HandTexturePath)) {
        if (Test-Path -LiteralPath $managedFile) {
            Remove-Item -LiteralPath $managedFile -Force
        }
    }
    if (Get-OptionalProperty $state 'requiemProbeSha256') {
        Remove-Item -LiteralPath $RequiemProbePath -Force
    }
    $stateLocalizationSha256 = Get-OptionalProperty $state 'spanishLocalizationSha256'
    if ($stateLocalizationSha256) {
        if ([bool](Get-OptionalProperty $state 'spanishLocalizationHadOriginal')) {
            Copy-Item -LiteralPath $LocalizationBackupPath -Destination $LocalizationPath -Force
            Remove-Item -LiteralPath $LocalizationBackupPath -Force
        } else {
            if (Test-Path -LiteralPath $LocalizationPath) {
                Remove-Item -LiteralPath $LocalizationPath -Force
            }
        }
    }
    if (Get-OptionalProperty $state 'requiemSpanishLocalizationSha256') {
        if ([bool](Get-OptionalProperty $state 'requiemSpanishLocalizationHadOriginal')) {
            Copy-Item -LiteralPath $RequiemLocalizationBackupPath -Destination $RequiemLocalizationPath -Force
            Remove-Item -LiteralPath $RequiemLocalizationBackupPath -Force
        } else {
            Remove-Item -LiteralPath $RequiemLocalizationPath -Force
        }
    }
    $stateAudioConfigCreated = [bool](Get-OptionalProperty $state 'audioConfigCreated')
    $stateAudioConfigSha256 = Get-OptionalProperty $state 'audioConfigSha256'
    if ($stateAudioConfigCreated -and
        (Test-Path -LiteralPath $AudioConfigPath -PathType Leaf) -and
        $stateAudioConfigSha256 -and
        (Get-Sha256 $AudioConfigPath) -eq [string]$stateAudioConfigSha256) {
        Remove-Item -LiteralPath $AudioConfigPath -Force
    }
    Remove-Item -LiteralPath $InstallStatePath -Force
    if (Test-Path -LiteralPath $VrAssetsPath -PathType Container) {
        Remove-Item -LiteralPath $VrAssetsPath -Recurse -Force
    }
    if ([bool](Get-OptionalProperty $state 'laaApplied')) {
        Copy-Item -LiteralPath $OriginalGamePath -Destination $GamePath -Force
        if ((Get-Sha256 $GamePath) -ne $ExpectedGameHashes[0]) {
            throw 'Canonical executable restore failed hash verification.'
        }
        Remove-Item -LiteralPath $OriginalGamePath -Force
    }
    Write-Host 'Black Plague normal Steam launch bootstrap restored cleanly.'
    } catch {
        Invoke-DeploymentRollback $deploymentSnapshot $_.Exception.Message
    }
    Complete-DeploymentSnapshot $deploymentSnapshot
    return
}

if ($Repair -and -not $PreviousState) {
    throw 'Black Plague repair requires an existing managed installation record.'
}

$ReleaseRoot = Join-Path $BuildRoot 'bin\Release'
function Get-DeploymentSource([string]$Id) {
    $payload = Get-DeploymentPayload $Id
    $sourceKind = [string]$payload.source.kind
    if ($sourceKind -eq 'build-output') {
        return Join-Path $ReleaseRoot ([string]$payload.source.path -replace '/', '\')
    }
    if ($sourceKind -eq 'repository-file' -or $sourceKind -eq 'repository-directory') {
        return Join-Path $RepoRoot ([string]$payload.source.path -replace '/', '\')
    }
    throw "Deployment payload '$Id' does not have a filesystem source."
}

$ProxySource = Get-DeploymentSource 'bootstrap_proxy'
$ProbeSource = Get-DeploymentSource 'probe'
$RequiemProbeSource = Get-DeploymentSource 'requiem_probe'
$OpenVrSource = Get-DeploymentSource 'openvr_loader'
$VrAssetsSource = Get-DeploymentSource 'openvr_actions'
$HandTextureSource = Get-DeploymentSource 'hand_texture'
$LocalizationSource = Get-DeploymentSource 'spanish_localization'
$RequiemLocalizationSource = Get-DeploymentSource 'requiem_spanish_localization'
$OpenAlSource = Join-Path $RepoRoot 'products/overture/dependencies/bin/win32/OpenAL32.dll'
if((Get-Sha256 $OpenAlSource) -ne $OpenAlHash) { throw 'Pinned OpenAL Soft payload is missing or invalid.' }

foreach ($required in @($ProxySource, $ProbeSource, $OpenVrSource)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Required Release artifact not found: $required"
    }
}
if (-not (Test-Path -LiteralPath $VrAssetsSource -PathType Container)) {
    throw "OpenVR assets not found: $VrAssetsSource"
}
if (-not (Test-Path -LiteralPath $HandTextureSource -PathType Leaf)) {
    throw "Rework hand texture not found: $HandTextureSource"
}
if ($CommunityTranslations -and -not (Test-Path -LiteralPath $LocalizationSource -PathType Leaf)) {
    throw "Spanish localization not found: $LocalizationSource"
}
if ($InstallRequiem -and
    (-not (Test-Path -LiteralPath $RequiemProbeSource -PathType Leaf) -or
     ($CommunityTranslations -and -not (Test-Path -LiteralPath $RequiemLocalizationSource -PathType Leaf)))) {
    throw 'Requiem probe or localization is missing from this package.'
}

# Validate the entire existing installation before changing its first file.
# A failed upgrade must leave third-party and user edits in place.
if ($PreviousState) {
    if ((Get-OptionalProperty $PreviousState 'schema') -ne 1) {
        throw 'Unsupported Black Plague installation record schema.'
    }
    if ($InstallRequiem -and (Get-OptionalProperty $PreviousState 'requiemProbeSha256') -and
        (Get-Sha256 $RequiemGamePath) -ne
            [string](Get-OptionalProperty $PreviousState 'requiemExeSha256')) {
        throw 'The managed Requiem executable changed; redeploy aborted.'
    }
    if ($InstallRequiem -and
        -not (Get-OptionalProperty $PreviousState 'requiemProbeSha256') -and
        (Test-Path -LiteralPath $RequiemProbePath)) {
        throw 'An unmanaged Requiem probe exists; redeploy aborted.'
    }
    if ($Repair) {
        foreach ($name in @('installedProxySha256', 'probeSha256', 'openVrSha256',
                            'handTextureSha256')) {
            if ([string](Get-OptionalProperty $PreviousState $name) -notmatch '^[0-9A-Fa-f]{64}$') {
                throw "Black Plague repair requires a recorded hash for $name."
            }
        }
        if (-not (Get-OptionalProperty $PreviousState 'vrAssetsSnapshot')) {
            throw 'Black Plague repair requires a recorded OpenVR directory snapshot.'
        }
    }
    if (-not $RepairLaaExecutable -and
        (Get-OptionalProperty $PreviousState 'gameExeSha256') -ne (Get-Sha256 $GamePath)) {
        throw 'The game executable changed after the prior installation; redeploy aborted.'
    }
    foreach ($managed in @(
        @($AlutPath, 'installedProxySha256'),
        @($ProbePath, 'probeSha256'),
        @($HandTexturePath, 'handTextureSha256'),
        @($LocalizationPath, 'spanishLocalizationSha256'),
        @($RequiemProbePath, 'requiemProbeSha256'),
        @($RequiemLocalizationPath, 'requiemSpanishLocalizationSha256')
    )) {
        $recordedHash = Get-OptionalProperty $PreviousState $managed[1]
        if (-not $Repair -and $recordedHash -and (Get-Sha256 $managed[0]) -ne [string]$recordedHash) {
            throw "Managed file changed after installation: $($managed[0])"
        }
    }
    if (-not $Repair -and (Get-Sha256 $OpenVrPath) -ne (Get-ExpectedOpenVrHash $PreviousState)) {
        throw 'The managed OpenVR loader changed after installation; redeploy aborted.'
    }
    if ($Repair) {
        Assert-RepairableManagedDirectory $VrAssetsPath (Get-ExpectedVrSnapshot $PreviousState)
    } else {
        Assert-ManagedDirectory $VrAssetsPath (Get-ExpectedVrSnapshot $PreviousState)
    }
    $previousAudioCreated = [bool](Get-OptionalProperty $PreviousState 'audioConfigCreated')
    $currentAudioHash = Get-Sha256 $AudioConfigPath
    if ($previousAudioCreated -and
        $currentAudioHash -ne [string](Get-OptionalProperty $PreviousState 'audioConfigSha256')) {
        if (-not ($Repair -and -not $currentAudioHash -and
            [string](Get-OptionalProperty $PreviousState 'audioConfigSha256') -eq $GeneratedAudioConfigHash)) {
            throw 'The managed OpenAL configuration changed after installation; redeploy aborted.'
        }
    }
    if ($Repair -and -not $previousAudioCreated -and -not $currentAudioHash) {
        throw 'Black Plague repair will not replace a missing user-owned OpenAL configuration.'
    }
    if ([bool](Get-OptionalProperty $PreviousState 'laaApplied')) {
        if ((-not $RepairLaaExecutable -and (Get-Sha256 $GamePath) -ne $ExpectedGameHashes[1]) -or
            (Get-Sha256 $OriginalGamePath) -ne $ExpectedGameHashes[0]) {
            throw 'The managed LAA executable or canonical backup changed; redeploy aborted.'
        }
    } elseif (Test-Path -LiteralPath $OriginalGamePath) {
        throw 'An unowned executable backup exists; redeploy aborted.'
    }
    if ($Repair -and (Get-Sha256 $OriginalAlutPath) -ne $ExpectedOriginalAlutHash) {
        throw 'Black Plague repair requires the verified original ALUT backup.'
    }
} else {
    if (Test-Path -LiteralPath $OriginalGamePath) {
        throw 'An executable backup exists without install state; redeploy aborted.'
    }
    foreach ($target in @($ProbePath, $OpenVrPath, $HandTexturePath)) {
        if (Test-Path -LiteralPath $target) {
            throw "Unmanaged payload already exists: $target"
        }
    }
    if ($InstallRequiem -and
        (Test-Path -LiteralPath $RequiemProbePath)) {
        throw "Unmanaged Requiem probe already exists: $RequiemProbePath"
    }
    if (Test-Path -LiteralPath $VrAssetsPath) {
        throw "Unmanaged OpenVR assets already exist: $VrAssetsPath"
    }
}

$LocalizationHadOriginal = $false
$LocalizationOriginalSha256 = $null
$previousLocalizationSha256 = Get-OptionalProperty $PreviousState 'spanishLocalizationSha256'
if ($previousLocalizationSha256) {
    $LocalizationHadOriginal = [bool](Get-OptionalProperty $PreviousState 'spanishLocalizationHadOriginal')
    $LocalizationOriginalSha256 = [string](Get-OptionalProperty $PreviousState 'spanishLocalizationOriginalSha256')
    if ($LocalizationHadOriginal) {
        if (-not (Test-Path -LiteralPath $LocalizationBackupPath -PathType Leaf)) {
            throw 'The managed Spanish localization backup is missing; redeploy aborted.'
        }
        if ($LocalizationOriginalSha256 -and
            (Get-Sha256 $LocalizationBackupPath) -ne $LocalizationOriginalSha256) {
            throw 'The managed Spanish localization backup failed hash validation; redeploy aborted.'
        }
    }
} else {
    $LocalizationHadOriginal = $CommunityTranslations -and (Test-Path -LiteralPath $LocalizationPath -PathType Leaf)
    if ($LocalizationHadOriginal) {
        if (Test-Path -LiteralPath $LocalizationBackupPath -PathType Leaf) {
            throw 'A Spanish localization backup exists without install state; redeploy aborted.'
        }
        $LocalizationOriginalSha256 = Get-Sha256 $LocalizationPath
    }
}

$RequiemLocalizationHadOriginal = $false
$RequiemLocalizationOriginalSha256 = $null
$previousRequiemLocalizationSha256 = Get-OptionalProperty $PreviousState 'requiemSpanishLocalizationSha256'
    if ($InstallRequiem -and $CommunityTranslations) {
    if ($previousRequiemLocalizationSha256) {
        $RequiemLocalizationHadOriginal = [bool](Get-OptionalProperty $PreviousState 'requiemSpanishLocalizationHadOriginal')
        $RequiemLocalizationOriginalSha256 = [string](Get-OptionalProperty $PreviousState 'requiemSpanishLocalizationOriginalSha256')
        if ($RequiemLocalizationHadOriginal -and
            (Get-Sha256 $RequiemLocalizationBackupPath) -ne $RequiemLocalizationOriginalSha256) {
            throw 'The Requiem localization backup failed hash validation; redeploy aborted.'
        }
        if (Get-OptionalProperty $PreviousState 'requiemProbeSha256') {
            foreach ($name in @('requiemProbeSha256', 'requiemSpanishLocalizationSha256')) {
                if ([string](Get-OptionalProperty $PreviousState $name) -notmatch '^[0-9A-Fa-f]{64}$') {
                    throw "Requiem repair requires a recorded hash for $name."
                }
            }
        }
    } else {
        $RequiemLocalizationHadOriginal = Test-Path -LiteralPath $RequiemLocalizationPath -PathType Leaf
        if ($RequiemLocalizationHadOriginal) {
            if (Test-Path -LiteralPath $RequiemLocalizationBackupPath) {
                throw 'A Requiem localization backup exists without install state; redeploy aborted.'
            }
            $RequiemLocalizationOriginalSha256 = Get-Sha256 $RequiemLocalizationPath
        }
    }
}

$PreviousLaaApplied = [bool](Get-OptionalProperty $PreviousState 'laaApplied')
$ApplyLaa = $LargeAddressAware -and -not $PreviousLaaApplied -and
    (Get-Sha256 $GamePath) -eq $ExpectedGameHashes[0]
$LaaTool = Join-Path $BuildRoot 'bin/Release/PenumbraVR.LaaTransform.exe'
if (($ApplyLaa -or $RepairLaaExecutable) -and -not (Test-Path -LiteralPath $LaaTool -PathType Leaf)) {
    throw "LAA transform tool not found: $LaaTool"
}
$OpenAlHadOriginal = [bool](Get-OptionalProperty $PreviousState 'openAlHadOriginal')
$OriginalOpenAlHash = Get-OptionalProperty $PreviousState 'originalOpenAlSha256'
if(Get-OptionalProperty $PreviousState 'openAlSha256') {
    if(-not $Repair -and (Get-Sha256 $OpenAlPath) -ne [string]$PreviousState.openAlSha256) { throw 'Managed OpenAL changed; redeploy aborted.' }
    if($OpenAlHadOriginal -and (Get-Sha256 $OriginalOpenAlPath) -ne $OriginalOpenAlHash) { throw 'Original OpenAL backup failed verification.' }
} else {
    if(Test-Path -LiteralPath $OriginalOpenAlPath) { throw 'Unowned OpenAL backup exists.' }
    $OriginalOpenAlHash=Get-Sha256 $OpenAlPath
    if($OriginalOpenAlHash -and $OriginalOpenAlHash -ne $OpenAlHash) { throw 'Unknown existing OpenAL DLL; no files changed. Move or remove that modification explicitly before installing.' }
    $OpenAlHadOriginal=[bool]$OriginalOpenAlHash
}
if($PreviousState) {
    if((Get-Sha256 $OriginalAlutPath) -ne $ExpectedOriginalAlutHash) { throw 'Original ALUT backup failed verification.' }
} elseif((Get-Sha256 $AlutPath) -ne $ExpectedOriginalAlutHash -or (Test-Path -LiteralPath $OriginalAlutPath)) { throw 'Unknown ALUT or unowned backup; no files changed.' }
foreach($pair in @(@($LocalizationBackupPath,'spanishLocalizationOriginalSha256'),@($RequiemLocalizationBackupPath,'requiemSpanishLocalizationOriginalSha256'))) {
    $recorded=Get-OptionalProperty $PreviousState $pair[1]
    if($recorded -and (Get-Sha256 $pair[0]) -ne $recorded) { throw 'Optional localization backup failed verification.' }
}
$Configuration=@()
$ConfigurationToRestore=@()
foreach($gameId in @('black_plague','requiem')) {
    $previous=@($PreviousConfiguration | Where-Object {$_.Game -eq $gameId})
    if($gameId -eq 'requiem' -and -not $InstallRequiem){$ConfigurationToRestore+=$previous;continue}
    $records=@(Get-PvrConfigurationRecords -Game $gameId -RedistRoot $GameRoot -Previous $previous -Apply:$RecommendedSettings -SettingsScope $SettingsScope)
    $Configuration+=$records
}
foreach($path in $ManagedTransactionPaths){Assert-ManagedTransactionPath $path | Out-Null}
foreach($plan in $ConfigurationToRestore){Test-PvrConfigurationPlan -Plan $plan -Restore}
if($RecommendedSettings){foreach($plan in $Configuration){Test-PvrConfigurationPlan -Plan $plan}}
$ActiveConfigurationPaths=@($ConfigurationToRestore | ForEach-Object {$_.Path})+@(if($RecommendedSettings){$Configuration | ForEach-Object {$_.Path}})
if($Preflight) {
    [pscustomobject]@{Components=@('shared','black_plague')+@(if($InstallRequiem){'requiem'})+@(if($CommunityTranslations){'spanish_translation'});Files=@($AlutPath,$ProbePath,$OpenAlPath,$OpenVrPath,$VrAssetsPath,$HandTexturePath)+@(if($InstallRequiem){$RequiemProbePath})+@(if($CommunityTranslations){$LocalizationPath})+@(if($CommunityTranslations -and $InstallRequiem){$RequiemLocalizationPath});Configuration=@($Configuration);RetiredConfiguration=@($ConfigurationToRestore);Operation=$(if($Repair){'repair'}else{'install'})}
    return
}
$deploymentSnapshot = New-DeploymentSnapshot
try {
foreach($plan in $ConfigurationToRestore){Restore-PvrConfiguration -Plan $plan}
if($RecommendedSettings){foreach($plan in $Configuration){Set-PvrConfiguration -Plan $plan}}
if($OpenAlHadOriginal -and -not (Get-OptionalProperty $PreviousState 'openAlSha256')) { Copy-Item -LiteralPath $OpenAlPath -Destination $OriginalOpenAlPath }
Copy-Item -LiteralPath $OpenAlSource -Destination $OpenAlPath -Force
if((Get-Sha256 $OpenAlPath) -ne $OpenAlHash) { throw 'Installed OpenAL failed verification.' }
if(-not $InstallRequiem -and (Get-OptionalProperty $PreviousState 'requiemProbeSha256')) { Remove-Item -LiteralPath $RequiemProbePath -Force }
foreach($selection in @(
    @($CommunityTranslations,$LocalizationPath,$LocalizationBackupPath,'spanishLocalizationSha256','spanishLocalizationHadOriginal'),
    @(($CommunityTranslations -and $InstallRequiem),$RequiemLocalizationPath,$RequiemLocalizationBackupPath,'requiemSpanishLocalizationSha256','requiemSpanishLocalizationHadOriginal')
)) {
    if(-not $selection[0] -and (Get-OptionalProperty $PreviousState $selection[3])) {
        if([bool](Get-OptionalProperty $PreviousState $selection[4])) { Copy-Item -LiteralPath $selection[2] -Destination $selection[1] -Force; Remove-Item -LiteralPath $selection[2] -Force }
        else { Remove-Item -LiteralPath $selection[1] -Force }
    }
}
if ($RepairLaaExecutable) {
    $preparedLaa = Join-Path $deploymentSnapshot.Root 'prepared-laa.exe'
    & $LaaTool $OriginalGamePath $preparedLaa | Out-Null
    if ($LASTEXITCODE -ne 0 -or (Get-Sha256 $preparedLaa) -ne $ExpectedGameHashes[1]) {
        throw 'The canonical backup did not reproduce the recorded LAA executable.'
    }
    Copy-Item -LiteralPath $preparedLaa -Destination $GamePath -Force
    if ((Get-Sha256 $GamePath) -ne $ExpectedGameHashes[1]) {
        throw 'Repaired LAA executable failed hash verification.'
    }
}
if ($ApplyLaa) {
    $preparedLaa = Join-Path $deploymentSnapshot.Root 'prepared-laa.exe'
    & $LaaTool $GamePath $preparedLaa | Out-Null
    if ($LASTEXITCODE -ne 0 -or (Get-Sha256 $preparedLaa) -ne $ExpectedGameHashes[1]) {
        throw 'The exact-build LAA transform did not produce the recorded variant.'
    }
    Copy-Item -LiteralPath $GamePath -Destination $OriginalGamePath
    if ((Get-Sha256 $OriginalGamePath) -ne $ExpectedGameHashes[0]) {
        throw 'The canonical executable backup failed hash verification.'
    }
    Copy-Item -LiteralPath $preparedLaa -Destination $GamePath -Force
    if ((Get-Sha256 $GamePath) -ne $ExpectedGameHashes[1]) {
        throw 'Installed LAA executable failed hash verification.'
    }
}
if (-not (Test-Path -LiteralPath $OriginalAlutPath -PathType Leaf)) {
    if ($PreviousState) {
        throw 'The managed ALUT backup is missing; redeploy aborted.'
    }
    $currentAlutHash = Get-Sha256 $AlutPath
    if ($currentAlutHash -ne $ExpectedOriginalAlutHash) {
        throw "Refusing to replace unknown alut.dll SHA-256: $currentAlutHash"
    }
    Copy-Item -LiteralPath $AlutPath -Destination $OriginalAlutPath
} elseif ((Get-Sha256 $OriginalAlutPath) -ne $ExpectedOriginalAlutHash) {
    throw 'Existing managed ALUT backup does not match the known retail DLL.'
} elseif (-not $PreviousState) {
    throw 'A managed ALUT backup exists without install state; redeploy aborted.'
}

Copy-Item -LiteralPath $ProxySource -Destination $AlutPath -Force
Copy-Item -LiteralPath $ProbeSource -Destination $ProbePath -Force
if ($InstallRequiem) {
    Copy-Item -LiteralPath $RequiemProbeSource -Destination $RequiemProbePath -Force
}
Copy-Item -LiteralPath $OpenVrSource -Destination $OpenVrPath -Force
if (Test-Path -LiteralPath $VrAssetsPath -PathType Container) {
    Remove-Item -LiteralPath $VrAssetsPath -Recurse -Force
}
Copy-Item -LiteralPath $VrAssetsSource -Destination $VrAssetsPath -Recurse
New-Item -ItemType Directory -Path $HandAssetsPath -Force | Out-Null
Copy-Item -LiteralPath $HandTextureSource -Destination $HandTexturePath -Force

if($CommunityTranslations) {
if ($LocalizationHadOriginal -and -not $previousLocalizationSha256) {
    Copy-Item -LiteralPath $LocalizationPath -Destination $LocalizationBackupPath
}
$LocalizationDirectory = Split-Path -Parent $LocalizationPath
New-Item -ItemType Directory -Path $LocalizationDirectory -Force | Out-Null
Copy-Item -LiteralPath $LocalizationSource -Destination $LocalizationPath -Force
}
if ($InstallRequiem -and $CommunityTranslations) {
    if ($RequiemLocalizationHadOriginal -and -not $previousRequiemLocalizationSha256) {
        Copy-Item -LiteralPath $RequiemLocalizationPath -Destination $RequiemLocalizationBackupPath
    }
    New-Item -ItemType Directory -Path (Split-Path -Parent $RequiemLocalizationPath) -Force | Out-Null
    Copy-Item -LiteralPath $RequiemLocalizationSource -Destination $RequiemLocalizationPath -Force
}

$AudioConfigCreated = [bool](Get-OptionalProperty $PreviousState 'audioConfigCreated')
if (-not (Test-Path -LiteralPath $AudioConfigPath -PathType Leaf)) {
    [System.IO.File]::WriteAllText(
        $AudioConfigPath,
        $GeneratedAudioConfigText,
        [System.Text.Encoding]::ASCII)
    if ((Get-Sha256 $AudioConfigPath) -ne $GeneratedAudioConfigHash) {
        throw 'Generated OpenAL configuration failed hash verification.'
    }
    $AudioConfigCreated = $true
}

foreach ($copy in @(
    @($ProxySource, $AlutPath),
    @($ProbeSource, $ProbePath),
    @($OpenVrSource, $OpenVrPath),
    @($HandTextureSource, $HandTexturePath)
)) {
    if ((Get-Sha256 $copy[0]) -ne (Get-Sha256 $copy[1])) {
        throw "Deployment verification failed for '$($copy[1])'."
    }
}
if ($InstallRequiem) {
    foreach ($copy in @(@($RequiemProbeSource, $RequiemProbePath))) {
        if ((Get-Sha256 $copy[0]) -ne (Get-Sha256 $copy[1])) {
            throw "Deployment verification failed for '$($copy[1])'."
        }
    }
}

$sourceVrFiles = @(Get-ChildItem -LiteralPath $VrAssetsSource -Recurse -File)
$destinationVrFiles = @(Get-ChildItem -LiteralPath $VrAssetsPath -Recurse -File)
if ($sourceVrFiles.Count -ne $destinationVrFiles.Count) {
    throw 'Deployment verification failed for the OpenVR action/binding directory.'
}
foreach ($sourceVrFile in $sourceVrFiles) {
    $relativeVrPath = $sourceVrFile.FullName.Substring($VrAssetsSource.Length).TrimStart('\')
    $destinationVrFile = Join-Path $VrAssetsPath $relativeVrPath
    if (-not (Test-Path -LiteralPath $destinationVrFile -PathType Leaf) -or
        (Get-Sha256 $sourceVrFile.FullName) -ne (Get-Sha256 $destinationVrFile)) {
        throw "Deployment verification failed for OpenVR payload '$relativeVrPath'."
    }
}

$state = [ordered]@{
    schema = 1
    frameworkVersion = (Get-Content -LiteralPath (Join-Path $RepoRoot 'release.json') -Raw | ConvertFrom-Json).version
    components = @('shared','black_plague') + @(if($InstallRequiem){'requiem'}) + @(if($CommunityTranslations){'spanish_translation'})
    configuration = @($Configuration)
    openAlSha256 = Get-Sha256 $OpenAlPath
    openAlHadOriginal = $OpenAlHadOriginal
    originalOpenAlSha256 = $OriginalOpenAlHash
    gameExeSha256 = Get-Sha256 $GamePath
    laaApplied = ($PreviousLaaApplied -or $ApplyLaa)
    originalGameExeSha256 = if ($PreviousLaaApplied -or $ApplyLaa) {
        Get-Sha256 $OriginalGamePath
    } else { $null }
    originalAlutSha256 = Get-Sha256 $OriginalAlutPath
    installedProxySha256 = Get-Sha256 $AlutPath
    probeSha256 = Get-Sha256 $ProbePath
    requiemExeSha256 = if ($InstallRequiem) { Get-Sha256 $RequiemGamePath } else { $null }
    requiemProbeSha256 = if ($InstallRequiem) { Get-Sha256 $RequiemProbePath } else { $null }
    openVrSha256 = Get-Sha256 $OpenVrPath
    vrAssetsSnapshot = Get-DirectorySnapshot $VrAssetsPath
    handTextureSha256 = Get-Sha256 $HandTexturePath
    spanishLocalizationSha256 = if($CommunityTranslations){ Get-Sha256 $LocalizationPath }else{$null}
    spanishLocalizationHadOriginal = $CommunityTranslations -and $LocalizationHadOriginal
    spanishLocalizationOriginalSha256 = if($CommunityTranslations){$LocalizationOriginalSha256}else{$null}
    requiemSpanishLocalizationSha256 = if ($InstallRequiem -and $CommunityTranslations) { Get-Sha256 $RequiemLocalizationPath } else { $null }
    requiemSpanishLocalizationHadOriginal = $InstallRequiem -and $CommunityTranslations -and $RequiemLocalizationHadOriginal
    requiemSpanishLocalizationOriginalSha256 = if ($InstallRequiem -and $CommunityTranslations) { $RequiemLocalizationOriginalSha256 } else { $null }
    audioConfigCreated = $AudioConfigCreated
    audioConfigSha256 = if ($AudioConfigCreated) { Get-Sha256 $AudioConfigPath } else { $null }
}
$state | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $InstallStatePath -Encoding UTF8

Write-Host 'Black Plague normal Steam launch bootstrap installed.'
Write-Host 'Start SteamVR, then use the normal Play button for Penumbra: Black Plague in Steam.'
Write-Host 'Validation scripts and Start-Black-Plague-VR.cmd remain available for diagnostics.'
} catch {
    Invoke-DeploymentRollback $deploymentSnapshot $_.Exception.Message
}
Complete-DeploymentSnapshot $deploymentSnapshot
