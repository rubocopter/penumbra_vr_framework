[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$OvertureExe)
$ErrorActionPreference='Stop'
$PSDefaultParameterValues=@{'*:SettingsScope'='DefaultFiles'}
$repo=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$installer=Join-Path $repo 'products/overture/scripts/deploy.ps1'
$package=Join-Path $repo 'products/overture/build/package/Release/PenumbraVR'
$temp=[IO.Path]::GetFullPath((Join-Path ([IO.Path]::GetTempPath()) ('PvrOvertureLanguage-'+[guid]::NewGuid().ToString('N'))))
function Assert($condition,[string]$message){if(-not $condition){throw $message}}
try {
    New-Item -ItemType Directory -Path $temp | Out-Null
    $redist=Join-Path $temp 'redist'
    & (Join-Path $PSScriptRoot 'New-GameContentFixture.ps1') -Root $redist -Games overture
    Copy-Item $OvertureExe (Join-Path $redist 'Penumbra.exe')
    $settings=Join-Path $redist 'config/default_settings.cfg'
    [IO.File]::WriteAllText($settings,'<Map File="fixture.dae" /><Game LanguageFile="Espanol.lang" /><Graphics LimitFPS="true" />')
    $args=@{InstallRoot=$temp;PackageRoot=$package;SkipTexturePack=$true}
    & $installer @args -CommunityTranslations | Out-Null
    foreach($missing in @($false,$true)) {
        if($missing){Remove-Item -LiteralPath (Join-Path $redist 'config/Espanol.lang')}
        & $installer @args -Repair | Out-Null
        Assert ([IO.File]::ReadAllText($settings).Contains('LanguageFile="Espanol.lang"')) 'Overture repair lost recorded Spanish selection'
        Assert (Test-Path -LiteralPath (Join-Path $redist 'config/Espanol.lang')) 'Overture repair failed to restore translation'
    }
    & $installer @args | Out-Null
    Assert ([IO.File]::ReadAllText($settings).Contains('LanguageFile="English.lang"')) 'Overture translation removal left missing selected language'
    & $installer -InstallRoot $temp -Restore | Out-Null
    Write-Host 'Overture language safety: recorded Spanish repair with present/missing file, removal and restore passed.'
} finally {
    $tempBase=[IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\')+'\'
    if(-not $temp.StartsWith($tempBase,[StringComparison]::OrdinalIgnoreCase)){throw 'Unsafe Overture language fixture cleanup path.'}
    if(Test-Path -LiteralPath $temp){Remove-Item -LiteralPath $temp -Recurse -Force}
}
