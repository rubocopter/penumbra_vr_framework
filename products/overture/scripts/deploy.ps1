[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',

    [string]$PackageRoot,

    [string]$InstallRoot,

    [ValidatePattern('^[0-9A-Fa-f]{64}$')]
    [string]$ExpectedExecutableSha256,

    [bool]$SteamLauncher = $true,

    [switch]$Restore,

    [switch]$Recover,

    [switch]$Repair,

    # Also restores these textures from deployment backups when already installed.
    [switch]$SkipTexturePack,
    [switch]$CommunityTranslations,
    [switch]$Preflight,
    [switch]$RecommendedSettings,
    [ValidateSet('DefaultFiles','DefaultAndUserFiles')][string]$SettingsScope='DefaultAndUserFiles'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$supportRoot=if(Test-Path -LiteralPath (Join-Path $PSScriptRoot 'tools/PenumbraVrConfiguration.psm1')){$PSScriptRoot}else{[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))}
Import-Module (Join-Path $supportRoot 'tools/PenumbraVrConfiguration.psm1') -Force
$personalConfiguration=(Get-PvrConfigurationPaths -Game overture -RedistRoot $PSScriptRoot)[1]
if ($Repair -and ($Restore -or $Recover)) {
    throw 'Overture repair cannot be combined with restore or interrupted-deployment recovery.'
}

function Get-NormalizedRoot([string]$path) {
    return [System.IO.Path]::GetFullPath($path).TrimEnd(
        [System.IO.Path]::DirectorySeparatorChar,
        [System.IO.Path]::AltDirectorySeparatorChar)
}

function Get-PathUnderRoot([string]$root, [string]$relativePath) {
    if ([System.IO.Path]::IsPathRooted($relativePath)) {
        throw "Managed path must be relative: $relativePath"
    }

    $normalizedRoot = Get-NormalizedRoot $root
    $candidate = [System.IO.Path]::GetFullPath((Join-Path $normalizedRoot $relativePath))
    $prefix = $normalizedRoot + [System.IO.Path]::DirectorySeparatorChar
    if (-not $candidate.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Managed path escapes the target root: $relativePath"
    }
    return $candidate
}

