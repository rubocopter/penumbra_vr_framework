[CmdletBinding()]
param(
    [string]$SteamRoot,
    [string[]]$ManualPaths = @()
)

$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'PenumbraVrPrerequisites.psm1') -Force
if (-not $SteamRoot) {
    foreach($key in @('HKCU:\Software\Valve\Steam','HKLM:\SOFTWARE\WOW6432Node\Valve\Steam')) {
        $registry=Get-ItemProperty -LiteralPath $key -ErrorAction SilentlyContinue
        if($registry.SteamPath) { $SteamRoot=$registry.SteamPath; break }
        if($registry.InstallPath) { $SteamRoot=$registry.InstallPath; break }
    }
    if(-not $SteamRoot) { $SteamRoot = Join-Path ${env:ProgramFiles(x86)} 'Steam' }
}
$buildInfoScript = Join-Path $PSScriptRoot 'Get-PenumbraBuildInfo.ps1'
$seen = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
$candidates = [System.Collections.Generic.List[object]]::new()

function Add-Candidate([string]$Path, [string]$Source, [bool]$AllowMissing=$false) {
    if (-not $AllowMissing -and -not (Test-Path -LiteralPath $Path -PathType Leaf)) { return }
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
    if((Test-Path -LiteralPath (Join-Path $Folder '.penumbravr/deploy-state.json')) -or
       (Test-Path -LiteralPath (Join-Path $Folder '.penumbravr-journal')) -or
       (Test-Path -LiteralPath (Join-Path $Folder 'redist/PenumbraVR.BlackPlague.install.json')) -or
       (Test-Path -LiteralPath (Join-Path $Folder 'redist/.penumbravr-bp-journal'))) {
        Add-Candidate (Join-Path $Folder 'redist/Penumbra.exe') $Source $true
    }
}

$libraries = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
if (Test-Path -LiteralPath $SteamRoot -PathType Container) {
    [void]$libraries.Add([System.IO.Path]::GetFullPath($SteamRoot))
    $libraryFile = Join-Path $SteamRoot 'steamapps/libraryfolders.vdf'
    if (Test-Path -LiteralPath $libraryFile -PathType Leaf) {
        $contents = Get-Content -LiteralPath $libraryFile -Raw
        foreach ($match in [regex]::Matches($contents, '"(?:path|[0-9]+)"\s*"([^"]+)"')) {
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
    foreach($id in @('22180','22120','22140')) {
        $manifest=Join-Path $library "steamapps/appmanifest_$id.acf"
        if(-not (Test-Path -LiteralPath $manifest -PathType Leaf)) { continue }
        $text=Get-Content -LiteralPath $manifest -Raw
        if($text -match '"installdir"\s*"([^"\\/:]+)"' -and $Matches[1] -notin @('.','..')) {
            Add-FromFolder (Join-Path $common $Matches[1]) 'Steam manifest'
        }
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

$results = [System.Collections.Generic.List[object]]::new()
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
    $results.Add([PSCustomObject][ordered]@{
        Path = $candidate.Path
        InstallRoot = $installRoot
        Source = $candidate.Source
        Game = $info.Game
        KnownBuild = $info.KnownBuild
        BuildId = $info.BuildId
        Variant = $info.Variant
        SHA256 = $info.SHA256
        Installable = ($info.KnownBuild -and $info.Game -in @('Overture', 'Black Plague'))
        ProbeError = $probeError
        Status = 'unknown-build'
        Issues = @()
        Managed = $false
        NeedsRecovery = $false
        RequiemPresent = Test-Path -LiteralPath (Join-Path $parent 'Requiem.exe') -PathType Leaf
    })
}
foreach ($requiem in @($results | Where-Object { $_.Game -eq 'Requiem' -and $_.KnownBuild })) {
    $sharedRoot = Split-Path -Parent $requiem.Path
    $companion = @($results | Where-Object {
        $_.Game -eq 'Black Plague' -and $_.KnownBuild -and
        (Split-Path -Parent $_.Path) -ieq $sharedRoot
    })
    # Both executables load one redist/alut.dll. The current transaction
    # requires the exact BP companion before enabling the Requiem payload.
    $requiem.Installable = $companion.Count -eq 1
}
foreach ($entry in @($results | Where-Object { $_.Game -eq 'Unknown' -and
                                              (Split-Path -Leaf $_.Path) -ieq 'Penumbra.exe' })) {
    $statePath = Join-Path $entry.InstallRoot '.penumbravr/deploy-state.json'
    if (-not (Test-Path -LiteralPath $statePath -PathType Leaf)) { continue }
    try {
        $state = Get-Content -LiteralPath $statePath -Raw | ConvertFrom-Json
        $redistRoot = [System.IO.Path]::GetFullPath(
            (Join-Path $entry.InstallRoot 'redist')).TrimEnd('\')
        $launcher = @($state.Files | Where-Object { $_.Path -eq 'Penumbra.exe' })
        if ($state.Version -eq 1 -and
            [System.IO.Path]::GetFullPath([string]$state.RedistRoot).TrimEnd('\') -ieq $redistRoot -and
            $launcher.Count -eq 1 -and
            [string]$launcher[0].DeployedHash -ieq [string]$entry.SHA256) {
            $entry.Game = 'Overture'
            $entry.KnownBuild = $true
            $entry.BuildId = 'overture-framework-managed'
            $entry.Installable = $true
        }
    } catch { continue }
}
foreach($entry in $results) {
    $redist=Split-Path -Parent $entry.Path
    $oState=Join-Path $entry.InstallRoot '.penumbravr/deploy-state.json'
    $bState=Join-Path $redist 'PenumbraVR.BlackPlague.install.json'
    $entry.Managed=(Test-Path -LiteralPath $oState -PathType Leaf) -or (Test-Path -LiteralPath $bState -PathType Leaf)
    $entry.NeedsRecovery=(Test-Path -LiteralPath (Join-Path $entry.InstallRoot '.penumbravr-journal')) -or (Test-Path -LiteralPath (Join-Path $redist '.penumbravr-bp-journal'))
    if($entry.NeedsRecovery) { $entry.Status='recovery-required'; $entry.Installable=$false; $entry.Issues=@('Recover the interrupted transaction before modifying this root.'); continue }
    if(-not $entry.KnownBuild) { $entry.Status=if($entry.Managed){'damaged-managed'}else{'unknown-build'}; $entry.Issues=@('Executable is missing, invalid or not an allowlisted build.'); continue }
    $game=switch($entry.Game){'Overture'{'overture'} 'Black Plague'{'black_plague'} 'Requiem'{'requiem'}}
    $missing=@(Test-PvrGameContent -Game $game -RedistRoot $redist | Where-Object {$_.Status -ne 'present'})
    $entry.Issues=@($missing | ForEach-Object {"$($_.Name): $($_.Status). $($_.Remediation)"})
    if($entry.Game -eq 'Requiem' -and -not $entry.Installable) { $entry.Issues+= 'Requiem requires its supported Black Plague companion in the same redist.' }
    if($missing.Count -or -not $entry.Installable) { $entry.Installable=$false; $entry.Status='incomplete' } else { $entry.Status='available' }
}
$results
