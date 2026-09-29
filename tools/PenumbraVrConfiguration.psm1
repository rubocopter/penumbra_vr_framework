$ErrorActionPreference='Stop'
Import-Module (Join-Path $PSScriptRoot 'PenumbraVrPrerequisites.psm1')
$script:Profile=Get-Content -LiteralPath (Join-Path (Split-Path -Parent $PSScriptRoot) 'assets/settings/installer-game-profile.json') -Raw | ConvertFrom-Json
function Get-PvrConfigurationPaths {
    param([ValidateSet('overture','black_plague','requiem')][string]$Game,[string]$RedistRoot)
    $default=if($Game -eq 'requiem'){'expansion01/config/requiem_default_settings.cfg'}else{'config/default_settings.cfg'}
    $user=switch($Game){'overture'{'Penumbra Overture/Episode1/settings.cfg'} 'black_plague'{'Penumbra/Black Plague/settings.cfg'} 'requiem'{'Penumbra/Requiem/settings.cfg'}}
    @([IO.Path]::GetFullPath((Join-Path $RedistRoot $default)),[IO.Path]::GetFullPath((Join-Path ([Environment]::GetFolderPath('MyDocuments')) $user)))
}
function Get-PvrConfigurationRecords {
    param([string]$Game,[string]$RedistRoot,[object[]]$Previous=@(),[switch]$Apply,[ValidateSet('DefaultFiles','DefaultAndUserFiles')][string]$SettingsScope='DefaultAndUserFiles')
    $Previous=@($Previous | Where-Object {$null -ne $_})
    $allowed=@(Get-PvrConfigurationPaths -Game $Game -RedistRoot $RedistRoot)
    foreach($record in @($Previous)) {
        if($record.Game -ne $Game -or [IO.Path]::GetFullPath([string]$record.Path) -notin $allowed){throw 'Recorded settings path is outside the configuration allowlist.'}
    }
    if(-not $Apply){return $Previous}
    $selectedPaths=if($SettingsScope -eq 'DefaultFiles'){@($allowed[0])}else{$allowed}
    foreach($path in $selectedPaths) {
        if(-not (Test-Path -LiteralPath $path -PathType Leaf)){continue}
        $plan=Get-PvrConfigurationPlan -Game $Game -Path $path
        $prior=@($Previous | Where-Object {$_.Path -ieq $path})
        if($prior.Count -gt 1){throw 'Duplicate configuration ownership.'}
        if($prior.Count) {
            $keys=@{}
            foreach($change in @($plan.Changes)){$keys[$change.Section+'/'+$change.Attribute]=$change}
            foreach($change in @($prior[0].Changes)){
                $key=$change.Section+'/'+$change.Attribute
                if($keys.ContainsKey($key)) {$keys[$key].HadOriginal=$change.HadOriginal;$keys[$key].Original=$change.Original}
                else {$keys[$key]=$change}
            }
            $plan.Changes=@($keys.Values | Sort-Object Section,Attribute)
        }
        $plan
    }
    if($SettingsScope -eq 'DefaultFiles'){foreach($record in $Previous){if($record.Path -ine $allowed[0]){$record}}}
}
function Get-PvrConfigurationPlan {
    param([ValidateSet('overture','black_plague','requiem')][string]$Game,[string]$Path)
    $doc=Read-PvrConfig $Path
    $changes=@()
    foreach($section in @($script:Profile.common.PSObject.Properties)+@($script:Profile.games.$Game.PSObject.Properties)) {
        $nodes=@($doc.SelectNodes('/PvrRoot/'+$section.Name))
        if($nodes.Count -gt 1){throw 'Duplicate configuration section.'}
        if(-not $nodes.Count){continue}
        foreach($attr in $section.Value.PSObject.Properties) {
            $node=$nodes[0];$before=$node.GetAttribute($attr.Name);$had=$node.HasAttribute($attr.Name)
            if(-not $had -or $before -cne [string]$attr.Value) {
                $changes+=[pscustomobject]@{Section=$section.Name;Attribute=$attr.Name;HadOriginal=$had;Original=$before;Applied=[string]$attr.Value}
            }
        }
    }
    [pscustomobject]@{Game=$Game;Path=[IO.Path]::GetFullPath($Path);BeforeSha256=(Get-FileHash -LiteralPath $Path).Hash;Changes=@($changes)}
}
function Write-PvrConfiguration($Doc,[string]$Path) {
    $temp=$Path+'.pvr-'+[guid]::NewGuid().ToString('N')
    $backup=$temp+'.replaced'
    try {
        [IO.File]::WriteAllText($temp,$Doc.DocumentElement.InnerXml,[Text.UTF8Encoding]::new($false))
        Read-PvrConfig $temp | Out-Null
        # PS5.1 coerces a null String argument to an empty path; use an explicit
        # temporary backup. The deployment journal owns interruption recovery.
        [IO.File]::Replace($temp,$Path,$backup)
    } finally {foreach($file in @($temp,$backup)){if(Test-Path -LiteralPath $file){Remove-Item -LiteralPath $file -Force}}}
}
function Invoke-PvrConfiguration($Plan,[bool]$Restore,[bool]$ValidateOnly=$false) {
    $doc=Read-PvrConfig $Plan.Path
    if(-not $Restore -and (Get-FileHash -LiteralPath $Plan.Path).Hash -ine $Plan.BeforeSha256) {throw 'Configuration changed after preview; generate a new plan.'}
    $changed=$false
    foreach($change in @($Plan.Changes)) {
        $allowed=@($script:Profile.common.PSObject.Properties)+@($script:Profile.games.($Plan.Game).PSObject.Properties)
        $value=@($allowed | Where-Object {$_.Name -ceq $change.Section} | ForEach-Object {$_.Value.PSObject.Properties} | Where-Object {$_.Name -ceq $change.Attribute})
        if($value.Count -ne 1 -or [string]$value[0].Value -cne [string]$change.Applied -or [string]$change.Original -match '[<>]' -or ([string]$change.Original).Length -gt 128) {throw 'Invalid recorded configuration change.'}
        $nodes=@($doc.SelectNodes('/PvrRoot/'+$change.Section))
        if($Restore -and -not $nodes.Count){continue}
        if($nodes.Count -ne 1){throw 'Configuration section changed.'}
        $node=$nodes[0]
        if($Restore) {
            if(-not $node.HasAttribute($change.Attribute) -or $node.GetAttribute($change.Attribute) -cne $change.Applied) {continue}
            if($change.HadOriginal){$node.SetAttribute($change.Attribute,[string]$change.Original)}else{$node.RemoveAttribute($change.Attribute)}
        } else {$node.SetAttribute($change.Attribute,[string]$change.Applied)}
        $changed=$true
    }
    if($changed -and -not $ValidateOnly){Write-PvrConfiguration $doc $Plan.Path}
}
function Set-PvrConfiguration {param($Plan) Invoke-PvrConfiguration $Plan $false}
function Restore-PvrConfiguration {param($Plan) Invoke-PvrConfiguration $Plan $true}
function Test-PvrConfigurationPlan {param($Plan,[switch]$Restore) Invoke-PvrConfiguration $Plan ([bool]$Restore) $true}
Export-ModuleMember -Function Get-PvrConfigurationPlan,Set-PvrConfiguration,Restore-PvrConfiguration,Get-PvrConfigurationPaths,Get-PvrConfigurationRecords,Test-PvrConfigurationPlan