function Get-FileHashValue([string]$path) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        return $null
    }
    return (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Get-RelativePath([string]$basePath, [string]$fullPath) {
    # Path.GetRelativePath only exists on .NET Framework 4.7.1+; Windows
    # PowerShell 5.1 on a legacy .NET Framework does not provide it. Uri is
    # available since .NET Framework 4.0, so this works everywhere.
    $base = [System.IO.Path]::GetFullPath($basePath).TrimEnd([System.IO.Path]::DirectorySeparatorChar)
    $baseUri = [System.Uri]($base + [System.IO.Path]::DirectorySeparatorChar)
    $fullUri = [System.Uri]([System.IO.Path]::GetFullPath($fullPath))
    return [System.Uri]::UnescapeDataString($baseUri.MakeRelativeUri($fullUri).ToString()).Replace('/', [System.IO.Path]::DirectorySeparatorChar)
}

function Get-SteamRoots {
    $roots = New-Object System.Collections.Generic.List[string]

    $registryPaths = @(
        'HKCU:\Software\Valve\Steam',
        'HKLM:\Software\WOW6432Node\Valve\Steam'
    )
    foreach ($registryPath in $registryPaths) {
        $steamProperties = Get-ItemProperty -LiteralPath $registryPath -ErrorAction SilentlyContinue
        if ($null -ne $steamProperties) {
            foreach ($propertyName in @('SteamPath', 'InstallPath')) {
                $property = $steamProperties.PSObject.Properties[$propertyName]
                if ($null -ne $property -and $property.Value) {
                    $roots.Add([string]$property.Value)
                }
            }
        }
    }

    if (${env:ProgramFiles(x86)}) {
        $roots.Add((Join-Path ${env:ProgramFiles(x86)} 'Steam'))
    }

    $expandedRoots = New-Object System.Collections.Generic.List[string]
    foreach ($root in @($roots | Select-Object -Unique)) {
        if (-not (Test-Path -LiteralPath $root -PathType Container)) {
            continue
        }
        $expandedRoots.Add($root)

        $libraryFile = Join-Path $root 'steamapps\libraryfolders.vdf'
        if (Test-Path -LiteralPath $libraryFile -PathType Leaf) {
            $libraryText = Get-Content -Raw -LiteralPath $libraryFile
            foreach ($match in [regex]::Matches($libraryText, '"path"\s+"([^"]+)"')) {
                $libraryRoot = $match.Groups[1].Value.Replace('\\', '\')
                if (Test-Path -LiteralPath $libraryRoot -PathType Container) {
                    $expandedRoots.Add($libraryRoot)
                }
            }
        }
    }

    return @($expandedRoots | Select-Object -Unique)
}

function Resolve-GamePaths([string]$requestedRoot) {
    $candidates = New-Object System.Collections.Generic.List[string]
    if ($requestedRoot) {
        $candidates.Add($requestedRoot)
    }
    else {
        foreach ($steamRoot in Get-SteamRoots) {
            $candidates.Add((Join-Path $steamRoot 'steamapps\common\Penumbra Overture'))
        }
    }

    foreach ($candidate in @($candidates | Select-Object -Unique)) {
        if (-not (Test-Path -LiteralPath $candidate -PathType Container)) {
            continue
        }

        $normalized = Get-NormalizedRoot $candidate
        if ((Split-Path -Leaf $normalized) -ieq 'redist') {
            $gameRoot = Split-Path -Parent $normalized
            $redistRoot = $normalized
        }
        else {
            $gameRoot = $normalized
            $redistRoot = Join-Path $gameRoot 'redist'
        }

        $hasManagedState = ($Repair -or $Restore) -and
            (Test-Path -LiteralPath (Join-Path $gameRoot '.penumbravr/deploy-state.json') -PathType Leaf)
        if (((Test-Path -LiteralPath (Join-Path $redistRoot 'Penumbra.exe') -PathType Leaf) -or $hasManagedState) -and
            ((Test-Path -LiteralPath (Join-Path $redistRoot 'config\English.lang') -PathType Leaf) -or $hasManagedState)) {
            return [pscustomobject]@{
                GameRoot = Get-NormalizedRoot $gameRoot
                RedistRoot = Get-NormalizedRoot $redistRoot
            }
        }
    }

    if ($requestedRoot) {
        throw "The requested path is not a Penumbra: Overture installation: $requestedRoot"
    }
    throw 'Could not locate the Steam installation of Penumbra: Overture. Pass -InstallRoot explicitly.'
}

function Resolve-PackageRoot([string]$requestedRoot, [string]$configuration) {
    if ($requestedRoot) {
        $candidate = Get-NormalizedRoot $requestedRoot
    }
    elseif (Test-Path -LiteralPath (Join-Path $PSScriptRoot 'Penumbra_vr.exe') -PathType Leaf) {
        # The installer is running from a packaged release.
        $candidate = Get-NormalizedRoot $PSScriptRoot
    }
    else {
        $repositoryRoot = Split-Path -Parent $PSScriptRoot
        $candidate = Get-NormalizedRoot (Join-Path $repositoryRoot "build\package\$configuration\PenumbraVR")
    }

    foreach ($requiredFile in @('Penumbra_vr.exe', 'openvr_api.dll', 'vr\actions.json')) {
        if (-not (Test-Path -LiteralPath (Join-Path $candidate $requiredFile) -PathType Leaf)) {
            throw "Package is incomplete; missing '$requiredFile' under $candidate"
        }
    }

    # Guard against deploying a stale package: if any engine or game source
    # file is newer than the packaged executable, packaging was skipped after
    # the last build and this deploy would silently ship old code.
    if ($candidate -like (Join-Path (Split-Path -Parent $PSScriptRoot) 'build*')) {
        $packagedExe = Get-Item -LiteralPath (Join-Path $candidate 'Penumbra_vr.exe')
        $sourceRoots = @('PenumbraOverture', 'HPL1Engine') | ForEach-Object {
            Join-Path (Split-Path -Parent $PSScriptRoot) $_
        }
        $newestSource = Get-ChildItem $sourceRoots -Recurse -Include *.cpp, *.hpp, *.h -ErrorAction SilentlyContinue |
            Sort-Object LastWriteTime -Descending |
            Select-Object -First 1
        if ($newestSource -and $newestSource.LastWriteTime -gt $packagedExe.LastWriteTime) {
            throw ("Package is stale: '{0}' ({1}) is newer than the packaged exe ({2}). " +
                   "Run scripts/build.ps1 -Deploy instead of deploy.ps1 alone.") -
                  $newestSource.Name, $newestSource.LastWriteTime, $packagedExe.LastWriteTime
        }
    }
    return $candidate
}

function Remove-EmptyManagedParents([string]$filePath, [string]$managedRoot) {
    $normalizedRoot = Get-NormalizedRoot $managedRoot
    $directory = Split-Path -Parent $filePath
    while ($directory -and $directory -ne $normalizedRoot) {
        $prefix = $normalizedRoot + [System.IO.Path]::DirectorySeparatorChar
        if (-not $directory.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase)) {
            throw "Refusing to remove a directory outside the managed root: $directory"
        }
        if (@(Get-ChildItem -LiteralPath $directory -Force).Count -ne 0) {
            break
        }
        Remove-Item -LiteralPath $directory -Force
        $directory = Split-Path -Parent $directory
    }
}

function Assert-ManagedTarget([string]$target, [string]$gameRoot) {
    $normalizedGame = Get-NormalizedRoot $gameRoot
    $gameItem = Get-Item -LiteralPath $normalizedGame -Force -ErrorAction SilentlyContinue
    if ($gameItem -and ($gameItem.Attributes -band [System.IO.FileAttributes]::ReparsePoint)) {
        throw "Deployment game root is a reparse point: $normalizedGame"
    }
    $normalizedTarget = [System.IO.Path]::GetFullPath($target)
    $prefix = $normalizedGame + [System.IO.Path]::DirectorySeparatorChar
    if (-not $normalizedTarget.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase) -and $normalizedTarget -ine $personalConfiguration) {
        throw "Deployment path escapes the game root: $normalizedTarget"
    }
    $cursor = $normalizedTarget
    while ($cursor -and $cursor -ine $normalizedGame) {
        $item = Get-Item -LiteralPath $cursor -Force -ErrorAction SilentlyContinue
        if ($item -and ($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint)) {
            throw "Deployment path is a reparse point: $cursor"
        }
        $cursor = Split-Path -Parent $cursor
    }
    return $normalizedTarget
}

