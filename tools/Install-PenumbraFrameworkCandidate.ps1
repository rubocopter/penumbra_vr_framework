[CmdletBinding()]
param(
    [ValidateSet('Overture', 'BlackPlague', 'Requiem')][string]$Game,
    [string]$GamePath,
    [string]$SteamRoot,
    [string[]]$ManualPaths = @(),
    [string[]]$Selections,
    [string]$LogPath,
    [switch]$LargeAddressAware,
    [switch]$Restore,
    [switch]$Recover,
    [switch]$Repair,
    [switch]$List
)

$ErrorActionPreference = 'Stop'
$packageRoot = Split-Path -Parent $PSScriptRoot
$discovery = Join-Path $PSScriptRoot 'Get-PenumbraInstallations.ps1'
$overtureInstaller = Join-Path $packageRoot 'products/overture/Install-PenumbraVR.ps1'
$blackPlagueInstaller = Join-Path $packageRoot 'products/black_plague/tools/Install-BlackPlagueSteamBootstrap.ps1'
foreach ($required in @($discovery, $overtureInstaller, $blackPlagueInstaller)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Candidate package is incomplete: $required"
    }
}

function Assert-PackageIntegrity([string]$root) {
    $manifest = Join-Path $root 'SHA256SUMS.txt'
    if (-not (Test-Path -LiteralPath $manifest -PathType Leaf)) {
        throw "Candidate package checksum manifest is missing: $manifest"
    }
    $expected = [System.Collections.Generic.Dictionary[string,string]]::new(
        [System.StringComparer]::OrdinalIgnoreCase)
    $prefix = [System.IO.Path]::GetFullPath($root).TrimEnd('\') + '\'
    foreach ($line in @(Get-Content -LiteralPath $manifest)) {
        if ($line -notmatch '^([0-9A-Fa-f]{64})  (.+)$') {
            throw "Invalid candidate package checksum line: $line"
        }
        $relative = $Matches[2]
        if ([System.IO.Path]::IsPathRooted($relative) -or
            $relative -match '(^|[\\/])\.\.([\\/]|$)') {
            throw "Unsafe candidate package path: $relative"
        }
        $target = [System.IO.Path]::GetFullPath((Join-Path $root $relative))
        if (-not $target.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase) -or
            $expected.ContainsKey($relative)) {
            throw "Invalid or duplicate candidate package path: $relative"
        }
        $expected.Add($relative.Replace('\', '/'), $Matches[1].ToUpperInvariant())
    }
    $files = @(Get-ChildItem -LiteralPath $root -Recurse -File)
    if ($expected.Count -ne ($files.Count - 1)) {
        throw 'Candidate package file count differs from SHA256SUMS.txt.'
    }
    foreach ($file in $files) {
        if ($file.FullName -ieq $manifest) { continue }
        if ($file.Attributes -band [System.IO.FileAttributes]::ReparsePoint) {
            throw "Candidate package path is a reparse point: $($file.FullName)"
        }
        $relative = $file.FullName.Substring($root.Length).TrimStart('\').Replace('\', '/')
        $hash = $null
        if (-not $expected.TryGetValue($relative, [ref]$hash) -or
            $hash -ne (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash) {
            throw "Candidate package hash mismatch: $relative"
        }
    }
}

Assert-PackageIntegrity $packageRoot
$packageManifestHash = (Get-FileHash -LiteralPath (Join-Path $packageRoot 'SHA256SUMS.txt') -Algorithm SHA256).Hash
if (-not $LogPath) {
    $localAppData = [Environment]::GetFolderPath('LocalApplicationData')
    if (-not $localAppData) { throw 'Cannot resolve a local installer log directory.' }
    $LogPath = Join-Path $localAppData 'PenumbraVR/installer.jsonl'
}
$LogPath = [System.IO.Path]::GetFullPath($LogPath)

function Write-InstallerEvent([string]$operation, [string]$game,
    [string]$targetPath, [string]$result, [string]$detail = '') {
    $record = [ordered]@{
        timeUtc = [DateTime]::UtcNow.ToString('o')
        packageManifestSha256 = $packageManifestHash
        operation = $operation
        game = $game
        path = $targetPath
        result = $result
    }
    if ($detail) { $record.detail = $detail }
    $directory = Split-Path -Parent $LogPath
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
    [System.IO.File]::AppendAllText($LogPath,
        (($record | ConvertTo-Json -Compress) + "`n"),
        [System.Text.UTF8Encoding]::new($false))
}

function Invoke-LoggedInstaller([string]$operation, [string]$game,
    [string]$targetPath, [string]$installer, [hashtable]$installerArgs) {
    Write-InstallerEvent $operation $game $targetPath 'started'
    try {
        & $installer @installerArgs
    } catch {
        $failure = $_
        try { Write-InstallerEvent $operation $game $targetPath 'failed' $failure.Exception.Message }
        catch { Write-Warning "Installer also failed to write log $LogPath`: $($_.Exception.Message)" }
        throw $failure
    }
    try { Write-InstallerEvent $operation $game $targetPath 'completed' }
    catch { Write-Warning "Installer completed but failed to write log $LogPath`: $($_.Exception.Message)" }
}

if ($PSBoundParameters.ContainsKey('Selections') -and ($Recover -or $Repair -or $List)) {
    throw '-Selections applies only to install or restore; use an explicit game and path for repair or recovery.'
}

if ($Recover) {
    if ($Game -notin @('Overture', 'BlackPlague') -or -not $GamePath -or $Restore -or
        $LargeAddressAware -or $List -or $Repair) {
        throw 'Recovery requires -Game Overture or -Game BlackPlague and an explicit -GamePath; omit other operation switches.'
    }
    $requested = [System.IO.Path]::GetFullPath($GamePath)
    $leaf = Split-Path -Leaf $requested
    if ($Game -eq 'Overture') {
        $gameRoot = if ($leaf -ieq 'Penumbra.exe') {
            Split-Path -Parent (Split-Path -Parent $requested)
        } elseif ($leaf -ieq 'redist') {
            Split-Path -Parent $requested
        } else { $requested }
        Invoke-LoggedInstaller 'recover' 'Overture' $gameRoot $overtureInstaller @{
            InstallRoot = $gameRoot; Recover = $true
        }
    } else {
        $executable = if ($leaf -ieq 'Penumbra.exe') { $requested }
            elseif ($leaf -ieq 'redist') { Join-Path $requested 'Penumbra.exe' }
            else { Join-Path $requested 'redist/Penumbra.exe' }
        Invoke-LoggedInstaller 'recover' 'Black Plague' $executable $blackPlagueInstaller @{
            GamePath = $executable; Recover = $true
        }
    }
    return
}

if ($Repair) {
    if ($Game -notin @('Overture', 'BlackPlague') -or -not $GamePath -or $Restore -or
        $LargeAddressAware -or $List) {
        throw 'Repair requires -Game Overture or -Game BlackPlague and an explicit -GamePath; omit other operation switches.'
    }
    $requested = [System.IO.Path]::GetFullPath($GamePath)
    $leaf = Split-Path -Leaf $requested
    if ($Game -eq 'Overture') {
        $gameRoot = if ($leaf -ieq 'Penumbra.exe') {
            Split-Path -Parent (Split-Path -Parent $requested)
        } elseif ($leaf -ieq 'redist') {
            Split-Path -Parent $requested
        } else { $requested }
        Invoke-LoggedInstaller 'repair' 'Overture' $gameRoot $overtureInstaller @{
            InstallRoot = $gameRoot
            PackageRoot = Join-Path $packageRoot 'products/overture'
            Repair = $true
        }
    } else {
        $executable = if ($leaf -ieq 'Penumbra.exe') { $requested }
            elseif ($leaf -ieq 'redist') { Join-Path $requested 'Penumbra.exe' }
            else { Join-Path $requested 'redist/Penumbra.exe' }
        Invoke-LoggedInstaller 'repair' 'Black Plague' $executable $blackPlagueInstaller @{
            GamePath = $executable
            BuildRoot = Join-Path $packageRoot 'products/black_plague/build'
            Repair = $true
        }
    }
    return
}

$discoveryArgs = @{}
if ($SteamRoot) { $discoveryArgs.SteamRoot = $SteamRoot }
if ($ManualPaths.Count) { $discoveryArgs.ManualPaths = $ManualPaths }
if ($GamePath) { $discoveryArgs.ManualPaths = @($ManualPaths) + @($GamePath) }
$found = @(& $discovery @discoveryArgs)

function Test-ManagedOverture($Installation) {
    if ((Split-Path -Leaf $Installation.Path) -ine 'Penumbra.exe') { return $false }
    $statePath = Join-Path $Installation.InstallRoot '.penumbravr/deploy-state.json'
    if (-not (Test-Path -LiteralPath $statePath -PathType Leaf)) { return $false }
    try {
        $state = Get-Content -LiteralPath $statePath -Raw | ConvertFrom-Json
        $expectedRedist = [System.IO.Path]::GetFullPath((Join-Path $Installation.InstallRoot 'redist')).TrimEnd('\')
        if ($state.Version -ne 1 -or
            [System.IO.Path]::GetFullPath([string]$state.RedistRoot).TrimEnd('\') -ine $expectedRedist) {
            return $false
        }
        $launcher = @($state.Files | Where-Object { $_.Path -eq 'Penumbra.exe' })
        return ($launcher.Count -eq 1 -and
            [string]$launcher[0].DeployedHash -ieq [string]$Installation.SHA256)
    } catch { return $false }
}

foreach ($entry in $found) {
    if ($entry.Game -eq 'Overture' -and $entry.KnownBuild) {
        $entry.Installable = $true
    } elseif ($entry.Game -eq 'Unknown' -and (Test-ManagedOverture $entry)) {
        $entry.Game = 'Overture'
        $entry.KnownBuild = $true
        $entry.BuildId = 'overture-framework-managed'
        $entry.Installable = $true
    }
}

Write-Host 'Detected Penumbra installations:'
if (-not $found.Count) { Write-Host '  None. Pass -ManualPaths or -GamePath.' }
for ($i = 0; $i -lt $found.Count; $i++) {
    $entry = $found[$i]
    $status = if ($entry.Installable) { 'candidate available' } else { 'not installable' }
    Write-Host ("  [{0}] {1} ({2}) {3}" -f ($i + 1), $entry.Game, $status, $entry.Path)
    if ($entry.ProbeError) { Write-Host ("      {0}" -f $entry.ProbeError) }
}
if ($List) { return }

if ($Game -eq 'Requiem') {
    throw 'Requiem is recognized but has no Framework gameplay VR backend or installable candidate yet.'
}
$choices = @($found | Where-Object { $_.Installable -and $_.Game -in @('Overture', 'Black Plague') })
if ($Game -eq 'Overture') { $choices = @($choices | Where-Object { $_.Game -eq 'Overture' }) }
if ($Game -eq 'BlackPlague') { $choices = @($choices | Where-Object { $_.Game -eq 'Black Plague' }) }
if ($GamePath) {
    $requestedPath = [System.IO.Path]::GetFullPath($GamePath).TrimEnd('\', '/')
    $choices = @($choices | Where-Object {
        $_.Path.TrimEnd('\', '/') -ieq $requestedPath -or
        $_.InstallRoot.TrimEnd('\', '/') -ieq $requestedPath -or
        (Split-Path -Parent $_.Path).TrimEnd('\', '/') -ieq $requestedPath
    })
}
if (-not $choices.Count) {
    throw 'No compatible candidate matched the requested game and path. Unknown builds are never modified.'
}
$selectedEntries = @()
if ($choices.Count -eq 1 -and -not $PSBoundParameters.ContainsKey('Selections')) {
    $selectedEntries = @($choices[0])
} else {
    $rawSelections = if ($PSBoundParameters.ContainsKey('Selections')) {
        @($Selections)
    } else {
        @(Read-Host 'Enter one or more game numbers, separated by commas')
    }
    $numbers = @()
    foreach ($selection in $rawSelections) {
        foreach ($piece in @($selection -split ',')) {
            $number = 0
            if (-not [int]::TryParse($piece.Trim(), [ref]$number)) {
                throw 'Invalid selection; no game was modified.'
            }
            $numbers += $number
        }
    }
    if (-not $numbers.Count) {
        throw 'Select at least one game; no game was modified.'
    }
    $seenNumbers = [System.Collections.Generic.HashSet[int]]::new()
    foreach ($number in $numbers) {
        if ($number -lt 1 -or $number -gt $found.Count -or
            -not $seenNumbers.Add($number)) {
            throw 'Invalid or duplicate selection; no game was modified.'
        }
        $candidate = $found[$number - 1]
        if (-not @($choices | Where-Object { $_.Path -ieq $candidate.Path }).Count) {
            throw 'Selected game is not compatible with this candidate; no game was modified.'
        }
        $selectedEntries += $candidate
    }
}
if ($LargeAddressAware -and @($selectedEntries | Where-Object { $_.Game -eq 'Overture' }).Count) {
    throw 'The LAA transform option applies only to the exact-build Black Plague candidate.'
}

foreach ($selected in $selectedEntries) {
    Write-Host ("{0} {1}: {2}" -f $(if ($Restore) { 'Restoring' } else { 'Installing' }),
        $selected.Game, $selected.Path)
    try {
        if ($selected.Game -eq 'Black Plague') {
            $installerArgs = @{
                GamePath = $selected.Path
                BuildRoot = Join-Path $packageRoot 'products/black_plague/build'
            }
            if ($Restore) { $installerArgs.Restore = $true }
            if ($LargeAddressAware) { $installerArgs.LargeAddressAware = $true }
            Invoke-LoggedInstaller $(if ($Restore) { 'restore' } else { 'install' }) `
                'Black Plague' $selected.Path $blackPlagueInstaller $installerArgs
        } elseif ($selected.Game -eq 'Overture') {
            $installerArgs = @{
                InstallRoot = $selected.InstallRoot
                PackageRoot = Join-Path $packageRoot 'products/overture'
                ExpectedExecutableSha256 = $selected.SHA256
            }
            if ($Restore) { $installerArgs.Restore = $true }
            Invoke-LoggedInstaller $(if ($Restore) { 'restore' } else { 'install' }) `
                'Overture' $selected.Path $overtureInstaller $installerArgs
        }
    } catch {
        throw "Framework candidate stopped at $($selected.Game) ($($selected.Path)); earlier selections may have completed independently: $($_.Exception.Message)"
    }
}
