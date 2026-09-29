# Presentation only. Exact-build checks and writes remain owned by the selector.
function ConvertTo-PvrUiGame {
    param($Entry,$Expansion,$Report,[string]$PackageVersion,[ValidateSet('en','es')][string]$Language='en')
    $product=if($Entry.ManagedProduct -in @('Overture','Black Plague')){$Entry.ManagedProduct}elseif($Entry.Game -in @('Overture','Black Plague')){$Entry.Game}else{'Unknown'}
    $identityConflict=$Entry.Managed -and $Entry.Game -in @('Overture','Black Plague') -and $Entry.Game -ne $product
    $game=switch($product){'Overture'{'Overture'} 'Black Plague'{'BlackPlague'} default{'Unknown'}}
    $components=@($Report.Components)
    $reqAvailable=$game -eq 'BlackPlague' -and $Expansion -and $Expansion.Game -eq 'Requiem' -and $Expansion.Installable -and (Split-Path -Parent $Expansion.Path) -ieq (Split-Path -Parent $Entry.Path)
    $state=if($Entry.NeedsRecovery){'RecoveryRequired'}elseif($Entry.Managed -and ($identityConflict -or -not $Report -or -not $Report.InstallationVerified -or -not $Entry.Installable)){'NeedsRepair'}elseif(-not $Entry.KnownBuild){'Unsupported'}elseif(-not $Entry.Installable){'Incomplete'}elseif(-not $Entry.Managed){'NotInstalled'}else{'Installed'}
    if($state -eq 'Installed'){
        try{if([version]$Report.Version -lt [version]$PackageVersion){$state='UpdateAvailable'}}catch{}
    }
    $spanishDefault= -not $Entry.Managed -and $Language -eq 'es'
    $baseSpanish=if($game -eq 'Overture'){$components -contains 'spanish_translation'}else{$components -contains 'spanish_black_plague'}
    $reqInstalled=$components -contains 'requiem'
    [pscustomobject]@{
        Game=$game;Title=$(if($game -eq 'Unknown'){'Unknown game'}else{'Penumbra: '+$product})
        Path=$Entry.Path;InstallRoot=$Entry.InstallRoot;StatusKey=$state;Version=[string]$Report.Version
        Managed=[bool]$Entry.Managed;NeedsRecovery=[bool]$Entry.NeedsRecovery;Installable=[bool]$Entry.Installable
        Selectable=($game -ne 'Unknown' -and ($Entry.Installable -or $Entry.Managed -or $Entry.NeedsRecovery))
        Selected=($game -ne 'Unknown' -and ($Entry.Installable -or $Entry.Managed))
        MaintenanceSelected=($game -ne 'Unknown' -and ($Entry.Managed -or $Entry.NeedsRecovery))
        Issues=@($Entry.Issues)+@(if($identityConflict){'The executable differs from the recorded installation owner. Use maintenance to restore the owned product.'});Report=$Report
        RequiemPresent=([bool]$Entry.RequiemPresent -or [bool]$Expansion);RequiemAvailable=[bool]$reqAvailable
        RequiemInstalled=$reqInstalled;RequiemSelected=($reqInstalled -or (-not $Entry.Managed -and $reqAvailable))
        SpanishBase=($baseSpanish -or $spanishDefault);SpanishRequiem=(($components -contains 'spanish_requiem') -or ($spanishDefault -and $reqAvailable))
        Textures=($components -contains 'texture_enhancements')
        OriginalSpanishBase=$baseSpanish;OriginalSpanishRequiem=($components -contains 'spanish_requiem');OriginalTextures=($components -contains 'texture_enhancements')
        RemoveBase=$false;RemoveRequiem=$false
    }
}

function Get-PvrUiAction {
    param([object[]]$Cards=@(),[bool]$RecommendedSettings=$true)
    $selected=@($Cards | Where-Object {$_.Selected})
    $enabled=$selected.Count -gt 0 -and -not @($selected | Where-Object {$_.StatusKey -in @('Unsupported','Incomplete','RecoveryRequired')}).Count
    $key=if(@($selected | Where-Object {$_.StatusKey -eq 'NeedsRepair'}).Count){'Repair'}elseif(@($selected | Where-Object {-not $_.Managed}).Count){'Install'}elseif(@($selected | Where-Object {$_.StatusKey -eq 'UpdateAvailable'}).Count){'Update'}else{'ApplyChanges'}
    [pscustomobject]@{Key=$key;Enabled=$enabled}
}

