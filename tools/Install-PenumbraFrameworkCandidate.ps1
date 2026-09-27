[CmdletBinding()]
param(
    [ValidateSet('Overture', 'BlackPlague', 'Requiem')][string]$Game,
    [string]$GamePath,
    [string]$SteamRoot,
    [string[]]$ManualPaths = @(),
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
        & $overtureInstaller -InstallRoot $gameRoot -Recover
    } else {
        $executable = if ($leaf -ieq 'Penumbra.exe') { $requested }
            elseif ($leaf -ieq 'redist') { Join-Path $requested 'Penumbra.exe' }
            else { Join-Path $requested 'redist/Penumbra.exe' }
        & $blackPlagueInstaller -GamePath $executable -Recover
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
        & $overtureInstaller -InstallRoot $gameRoot -PackageRoot (Join-Path $packageRoot 'products/overture') -Repair
    } else {
        $executable = if ($leaf -ieq 'Penumbra.exe') { $requested }
            elseif ($leaf -ieq 'redist') { Join-Path $requested 'Penumbra.exe' }
            else { Join-Path $requested 'redist/Penumbra.exe' }
        & $blackPlagueInstaller -GamePath $executable -BuildRoot (Join-Path $packageRoot 'products/black_plague/build') -Repair
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
    $requestedPath = [System.IO.Path]::GetFullPath($GamePath)
    $choices = @($choices | Where-Object { $_.Path -ieq $requestedPath })
}
if (-not $choices.Count) {
    throw 'No compatible candidate matched the requested game and path. Unknown builds are never modified.'
}
if ($choices.Count -eq 1) {
    $selected = $choices[0]
} else {
    $answer = Read-Host 'Enter the number of the game to modify'
    $number = 0
    if (-not [int]::TryParse($answer, [ref]$number) -or
        $number -lt 1 -or $number -gt $found.Count) {
        throw 'Invalid selection; no game was modified.'
    }
    $selected = $found[$number - 1]
    if (-not @($choices | Where-Object { $_.Path -ieq $selected.Path }).Count) {
        throw 'Selected game is not compatible with this candidate.'
    }
}

Write-Host ("{0} {1}: {2}" -f $(if ($Restore) { 'Restoring' } else { 'Installing' }),
    $selected.Game, $selected.Path)
if ($selected.Game -eq 'Black Plague') {
    $args = @{ GamePath = $selected.Path; BuildRoot = Join-Path $packageRoot 'products/black_plague/build' }
    if ($Restore) { $args.Restore = $true }
    if ($LargeAddressAware) { $args.LargeAddressAware = $true }
    & $blackPlagueInstaller @args
} elseif ($selected.Game -eq 'Overture') {
    if ($LargeAddressAware) {
        throw 'The LAA transform option applies only to the exact-build Black Plague candidate.'
    }
    $args = @{
        InstallRoot = $selected.InstallRoot
        PackageRoot = Join-Path $packageRoot 'products/overture'
        ExpectedExecutableSha256 = $selected.SHA256
    }
    if ($Restore) { $args.Restore = $true }
    & $overtureInstaller @args
}