function Get-SnapshotFiles([string]$root) {
    if (-not (Test-Path -LiteralPath $root -PathType Container)) { return @() }
    $files = New-Object System.Collections.Generic.List[string]
    foreach ($item in @(Get-ChildItem -LiteralPath $root -Recurse -Force)) {
        if ($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) {
            throw "Deployment state contains a reparse point: $($item.FullName)"
        }
        if (-not $item.PSIsContainer) {
            $relative = (Get-RelativePath $root $item.FullName).Replace('\', '/')
            $files.Add($relative + ':' + (Get-FileHashValue $item.FullName))
        }
    }
    return @($files | Sort-Object)
}

function New-DeploymentSnapshot([string[]]$targetPaths, [string]$gameRoot) {
    $root = Assert-ManagedTarget (Join-Path $gameRoot '.penumbravr-journal') $gameRoot
    $staging = Assert-ManagedTarget (Join-Path $gameRoot ('.penumbravr-journal-staging-' + [guid]::NewGuid().ToString('N'))) $gameRoot
    if (Test-Path -LiteralPath $root) {
        throw "An interrupted Overture deployment needs recovery before another write: $root"
    }
    $entries = New-Object System.Collections.Generic.List[object]
    try {
        New-Item -ItemType Directory -Path $staging | Out-Null
        $index = 0
        foreach ($candidate in @($targetPaths | Select-Object -Unique)) {
            $target = Assert-ManagedTarget $candidate $gameRoot
            $item = Get-Item -LiteralPath $target -Force -ErrorAction SilentlyContinue
            $kind = if ($null -eq $item) { 'absent' } elseif ($item.PSIsContainer) { 'directory' } else { 'file' }
            $missingParents = New-Object System.Collections.Generic.List[string]
            $parent = Split-Path -Parent $target
            while ($parent -and $parent -ine (Get-NormalizedRoot $gameRoot) -and
                -not (Test-Path -LiteralPath $parent -PathType Container)) {
                $missingParents.Add($parent)
                $parent = Split-Path -Parent $parent
            }
            $copy = Join-Path $staging ('item-' + $index)
            $snapshotHash = $null
            $snapshotFiles = @()
            if ($kind -ne 'absent') {
                if ($kind -eq 'directory') { $before = @(Get-SnapshotFiles $target) }
                Copy-Item -LiteralPath $target -Destination $copy -Recurse -Force
                if ($kind -eq 'file') { $snapshotHash = Get-FileHashValue $copy }
                if ($kind -eq 'file' -and (Get-FileHashValue $target) -ne $snapshotHash) {
                    throw "Deployment snapshot failed hash verification: $target"
                }
                if ($kind -eq 'directory') {
                    $snapshotFiles = @(Get-SnapshotFiles $copy)
                    if (($snapshotFiles -join '|') -cne ($before -join '|')) {
                        throw "Deployment snapshot failed directory verification: $target"
                    }
                }
            }
            $entries.Add([pscustomobject]@{
                Target = $target; Copy = (Join-Path $root ('item-' + $index)); Kind = $kind
                MissingParents = $missingParents.ToArray()
                SnapshotHash = $snapshotHash; SnapshotFiles = $snapshotFiles
            })
            $index++
        }
        [ordered]@{
            Version = 1
            GameRoot = Get-NormalizedRoot $gameRoot
            Entries = $entries.ToArray()
        } | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $staging 'journal.json') -Encoding utf8
        [System.IO.Directory]::Move($staging, $root)
        return [pscustomobject]@{ Root = $root; Entries = $entries.ToArray() }
    }
    catch {
        if (Test-Path -LiteralPath $staging -PathType Container) {
            Remove-Item -LiteralPath $staging -Recurse -Force
        }
        throw
    }
}