function New-PvrUiRequests {
    param([object[]]$Cards,[ValidateSet('Install','Repair','Verify','Diagnostics','Recover','Uninstall')][string]$Operation,
          [bool]$RecommendedSettings=$true,[ValidateSet('DefaultFiles','DefaultAndUserFiles')][string]$SettingsScope='DefaultAndUserFiles')
    foreach($card in $Cards){
        if($Operation -eq 'Uninstall'){
            if(-not $card.RemoveBase -and -not $card.RemoveRequiem){continue}
        }elseif(-not $card.Selected){continue}
        if($card.Game -eq 'Unknown'){throw 'Unsupported game cannot be modified.'}
        $a=@{Game=$card.Game;GamePath=$card.Path}
        $actual=$Operation
        switch($Operation){
            'Install' {
                if($card.NeedsRecovery -or $card.StatusKey -in @('Unsupported','Incomplete')){throw 'Selected installation must be corrected before installing.'}
                if($card.StatusKey -eq 'NeedsRepair'){$a.Repair=$true;$actual='Repair'}else{
                    $a.RecommendedSettings=$RecommendedSettings;$a.SettingsScope=$SettingsScope
                    if($card.Game -eq 'Overture'){$a.InstallSpanishOverture=[bool]$card.SpanishBase;$a.TextureEnhancements=[bool]$card.Textures}else{
                        if($card.RequiemSelected -and -not $card.RequiemAvailable){throw 'Requiem requires a supported expansion in the same Black Plague folder.'}
                        $a.RequiemMode=if($card.RequiemSelected){'Enable'}else{'Disable'}
                        $a.InstallSpanishBlackPlague=[bool]$card.SpanishBase
                        $a.InstallSpanishRequiem=[bool]($card.SpanishRequiem -and $card.RequiemSelected)
                    }
                }
            }
            'Repair' {if(-not $card.Managed -or $card.NeedsRecovery){throw 'Repair requires a managed installation without an interrupted transaction.'};$a.Repair=$true}
            'Verify' {$a.Verify=$true}
            'Recover' {if(-not $card.NeedsRecovery){continue};$a.Recover=$true}
            'Uninstall' {if(-not $card.Managed -or $card.NeedsRecovery){throw 'Recover an interrupted installation before uninstalling.'};$a.Restore=$true;if(-not $card.RemoveBase){if($card.Game -ne 'BlackPlague' -or -not $card.RequiemInstalled){throw 'No installed expansion selected.'};$a.Game='Requiem'}}
        }
        [pscustomobject]@{Card=$card;Operation=$actual;Arguments=$a;RequiresReview=($actual -in @('Repair','Recover','Uninstall'))}
    }
}
function Get-PvrUiCheckGroups {
    param($Report)
    $buckets=[ordered]@{Executable=@();Framework=@();Bindings=@();Dependencies=@();Configuration=@()}
    foreach($check in $Report.Checks){
        $key=switch -Regex ($check.Name){
            '^Exact build|^(Core|Owned) Penumbra\.exe' {'Executable';break}
            '^Action manifest|^Binding |^(Owned|Core) vr/' {'Bindings';break}
            '^Configuration |^config/|\.lang$' {'Configuration';break}
            '^State |^Component graph|^Requiem component|^Owned |^Core |^Backup |^Original |^Managed state' {'Framework';break}
            default {'Dependencies'}
        }
        $buckets[$key]+=$check
    }
    foreach($key in $buckets.Keys){$checks=@($buckets[$key]);[pscustomobject]@{Key=$key;Passed=($checks.Count -gt 0 -and -not @($checks | Where-Object {$_.Required -and $_.Status -notin @('present','provided')}).Count);Checks=$checks}}
}
function Format-PvrUiOutcomes {
    param([object[]]$Requests,[string]$Label)
    (@($Requests | ForEach-Object {$_.Card.Title+' — '+$_.Card.InstallRoot+$(if($Label){' — '+$Label})}) -join "`r`n")
}
Export-ModuleMember -Function ConvertTo-PvrUiGame,Get-PvrUiAction,New-PvrUiRequests,Get-PvrUiCheckGroups,Format-PvrUiOutcomes
