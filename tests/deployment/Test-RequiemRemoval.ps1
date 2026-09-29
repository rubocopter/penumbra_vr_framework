[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$PackagePath,[Parameter(Mandatory=$true)][string]$BlackPlagueExe,[Parameter(Mandatory=$true)][string]$RequiemExe,[Parameter(Mandatory=$true)][string]$RetailAlut)
$ErrorActionPreference='Stop'
$PSDefaultParameterValues=@{'*:SettingsScope'='DefaultFiles'}
$repo=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Import-Module (Join-Path $repo 'tools/PenumbraVrInstallerUi.psm1') -Force
$temp=Join-Path ([IO.Path]::GetTempPath()) ('PvrRemoveReq-'+[guid]::NewGuid().ToString('N'))
function Assert($ok,[string]$message){if(-not $ok){throw $message}}
try{
    New-Item -ItemType Directory -Path $temp | Out-Null
    $payload=Join-Path $temp 'payload'
    if([IO.Path]::GetExtension($PackagePath) -eq '.exe'){& $PackagePath --extract-to $payload | Out-Null;if($LASTEXITCODE -ne 0){throw 'Setup extraction failed.'}}else{Expand-Archive -LiteralPath $PackagePath -DestinationPath $payload}
    $selector=Join-Path $payload 'tools/Install-PenumbraFrameworkCandidate.ps1'
    $discovery=Join-Path $payload 'tools/Get-PenumbraInstallations.ps1'
    $vrpath=Join-Path $temp 'openvrpaths.vrpath'
    foreach($route in @('explicit','selection')){
        $root=Join-Path $temp "$route/redist";New-Item -ItemType Directory -Path $root -Force | Out-Null
        Copy-Item $BlackPlagueExe (Join-Path $root 'Penumbra.exe');Copy-Item $RequiemExe (Join-Path $root 'Requiem.exe');Copy-Item $RetailAlut (Join-Path $root 'alut.dll')
        & (Join-Path $PSScriptRoot 'New-GameContentFixture.ps1') -Root $root -Games black_plague,requiem
        & (Join-Path $PSScriptRoot 'New-GameRuntimeFixture.ps1') -Root $root -Game black_plague -OpenVrPathsPath $vrpath
        $common=@{SteamRoot=(Join-Path $temp 'no-steam');LogPath=(Join-Path $temp 'installer.jsonl');OpenVrPathsPath=$vrpath}
        & $selector -Game BlackPlague -GamePath $root -RequiemMode Enable -InstallSpanishBlackPlague -InstallSpanishRequiem @common 6>$null | Out-Null
        $spanish=Join-Path $root 'config/Espanol.lang';$before=(Get-FileHash $spanish).Hash
        $found=@(& $discovery -SteamRoot $common.SteamRoot -ManualPaths $root)
        $base=$found | Where-Object {$_.Game -eq 'Black Plague'};$exp=$found | Where-Object {$_.Game -eq 'Requiem'}
        $report=& $selector -Game BlackPlague -GamePath $root -Verify
        $card=ConvertTo-PvrUiGame -Entry $base -Expansion $exp -Report $report -PackageVersion '1.0.0';$card.RemoveRequiem=$true
        $remove=@(New-PvrUiRequests -Cards @($card) -Operation Uninstall)
        if($route -eq 'explicit'){$a=$remove[0].Arguments;& $selector @a @common 6>$null | Out-Null}else{
            $index=0;for($i=0;$i -lt $found.Count;$i++){if($found[$i].Game -eq 'Requiem'){$index=$i+1}}
            & $selector -ManualPaths $root -Selections ([string]$index) -Restore @common 6>$null | Out-Null
        }
        Assert ((Test-Path $spanish) -and (Get-FileHash $spanish).Hash -eq $before) "$route Requiem-only removal changed Black Plague Spanish."
        $state=Get-Content (Join-Path $root 'PenumbraVR.BlackPlague.install.json') -Raw | ConvertFrom-Json
        Assert ($state.components -contains 'black_plague' -and $state.components -contains 'spanish_black_plague' -and $state.components -notcontains 'requiem' -and -not (Test-Path (Join-Path $root 'PenumbraVR.Requiem.Probe.dll'))) "$route component graph was not preserved."
        & $selector -Game BlackPlague -GamePath $root -Restore @common 6>$null | Out-Null
        Assert ((Get-FileHash (Join-Path $root 'Penumbra.exe')).Hash -eq (Get-FileHash $BlackPlagueExe).Hash -and -not (Test-Path $spanish)) "$route original files were not restored."
    }
    'GUI and numbered Requiem-only removal preserve Black Plague Spanish and restore cleanly.'
}finally{
    $absolute=[IO.Path]::GetFullPath($temp);$prefix=[IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\')+'\'
    if($absolute.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase) -and (Test-Path -LiteralPath $absolute)){Remove-Item -LiteralPath $absolute -Recurse -Force}
}
