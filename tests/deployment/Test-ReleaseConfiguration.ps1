$ErrorActionPreference='Stop'
$repo=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Import-Module (Join-Path $repo 'tools/PenumbraVrConfiguration.psm1') -Force
$temp=Join-Path ([IO.Path]::GetTempPath()) ('PvrConfig-'+[guid]::NewGuid().ToString('N'))
function Assert($condition,[string]$message){if(-not $condition){throw $message}}
try {
    New-Item -ItemType Directory -Path $temp | Out-Null
    $path=Join-Path $temp 'settings.cfg'
    $original='<!--preserve--><Graphics LimitFPS="true" PostEffects="false" Refractions="false" Custom="kept" /><Screen Width="1234" Vsync="true" /><Game LanguageFile="custom.lang" /><VR PlayerHeight="1.9" />'
    [IO.File]::WriteAllText($path,$original)
    $plan=Get-PvrConfigurationPlan -Game black_plague -Path $path
    Assert (-not @($plan.Changes | Where-Object {$_.Attribute -match 'Height|Language|Width|Mirror'}).Count) 'Personal values included'
    Set-PvrConfiguration -Plan $plan
    $text=[IO.File]::ReadAllText($path)
    Assert ($text.Contains('<!--preserve-->') -and $text.Contains('Custom="kept"') -and $text.Contains('Width="1234"') -and $text.Contains('PostEffects="true"') -and $text.Contains('Refractions="true"')) 'Required state or unknown data lost'
    $hash=(Get-FileHash $path).Hash
    Set-PvrConfiguration -Plan (Get-PvrConfigurationPlan -Game black_plague -Path $path)
    Assert ((Get-FileHash $path).Hash -eq $hash) 'Repeated apply changed file'
    $text=$text.Replace('PostEffects="true"','PostEffects="custom-user-edit"')
    [IO.File]::WriteAllText($path,$text)
    Restore-PvrConfiguration -Plan $plan
    $text=[IO.File]::ReadAllText($path)
    Assert ($text.Contains('PostEffects="custom-user-edit"') -and $text.Contains('LimitFPS="true"') -and $text.Contains('Refractions="false"') -and -not $text.Contains('FSAA=')) 'Restore overwrote user edit or lost original attributes'
    [IO.File]::WriteAllText($path,'<Game LanguageFile="custom.lang" />')
    $before=(Get-FileHash $path).Hash
    Test-PvrConfigurationPlan -Plan $plan -Restore
    Restore-PvrConfiguration -Plan $plan
    Assert ((Get-FileHash $path).Hash -eq $before) 'Restore recreated a section deleted by the user'
    [IO.File]::WriteAllText($path,'<!DOCTYPE x [<!ENTITY secret SYSTEM "file:///test">]><Graphics/>')
    $failed=$false;try{Get-PvrConfigurationPlan -Game overture -Path $path | Out-Null}catch{$failed=$true}
    Assert $failed 'External entity accepted'
    $root=Join-Path $temp 'redist'
    New-Item -ItemType Directory -Path (Join-Path $root 'config') -Force | Out-Null
    $settings=Join-Path $root 'config/default_settings.cfg'
    [IO.File]::WriteAllText((Join-Path $root 'config/English.lang'),'<LANGUAGE/>')
    [IO.File]::WriteAllText($settings,'<Game LanguageFile="Espanol.lang" CurrentUser="my-save" /><VR PlayerHeight="1.9" /><Graphics LimitFPS="true" />')
    $records=@(Get-PvrConfigurationRecords -Game overture -RedistRoot $root -SettingsScope DefaultFiles)
    Assert (@($records.Changes | Where-Object {$_.Attribute -eq 'LanguageFile' -and $_.Applied -ieq 'English.lang'}).Count -eq 1) 'Missing selected language was not repaired without recommended graphics'
    foreach($record in $records){Set-PvrConfiguration -Plan $record}
    $text=[IO.File]::ReadAllText($settings)
    Assert ($text.Contains('LanguageFile="English.lang"') -and $text.Contains('CurrentUser="my-save"') -and $text.Contains('PlayerHeight="1.9"') -and $text.Contains('LimitFPS="true"')) 'Language repair changed personal or graphical settings'
    foreach($record in $records){Restore-PvrConfiguration -Plan $record}
    Assert ([IO.File]::ReadAllText($settings).Contains('LanguageFile="Espanol.lang"')) 'Language ownership did not restore original'
    [IO.File]::WriteAllText((Join-Path $root 'config/Espanol.lang'),'<LANGUAGE/>')
    $records=@(Get-PvrConfigurationRecords -Game overture -RedistRoot $root -SettingsScope DefaultFiles)
    Assert (-not @($records.Changes | Where-Object {$_.Attribute -eq 'LanguageFile'}).Count) 'Installed language preference was overwritten'
    # Substitute only the OS Documents path; parse/write real isolated files.
    $userSettings=Join-Path $temp 'Documents/settings.cfg'
    New-Item -ItemType Directory -Path (Split-Path -Parent $userSettings) | Out-Null
    $module=Get-Module PenumbraVrConfiguration
    & $module {param($paths) $script:FixturePaths=$paths; function script:Get-PvrConfigurationPaths {param($Game,$RedistRoot) $script:FixturePaths}} @($settings,$userSettings)
    foreach($oldRecord in @($false,$true)) {
        [IO.File]::WriteAllText($userSettings,'<Game LanguageFile="missing.lang" />')
        $previous=Get-PvrConfigurationPlan -Game overture -Path $userSettings -RedistRoot $root -ApplyRecommendedSettings:$false
        if($oldRecord){$previous.PSObject.Properties.Remove('ApplyKeys')}
        Set-PvrConfiguration -Plan $previous
        [IO.File]::WriteAllText($userSettings,'<Game LanguageFile="Espanol.lang" />')
        $before=(Get-FileHash $userSettings).Hash
        $records=@(Get-PvrConfigurationRecords -Game overture -RedistRoot $root -Previous @($previous) -SettingsScope DefaultFiles)
        foreach($record in $records){Test-PvrConfigurationPlan -Plan $record; Set-PvrConfiguration -Plan $record}
        Assert ((Get-FileHash $userSettings).Hash -eq $before) 'DefaultFiles touched retained Documents ownership'
        Assert (@($records | Where-Object {$_.Path -eq $userSettings}).Count -eq 1) 'DefaultFiles lost Documents restoration ownership'
    }
    Write-Host 'Configuration: targeted fragments, unknown values/comments, idempotence, user-edit restore and DTD rejection passed.'
} finally {if(Test-Path $temp){Remove-Item -LiteralPath $temp -Recurse -Force}}
