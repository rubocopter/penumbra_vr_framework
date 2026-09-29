[CmdletBinding()]
param(
    [string]$SteamRoot,
    [string[]]$ManualPaths = @()
)

$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'PenumbraVrPrerequisites.psm1') -Force
Import-Module (Join-Path $PSScriptRoot 'PenumbraVrConfiguration.psm1') -Force
$packageRoot = Split-Path -Parent $PSScriptRoot

function Get-RecoveryPath([string]$Path) {
    if (-not $Path -or -not [IO.Path]::IsPathRooted($Path)) { throw 'Recovery path must be absolute.' }
    $absolute = [IO.Path]::GetFullPath($Path).TrimEnd('\','/')
    for ($cursor = $absolute; $cursor; $cursor = Split-Path -Parent $cursor) {
        $item = Get-Item -LiteralPath $cursor -Force -ErrorAction SilentlyContinue
        if ($item -and ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw 'Recovery path traverses a reparse point.'
        }
    }
    return $absolute
}

function Assert-RecoveryJournal([string]$Product, [string]$InstallRoot, [string]$RedistRoot) {
    # Passive validation mirrors the owning transactions; it never invokes recovery.
    $gameRoot = Get-RecoveryPath $(if ($Product -eq 'Overture') { $InstallRoot } else { $RedistRoot })
    $journal = Get-RecoveryPath (Join-Path $gameRoot $(if ($Product -eq 'Overture') { '.penumbravr-journal' } else { '.penumbravr-bp-journal' }))
    $manifest = Get-RecoveryPath (Join-Path $journal 'journal.json')
    $record = Get-Content -LiteralPath $manifest -Raw | ConvertFrom-Json
    if ($record.Version -ne 1 -or (Get-RecoveryPath ([string]$record.GameRoot)) -ine $gameRoot) {
        throw 'Recovery journal schema or game root does not match.'
    }
    $entries = @($record.Entries)
    if (-not $entries.Count) { throw 'Recovery journal has no entries.' }
    $settings = if ($Product -eq 'Overture') {
        @(Get-PvrConfigurationPaths -Game overture -RedistRoot $RedistRoot)
    } else {
        @(Get-PvrConfigurationPaths -Game black_plague -RedistRoot $RedistRoot) +
        @(Get-PvrConfigurationPaths -Game requiem -RedistRoot $RedistRoot)
    }
    if ($Product -eq 'Black Plague') {
        $deployment = Get-Content -LiteralPath (Join-Path $packageRoot 'assets/deployment/manifest.json') -Raw | ConvertFrom-Json
        $productRecord = @($deployment.products | Where-Object { $_.game -eq 'black_plague' })
        if ($deployment.schemaVersion -ne 1 -or $productRecord.Count -ne 1) { throw 'Invalid deployment manifest.' }
        function Destination([string]$id) {
            $payload = @($productRecord[0].payloads | Where-Object { $_.id -eq $id })
            if ($payload.Count -ne 1) { throw 'Invalid recovery payload definition.' }
            Join-Path $gameRoot ([string]$payload[0].destination)
        }
        $targets = @(
            (Join-Path $gameRoot 'Penumbra.exe'), (Join-Path $gameRoot 'PenumbraVR_Penumbra_original.exe'),
            (Destination 'bootstrap_proxy'), (Join-Path $gameRoot 'PenumbraVR_alut_original.dll'),
            (Destination 'probe'), (Destination 'openvr_loader'), (Destination 'openvr_actions'),
            (Destination 'hand_texture'), (Destination 'spanish_localization'),
            (Join-Path $gameRoot 'PenumbraVR_Espanol_original.lang'), (Destination 'openal_hrtf_config'),
            (Join-Path $gameRoot 'PenumbraVR.BlackPlague.install.json'), (Destination 'requiem_probe'),
            (Destination 'requiem_spanish_localization'), (Join-Path $gameRoot 'PenumbraVR_Espanol_exp_original.lang'),
            (Join-Path $gameRoot 'OpenAL32.dll'), (Join-Path $gameRoot 'PenumbraVR_OpenAL_original.dll')
        ) + $settings
        if ($entries.Count -notin @(12,15,17,21)) { throw 'Recovery journal has the wrong managed set.' }
    }
    $seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    for ($i = 0; $i -lt $entries.Count; $i++) {
        $entry = $entries[$i]
        $target = Get-RecoveryPath ([string]$entry.Target)
        $copy = Get-RecoveryPath ([string]$entry.Copy)
        $personal = $target -in $settings -and -not $target.StartsWith($gameRoot+'\',[StringComparison]::OrdinalIgnoreCase)
        if ((-not $target.StartsWith($gameRoot+'\',[StringComparison]::OrdinalIgnoreCase) -and -not $personal) -or
            -not $seen.Add($target) -or $copy -ine (Join-Path $journal "item-$i")) {
            throw 'Recovery entry escapes its root, is duplicated or has an invalid copy path.'
        }
        if ($Product -eq 'Black Plague') {
            if ($target -ine [IO.Path]::GetFullPath($targets[$i])) { throw 'Recovery target does not match its managed position.' }
            if ($entry.Kind -eq 'untouched' -and $i -ge 17 -and $i -lt 21) {
                if (Test-Path -LiteralPath $copy) { throw 'Untouched settings must not have a recovery copy.' }
                continue
            }
        } else {
            if ($target -ine (Join-Path $gameRoot '.penumbravr') -and -not $personal -and
                -not $target.StartsWith($RedistRoot.TrimEnd('\')+'\',[StringComparison]::OrdinalIgnoreCase)) {
                throw 'Invalid Overture managed target.'
            }
            foreach ($prior in $seen) {
                if ($prior -ine $target -and ($target.StartsWith($prior+'\',[StringComparison]::OrdinalIgnoreCase) -or
                    $prior.StartsWith($target+'\',[StringComparison]::OrdinalIgnoreCase))) { throw 'Overlapping recovery targets.' }
            }
            foreach ($missingParent in @($entry.MissingParents)) {
                if (-not $missingParent) { continue }
                $parent = Get-RecoveryPath ([string]$missingParent)
                if (-not $parent.StartsWith($gameRoot+'\',[StringComparison]::OrdinalIgnoreCase) -and -not $personal) { throw 'Recovery parent escapes game root.' }
                if (-not $target.StartsWith($parent+'\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Invalid recovery parent.' }
            }
        }
        if ($entry.Kind -notin @('absent','file','directory') -or ($personal -and $entry.Kind -eq 'directory')) { throw 'Invalid recovery entry kind.' }
        if ($entry.Kind -eq 'absent') {
            if (Test-Path -LiteralPath $copy) { throw 'Absent entry has an unexpected recovery copy.' }
        } elseif ($entry.Kind -eq 'file') {
            if ([string]$entry.SnapshotHash -notmatch '^[0-9a-fA-F]{64}$' -or
                (Get-FileHash -LiteralPath $copy -Algorithm SHA256).Hash -ine [string]$entry.SnapshotHash) { throw 'Recovery copy hash does not match.' }
        } else {
            if (-not (Test-Path -LiteralPath $copy -PathType Container)) { throw 'Recovery directory copy is missing.' }
            $items = @(Get-ChildItem -LiteralPath $copy -Recurse -Force)
            foreach ($item in $items) { [void](Get-RecoveryPath $item.FullName) }
            $files = @($items | Where-Object { -not $_.PSIsContainer } | ForEach-Object {
                [ordered]@{path=$_.FullName.Substring($copy.Length).TrimStart('\').Replace('\','/');sha256=(Get-FileHash -LiteralPath $_.FullName).Hash.ToUpperInvariant()}
            } | Sort-Object { $_.path })
            if ($Product -eq 'Overture') {
                $snapshot = @($files | ForEach-Object { $_.path+':'+$_.sha256.ToLowerInvariant() } | Sort-Object)
                if (($snapshot -join '|') -cne (@($entry.SnapshotFiles) -join '|')) { throw 'Recovery directory snapshot does not match.' }
            } else {
                $directories = @($items | Where-Object { $_.PSIsContainer } | ForEach-Object { $_.FullName.Substring($copy.Length).TrimStart('\').Replace('\','/') } | Sort-Object)
                $snapshot = [pscustomobject]@{files=$files;directories=$directories}
                if (($snapshot | ConvertTo-Json -Depth 5 -Compress) -cne ($entry.SnapshotDirectory | ConvertTo-Json -Depth 5 -Compress)) { throw 'Recovery directory snapshot does not match.' }
            }
        }
    }
}
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
    if ((Split-Path -Leaf $Folder) -ieq 'redist') {
        $installRoot = Split-Path -Parent $Folder
        if ((Test-Path -LiteralPath (Join-Path $installRoot '.penumbravr/deploy-state.json')) -or
            (Test-Path -LiteralPath (Join-Path $installRoot '.penumbravr-journal')) -or
            (Test-Path -LiteralPath (Join-Path $Folder 'PenumbraVR.BlackPlague.install.json')) -or
            (Test-Path -LiteralPath (Join-Path $Folder '.penumbravr-bp-journal'))) {
            Add-Candidate (Join-Path $Folder 'Penumbra.exe') $Source $true
        }
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
        ManagedProduct = $null
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
    if(Test-Path -LiteralPath $oState -PathType Leaf){$entry.ManagedProduct='Overture'}elseif(Test-Path -LiteralPath $bState -PathType Leaf){$entry.ManagedProduct='Black Plague'}
    $entry.NeedsRecovery=(Test-Path -LiteralPath (Join-Path $entry.InstallRoot '.penumbravr-journal')) -or (Test-Path -LiteralPath (Join-Path $redist '.penumbravr-bp-journal'))
    if($entry.NeedsRecovery) {
        $entry.Status='recovery-required'; $entry.Installable=$false
        $entry.Issues=@('Recover the interrupted transaction before modifying this root.')
        $owners=@()
        foreach ($product in @('Overture','Black Plague')) {
            $journal=if($product -eq 'Overture'){Join-Path $entry.InstallRoot '.penumbravr-journal'}else{Join-Path $redist '.penumbravr-bp-journal'}
            if (-not (Test-Path -LiteralPath $journal)) { continue }
            try { Assert-RecoveryJournal $product $entry.InstallRoot $redist; $owners+=$product }
            catch { $entry.Issues+= "$product recovery journal is invalid: $($_.Exception.Message)" }
        }
        if ($owners.Count -eq 1) { $entry.Managed=$true; $entry.ManagedProduct=$owners[0] }
        elseif ($owners.Count -gt 1) { $entry.ManagedProduct=$null; $entry.Issues+='Recovery owner is ambiguous; both product journals are present.' }
        continue
    }
    if(-not $entry.KnownBuild) { $entry.Status=if($entry.Managed){'damaged-managed'}else{'unknown-build'}; $entry.Issues=@('Executable is missing, invalid or not an allowlisted build.'); continue }
    $game=switch($entry.Game){'Overture'{'overture'} 'Black Plague'{'black_plague'} 'Requiem'{'requiem'}}
    $checks=@(Test-PvrGameContent -Game $game -RedistRoot $redist)+
        @(Get-PvrPrerequisites -Game $game -RedistRoot $redist -PackageRoot $packageRoot -SkipSteamVr -ForInstall)
    $missing=@($checks | Where-Object {$_.Required -and $_.Status -notin @('present','provided')})
    $entry.Issues=@($missing | ForEach-Object {"$($_.Name): $($_.Status). $($_.Remediation)"})
    if($entry.Game -eq 'Requiem' -and -not $entry.Installable) { $entry.Issues+= 'Requiem requires its supported Black Plague companion in the same redist.' }
    if($missing.Count -or -not $entry.Installable) { $entry.Installable=$false; $entry.Status='incomplete' } else { $entry.Status='available' }
}
foreach($entry in @($results | Where-Object {$_.Game -eq 'Requiem' -and $_.Installable})){
    $base=@($results | Where-Object {$_.Game -eq 'Black Plague' -and $_.Installable -and (Split-Path -Parent $_.Path) -ieq (Split-Path -Parent $entry.Path)})
    if($base.Count -ne 1){$entry.Installable=$false;$entry.Status='incomplete';$entry.Issues+= 'Requiem requires a complete validated Black Plague base in the same redist.'}
}
$results
