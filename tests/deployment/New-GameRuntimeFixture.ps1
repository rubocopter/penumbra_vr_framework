param([string]$Root,[string]$Game,[string]$OpenVrPathsPath)
$repo=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$manifest=Get-Content (Join-Path $repo 'assets/deployment/prerequisites.json') -Raw | ConvertFrom-Json
# Architecture fixtures only: these copies do not prove a working native game.
foreach($dll in $manifest.gameLibraries){
    if($dll.games -and $Game -notin $dll.games){continue}
    $path=Join-Path $Root $dll.path
    if(-not (Test-Path -LiteralPath $path)){Copy-Item -LiteralPath (Join-Path $repo 'products/overture/dependencies/bin/win32/OpenAL32.dll') -Destination $path}
}
if($OpenVrPathsPath){$runtime=Join-Path (Split-Path -Parent $OpenVrPathsPath) 'steamvr-fixture';New-Item -ItemType Directory -Path (Join-Path $runtime 'bin/win64') -Force | Out-Null;[IO.File]::WriteAllText((Join-Path $runtime 'bin/win64/vrserver.exe'),'passive runtime registration fixture');[IO.File]::WriteAllText($OpenVrPathsPath,(@{runtime=@($runtime)} | ConvertTo-Json))}
