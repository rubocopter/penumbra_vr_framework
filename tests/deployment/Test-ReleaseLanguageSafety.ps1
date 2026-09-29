[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$BlackPlagueExe,
      [Parameter(Mandatory=$true)][string]$RequiemExe,
      [Parameter(Mandatory=$true)][string]$RetailAlut)
$ErrorActionPreference='Stop'
$PSDefaultParameterValues=@{'*:SettingsScope'='DefaultFiles'}
$repo=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$installer=Join-Path $repo 'tools/Install-BlackPlagueSteamBootstrap.ps1'
$temp=[IO.Path]::GetFullPath((Join-Path ([IO.Path]::GetTempPath()) ('PvrLanguage-'+[guid]::NewGuid().ToString('N'))))
function Assert($condition,[string]$message){if(-not $condition){throw $message}}
try {
    New-Item -ItemType Directory -Path $temp | Out-Null
    & (Join-Path $PSScriptRoot 'New-GameContentFixture.ps1') -Root $temp
    $exe=Join-Path $temp 'Penumbra.exe'
    Copy-Item $BlackPlagueExe $exe
    Copy-Item $RequiemExe (Join-Path $temp 'Requiem.exe')
    Copy-Item $RetailAlut (Join-Path $temp 'alut.dll')
    $bpSettings=Join-Path $temp 'config/default_settings.cfg'
    $rSettings=Join-Path $temp 'expansion01/config/requiem_default_settings.cfg'
    [IO.File]::WriteAllText($bpSettings,'<Map File="fixture.dae" /><Game LanguageFile="Espanol.lang" CurrentUser="save" /><Graphics LimitFPS="true" />')
    [IO.File]::WriteAllText($rSettings,'<Map File="fixture.dae" /><Game LanguageFile="Espanol_exp.lang" />')
    $args=@{GamePath=$exe;BuildRoot=(Join-Path $repo 'build');RequiemMode='Enable'}
    $preview=& $installer @args -InstallSpanishRequiem -Preflight
    Assert (@($preview.Configuration.Changes | Where-Object {$_.Attribute -eq 'LanguageFile'}).Count -eq 1) 'Preflight ignored staged Spanish or missed absent BP language'
    Assert ([IO.File]::ReadAllText($bpSettings).Contains('Espanol.lang')) 'Preview wrote settings'
    & $installer @args -InstallSpanishRequiem | Out-Null
    Assert ([IO.File]::ReadAllText($bpSettings).Contains('LanguageFile="English.lang"')) 'Missing BP language did not fall back'
    Assert ([IO.File]::ReadAllText($bpSettings).Contains('LimitFPS="true"')) 'Language safety enabled graphical recommendations'
    Assert ([IO.File]::ReadAllText($rSettings).Contains('LanguageFile="Espanol_exp.lang"')) 'Selected Spanish expansion preference was lost'
    & $installer @args | Out-Null
    Assert ([IO.File]::ReadAllText($rSettings).Contains('LanguageFile="English_exp.lang"')) 'Removing managed Spanish left an unusable expansion language'
    [IO.File]::WriteAllText((Join-Path $temp 'config/custom.lang'),'<LANGUAGE/>')
    [IO.File]::WriteAllText($bpSettings,([IO.File]::ReadAllText($bpSettings).Replace('English.lang','custom.lang')))
    & $installer -GamePath $exe -Repair | Out-Null
    Assert ([IO.File]::ReadAllText($bpSettings).Contains('LanguageFile="custom.lang"')) 'Repair overwrote a later valid language selection'
    & $installer -GamePath $exe -Restore | Out-Null
    Assert ([IO.File]::ReadAllText($bpSettings).Contains('LanguageFile="custom.lang"')) 'Restore overwrote a later language edit'
    Assert ([IO.File]::ReadAllText($rSettings).Contains('LanguageFile="Espanol_exp.lang"')) 'Restore lost original language ownership'
    Write-Host 'Language safety: read-only preview, missing language fallback, staged translation, removal, repair and later user edits passed.'
} finally {
    $tempBase=[IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\')+'\'
    if(-not $temp.StartsWith($tempBase,[StringComparison]::OrdinalIgnoreCase)){throw 'Unsafe language fixture cleanup path.'}
    if(Test-Path -LiteralPath $temp){Remove-Item -LiteralPath $temp -Recurse -Force}
}
