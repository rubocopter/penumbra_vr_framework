param([Parameter(Mandatory=$true)][string]$BlackPlagueExe,[Parameter(Mandatory=$true)][string]$RequiemExe,[Parameter(Mandatory=$true)][string]$RetailAlut)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$installer=Join-Path $repo 'tools/Install-BlackPlagueSteamBootstrap.ps1'
$temp=Join-Path ([IO.Path]::GetTempPath()) ('PvrComponents-'+[guid]::NewGuid().ToString('N'))
function Assert($condition,[string]$message){if(-not $condition){throw $message}}
try {
    New-Item -ItemType Directory -Path $temp | Out-Null
    $exe=Join-Path $temp 'Penumbra.exe'; $rExe=Join-Path $temp 'Requiem.exe'
    Copy-Item $BlackPlagueExe $exe; Copy-Item $RetailAlut (Join-Path $temp 'alut.dll')
    & (Join-Path $PSScriptRoot 'New-GameContentFixture.ps1') -Root $temp -Games black_plague
    $settings=Join-Path $temp 'config/default_settings.cfg'
    [IO.File]::WriteAllText($settings,'<Map File="fixture.dae" /><Graphics LimitFPS="true" PostEffects="false" /><Screen Vsync="true" />')
    $args=@{GamePath=$exe;BuildRoot=(Join-Path $repo 'build');RequiemMode='Disable'}
    & $installer @args
    Assert (Test-Path (Join-Path $temp 'OpenAL32.dll')) 'App-local OpenAL missing'
    Assert (-not (Test-Path (Join-Path $temp 'config/Espanol.lang'))) 'Core installed optional translation'
    Assert (-not (Test-Path (Join-Path $temp 'expansion01'))) 'BP-only created expansion'
    & $installer @args -RecommendedSettings -SettingsScope DefaultFiles
    Assert ([IO.File]::ReadAllText($settings).Contains('LimitFPS="false"')) 'Settings patch not applied'
    [IO.File]::WriteAllText($settings,([IO.File]::ReadAllText($settings).Replace('PostEffects="true"','PostEffects="later-edit"')))
    $savedSettings=[IO.File]::ReadAllBytes($settings)
    [IO.File]::WriteAllText($settings,'<Graphics')
    $beforePreflight=(Get-FileHash (Join-Path $temp 'alut.dll')).Hash
    $failed=$false;try {& $installer -GamePath $exe -Restore -Preflight | Out-Null}catch{$failed=$true}
    Assert $failed 'Removal preflight accepted malformed owned settings'
    Assert ((Get-FileHash (Join-Path $temp 'alut.dll')).Hash -eq $beforePreflight) 'Removal preflight changed payload'
    [IO.File]::WriteAllBytes($settings,$savedSettings)
    Copy-Item $RequiemExe $rExe
    $args.RequiemMode='Enable'
    $failed=$false;try {& $installer @args}catch{$failed=$true}
    Assert $failed 'Requiem enabled without data'
    & (Join-Path $PSScriptRoot 'New-GameContentFixture.ps1') -Root $temp -Games requiem
    & $installer @args
    Assert (Test-Path (Join-Path $temp 'PenumbraVR.Requiem.Probe.dll')) 'Requiem missing'
    Copy-Item $BlackPlagueExe $rExe -Force
    $stateHash=(Get-FileHash (Join-Path $temp 'PenumbraVR.BlackPlague.install.json')).Hash
    $failed=$false;try {& $installer -GamePath $exe -Repair -Preflight | Out-Null}catch{$failed=$true}
    Assert $failed 'Repair accepted another game as the managed Requiem executable'
    Assert ((Get-FileHash (Join-Path $temp 'PenumbraVR.BlackPlague.install.json')).Hash -eq $stateHash) 'Rejected repair changed state'
    Copy-Item $RequiemExe $rExe -Force
    $bpHash=(Get-FileHash (Join-Path $temp 'PenumbraVR.BlackPlague.Probe.dll')).Hash
    Remove-Item $rExe
    $failed=$false;try {& $installer -GamePath $exe -Repair -Preflight | Out-Null}catch{$failed=$true}
    Assert $failed 'Repair silently removed the recorded R layer after its executable disappeared'
    $args.RequiemMode='Disable'
    & $installer @args
    Assert (-not (Test-Path (Join-Path $temp 'PenumbraVR.Requiem.Probe.dll'))) 'Missing EXE prevented removal'
    Assert ((Get-FileHash (Join-Path $temp 'PenumbraVR.BlackPlague.Probe.dll')).Hash -eq $bpHash) 'Requiem removal changed BP'
    & $installer -GamePath $exe -Restore
    Assert ([IO.File]::ReadAllText($settings).Contains('LimitFPS="true"') -and [IO.File]::ReadAllText($settings).Contains('PostEffects="later-edit"')) 'Settings original or later edit lost'
    Assert (-not (Test-Path (Join-Path $temp 'OpenAL32.dll'))) 'OpenAL not restored'
    Assert ((Get-FileHash (Join-Path $temp 'alut.dll')).Hash -eq (Get-FileHash $RetailAlut).Hash) 'ALUT original not restored'
    [IO.File]::WriteAllText((Join-Path $temp 'OpenAL32.dll'),'unknown mod')
    $failed=$false;try {& $installer @args}catch{$failed=$true}
    Assert $failed 'Unknown OpenAL overwritten'
    Assert ((Get-FileHash (Join-Path $temp 'alut.dll')).Hash -eq (Get-FileHash $RetailAlut).Hash) 'Unknown OpenAL rejection changed ALUT'
    Write-Host 'Release components: B-only, B/R, B/R to B, missing R EXE, optional translation, OpenAL restore/conflict passed.'
} finally {if(Test-Path $temp){Remove-Item -LiteralPath $temp -Recurse -Force}}
