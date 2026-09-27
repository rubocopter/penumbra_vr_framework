[CmdletBinding()]
param(
    [string]$SteamRoot,
    [string[]]$ManualPaths = @()
)

$ErrorActionPreference = 'Stop'
if (-not $SteamRoot) {
    $SteamRoot = Join-Path ${env:ProgramFiles(x86)} 'Steam'
}
$buildInfoScript = Join-Path $PSScriptRoot 'Get-PenumbraBuildInfo.ps1'
$seen = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
$candidates = [System.Collections.Generic.List[object]]::new()

function Add-Candidate([string]$Path, [string]$Source) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return }
    $absolute = [System.IO.Path]::GetFullPath($Path)
    if ($seen.Add($absolute)) {
        $candidates.Add([PSCustomObject]@{ Path = $absolute; Source = $Source })
    }
}

function Add-FromFolder([string]$Folder, [string]$Source) {
    foreach ($relative in @('Penumbra.exe', 'Requiem.exe',
                           'redist/Penumbra.exe', 'redist/Requiem.exe')) {
        Add-Candidate (Join-Path $Folder $relative) $Source
    }
}

$libraries = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
if (Test-Path -LiteralPath $SteamRoot -PathType Container) {
    [void]$libraries.Add([System.IO.Path]::GetFullPath($SteamRoot))
    $libraryFile = Join-Path $SteamRoot 'steamapps/libraryfolders.vdf'
    if (Test-Path -LiteralPath $libraryFile -PathType Leaf) {
        $contents = Get-Content -LiteralPath $libraryFile -Raw
        foreach ($match in [regex]::Matches($contents, '(?m)"path"\s*"([^"]+)"')) {
            $library = $match.Groups[1].Value.Replace('\\', '\')
            if (Test-Path -LiteralPath $library -PathType Container) {
                [void]$libraries.Add([System.IO.Path]::GetFullPath($library))
            }
        }
    }
}
foreach ($library in @($libraries | Sort-Object)) {
    $common = Join-Path $library 'steamapps/common'
    foreach ($folder in @('Penumbra Overture', 'Penumbra Black Plague')) {
        Add-FromFolder (Join-Path $common $folder) 'Steam'
    }
}
foreach ($manual in $ManualPaths) {
    if (-not (Test-Path -LiteralPath $manual)) {
        throw "Manual path does not exist: $manual"
    }
    if (Test-Path -LiteralPath $manual -PathType Leaf) {
        Add-Candidate $manual 'Manual'
    } else {
        Add-FromFolder $manual 'Manual'
    }
}

foreach ($candidate in @($candidates | Sort-Object Path)) {
    $probeError = $null
    try {
        $info = & $buildInfoScript -Path $candidate.Path
    } catch {
        $probeError = $_.Exception.Message
        $info = [PSCustomObject]@{
            Game = 'Unknown'
            KnownBuild = $false
            BuildId = 'unknown'
            Variant = 'unknown'
            SHA256 = $null
        }
    }
    $parent = Split-Path -Parent $candidate.Path
    $installRoot = if ((Split-Path -Leaf $parent) -ieq 'redist') {
        Split-Path -Parent $parent
    } else { $parent }
    [PSCustomObject][ordered]@{
        Path = $candidate.Path
        InstallRoot = $installRoot
        Source = $candidate.Source
        Game = $info.Game
        KnownBuild = $info.KnownBuild
        BuildId = $info.BuildId
        Variant = $info.Variant
        SHA256 = $info.SHA256
        Installable = ($info.KnownBuild -and $info.Game -eq 'Black Plague')
        ProbeError = $probeError
    }
}
