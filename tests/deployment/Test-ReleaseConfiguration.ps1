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
    Write-Host 'Configuration: targeted fragments, unknown values/comments, idempotence, user-edit restore and DTD rejection passed.'
} finally {if(Test-Path $temp){Remove-Item -LiteralPath $temp -Recurse -Force}}