function Read-DeploymentSnapshot([string]$gameRoot) {
    $root = Assert-ManagedTarget (Join-Path $gameRoot '.penumbravr-journal') $gameRoot
    $manifest = Join-Path $root 'journal.json'
    if (-not (Test-Path -LiteralPath $manifest -PathType Leaf)) {
        throw "Overture recovery journal is missing or incomplete: $manifest"
    }
    $record = Get-Content -LiteralPath $manifest -Raw | ConvertFrom-Json
    if ([int]$record.Version -ne 1 -or
        (Get-NormalizedRoot ([string]$record.GameRoot)) -ine (Get-NormalizedRoot $gameRoot)) {
        throw "Overture recovery journal belongs to a different game or version: $manifest"
    }
    $entries = @($record.Entries)
    if (-not $entries.Count) { throw "Overture recovery journal is empty: $manifest" }
    $seen = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    $stateRoot = Join-Path (Get-NormalizedRoot $gameRoot) '.penumbravr'
    $redistPrefix = (Join-Path (Get-NormalizedRoot $gameRoot) 'redist') + [System.IO.Path]::DirectorySeparatorChar
    for ($i = 0; $i -lt $entries.Count; $i++) {
        $entry = $entries[$i]
        $target = Assert-ManagedTarget ([string]$entry.Target) $gameRoot
        $copy = Assert-ManagedTarget ([string]$entry.Copy) $gameRoot
        if (($target -ine $stateRoot -and
             -not $target.StartsWith($redistPrefix, [System.StringComparison]::OrdinalIgnoreCase) -and $target -ine $personalConfiguration) -or
            $target -ieq $root -or
            $target.StartsWith($root + [System.IO.Path]::DirectorySeparatorChar,
                [System.StringComparison]::OrdinalIgnoreCase) -or
            -not $seen.Add($target) -or
            $copy -ine (Join-Path $root ('item-' + $i)) -or
            $entry.Kind -notin @('absent', 'file', 'directory')) {
            throw "Invalid Overture recovery journal entry: $target"
        }
        if($target -ieq $personalConfiguration -and $entry.Kind -eq 'directory'){throw 'A settings recovery target cannot be a directory.'}
        foreach ($prior in $seen) {
            if ($prior -ieq $target) { continue }
            if ($target.StartsWith($prior + [System.IO.Path]::DirectorySeparatorChar,
                    [System.StringComparison]::OrdinalIgnoreCase) -or
                $prior.StartsWith($target + [System.IO.Path]::DirectorySeparatorChar,
                    [System.StringComparison]::OrdinalIgnoreCase)) {
                throw "Overlapping Overture recovery targets: $target and $prior"
            }
        }
        foreach ($candidate in @($entry.MissingParents)) {
            if (-not $candidate) { continue }
            $parent = Assert-ManagedTarget ([string]$candidate) $gameRoot
            if (-not $target.StartsWith($parent + [System.IO.Path]::DirectorySeparatorChar,
                    [System.StringComparison]::OrdinalIgnoreCase)) {
                throw "Invalid Overture recovery parent: $parent"
            }
        }
        if ($entry.Kind -eq 'file' -and
            ((Get-FileHashValue $copy) -ne [string]$entry.SnapshotHash)) {
            throw "Overture recovery copy failed hash verification: $copy"
        }
        if ($entry.Kind -eq 'directory' -and
            (-not (Test-Path -LiteralPath $copy -PathType Container) -or
             (@(Get-SnapshotFiles $copy) -join '|') -cne (@($entry.SnapshotFiles) -join '|'))) {
            throw "Overture recovery directory failed verification: $copy"
        }
        if ($entry.Kind -eq 'absent' -and (Test-Path -LiteralPath $copy)) {
            throw "Unexpected Overture recovery copy: $copy"
        }
    }
    return [pscustomobject]@{ Root = $root; Entries = $entries }
}

function Complete-DeploymentSnapshot($snapshot, [string]$gameRoot) {
    $root = Assert-ManagedTarget $snapshot.Root $gameRoot
    if ($root -ine (Join-Path (Get-NormalizedRoot $gameRoot) '.penumbravr-journal')) {
        throw "Unsafe Overture journal completion path: $root"
    }
    $resolved = Assert-ManagedTarget (Join-Path $gameRoot ('.penumbravr-journal-resolved-' + [guid]::NewGuid().ToString('N'))) $gameRoot
    [System.IO.Directory]::Move($root, $resolved)
    try { Remove-Item -LiteralPath $resolved -Recurse -Force }
    catch { Write-Warning "Completed Overture journal retained for cleanup: $resolved" }
}

