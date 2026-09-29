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
    [switch]$List,
    [switch]$Plan,
    [switch]$Verify,
    [string]$DiagnosticOutputPath,
    [ValidateSet('Auto','Enable','Disable')][string]$RequiemMode='Auto',
    [switch]$CommunityTranslations,
    [switch]$InstallSpanishOverture,
    [switch]$InstallSpanishBlackPlague,
    [switch]$InstallSpanishRequiem,
    [switch]$TextureEnhancements,
    [switch]$RecommendedSettings,
    [ValidateSet('DefaultFiles','DefaultAndUserFiles')][string]$SettingsScope='DefaultAndUserFiles',
    [string]$OpenVrPathsPath
)

$ErrorActionPreference = 'Stop'
if(@($Restore,$Recover,$Repair,$List,$Verify,[bool]$DiagnosticOutputPath | Where-Object {$_}).Count -gt 1){throw 'Choose one operation: install, restore, recover, repair, list, verify or diagnostics.'}
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
$release=Get-Content -LiteralPath (Join-Path $packageRoot 'release.json') -Raw | ConvertFrom-Json
Import-Module (Join-Path $PSScriptRoot 'PenumbraVrPrerequisites.psm1') -Force
$verifier=Join-Path $PSScriptRoot 'Test-PenumbraVrInstallation.ps1'
if($Verify -or $DiagnosticOutputPath) {
    if(-not $Game -or -not $GamePath){throw 'Verification/diagnostics require an explicit game and path.'}
    $id=switch($Game){'Overture'{'overture'} 'BlackPlague'{'black_plague'} 'Requiem'{'requiem'}}
    if($DiagnosticOutputPath){& (Join-Path $PSScriptRoot 'Collect-PenumbraVrDiagnostics.ps1') -Game $id -GamePath $GamePath -PackageRoot $packageRoot -OutputPath $DiagnosticOutputPath}
    else {& $verifier -Game $id -GamePath $GamePath -PackageRoot $packageRoot -OpenVrPathsPath $OpenVrPathsPath}
    return
}
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
        frameworkVersion = $release.version
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
    $payloadPlan=$null;$maintenanceChecks=@()
    if($operation -eq 'repair'){
        $payloadPlan=& $installer @installerArgs -Preflight
        $id=switch($game){'Overture'{'overture'} 'Black Plague'{'black_plague'} 'Requiem'{'requiem'}}
        $redist=if($id -eq 'overture'){Join-Path $installerArgs.InstallRoot 'redist'}else{Split-Path -Parent $installerArgs.GamePath}
        $ids=if($id -eq 'overture'){@('overture')}else{@('black_plague')+@(if($payloadPlan.Components -contains 'requiem'){'requiem'})}
        foreach($gameId in $ids){
            $contentChecks=@(Test-PvrGameContent -Game $gameId -RedistRoot $redist)
            foreach($check in $contentChecks){
                # An owner-validated payload supplies a missing/damaged managed
                # config such as Overture's English overlay during repair.
                if($check.Name -in @($payloadPlan.Files)){$check.Status='provided'}
            }
            $maintenanceChecks+=$contentChecks
            $maintenanceChecks+=@(Get-PvrPrerequisites -Game $gameId -RedistRoot $redist -PackageRoot $packageRoot -ForInstall -SkipSteamVr)
        }
        $failed=@($maintenanceChecks | Where-Object {$_.Required -and $_.Status -notin @('present','provided')})
        if($failed.Count){throw "Repair preflight failed; no game files changed: $(($failed | ForEach-Object {"$($_.Name): $($_.Status). $($_.Remediation)"}) -join '; ')"}
    }
    if($Plan){
        if($operation -eq 'recover'){throw 'Recovery uses the verified durable journal; it cannot be planned as an install.'}
        if(-not $payloadPlan){$payloadPlan=& $installer @installerArgs -Preflight}
        [pscustomobject]@{Version=$release.version;Operation=$operation;Game=$game;Path=$targetPath;Arguments=$installerArgs;Payload=$payloadPlan;Checks=$maintenanceChecks;PreflightPassed=$true}
        return
    }
    Write-InstallerEvent $operation $game $targetPath 'started'
    try {
        if($operation -ne 'recover'){& $installer @installerArgs -Preflight | Out-Null}
        & $installer @installerArgs
        if($operation -in @('install','repair')){
            $id=switch($game){'Overture'{'overture'} 'Black Plague'{'black_plague'} 'Requiem'{'requiem'}}
            $result=& $verifier -Game $id -GamePath $targetPath -PackageRoot $packageRoot -OpenVrPathsPath $OpenVrPathsPath -SkipSteamVr:($operation -eq 'repair')
            Write-InstallerEvent 'verify' $game $targetPath $(if($result.InstallationVerified){'completed'}else{'failed'}) ($result | ConvertTo-Json -Depth 10 -Compress)
            if(-not $result.InstallationVerified){throw "Installation verification failed: $((@($result.Checks | Where-Object {$_.Status -notin @('present','provided')})).Name -join ', ')."}
        }
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
    if ($Game -notin @('Overture', 'BlackPlague', 'Requiem') -or -not $GamePath -or $Restore -or
        $LargeAddressAware -or $List -or $Repair) {
        throw 'Recovery requires an explicit game and -GamePath; omit other operation switches.'
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
            elseif ($leaf -ieq 'Requiem.exe') { Join-Path (Split-Path -Parent $requested) 'Penumbra.exe' }
            elseif ($leaf -ieq 'redist') { Join-Path $requested 'Penumbra.exe' }
            else { Join-Path $requested 'redist/Penumbra.exe' }
        Invoke-LoggedInstaller 'recover' $(if ($Game -eq 'Requiem') { 'Requiem' } else { 'Black Plague' }) $executable $blackPlagueInstaller @{
            GamePath = $executable; Recover = $true
        }
    }
    return
}

