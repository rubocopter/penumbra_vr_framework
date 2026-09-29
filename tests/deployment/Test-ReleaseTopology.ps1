param([Parameter(Mandatory=$true)][string]$PackagePath,[Parameter(Mandatory=$true)][string]$OvertureExe,[Parameter(Mandatory=$true)][string]$BlackPlagueExe,[Parameter(Mandatory=$true)][string]$RequiemExe,[Parameter(Mandatory=$true)][string]$RetailAlut)
$ErrorActionPreference='Stop'
$temp=Join-Path ([IO.Path]::GetTempPath()) ('PvrTopology-'+[guid]::NewGuid().ToString('N'))
$temp=[IO.Path]::GetFullPath($temp)
if(-not $temp.StartsWith([IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\')+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Unsafe topology fixture path.'}
function Assert($ok,[string]$message){if(-not $ok){throw $message}}
function New-Root([string]$scenario,[string]$game,[bool]$expansion=$false){
    $root=Join-Path $temp "$scenario/$game/redist"
    New-Item -ItemType Directory -Path $root -Force | Out-Null
    $exe=if($game -eq 'overture'){$OvertureExe}else{$BlackPlagueExe}
    Copy-Item $exe (Join-Path $root 'Penumbra.exe')
    if($game -ne 'overture'){Copy-Item $RetailAlut (Join-Path $root 'alut.dll')}
    & (Join-Path $PSScriptRoot 'New-GameContentFixture.ps1') -Root $root -Games $game
    & (Join-Path $PSScriptRoot 'New-GameRuntimeFixture.ps1') -Root $root -Game $game
    if($expansion){Add-Expansion $root}
    return $root
}
function Add-Expansion([string]$root){Copy-Item $RequiemExe (Join-Path $root 'Requiem.exe'); & (Join-Path $PSScriptRoot 'New-GameContentFixture.ps1') -Root $root -Games requiem}
function Run([string]$game,[string]$root,[string]$operation='install'){
    $a=@{Game=$game;GamePath=$root;SteamRoot=(Join-Path $temp 'no-steam');LogPath=(Join-Path $temp 'installer.jsonl');OpenVrPathsPath=$vrpath}
    if($operation -eq 'restore'){$a.Restore=$true};if($operation -eq 'repair'){$a.Repair=$true}
    & $selector @a 6>$null | Out-Null
}
function Verify([string]$game,[string]$root){
    $r=& $verifier -Game $game -GamePath $root -PackageRoot $package -OpenVrPathsPath $vrpath
    Assert $r.InstallationVerified ("$game verification failed: "+(($r.Checks | Where-Object {$_.Status -notin @('present','provided')}).Name -join ', '))
}
function Assert-Original([string]$root,[string]$original){Assert ((Get-FileHash (Join-Path $root 'Penumbra.exe')).Hash -eq (Get-FileHash $original).Hash) 'Original executable not restored'}
try{
    New-Item -ItemType Directory -Path $temp | Out-Null
    $package=Join-Path $temp 'package';Expand-Archive -LiteralPath $PackagePath -DestinationPath $package
    $selector=Join-Path $package 'tools/Install-PenumbraFrameworkCandidate.ps1';$verifier=Join-Path $package 'tools/Test-PenumbraVrInstallation.ps1'
    $vrpath=Join-Path $temp 'openvrpaths.vrpath'
    $o=New-Root A overture
    & (Join-Path $PSScriptRoot 'New-GameRuntimeFixture.ps1') -Root $o -Game overture -OpenVrPathsPath $vrpath
    Run Overture $o;Verify overture $o;Run Overture $o repair;Run Overture $o restore;Assert-Original $o $OvertureExe
    Write-Host 'A: Overture only install/verify/repair/exact restore passed.'
    $b=New-Root B black_plague
    Run BlackPlague $b;Verify black_plague $b
    $state=Get-Content (Join-Path $b 'PenumbraVR.BlackPlague.install.json') -Raw | ConvertFrom-Json
    Assert ($state.components -notcontains 'requiem') 'B-only install created Requiem component'
    Assert (-not(Test-Path (Join-Path $b 'PenumbraVR.Requiem.Probe.dll'))) 'B-only installed R payload'
    Run BlackPlague $b repair;Run BlackPlague $b restore;Assert-Original $b $BlackPlagueExe
    Write-Host 'B: Black Plague only install/verify/repair/exact restore passed.'
    $b=New-Root C black_plague $true
    Run BlackPlague $b;Verify requiem $b
    $bpHash=(Get-FileHash (Join-Path $b 'PenumbraVR.BlackPlague.Probe.dll')).Hash
    Run Requiem $b restore;Verify black_plague $b
    Assert ((Get-FileHash (Join-Path $b 'PenumbraVR.BlackPlague.Probe.dll')).Hash -eq $bpHash) 'R removal changed B'
    Run BlackPlague $b restore;Assert-Original $b $BlackPlagueExe
    Assert ((Get-FileHash (Join-Path $b 'alut.dll')).Hash -eq (Get-FileHash $RetailAlut).Hash) 'Original audio library not restored'
    Write-Host 'C: B/R install, R-layer removal preserving B and exact restore passed.'
    foreach($scenario in @('D','E')){
        $o=New-Root $scenario overture;$b=New-Root $scenario black_plague ($scenario -eq 'E')
        $a=@{ManualPaths=@($o,$b);SteamRoot=(Join-Path $temp 'no-steam');Selections=@('1','2')+@(if($scenario -eq 'E'){'3'});LogPath=(Join-Path $temp 'installer.jsonl');OpenVrPathsPath=$vrpath}
        $plans=@(& $selector @a -Plan 6>$null)
        Assert ($plans.Count -eq 2) 'Shared root was duplicated or independent root omitted'
        & $selector @a 6>$null | Out-Null
        Verify overture $o;Verify black_plague $b
        if($scenario -eq 'E'){Verify requiem $b}
        $bpHash=(Get-FileHash (Join-Path $b 'PenumbraVR.BlackPlague.Probe.dll')).Hash
        Run Overture $o restore;Verify black_plague $b
        Assert ((Get-FileHash (Join-Path $b 'PenumbraVR.BlackPlague.Probe.dll')).Hash -eq $bpHash) 'Removing O changed B'
        Run BlackPlague $b restore;Assert-Original $o $OvertureExe;Assert-Original $b $BlackPlagueExe
        Write-Host "$scenario`: two-root install/verify, shared-root deduplication, independent removal/exact restore passed."
    }
    $b=New-Root F black_plague;Run BlackPlague $b
    $originalBackup=(Get-FileHash (Join-Path $b 'PenumbraVR_alut_original.dll')).Hash
    Add-Expansion $b;Run BlackPlague $b;Verify requiem $b
    Assert ((Get-FileHash (Join-Path $b 'PenumbraVR_alut_original.dll')).Hash -eq $originalBackup) 'Adding R changed base backup'
    $o=New-Root F overture;Run Overture $o;Verify overture $o;Verify requiem $b
    Run BlackPlague $b restore;Verify overture $o;Run Overture $o restore
    Assert-Original $b $BlackPlagueExe;Assert-Original $o $OvertureExe
    Write-Host 'F: B first, add R, add O, preserve previous target and independent removals passed.'
    Write-Host 'Topology A-F passed with synthetic content/runtime fixtures; no game was launched.'
}finally{if(Test-Path -LiteralPath $temp){Remove-Item -LiteralPath $temp -Recurse -Force}}
