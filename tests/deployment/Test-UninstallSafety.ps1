[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$BlackPlagueExe,
    [Parameter(Mandatory=$true)][string]$RequiemExe,
    [Parameter(Mandatory=$true)][string]$RetailAlut,
    [string]$BuildRoot,
    [ValidateSet('All','DeletedSettings','AlutType','AlutPostHash','Language')][string]$Check='All'
)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if(-not $BuildRoot){$BuildRoot=Join-Path $repo 'build'}
$tempBase=[IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\')+'\'
$fixture=[IO.Path]::GetFullPath((Join-Path $tempBase ('PvrUninstall-'+[guid]::NewGuid().ToString('N'))))
if(-not $fixture.StartsWith($tempBase,[StringComparison]::OrdinalIgnoreCase)){throw 'Unsafe uninstall fixture.'}
$failures=[Collections.Generic.List[string]]::new()
$global:PvrUninstallBadAlutCopy=$null
function Assert($condition,[string]$message){if(-not $condition){throw $message}}
function Hash([string]$path){(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}
function Write-File([string]$path,[string]$text){
    New-Item -ItemType Directory -Path (Split-Path -Parent $path) -Force | Out-Null
    [IO.File]::WriteAllText($path,$text)
}
# Replace only the OS Documents location. Every install/preview/restore and XML
# read/write below runs production code against real files, including user plans.
function Import-Module {
    [CmdletBinding()]param([Parameter(Position=0)][string]$Name,[switch]$Force)
    Microsoft.PowerShell.Core\Import-Module @PSBoundParameters -Global
    if((Split-Path -Leaf $Name) -eq 'PenumbraVrConfiguration.psm1') {
        & (Get-Module PenumbraVrConfiguration) {
            param($documents)
            $script:FixtureDocuments=$documents
            $script:NativePaths=${function:Get-PvrConfigurationPaths}
            function script:Get-PvrConfigurationPaths {
                param($Game,$RedistRoot)
                $paths=@(& $script:NativePaths -Game $Game -RedistRoot $RedistRoot)
                $paths[1]=Join-Path $script:FixtureDocuments "$Game/settings.cfg"
                $paths
            }
        } $global:PvrUninstallDocuments
        function global:Get-PvrConfigurationPaths {
            param($Game,$RedistRoot)
            & (Get-Module PenumbraVrConfiguration) {
                param($gameId,$root)
                Get-PvrConfigurationPaths -Game $gameId -RedistRoot $root
            } $Game $RedistRoot
        }
    }
}
# Inject one silent copy failure to prove restore verifies the resulting bytes.
# Snapshot creation and rollback use the real Copy-Item and real filesystem.
function Copy-Item {
    [CmdletBinding()]param([string]$LiteralPath,[string]$Destination,[switch]$Force,[switch]$Recurse)
    if($global:PvrUninstallBadAlutCopy -and $Destination -ieq $global:PvrUninstallBadAlutCopy) {
        $global:PvrUninstallBadAlutCopy=$null
        [IO.File]::WriteAllText($Destination,'silently corrupted copy')
        return
    }
    Microsoft.PowerShell.Management\Copy-Item @PSBoundParameters
}
function New-Install([string]$name,[bool]$overture,[bool]$originalSpanish=$false,[bool]$initialSpanish=$false) {
    $root=Join-Path $fixture $name
    $global:PvrUninstallDocuments=Join-Path $root 'Documents'
    $redist=Join-Path $root 'redist'
    $games=if($overture){@('overture')}else{@('black_plague','requiem')}
    & (Join-Path $PSScriptRoot 'New-GameContentFixture.ps1') -Root $redist -Games $games
    $installer=if($overture){Join-Path $repo 'products/overture/scripts/deploy.ps1'}else{Join-Path $repo 'tools/Install-BlackPlagueSteamBootstrap.ps1'}
    $exe=Join-Path $redist 'Penumbra.exe'
    Copy-Item -LiteralPath $BlackPlagueExe -Destination $exe
    if($overture) {
        # Overture deployment supports arbitrary source packages; no product build.
        $package=Join-Path $root 'package'
        Write-File (Join-Path $package 'Penumbra_vr.exe') 'fixture VR executable'
        Write-File (Join-Path $package 'openvr_api.dll') 'fixture loader'
        Write-File (Join-Path $package 'vr/actions.json') '{}'
        Write-File (Join-Path $package 'config/Espanol.lang') '<LANGUAGE />'
        Write-File (Join-Path $package 'release.json') '{"schemaVersion":1,"version":"1.0.0","channel":"release-candidate"}'
        $installArgs=@{InstallRoot=$root;PackageRoot=$package;CommunityTranslations=$true}
        $restoreArgs=@{InstallRoot=$root;Restore=$true}
        $state=Join-Path $root '.penumbravr/deploy-state.json'
    } else {
        Copy-Item -LiteralPath $RequiemExe -Destination (Join-Path $redist 'Requiem.exe')
        Copy-Item -LiteralPath $RetailAlut -Destination (Join-Path $redist 'alut.dll')
        $installArgs=@{GamePath=$exe;BuildRoot=$BuildRoot;RequiemMode='Enable';CommunityTranslations=$true}
        $restoreArgs=@{GamePath=$exe;Restore=$true}
        $state=Join-Path $redist 'PenumbraVR.BlackPlague.install.json'
    }
    Import-Module (Join-Path $repo 'tools/PenumbraVrConfiguration.psm1') -Force
    $paths=@();$languages=@()
    foreach($game in $games) {
        $allowed=@(Get-PvrConfigurationPaths -Game $game -RedistRoot $redist)
        Assert ($allowed.Count -eq 2 -and $allowed[1] -eq (Join-Path $global:PvrUninstallDocuments "$game/settings.cfg")) 'OS Documents substitution failed; refusing installation'
        $language=if($game -eq 'requiem'){'Espanol_exp.lang'}else{'Espanol.lang'}
        $english=if($game -eq 'requiem'){'English_exp.lang'}else{'English.lang'}
        $directory=if($game -eq 'requiem'){'expansion01/config'}else{'config'}
        $languages+=Join-Path $redist "$directory/$language"
        if($originalSpanish){Write-File $languages[-1] 'original Spanish'}
        $defaultPath=if($game -eq 'requiem'){Join-Path $redist 'expansion01/config/requiem_default_settings.cfg'}else{Join-Path $redist 'config/default_settings.cfg'}
        foreach($path in @($defaultPath,(Join-Path $global:PvrUninstallDocuments "$game/settings.cfg"))) {
            Assert ($path.StartsWith($fixture+'\',[StringComparison]::OrdinalIgnoreCase)) 'Settings fixture escaped temporary root'
            $selected=if($initialSpanish){$language}else{$english}
            Write-File $path "<Map File=`"fixture.dae`" /><Game LanguageFile=`"$selected`" CurrentUser=`"keep-save`" /><Graphics LimitFPS=`"true`" />"
            $paths+=$path
        }
    }
    & $installer @installArgs -RecommendedSettings -SettingsScope DefaultAndUserFiles 6>$null | Out-Null
    [pscustomobject]@{Root=$root;Redist=$redist;Installer=$installer;RestoreArgs=$restoreArgs;State=$state;Paths=$paths;Languages=$languages;Games=$games}
}
function Case([string]$name,[scriptblock]$body) {
    try { & $body; Write-Host "PASS: $name" }
    catch { $failures.Add("${name}: $($_.Exception.Message)");Write-Host "FAIL: ${name}: $($_.Exception.Message)" }
}
try {
    foreach($overture in @($false,$true)) {
        $product=if($overture){'Overture'}else{'BP-Requiem'}
        if($Check -in @('All','DeletedSettings')) {
            Case "$product deleted personal settings" {
                $i=New-Install "$product-deleted" $overture
                $userPaths=@($i.Paths | Where-Object {$_ -like "$($i.Root)\Documents\*"})
                Assert ($userPaths.Count -eq $i.Games.Count) 'Fixture did not include personal settings'
                foreach($path in $userPaths){Remove-Item -LiteralPath $path}
                $restore=$i.RestoreArgs
                & $i.Installer @restore -Preflight 6>$null | Out-Null
                Assert (Test-Path -LiteralPath $i.State) 'Preflight removed state'
                & $i.Installer @restore 6>$null | Out-Null
                foreach($path in $userPaths){Assert (-not(Test-Path -LiteralPath $path)) 'Restore recreated user-deleted settings'}
                Assert (-not(Test-Path -LiteralPath $i.State)) 'Uninstall did not complete'
            }
            foreach($kind in @('directory','junction')) {
                Case "$product rejects settings $kind" {
                    $i=New-Install "$product-$kind" $overture
                    $path=@($i.Paths | Where-Object {$_ -like "$($i.Root)\Documents\*"})[0]
                    Remove-Item -LiteralPath $path
                    if($kind -eq 'directory'){New-Item -ItemType Directory -Path $path | Out-Null}
                    else {
                        $outside=Join-Path $i.Root 'outside-settings'
                        Write-File (Join-Path $outside 'settings.cfg') '<Game LanguageFile="English.lang" />'
                        # A parent junction with an absent leaf must not be treated as deletion.
                        Remove-Item -LiteralPath (Join-Path $outside 'settings.cfg')
                        $parent=Split-Path -Parent $path
                        Remove-Item -LiteralPath $parent
                        New-Item -ItemType Junction -Path $parent -Target $outside | Out-Null
                    }
                    $before=Hash $i.State;$failed=$false;$restore=$i.RestoreArgs
                    try{& $i.Installer @restore -Preflight 6>$null | Out-Null}catch{$failed=$true}
                    Assert $failed 'Unsafe settings path was accepted by restore preflight'
                    Assert ((Hash $i.State) -eq $before) 'Rejected restore changed state'
                    if($kind -eq 'junction'){[IO.Directory]::Delete((Split-Path -Parent $path))}
                }
            }
        }
        if($Check -in @('All','Language')) {
            foreach($mode in @('selected-later','recorded-original','custom','original-payload','no-english','late-failure')) {
                Case "$product uninstall language $mode" {
                    $i=New-Install "$product-$mode" $overture ($mode -eq 'original-payload') ($mode -eq 'recorded-original')
                    foreach($path in $i.Paths) {
                        $exp=$path -match '[\\/]Documents[\\/]requiem[\\/]|[\\/]expansion01[\\/]';$language=if($exp){'Espanol_exp.lang'}else{'Espanol.lang'}
                        if($mode -eq 'custom') {
                            $directory=if($exp){'expansion01/config'}else{'config'}
                            Write-File (Join-Path $i.Redist "$directory/custom.lang") '<LANGUAGE/>'
                            $language='custom.lang'
                        }
                        $text=[IO.File]::ReadAllText($path) -replace 'LanguageFile="[^"]*"',"LanguageFile=`"$language`""
                        [IO.File]::WriteAllText($path,$text)
                    }
                    $restore=$i.RestoreArgs
                    $before=@($i.Paths | ForEach-Object {Hash $_})
                    if($mode -eq 'no-english') {
                        foreach($relative in @('config/English.lang','expansion01/config/English_exp.lang')){
                            $englishPath=Join-Path $i.Redist $relative
                            if(Test-Path -LiteralPath $englishPath){Remove-Item -LiteralPath $englishPath}
                        }
                        $failed=$false;try{& $i.Installer @restore -Preflight 6>$null | Out-Null}catch{$failed=$true}
                        Assert $failed 'Missing English fallback did not block uninstall before writes'
                        Assert (Test-Path -LiteralPath $i.State) 'Failed preflight lost state'
                    } elseif($mode -eq 'late-failure') {
                        $lockPath=if($overture){Join-Path $i.Redist 'openvr_api.dll'}else{Join-Path $i.Redist 'PenumbraVR.BlackPlague.Probe.dll'}
                        $lock=[IO.File]::Open($lockPath,'Open','Read','Read')
                        try {
                            $failure=$null;try{& $i.Installer @restore 6>$null | Out-Null}catch{$failure=$_.Exception.Message}
                            Assert ($failure -like '*managed files were restored*') "Late uninstall did not roll back: $failure"
                        } finally {$lock.Dispose()}
                        Assert (Test-Path -LiteralPath $i.State) 'Rollback lost installation state'
                        foreach($path in $i.Languages){Assert (Test-Path -LiteralPath $path) 'Rollback lost selected translation'}
                    } else {
                        & $i.Installer @restore -Preflight 6>$null | Out-Null
                        Assert ((@($i.Paths | ForEach-Object {Hash $_}) -join ',') -eq ($before -join ',')) 'Preview changed settings'
                        & $i.Installer @restore 6>$null | Out-Null
                        Assert (-not(Test-Path -LiteralPath $i.State)) 'Uninstall left state'
                        foreach($path in $i.Paths) {
                            $expected=if($mode -eq 'custom'){'custom.lang'}elseif($mode -eq 'original-payload'){'Espanol.lang'}else{'English.lang'}
                            if($path -match '[\\/]Documents[\\/]requiem[\\/]|[\\/]expansion01[\\/]'){$expected=$expected.Replace('Espanol.lang','Espanol_exp.lang').Replace('English.lang','English_exp.lang')}
                            $text=[IO.File]::ReadAllText($path)
                            Assert ($text.Contains("LanguageFile=`"$expected`"") -and $text.Contains('CurrentUser="keep-save"') -and $text.Contains('LimitFPS="true"')) "Invalid language or lost preferences after uninstall: $path : $text"
                        }
                        foreach($path in $i.Languages){Assert ((Test-Path -LiteralPath $path) -eq ($mode -eq 'original-payload')) 'Wrong translation ownership after uninstall'}
                    }
                    if($mode -in @('no-english','late-failure')) {
                        Assert ((@($i.Paths | ForEach-Object {Hash $_}) -join ',') -eq ($before -join ',')) 'Failed uninstall changed settings'
                    }
                }
            }
        }
    }
    if($Check -in @('All','AlutType')) {
        Case 'ALUT directory conflict preserves backup and state' {
            $i=New-Install 'alut-directory' $false
            $alut=Join-Path $i.Redist 'alut.dll';$backup=Join-Path $i.Redist 'PenumbraVR_alut_original.dll'
            $before=Hash $i.State;$original=Hash $backup
            Remove-Item -LiteralPath $alut
            New-Item -ItemType Directory -Path $alut | Out-Null
            $failed=$false;$restore=$i.RestoreArgs
            try{& $i.Installer @restore 6>$null | Out-Null}catch{$failed=$true}
            Assert $failed 'Uninstall accepted an ALUT directory and discarded the original backup'
            Assert ((Hash $backup) -eq $original -and (Hash $i.State) -eq $before) 'ALUT conflict lost recovery ownership'
            Assert (@(Get-ChildItem -LiteralPath $alut -Force).Count -eq 0) 'Restore copied ALUT into a directory'
        }
    }
    if($Check -in @('All','AlutPostHash')) {
        Case 'ALUT silent copy corruption rolls back' {
            $i=New-Install 'alut-posthash' $false
            $alut=Join-Path $i.Redist 'alut.dll';$backup=Join-Path $i.Redist 'PenumbraVR_alut_original.dll'
            $before=Hash $i.State;$proxy=Hash $alut;$original=Hash $backup
            $global:PvrUninstallBadAlutCopy=$alut;$restore=$i.RestoreArgs;$failure=$null
            try{& $i.Installer @restore 6>$null | Out-Null}catch{$failure=$_.Exception.Message}
            Assert ($failure -like '*ALUT*hash verification*') "Uninstall did not detect corrupted ALUT bytes: $failure"
            Assert ((Hash $backup) -eq $original -and (Hash $i.State) -eq $before -and (Hash $alut) -eq $proxy) 'ALUT posthash failure lost original or installed state'
            & $i.Installer @restore 6>$null | Out-Null
            Assert ((Hash $alut) -eq $original -and -not(Test-Path -LiteralPath $backup)) 'Verified ALUT restore did not complete'
        }
    }
    if($failures.Count){throw ($failures -join "`n")}
    Write-Host 'Uninstall safety: all selected transaction fixtures passed.'
} finally {
    if(Test-Path -LiteralPath $fixture){Remove-Item -LiteralPath $fixture -Recurse -Force}
}