if ($Repair -or ($Restore -and $Game -and $GamePath)) {
    if ($Game -notin @('Overture', 'BlackPlague', 'Requiem') -or -not $GamePath -or
        $LargeAddressAware -or $List) {
        throw 'Repair requires an explicit game and -GamePath; omit other operation switches.'
    }
    $requested = [System.IO.Path]::GetFullPath($GamePath)
    $leaf = Split-Path -Leaf $requested
    if ($Game -eq 'Overture') {
        $gameRoot = if ($leaf -ieq 'Penumbra.exe') {
            Split-Path -Parent (Split-Path -Parent $requested)
        } elseif ($leaf -ieq 'redist') {
            Split-Path -Parent $requested
        } else { $requested }
        Invoke-LoggedInstaller $(if($Restore){'restore'}else{'repair'}) 'Overture' $gameRoot $overtureInstaller @{
            InstallRoot = $gameRoot
            PackageRoot = Join-Path $packageRoot 'products/overture'
            Repair = [bool]$Repair
            Restore = [bool]$Restore
        }
    } else {
        $executable = if ($leaf -ieq 'Penumbra.exe') { $requested }
            elseif ($leaf -ieq 'Requiem.exe') { Join-Path (Split-Path -Parent $requested) 'Penumbra.exe' }
            elseif ($leaf -ieq 'redist') { Join-Path $requested 'Penumbra.exe' }
            else { Join-Path $requested 'redist/Penumbra.exe' }
        $maintenanceArgs=@{
            GamePath = $executable
            BuildRoot = Join-Path $packageRoot 'products/black_plague/build'
            Repair = [bool]$Repair
            Restore = [bool]$Restore
        }
        $operation=if($Restore){'restore'}else{'repair'}
        $label=if($Game -eq 'Requiem'){'Requiem'}else{'Black Plague'}
        if($Restore -and $Game -eq 'Requiem'){$maintenanceArgs.Restore=$false;$maintenanceArgs.RequiemMode='Disable';$operation='remove-requiem';$label='Black Plague'}
        Invoke-LoggedInstaller $operation $label $executable $blackPlagueInstaller $maintenanceArgs
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

Write-Host 'Detected Penumbra installations:'
if (-not $found.Count) { Write-Host '  None. Pass -ManualPaths or -GamePath.' }
for ($i = 0; $i -lt $found.Count; $i++) {
    $entry = $found[$i]
    $status = if ($entry.Installable) { 'candidate available' } else { 'not installable' }
    Write-Host ("  [{0}] {1} ({2}) {3}" -f ($i + 1), $entry.Game, $status, $entry.Path)
    if ($entry.ProbeError) { Write-Host ("      {0}" -f $entry.ProbeError) }
}
if ($List) { return }

$choices = @($found | Where-Object { ($_.Installable -or ($Restore -and $_.Managed)) -and $_.Game -in @('Overture', 'Black Plague', 'Requiem') })
if ($Game -eq 'Overture') { $choices = @($choices | Where-Object { $_.Game -eq 'Overture' }) }
if ($Game -eq 'BlackPlague') { $choices = @($choices | Where-Object { $_.Game -eq 'Black Plague' }) }
if ($Game -eq 'Requiem') { $choices = @($choices | Where-Object { $_.Game -eq 'Requiem' }) }
if ($GamePath) {
    $requestedPath = [System.IO.Path]::GetFullPath($GamePath).TrimEnd('\', '/')
    $choices = @($choices | Where-Object {
        $_.Path.TrimEnd('\', '/') -ieq $requestedPath -or
        $_.InstallRoot.TrimEnd('\', '/') -ieq $requestedPath -or
        (Split-Path -Parent $_.Path).TrimEnd('\', '/') -ieq $requestedPath
    })
}
if (-not $choices.Count) {
    throw "No compatible candidate matched the requested game and path. Unknown builds are never modified. $(($found.Issues | Select-Object -Unique) -join ' ')"
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
            throw "Preflight failed; no selected root was changed: $($candidate.Game): $($candidate.Status). $($candidate.Issues -join ' ')"
        }
        $selectedEntries += $candidate
    }
}
if ($LargeAddressAware -and @($selectedEntries | Where-Object { $_.Game -eq 'Overture' }).Count) {
    throw 'The LAA transform option applies only to the exact-build Black Plague candidate.'
}

$transactions=@()
$handled=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
foreach($selected in $selectedEntries){
    $redist=Split-Path -Parent $selected.Path
    $operation=if($Restore){'restore'}else{'install'}
    if($selected.Game -eq 'Overture'){
        $args=@{InstallRoot=$selected.InstallRoot;PackageRoot=(Join-Path $packageRoot 'products/overture');Restore=[bool]$Restore}
        if(-not $Restore){
            $args.ExpectedExecutableSha256=$selected.SHA256
            $statePath=Join-Path $selected.InstallRoot '.penumbravr/deploy-state.json'
            $state=if(Test-Path -LiteralPath $statePath){Get-Content -LiteralPath $statePath -Raw | ConvertFrom-Json}else{$null}
            $args.SkipTexturePack=-not $TextureEnhancements
            if($state -and -not $PSBoundParameters.ContainsKey('TextureEnhancements')){
                $texturePaths=(Get-Content -LiteralPath (Join-Path $packageRoot 'products/overture/docs/TEXTURE_SELECTION.json') -Raw | ConvertFrom-Json).Files.Path
                $args.SkipTexturePack=-not @($state.Files | Where-Object {$_.Path -in $texturePaths}).Count
            }
            $args.CommunityTranslations=if($PSBoundParameters.ContainsKey('InstallSpanishOverture')){[bool]$InstallSpanishOverture}elseif($PSBoundParameters.ContainsKey('CommunityTranslations')){[bool]$CommunityTranslations}else{[bool]@($state.Files | Where-Object {$_.Path -eq 'config/Espanol.lang'}).Count}
            $args.RecommendedSettings=[bool]$RecommendedSettings;$args.SettingsScope=$SettingsScope
        }
        $installer=$overtureInstaller;$label='Overture';$target=$selected.Path;$id='overture'
    }else{
        if(-not $handled.Add($redist)){continue}
        $members=@($selectedEntries | Where-Object {(Split-Path -Parent $_.Path) -ieq $redist})
        $label=if(@($members | Where-Object {$_.Game -eq 'Requiem'}).Count){'Requiem'}else{'Black Plague'}
        $args=@{GamePath=(Join-Path $redist 'Penumbra.exe');BuildRoot=(Join-Path $packageRoot 'products/black_plague/build');Restore=[bool]$Restore}
        if($Restore -and $label -eq 'Requiem' -and -not @($members | Where-Object {$_.Game -eq 'Black Plague'}).Count){$args.Restore=$false;$args.RequiemMode='Disable';$operation='remove-requiem';$label='Black Plague'}
        if(-not $Restore){
            if($label -eq 'Requiem' -and $RequiemMode -eq 'Disable'){throw 'Selected Requiem cannot be disabled in the same install plan.'}
            $args.RequiemMode=if($label -eq 'Requiem'){'Enable'}else{$RequiemMode}
            if($PSBoundParameters.ContainsKey('InstallSpanishBlackPlague')){$args.InstallSpanishBlackPlague=[bool]$InstallSpanishBlackPlague}
            if($PSBoundParameters.ContainsKey('InstallSpanishRequiem')){$args.InstallSpanishRequiem=[bool]$InstallSpanishRequiem}
            if($CommunityTranslations -and
               -not $PSBoundParameters.ContainsKey('InstallSpanishBlackPlague') -and
               -not $PSBoundParameters.ContainsKey('InstallSpanishRequiem')){$args.CommunityTranslations=$true}
            $args.RecommendedSettings=[bool]$RecommendedSettings;$args.SettingsScope=$SettingsScope
            $args.LargeAddressAware=[bool]$LargeAddressAware
        }
        $installer=$blackPlagueInstaller;$target=$args.GamePath;$id=if($label -eq 'Requiem'){'requiem'}else{'black_plague'}
    }
    $checks=@()
    if(-not $Restore){
        $ids=@($id)+@(if($id -eq 'requiem'){'black_plague'})
        if($id -eq 'black_plague' -and ($args.RequiemMode -eq 'Enable' -or ($args.RequiemMode -eq 'Auto' -and (Test-Path -LiteralPath (Join-Path $redist 'Requiem.exe'))))){$ids+='requiem'}
        foreach($gameId in $ids){$checks+=@(Test-PvrGameContent -Game $gameId -RedistRoot $redist);$checks+=@(Get-PvrPrerequisites -Game $gameId -RedistRoot $redist -PackageRoot $packageRoot -ForInstall -OpenVrPathsPath $OpenVrPathsPath)}
        $failed=@($checks | Where-Object {$_.Required -and $_.Status -notin @('present','provided')})
        if($failed.Count){throw "Preflight failed; no selected root was changed: $(($failed | ForEach-Object {"$($_.Name): $($_.Status). $($_.Remediation)"}) -join '; ')"}
    }
    $payloadPlan=& $installer @args -Preflight
    $transactions+=[pscustomobject]@{Version=$release.version;Operation=$operation;Game=$label;Path=$target;Installer=$installer;Arguments=$args;Payload=$payloadPlan;Checks=$checks;PreflightPassed=$true}
}
if($Plan){$transactions;return}
foreach($transaction in $transactions){
    Write-InstallerEvent 'plan' $transaction.Game $transaction.Path 'completed' ($transaction | ConvertTo-Json -Depth 10 -Compress)
    try{Invoke-LoggedInstaller $transaction.Operation $transaction.Game $transaction.Path $transaction.Installer $transaction.Arguments}
    catch{throw "Framework candidate stopped at $($transaction.Game); earlier roots may have completed independently: $($_.Exception.Message)"}
}