function Restore-DeploymentSnapshot($snapshot, [string]$gameRoot) {
    foreach ($entry in @($snapshot.Entries)) {
        $target = Assert-ManagedTarget $entry.Target $gameRoot
        $item = Get-Item -LiteralPath $target -Force -ErrorAction SilentlyContinue
        if ($entry.Kind -eq 'file' -and $item -and -not $item.PSIsContainer -and
            (Get-FileHashValue $target) -eq (Get-FileHashValue $entry.Copy)) { continue }
        if ($entry.Kind -eq 'directory' -and $item -and $item.PSIsContainer -and
            (@(Get-SnapshotFiles $target) -join '|') -ceq (@(Get-SnapshotFiles $entry.Copy) -join '|')) { continue }
        if ($item) {
            if ($item.PSIsContainer -and $entry.Kind -ne 'directory' -and
                $target -ine (Join-Path $gameRoot '.penumbravr')) {
                throw "Rollback target became an unexpected directory: $target"
            }
            if ($item.PSIsContainer) { Remove-Item -LiteralPath $target -Recurse -Force }
            else { Remove-Item -LiteralPath $target -Force }
        }
        if ($entry.Kind -eq 'absent') {
            foreach ($candidate in @($entry.MissingParents)) {
                if (-not $candidate) { continue }
                $directory = Assert-ManagedTarget $candidate $gameRoot
                if (-not (Test-Path -LiteralPath $directory -PathType Container)) { continue }
                if (@(Get-ChildItem -LiteralPath $directory -Force).Count -ne 0) { break }
                Remove-Item -LiteralPath $directory -Force
            }
        }
        if ($entry.Kind -ne 'absent') {
            New-Item -ItemType Directory -Path (Split-Path -Parent $target) -Force | Out-Null
            Copy-Item -LiteralPath $entry.Copy -Destination $target -Recurse -Force
            if ($entry.Kind -eq 'file' -and (Get-FileHashValue $target) -ne (Get-FileHashValue $entry.Copy)) {
                throw "Rollback failed hash verification: $target"
            }
            if ($entry.Kind -eq 'directory' -and
                (@(Get-SnapshotFiles $target) -join '|') -cne (@(Get-SnapshotFiles $entry.Copy) -join '|')) {
                throw "Rollback failed directory verification: $target"
            }
        }
    }
}

function Invoke-DeploymentRollback($snapshot, [string]$gameRoot, [string]$failure) {
    try {
        $verifiedSnapshot = Read-DeploymentSnapshot $gameRoot
        Restore-DeploymentSnapshot $verifiedSnapshot $gameRoot
        Complete-DeploymentSnapshot $verifiedSnapshot $gameRoot
    }
    catch { throw "Deployment failed: $failure. Rollback also failed: $($_.Exception.Message). Snapshot retained: $($snapshot.Root)" }
    throw "Deployment failed and managed files were restored: $failure"
}

if ($Recover) {
    if (-not $InstallRoot) { throw 'Pass -InstallRoot explicitly when recovering an interrupted Overture deployment.' }
    $recoveryRoot = Get-NormalizedRoot $InstallRoot
    if ((Split-Path -Leaf $recoveryRoot) -ieq 'redist') {
        $recoveryRoot = Get-NormalizedRoot (Split-Path -Parent $recoveryRoot)
    }
    if (-not (Test-Path -LiteralPath $recoveryRoot -PathType Container)) {
        throw "Overture recovery game folder is missing: $recoveryRoot"
    }
    $recoverySnapshot = Read-DeploymentSnapshot $recoveryRoot
    Restore-DeploymentSnapshot $recoverySnapshot $recoveryRoot
    Complete-DeploymentSnapshot $recoverySnapshot $recoveryRoot
    Write-Host "Interrupted Overture deployment recovered at $recoveryRoot" -ForegroundColor Green
    return
}

$paths = Resolve-GamePaths $InstallRoot
$gameRoot = $paths.GameRoot
$redistRoot = $paths.RedistRoot
if (Test-Path -LiteralPath (Join-Path $gameRoot '.penumbravr-journal')) {
    throw "An interrupted Overture deployment needs -Recover -InstallRoot '$gameRoot' before another write."
}
if ($ExpectedExecutableSha256) {
    $executablePath = Join-Path $redistRoot 'Penumbra.exe'
    if ((Get-FileHashValue $executablePath) -ine $ExpectedExecutableSha256) {
        throw "Overture executable changed after build selection: $executablePath"
    }
}
$stateRoot = Join-Path $gameRoot '.penumbravr'
$statePath = Join-Path $stateRoot 'deploy-state.json'
$backupRoot = Join-Path $stateRoot 'backup'

$previousState = $null
if (Test-Path -LiteralPath $statePath -PathType Leaf) {
    $previousState = Get-Content -Raw -LiteralPath $statePath | ConvertFrom-Json
    if ([int]$previousState.Version -ne 1) {
        throw "Unsupported deployment state version in $statePath"
    }
    if ((Get-NormalizedRoot ([string]$previousState.RedistRoot)) -ne $redistRoot) {
        throw 'Deployment state belongs to a different Penumbra installation.'
    }
}

if ($Repair -and $null -eq $previousState) {
    throw 'Overture repair requires an existing managed deployment state.'
}
$configurationProperty=if($previousState){$previousState.PSObject.Properties['Configuration']}else{$null}
$previousConfiguration=if($configurationProperty){@($configurationProperty.Value)}else{@()}
$previousConfiguration=@(Get-PvrConfigurationRecords -Game overture -RedistRoot $redistRoot -Previous $previousConfiguration)

