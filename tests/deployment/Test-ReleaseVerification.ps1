param([Parameter(Mandatory=$true)][string]$BlackPlagueExe,[Parameter(Mandatory=$true)][string]$RetailAlut,[Parameter(Mandatory=$true)][string]$RequiemExe)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$temp=Join-Path ([IO.Path]::GetTempPath()) ('PvrVerify-'+[guid]::NewGuid().ToString('N'))
function Assert($condition,[string]$message){if(-not $condition){throw $message}}
try {
    New-Item -ItemType Directory -Path $temp | Out-Null
    $exe=Join-Path $temp 'Penumbra.exe'
    Copy-Item $BlackPlagueExe $exe;Copy-Item $RetailAlut (Join-Path $temp 'alut.dll')
    & (Join-Path $PSScriptRoot 'New-GameContentFixture.ps1') -Root $temp -Games black_plague
    $manifest=Get-Content (Join-Path $repo 'assets/deployment/prerequisites.json') -Raw | ConvertFrom-Json
    foreach($dll in $manifest.gameLibraries){if($dll.path -eq 'alut.dll'){continue};Copy-Item (Join-Path $repo 'products/overture/dependencies/bin/win32/OpenAL32.dll') (Join-Path $temp $dll.path)}
    & (Join-Path $repo 'tools/Install-BlackPlagueSteamBootstrap.ps1') -GamePath $exe -RequiemMode Disable
    $verify=Join-Path $repo 'tools/Test-PenumbraVrInstallation.ps1'
    $before=@(Get-ChildItem $temp -Recurse -File | ForEach-Object {(Get-FileHash $_.FullName).Hash}) -join '|'
    $result=& $verify -Game black_plague -GamePath $exe -PackageRoot $repo -SkipSteamVr
    Assert $result.InstallationVerified ($result.Checks | Where-Object {$_.Status -notin @('present','provided')} | ConvertTo-Json)
    $after=@(Get-ChildItem $temp -Recurse -File | ForEach-Object {(Get-FileHash $_.FullName).Hash}) -join '|'
    Assert ($before -ceq $after) 'Verification changed target files'
    Copy-Item -LiteralPath $RequiemExe -Destination $exe -Force
    Assert (-not (& $verify -Game black_plague -GamePath $exe -PackageRoot $repo -SkipSteamVr).InstallationVerified) 'Recognized executable from another game accepted'
    Copy-Item -LiteralPath $BlackPlagueExe -Destination $exe -Force
    $statePath=Join-Path $temp 'PenumbraVR.BlackPlague.install.json'
    $stateBytes=[IO.File]::ReadAllBytes($statePath)
    $state=Get-Content $statePath -Raw | ConvertFrom-Json
    Copy-Item $RequiemExe (Join-Path $temp 'Requiem.exe')
    Copy-Item (Join-Path $repo 'build/bin/Release/PenumbraVR.Requiem.Probe.dll') (Join-Path $temp 'PenumbraVR.Requiem.Probe.dll')
    $state.requiemExeSha256=(Get-FileHash $RequiemExe).Hash
    $state.requiemProbeSha256=(Get-FileHash (Join-Path $temp 'PenumbraVR.Requiem.Probe.dll')).Hash
    $state.components+= 'requiem'
    $state | ConvertTo-Json -Depth 12 | Set-Content $statePath
    Assert (-not (& $verify -Game black_plague -GamePath $exe -PackageRoot $repo -SkipSteamVr).InstallationVerified) 'Verifying base ignored incomplete installed expansion'
    & (Join-Path $PSScriptRoot 'New-GameContentFixture.ps1') -Root $temp -Games requiem
    $state.components=@('shared','black_plague')
    $state | ConvertTo-Json -Depth 12 | Set-Content $statePath
    Assert (-not (& $verify -Game black_plague -GamePath $exe -PackageRoot $repo -SkipSteamVr).InstallationVerified) 'Payload/component disagreement accepted'
    [IO.File]::WriteAllBytes($statePath,$stateBytes)
    Remove-Item (Join-Path $temp 'Requiem.exe'),(Join-Path $temp 'PenumbraVR.Requiem.Probe.dll')
    Remove-Item (Join-Path $temp 'libpng12.dll')
    Assert (-not (& $verify -Game black_plague -GamePath $exe -PackageRoot $repo -SkipSteamVr).InstallationVerified) 'Missing dynamic codec accepted'
    [IO.File]::WriteAllText((Join-Path $temp 'PenumbraVR_alut_original.dll'),'broken original')
    $result=& $verify -Game black_plague -GamePath $exe -PackageRoot $repo -SkipSteamVr
    Assert (@($result.Checks | Where-Object {$_.Name -eq 'Original ALUT backup' -and $_.Status -eq 'invalid'}).Count -eq 1) 'Bad original hidden'
    [IO.File]::WriteAllText((Join-Path $temp 'vr/actions.json'),'{}')
    Assert (-not (& $verify -Game black_plague -GamePath $exe -PackageRoot $repo -SkipSteamVr).InstallationVerified) 'Broken bindings accepted'
    Write-Host 'Installed verifier: valid managed root, read-only, missing codec, broken original and malformed action manifest passed.'
} finally {if(Test-Path $temp){Remove-Item -LiteralPath $temp -Recurse -Force}}
