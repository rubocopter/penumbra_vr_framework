[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$module=Join-Path $repo 'tools/PenumbraVrInstallerUi.psm1'
if(-not (Test-Path $module)){throw 'Installer presentation model is missing.'}
Import-Module $module -Force
function Assert($ok,[string]$reason){if(-not $ok){throw $reason}}
function Entry([string]$game,[string]$path='C:\Games\BP\redist\Penumbra.exe'){
    [pscustomobject]@{Game=$game;Path=$path;InstallRoot=(Split-Path -Parent (Split-Path -Parent $path));Managed=$false;ManagedProduct=$null;Installable=$true;KnownBuild=$true;Status='available';NeedsRecovery=$false;RequiemPresent=$false;Issues=@()}
}
$bp=Entry 'Black Plague'
$req=Entry 'Requiem' 'C:\Games\BP\redist\Requiem.exe'
$o=Entry 'Overture' 'C:\Games\Overture\redist\Penumbra.exe'
$fresh=ConvertTo-PvrUiGame -Entry $bp -Expansion $req -PackageVersion '1.0.0' -Language en
Assert ($fresh.StatusKey -eq 'NotInstalled' -and $fresh.RequiemAvailable -and $fresh.RequiemSelected -and -not $fresh.SpanishBase -and -not $fresh.SpanishRequiem) 'Fresh English defaults must expose nested Requiem without translations.'
$spanish=ConvertTo-PvrUiGame -Entry $o -PackageVersion '1.0.0' -Language es
Assert ($spanish.SpanishBase -and -not $spanish.Textures) 'Spanish selects translation, textures remain optional.'
$bp.Managed=$true
$report=[pscustomobject]@{Version='1.0.0';Components=@('shared','black_plague','requiem','spanish_black_plague','spanish_requiem');InstallationVerified=$true;Checks=@()}
$owned=ConvertTo-PvrUiGame -Entry $bp -Expansion $req -Report $report -PackageVersion '1.0.0' -Language en
Assert ($owned.StatusKey -eq 'Installed' -and $owned.SpanishBase -and $owned.SpanishRequiem -and $owned.RequiemInstalled) 'Owned components must remain selected in English.'
$oldReport=[pscustomobject]@{Version='0.9.0';Components=@('shared','black_plague');InstallationVerified=$true;Checks=@()}
$older=ConvertTo-PvrUiGame -Entry $bp -Expansion $req -Report $oldReport -PackageVersion '1.0.0' -Language en
Assert ($older.StatusKey -eq 'UpdateAvailable' -and -not $older.RequiemSelected) 'Updating an owned base must not silently add Requiem.'
Assert ((Get-PvrUiAction -Cards @($older) -RecommendedSettings $false).Key -eq 'Update') 'Older installed version must offer Update.'
$brokenReport=[pscustomobject]@{Version='1.0.0';Components=$report.Components;InstallationVerified=$false;Checks=@([pscustomobject]@{Name='Owned alut.dll';Required=$true;Status='invalid';Remediation='Repair'})}
$broken=ConvertTo-PvrUiGame -Entry $bp -Expansion $req -Report $brokenReport -PackageVersion '1.0.0' -Language en
Assert ($broken.StatusKey -eq 'NeedsRepair' -and (Get-PvrUiAction -Cards @($broken)).Key -eq 'Repair') 'Damaged files must route to repair.'
$repairRequest=@(New-PvrUiRequests -Cards @($broken) -Operation Install)
Assert ($repairRequest[0].Arguments.Repair -and $repairRequest[0].RequiresReview) 'Automatic repair must preserve the maintenance review step.'
$conflicting=Entry 'Black Plague' 'C:\Games\Overture\redist\Penumbra.exe';$conflicting.Managed=$true;$conflicting.ManagedProduct='Overture'
$ownedOverture=ConvertTo-PvrUiGame -Entry $conflicting -Report ([pscustomobject]@{Version='1.0.0';Components=@('shared','overture');InstallationVerified=$true}) -PackageVersion '1.0.0'
$ownerRepair=@(New-PvrUiRequests -Cards @($ownedOverture) -Operation Install)
Assert ($ownedOverture.StatusKey -eq 'NeedsRepair' -and $ownerRepair[0].Arguments.Game -eq 'Overture' -and $ownerRepair[0].Arguments.Repair) 'A conflicting recognized executable must never override the recorded maintenance owner.'
$bp.NeedsRecovery=$true;$bp.Installable=$false;$bp.Status='recovery-required'
$recovery=ConvertTo-PvrUiGame -Entry $bp -PackageVersion '1.0.0'
Assert (-not (Get-PvrUiAction -Cards @($recovery)).Enabled) 'An interrupted transaction must block the main action.'
$unknown=Entry 'Unknown';$unknown.Installable=$false;$unknown.KnownBuild=$false;$unknown.Status='unknown-build'
$unsupported=ConvertTo-PvrUiGame -Entry $unknown -PackageVersion '1.0.0'
Assert ($unsupported.StatusKey -eq 'Unsupported' -and -not $unsupported.Selectable) 'Unsupported executables must not be selectable.'
$fresh.SpanishBase=$false;$fresh.SpanishRequiem=$false
$spanish.Textures=$true
$calls=@(New-PvrUiRequests -Cards @($fresh,$spanish) -Operation Install -RecommendedSettings $true -SettingsScope DefaultFiles)
Assert ($calls.Count -eq 2 -and $calls[0].Arguments.RequiemMode -eq 'Enable' -and -not $calls[0].Arguments.InstallSpanishBlackPlague -and $calls[1].Arguments.InstallSpanishOverture -and $calls[1].Arguments.TextureEnhancements -and $calls[1].Arguments.SettingsScope -eq 'DefaultFiles') 'Per-game choices must reach the selector independently.'
$copy=ConvertTo-PvrUiGame -Entry (Entry 'Black Plague' 'D:\Games\BP\redist\Penumbra.exe') -Expansion $req -PackageVersion '1.0.0'
$copy.SpanishBase=$true
$two=@(New-PvrUiRequests -Cards @($fresh,$copy) -Operation Install)
Assert ($two.Count -eq 2 -and $two[0].Arguments.GamePath -ne $two[1].Arguments.GamePath -and -not $two[0].Arguments.InstallSpanishBlackPlague -and $two[1].Arguments.InstallSpanishBlackPlague -and -not $copy.RequiemAvailable) 'Two copies must retain independent selections; expansion cannot cross roots.'
$outcomes=Format-PvrUiOutcomes -Requests @($calls[0],$two[1]) -Label 'Completed'
Assert ($outcomes -match [regex]::Escape('C:\Games\BP') -and $outcomes -match [regex]::Escape('D:\Games\BP')) 'Results must identify every selected copy by its root.'
$owned.RemoveRequiem=$true
$remove=@(New-PvrUiRequests -Cards @($owned) -Operation Uninstall)
Assert ($remove.Count -eq 1 -and $remove[0].Arguments.Game -eq 'Requiem' -and $remove[0].Arguments.Restore) 'Requiem-only removal must use the owning internal operation.'
$owned.RemoveBase=$true
$remove=@(New-PvrUiRequests -Cards @($owned) -Operation Uninstall)
Assert ($remove.Count -eq 1 -and $remove[0].Arguments.Game -eq 'BlackPlague') 'Removing Black Plague must restore its shared root once.'
$owned.Selected=$false
Assert (@(New-PvrUiRequests -Cards @($owned) -Operation Install).Count -eq 0) 'Unchecked game must not produce a write request.'
$checkReport=[pscustomobject]@{Checks=@(
    [pscustomobject]@{Name='Exact build Penumbra.exe';Required=$true;Status='present'},
    [pscustomobject]@{Name='Owned alut.dll';Required=$true;Status='present'},
    [pscustomobject]@{Name='Action manifest and bindings';Required=$true;Status='present'},
    [pscustomobject]@{Name='Import SDL.dll';Required=$true;Status='missing'},
    [pscustomobject]@{Name='Configuration black_plague';Required=$true;Status='present'}
)}
$groups=@(Get-PvrUiCheckGroups -Report $checkReport)
Assert ($groups.Count -eq 5 -and ($groups | Where-Object {$_.Key -eq 'Dependencies'}).Passed -eq $false -and ($groups | Where-Object {$_.Key -eq 'Bindings'}).Passed) 'Friendly check groups must expose a missing dependency without masking successful binding checks.'
'Installer UI state and routing checks passed.'