if ($Restore) {
    if ($null -eq $previousState) {
        throw "No Penumbra VR deployment state was found under $stateRoot"
    }

    # Validate the whole managed set before the first restore write. A later
    # external edit must not leave earlier files already restored.
    foreach ($entry in $previousState.Files) {
        $relativePath = [string]$entry.Path
        $targetPath = Get-PathUnderRoot $redistRoot $relativePath
        $targetHash = Get-FileHashValue $targetPath
        $deployedHash = [string]$entry.DeployedHash
        if ([bool]$entry.HadOriginal) {
            $backupPath = Get-PathUnderRoot $backupRoot $relativePath
            if ((Get-FileHashValue $backupPath) -ne [string]$entry.OriginalHash) {
                throw "Cannot restore '$relativePath'; its original backup is missing or changed."
            }
            if ($targetHash -and $targetHash -ne $deployedHash -and
                $targetHash -ne [string]$entry.OriginalHash) {
                throw "Refusing to overwrite externally modified managed file '$targetPath'."
            }
        }
        elseif ($targetHash -and $targetHash -ne $deployedHash) {
            throw "Refusing to delete externally modified managed file '$targetPath'."
        }
    }

    $restoreTargets = @($previousState.Files | ForEach-Object {
        Get-PathUnderRoot $redistRoot ([string]$_.Path)
    }) + @($stateRoot) + @($previousConfiguration | ForEach-Object {$_.Path})
    foreach($plan in $previousConfiguration){Test-PvrConfigurationPlan -Plan $plan -Restore}
    if($Preflight) { [pscustomobject]@{Files=@($restoreTargets);Configuration=@($previousConfiguration);Operation='restore'}; return }
    $deploymentSnapshot = New-DeploymentSnapshot $restoreTargets $gameRoot
    try {
    foreach($plan in $previousConfiguration){Restore-PvrConfiguration -Plan $plan}
    foreach ($entry in $previousState.Files) {
        $relativePath = [string]$entry.Path
        $targetPath = Get-PathUnderRoot $redistRoot $relativePath
        $targetHash = Get-FileHashValue $targetPath
        $deployedHash = [string]$entry.DeployedHash

        if ([bool]$entry.HadOriginal) {
            $backupPath = Get-PathUnderRoot $backupRoot $relativePath
            if (-not (Test-Path -LiteralPath $backupPath -PathType Leaf)) {
                throw "Cannot restore '$relativePath'; its original backup is missing."
            }
            $originalHash = [string]$entry.OriginalHash
            if ((Get-FileHashValue $backupPath) -ne $originalHash) {
                throw "Cannot restore '$relativePath'; its original backup failed hash validation."
            }
            if ($targetHash -and $targetHash -ne $deployedHash -and $targetHash -ne $originalHash) {
                throw "Refusing to overwrite externally modified managed file '$targetPath'."
            }
            $targetDirectory = Split-Path -Parent $targetPath
            New-Item -ItemType Directory -Path $targetDirectory -Force | Out-Null
            Copy-Item -LiteralPath $backupPath -Destination $targetPath -Force
            if ((Get-FileHashValue $targetPath) -ne $originalHash) {
                throw "Restored original failed hash verification: $relativePath"
            }
            Write-Host "Restored original: $relativePath"
        }
        elseif ($targetHash) {
            if ($targetHash -ne $deployedHash) {
                throw "Refusing to delete externally modified managed file '$targetPath'."
            }
            Remove-Item -LiteralPath $targetPath -Force
            Remove-EmptyManagedParents $targetPath $redistRoot
            Write-Host "Removed mod file: $relativePath"
        }
    }

    $resolvedStateRoot = Get-NormalizedRoot $stateRoot
    $expectedStatePrefix = $gameRoot + [System.IO.Path]::DirectorySeparatorChar
    if (-not $resolvedStateRoot.StartsWith($expectedStatePrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to remove deployment state outside the game root: $resolvedStateRoot"
    }
    Remove-Item -LiteralPath $resolvedStateRoot -Recurse -Force
    } catch {
        Invoke-DeploymentRollback $deploymentSnapshot $gameRoot $_.Exception.Message
    }
    Complete-DeploymentSnapshot $deploymentSnapshot $gameRoot
    Write-Host "Penumbra VR restored to its pre-deployment state at $redistRoot" -ForegroundColor Green
    return
}

$resolvedPackageRoot = Resolve-PackageRoot $PackageRoot $Configuration
$mappings = @{}
Get-ChildItem -LiteralPath $resolvedPackageRoot -File -Recurse | ForEach-Object {
    $relativePath = (Get-RelativePath $resolvedPackageRoot $_.FullName).Replace('\', '/')
    $mappings[$relativePath] = [pscustomobject]@{
        Path = $relativePath
        SourcePath = $_.FullName
    }
}

if ($SteamLauncher) {
    $mappings['Penumbra.exe'] = [pscustomobject]@{
        Path = 'Penumbra.exe'
        SourcePath = Join-Path $resolvedPackageRoot 'Penumbra_vr.exe'
    }
}
if(-not $CommunityTranslations) { $mappings.Remove('config/Espanol.lang') }
if($Repair) {
    # Repair the recorded component set; the GUI's optional defaults must not
    # change ownership or accidentally adopt newly introduced package files.
    $recorded=@{}
    foreach($entry in $previousState.Files) { $recorded[[string]$entry.Path]=$true }
    foreach($key in @($mappings.Keys)) { if(-not $recorded.ContainsKey($key)) { $mappings.Remove($key) } }
    if($recorded.ContainsKey('config/Espanol.lang')) { $mappings['config/Espanol.lang']=[pscustomobject]@{Path='config/Espanol.lang';SourcePath=(Join-Path $resolvedPackageRoot 'config/Espanol.lang')} }
}

$textureManifest = Join-Path $resolvedPackageRoot 'docs\TEXTURE_SELECTION.json'
$selectedTexturePaths=@()
if(Test-Path -LiteralPath $textureManifest){
    $selection = Get-Content -Raw -LiteralPath $textureManifest | ConvertFrom-Json
    if ($selection.Version -notin @(1,2)) { throw 'Unsupported texture selection manifest.' }
    $selectedTexturePaths=@($selection.Files.Path)
}elseif($SkipTexturePack){throw 'Texture selection manifest is missing.'}
if ($SkipTexturePack) {
    foreach ($texture in $selection.Files) {
        $texturePath = [string]$texture.Path
        if ($texturePath -notmatch '^(textures|models)/([a-z0-9_]+/)+[a-z0-9_]+\.jpg$' -or
            $texturePath -match '^models/(hud_objects|items|player)/') { throw 'Invalid or protected selected texture path.' }
        $mappings.Remove($texturePath)
    }
    Write-Host 'Selected texture pack skipped; previous managed copies will be restored from backup.'
}

$previousEntries = @{}
if ($null -ne $previousState) {
    foreach ($entry in $previousState.Files) {
        $previousEntries[[string]$entry.Path] = $entry
    }
}

# Validate the complete previous deployment before retiring or replacing any
# file. Steam/user edits are not silently adopted as new originals.
if ($Repair) {
    if ($mappings.Count -ne $previousEntries.Count) {
        throw 'Overture repair requires the same managed file set as the recorded deployment.'
    }
    foreach ($entry in $previousEntries.Values) {
        if ([string]$entry.DeployedHash -notmatch '^[0-9A-Fa-f]{64}$' -or
            ([bool]$entry.HadOriginal -and [string]$entry.OriginalHash -notmatch '^[0-9A-Fa-f]{64}$')) {
            throw "Overture repair requires recorded hashes for '$($entry.Path)'."
        }
    }
    foreach ($relativePath in $mappings.Keys) {
        if (-not $previousEntries.ContainsKey($relativePath)) {
            throw "Overture repair cannot introduce an unrecorded managed path: $relativePath"
        }
    }
}
foreach ($entry in $previousEntries.Values) {
    $relativePath = [string]$entry.Path
    $targetPath = Get-PathUnderRoot $redistRoot $relativePath
    $targetHash = Get-FileHashValue $targetPath
    $deployedHash = [string]$entry.DeployedHash
    if ([bool]$entry.HadOriginal) {
        $backupPath = Get-PathUnderRoot $backupRoot $relativePath
        if ((Get-FileHashValue $backupPath) -ne [string]$entry.OriginalHash) {
            throw "Cannot upgrade '$relativePath'; its original backup is missing or changed."
        }
        if (-not $Repair -and $targetHash -and $targetHash -ne $deployedHash -and
            $targetHash -ne [string]$entry.OriginalHash) {
            throw "Refusing to overwrite externally modified managed file '$targetPath'."
        }
    }
    elseif (-not $Repair -and $targetHash -and $targetHash -ne $deployedHash) {
        throw "Refusing to overwrite externally modified managed file '$targetPath'."
    }
}

# Restore or remove only files recorded by the previous deployment. Never mirror
# the whole redist directory: nearly all files there belong to the commercial game.
$installTargets = New-Object System.Collections.Generic.List[string]
foreach ($relativePath in @($previousEntries.Keys) + @($mappings.Keys)) {
    $installTargets.Add((Get-PathUnderRoot $redistRoot ([string]$relativePath)))
}
$installTargets.Add($stateRoot)
$configurationPlans=@(Get-PvrConfigurationRecords -Game overture -RedistRoot $redistRoot -Previous $previousConfiguration -Apply:$RecommendedSettings -SettingsScope $SettingsScope)
if($RecommendedSettings){foreach($plan in $configurationPlans){Test-PvrConfigurationPlan -Plan $plan}}
foreach($plan in $configurationPlans){$installTargets.Add([string]$plan.Path)}
foreach($path in $installTargets){Assert-ManagedTarget $path $gameRoot | Out-Null}
if($Preflight) { [pscustomobject]@{Files=@($mappings.Keys | Sort-Object);RetiredFiles=@($previousEntries.Keys | Where-Object {-not $mappings.ContainsKey($_)});Configuration=@($configurationPlans);Operation=$(if($Repair){'repair'}else{'install'})}; return }
$deploymentSnapshot = New-DeploymentSnapshot $installTargets.ToArray() $gameRoot
try {
if($RecommendedSettings){foreach($plan in $configurationPlans){Set-PvrConfiguration -Plan $plan}}
foreach ($previousPath in @($previousEntries.Keys)) {
    if ($mappings.ContainsKey($previousPath)) {
        continue
    }

    $entry = $previousEntries[$previousPath]
    $targetPath = Get-PathUnderRoot $redistRoot $previousPath
    $targetHash = Get-FileHashValue $targetPath
    $deployedHash = [string]$entry.DeployedHash

    if ([bool]$entry.HadOriginal) {
        $backupPath = Get-PathUnderRoot $backupRoot $previousPath
        if (-not (Test-Path -LiteralPath $backupPath -PathType Leaf)) {
            throw "Cannot retire '$previousPath'; its original backup is missing."
        }
        $originalHash = [string]$entry.OriginalHash
        if ((Get-FileHashValue $backupPath) -ne $originalHash) {
            throw "Cannot retire '$previousPath'; its original backup failed hash validation."
        }
        if ($targetHash -and $targetHash -ne $deployedHash -and $targetHash -ne $originalHash) {
            throw "Refusing to overwrite externally modified stale file '$targetPath'."
        }
        $targetDirectory = Split-Path -Parent $targetPath
        New-Item -ItemType Directory -Path $targetDirectory -Force | Out-Null
        Copy-Item -LiteralPath $backupPath -Destination $targetPath -Force
        if ((Get-FileHashValue $targetPath) -ne $originalHash) {
            throw "Retired original failed hash verification: $previousPath"
        }
        Write-Host "Restored retired original: $previousPath"
    }
    elseif ($targetHash) {
        if ($targetHash -ne $deployedHash) {
            throw "Refusing to delete externally modified stale file '$targetPath'."
        }
        Remove-Item -LiteralPath $targetPath -Force
        Remove-EmptyManagedParents $targetPath $redistRoot
        Write-Host "Removed stale mod file: $previousPath"
    }
}

New-Item -ItemType Directory -Path $backupRoot -Force | Out-Null
$newEntries = New-Object System.Collections.Generic.List[object]
foreach ($mapping in @($mappings.Values | Sort-Object Path)) {
    $relativePath = [string]$mapping.Path
    $sourcePath = [string]$mapping.SourcePath
    $targetPath = Get-PathUnderRoot $redistRoot $relativePath
    $targetDirectory = Split-Path -Parent $targetPath
    $previousEntry = $previousEntries[$relativePath]

    if ($null -ne $previousEntry) {
        $hadOriginal = [bool]$previousEntry.HadOriginal
        $originalHash = if ($hadOriginal) { [string]$previousEntry.OriginalHash } else { $null }
    }
    else {
        $hadOriginal = Test-Path -LiteralPath $targetPath -PathType Leaf
        $originalHash = if ($hadOriginal) { Get-FileHashValue $targetPath } else { $null }
        if ($hadOriginal) {
            $backupPath = Get-PathUnderRoot $backupRoot $relativePath
            $backupDirectory = Split-Path -Parent $backupPath
            New-Item -ItemType Directory -Path $backupDirectory -Force | Out-Null
            Copy-Item -LiteralPath $targetPath -Destination $backupPath -Force
            if ((Get-FileHashValue $backupPath) -ne $originalHash) {
                throw "Original backup failed hash verification: $relativePath"
            }
            Write-Host "Backed up original: $relativePath"
        }
    }

    New-Item -ItemType Directory -Path $targetDirectory -Force | Out-Null
    Copy-Item -LiteralPath $sourcePath -Destination $targetPath -Force
    $deployedHash = Get-FileHashValue $targetPath
    if ($deployedHash -ne (Get-FileHashValue $sourcePath)) {
        throw "Deployed file failed hash verification: $relativePath"
    }
    $newEntries.Add([pscustomobject]@{
        Path = $relativePath
        HadOriginal = $hadOriginal
        OriginalHash = $originalHash
        DeployedHash = $deployedHash
    })
}

New-Item -ItemType Directory -Path $stateRoot -Force | Out-Null
$newState = [ordered]@{
    Version = 1
    FrameworkVersion = (Get-Content -LiteralPath (Join-Path $resolvedPackageRoot 'release.json') -Raw | ConvertFrom-Json).version
    Components = @('shared','overture') + @(if($mappings.ContainsKey('config/Espanol.lang')){'spanish_translation'}) + @(if(@($mappings.Keys | Where-Object {$_ -in $selectedTexturePaths}).Count){'texture_enhancements'})
    Configuration = @($configurationPlans)
    RedistRoot = $redistRoot
    PackageRoot = $resolvedPackageRoot
    SteamLauncher = $SteamLauncher
    DeployedAt = (Get-Date).ToString('o')
    Files = $newEntries.ToArray()
}
$temporaryStatePath = Join-Path $stateRoot 'deploy-state.json.tmp'
$newState | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $temporaryStatePath -Encoding utf8
Move-Item -LiteralPath $temporaryStatePath -Destination $statePath -Force
} catch {
    Invoke-DeploymentRollback $deploymentSnapshot $gameRoot $_.Exception.Message
}
Complete-DeploymentSnapshot $deploymentSnapshot $gameRoot

Write-Host "Penumbra VR synchronized to $redistRoot" -ForegroundColor Green
if ($SteamLauncher) {
    Write-Host 'Steam launcher integration is active: Steam will start the VR executable through Penumbra.exe.' -ForegroundColor Green
}
