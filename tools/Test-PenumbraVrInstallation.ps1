[CmdletBinding()]
param([Parameter(Mandatory=$true)][ValidateSet('overture','black_plague','requiem')][string]$Game,
      [Parameter(Mandatory=$true)][string]$GamePath,[string]$PackageRoot,[string]$OpenVrPathsPath,[switch]$SkipSteamVr)
$ErrorActionPreference='Stop'
if(-not $PackageRoot){$PackageRoot=Split-Path -Parent $PSScriptRoot}
Import-Module (Join-Path $PSScriptRoot 'PenumbraVrPrerequisites.psm1') -Force
Import-Module (Join-Path $PSScriptRoot 'PenumbraVrConfiguration.psm1') -Force
$requested=[IO.Path]::GetFullPath($GamePath).TrimEnd('\')
$leaf=Split-Path -Leaf $requested
$redist=if($leaf -in @('Penumbra.exe','Requiem.exe')){Split-Path -Parent $requested}elseif($leaf -ieq 'redist'){$requested}elseif(Test-Path -LiteralPath (Join-Path $requested 'redist')){Join-Path $requested 'redist'}else{$requested}
$installRoot=if((Split-Path -Leaf $redist) -ieq 'redist'){Split-Path -Parent $redist}else{$redist}
$checks=[Collections.Generic.List[object]]::new()
function Check([string]$name,[bool]$ok,[string]$detail,[string]$remedy='Repair with the verified installer package; keep the original backups.') {
    $checks.Add([pscustomobject]@{Name=$name;Required=$true;Status=$(if($ok){'present'}else{'invalid'});Detail=$detail;Remediation=$remedy})
}
function SafePath([string]$root,[string]$relative) {
    if([IO.Path]::IsPathRooted($relative) -or $relative -match '(^|[\\/])\.\.([\\/]|$)'){throw 'Unsafe managed path.'}
    $path=[IO.Path]::GetFullPath((Join-Path $root $relative))
    if(-not $path.StartsWith([IO.Path]::GetFullPath($root).TrimEnd('\')+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Managed path escapes root.'}
    for($cursor=$path;$cursor;$cursor=Split-Path -Parent $cursor){
        $item=Get-Item -LiteralPath $cursor -Force -ErrorAction SilentlyContinue
        if($item -and ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Managed path traverses a reparse point.'}
    }
    $path
}
function Hash([string]$path){if(Test-Path -LiteralPath $path -PathType Leaf){(Get-FileHash -LiteralPath $path).Hash}else{$null}}
$version='unknown';$components=@();$state=$null;$installedGames=@($Game)+@(if($Game -eq 'requiem'){'black_plague'})
try {
    $statePath=if($Game -eq 'overture'){SafePath $installRoot '.penumbravr/deploy-state.json'}else{SafePath $redist 'PenumbraVR.BlackPlague.install.json'}
    if((Get-Item -LiteralPath $statePath).Length -gt 4194304){throw 'State exceeds 4 MiB.'}
    $state=Get-Content -LiteralPath $statePath -Raw | ConvertFrom-Json
    if($Game -eq 'overture'){
        Check 'State schema and root' ($state.Version -eq 1 -and [IO.Path]::GetFullPath([string]$state.RedistRoot).TrimEnd('\') -ieq $redist) 'Overture ownership'
        $version=$state.FrameworkVersion;$components=@($state.Components)
        $seen=@{}
        foreach($entry in @($state.Files)){
            $path=SafePath $redist $entry.Path
            if($seen.ContainsKey([string]$entry.Path)){throw 'Duplicate managed file.'};$seen[[string]$entry.Path]=$true
            Check ('Owned '+$entry.Path) ([string]$entry.DeployedHash -match '^[0-9a-fA-F]{64}$' -and (Hash $path) -ieq $entry.DeployedHash) ([string]$entry.Path)
            if($entry.HadOriginal){Check ('Backup '+$entry.Path) ([string]$entry.OriginalHash -match '^[0-9a-fA-F]{64}$' -and (Hash (SafePath (Join-Path $installRoot '.penumbravr/backup') $entry.Path)) -ieq $entry.OriginalHash) ([string]$entry.Path)}
        }
        foreach($core in @('Penumbra.exe','Penumbra_vr.exe','openvr_api.dll','OpenAL32.dll','msvcp140.dll','vcruntime140.dll','vr/actions.json')){Check ('Core '+$core) $seen.ContainsKey($core) $core}
        $configuration=@($state.Configuration)
    }else{
        $version=$state.frameworkVersion;$components=@($state.components)
        Check 'State schema' ($state.schema -eq 1) 'Shared B/R ownership'
        Check 'Component graph' ($components -contains 'shared' -and $components -contains 'black_plague' -and ($Game -ne 'requiem' -or $components -contains 'requiem')) 'Requiem requires Black Plague'
        $installedGames=@('black_plague')+@(if($components -contains 'requiem'){'requiem'})
        $requiemDeclared=$components -contains 'requiem'
        Check 'Requiem component ownership' (($requiemDeclared -eq [bool]$state.requiemProbeSha256) -and ($requiemDeclared -eq [bool]$state.requiemExeSha256) -and ($requiemDeclared -or -not $state.requiemSpanishLocalizationSha256) -and ($requiemDeclared -or -not (Test-Path -LiteralPath (SafePath $redist 'PenumbraVR.Requiem.Probe.dll')))) 'Expansion payload and executable records must agree with the component graph.'
        $owned=@{ 'alut.dll'='installedProxySha256';'PenumbraVR.BlackPlague.Probe.dll'='probeSha256';'openvr_api.dll'='openVrSha256';'OpenAL32.dll'='openAlSha256';'assets/rework/HAND_Low_C.jpg'='handTextureSha256' }
        if($components -contains 'requiem'){$owned['PenumbraVR.Requiem.Probe.dll']='requiemProbeSha256'}
        if($state.spanishLocalizationSha256){$owned['config/Espanol.lang']='spanishLocalizationSha256'}
        if($state.requiemSpanishLocalizationSha256){$owned['expansion01/config/Espanol_exp.lang']='requiemSpanishLocalizationSha256'}
        if($state.audioConfigCreated){$owned['alsoft.ini']='audioConfigSha256'}
        foreach($relative in $owned.Keys){$hash=$state.($owned[$relative]);Check ('Owned '+$relative) ([string]$hash -match '^[0-9a-fA-F]{64}$' -and (Hash (SafePath $redist $relative)) -ieq $hash) $relative}
        Check 'Original ALUT backup' ((Hash (SafePath $redist 'PenumbraVR_alut_original.dll')) -eq 'D81DEA8E88E35C319F7F2D8AAEB14C63A4986131492D3DF860D1F2C18B844590') 'Original retail library'
        if($state.laaApplied){Check 'Original game backup' ((Hash (SafePath $redist 'PenumbraVR_Penumbra_original.exe')) -eq 'FD316F7586737A63EBA989ECE2271280FE6A98582A1319FE2151385A3DF97BFF') 'Canonical executable'}
        foreach($backup in @(@($state.openAlHadOriginal,'PenumbraVR_OpenAL_original.dll',$state.originalOpenAlSha256),@($state.spanishLocalizationHadOriginal,'PenumbraVR_Espanol_original.lang',$state.spanishLocalizationOriginalSha256),@($state.requiemSpanishLocalizationHadOriginal,'PenumbraVR_Espanol_exp_original.lang',$state.requiemSpanishLocalizationOriginalSha256))){if($backup[0]){Check ('Backup '+$backup[1]) ([string]$backup[2] -match '^[0-9a-fA-F]{64}$' -and (Hash (SafePath $redist $backup[1])) -ieq $backup[2]) $backup[1]}}
        foreach($entry in @($state.vrAssetsSnapshot.files)){Check ('Binding '+$entry.path) ((Hash (SafePath (Join-Path $redist 'vr') $entry.path)) -ieq $entry.sha256) $entry.path}
        $configuration=@($state.configuration)
        foreach($exeName in @('Penumbra.exe')+@(if($components -contains 'requiem'){'Requiem.exe'})){
            $info=& (Join-Path $PSScriptRoot 'Get-PenumbraBuildInfo.ps1') -Path (SafePath $redist $exeName)
            $expectedGame=if($exeName -eq 'Penumbra.exe'){'Black Plague'}else{'Requiem'}
            $recordedHash=if($exeName -eq 'Penumbra.exe'){$state.gameExeSha256}else{$state.requiemExeSha256}
            Check ('Exact build '+$exeName) ($info.KnownBuild -and $info.Game -eq $expectedGame -and $info.Variant -in @('observed','large-address-aware') -and $info.SHA256 -ieq $recordedHash) ($expectedGame+': '+$info.BuildId)
        }
    }
    foreach($gameId in $installedGames){
        Get-PvrConfigurationRecords -Game $gameId -RedistRoot $redist -Previous @($configuration | Where-Object {$null -ne $_ -and $_.Game -eq $gameId}) | Out-Null
    }
    foreach($plan in @($configuration | Where-Object {$null -ne $_})){
        if($plan.Game -notin @('overture','black_plague','requiem')){throw 'Unknown configuration owner.'}
        Get-PvrConfigurationRecords -Game $plan.Game -RedistRoot $redist -Previous @($plan) | Out-Null
        Read-PvrConfig $plan.Path | Out-Null
        Check ('Configuration '+$plan.Game) $true 'Parsed targeted configuration; personal choices may differ.'
    }
}catch{Check 'Managed state' $false $_.Exception.Message}
foreach($gameId in $installedGames){
    foreach($check in @(Test-PvrGameContent -Game $gameId -RedistRoot $redist)+@(Get-PvrPrerequisites -Game $gameId -RedistRoot $redist -OpenVrPathsPath $OpenVrPathsPath -SkipSteamVr:$SkipSteamVr)){$checks.Add($check)}
}
try{
    $manifest=Get-Content -LiteralPath (SafePath $redist 'vr/actions.json') -Raw | ConvertFrom-Json
    if(@($manifest.default_bindings).Count -ne 8 -or -not @($manifest.actions).Count){throw 'Incomplete action manifest.'}
    foreach($binding in $manifest.default_bindings){$json=Get-Content -LiteralPath (SafePath (Join-Path $redist 'vr') $binding.binding_url) -Raw | ConvertFrom-Json;if(-not $json.bindings -or $json.controller_type -ne $binding.controller_type){throw 'Invalid controller binding.'}}
    Check 'Action manifest and bindings' $true 'Eight profiles parsed; hardware validation remains separate.'
}catch{Check 'Action manifest and bindings' $false $_.Exception.Message}
$verified= -not @($checks | Where-Object {$_.Required -and $_.Status -notin @('present','provided')}).Count
[pscustomobject]@{Game=$Game;Version=$version;Components=@($components);Checks=$checks.ToArray();InstallationVerified=$verified;HeadsetValidated=$null}
