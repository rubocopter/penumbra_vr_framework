param([Parameter(Mandatory=$true)][string]$BlackPlagueExe,[Parameter(Mandatory=$true)][string]$RetailAlut,[string]$PackagePath)
$ErrorActionPreference='Stop'
# Fixture deployments must never edit the real Documents configuration.
$PSDefaultParameterValues = @{ '*:SettingsScope' = 'DefaultFiles' }
$repo=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$temp=Join-Path ([IO.Path]::GetTempPath()) ('PvrMaintenance-'+[guid]::NewGuid().ToString('N'))
try{
    New-Item -ItemType Directory -Path $temp | Out-Null
    if(-not $PackagePath){$PackagePath=Join-Path $temp 'package.zip';& (Join-Path $repo 'tools/Package-FrameworkCandidate.ps1') -OutputPath $PackagePath 6>$null}
    $package=Join-Path $temp 'package';Expand-Archive -LiteralPath $PackagePath -DestinationPath $package
    $root=Join-Path $temp 'game/redist';New-Item -ItemType Directory -Path $root -Force | Out-Null
    Copy-Item $BlackPlagueExe (Join-Path $root 'Penumbra.exe');Copy-Item $RetailAlut (Join-Path $root 'alut.dll')
    & (Join-Path $PSScriptRoot 'New-GameContentFixture.ps1') -Root $root -Games black_plague
    $vrpath=Join-Path $temp 'openvrpaths.vrpath'
    & (Join-Path $PSScriptRoot 'New-GameRuntimeFixture.ps1') -Root $root -Game black_plague -OpenVrPathsPath $vrpath
    $selector=Join-Path $package 'tools/Install-PenumbraFrameworkCandidate.ps1'
    $arguments=@{Game='BlackPlague';GamePath=$root;SteamRoot=(Join-Path $temp 'no-steam');RequiemMode='Disable';OpenVrPathsPath=$vrpath;LogPath=(Join-Path $temp 'installer.jsonl')}
    & $selector @arguments 6>$null | Out-Null
    $codec=Join-Path $root 'libpng12.dll';Remove-Item -LiteralPath $codec
    $before=@(Get-ChildItem $root -Recurse -File | Sort-Object FullName | ForEach-Object {(Get-FileHash $_.FullName).Hash}) -join '|'
    foreach($planOnly in @($true,$false)){
        $failed=$false
        try{& $selector -Game BlackPlague -GamePath $root -Repair -Plan:$planOnly -LogPath (Join-Path $temp 'repair.jsonl') 6>$null | Out-Null}catch{$failed=$_.Exception.Message -match 'Preflight|PNG decoder'}
        if(-not $failed){throw 'Repair preflight accepted missing game runtime.'}
        $after=@(Get-ChildItem $root -Recurse -File | Sort-Object FullName | ForEach-Object {(Get-FileHash $_.FullName).Hash}) -join '|'
        if($before -cne $after){throw 'Failed repair changed game files.'}
    }
    Write-Host 'Maintenance preflight: missing game runtime rejected in preview/apply without target writes.'
}finally{if(Test-Path -LiteralPath $temp){Remove-Item -LiteralPath $temp -Recurse -Force}}
