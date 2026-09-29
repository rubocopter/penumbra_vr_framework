param([Parameter(Mandatory=$true)][string]$ReleaseDirectory)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$release=Get-Content (Join-Path $repo 'release.json') -Raw | ConvertFrom-Json
$dir=[IO.Path]::GetFullPath($ReleaseDirectory)
$setup=Join-Path $dir "PenumbraVR-Setup-$($release.version).zip"
$source=Join-Path $dir "PenumbraVR-Source-$($release.version).zip"
foreach($file in @($setup,$source,(Join-Path $dir 'build-report.json'),(Join-Path $dir 'SHA256SUMS.txt'))){if(-not(Test-Path -LiteralPath $file -PathType Leaf)){throw "Release artifact missing: $file"}}
$seen=@{}
foreach($line in Get-Content (Join-Path $dir 'SHA256SUMS.txt')){
    if($line -notmatch '^([A-F0-9]{64})  ([A-Za-z0-9._-]+)$'){throw 'Invalid outer checksum'}
    $name=$Matches[2];$hash=$Matches[1]
    if($seen.ContainsKey($name)){throw 'Duplicate outer checksum'};$seen[$name]=$true
    if((Get-FileHash (Join-Path $dir $name)).Hash -cne $hash){throw 'Outer checksum mismatch'}
}
if($seen.Count -ne 3 -or @([IO.Path]::GetFileName($setup),[IO.Path]::GetFileName($source),'build-report.json' | Where-Object {-not $seen.ContainsKey($_)}).Count){throw 'Incomplete outer checksums'}
$report=Get-Content (Join-Path $dir 'build-report.json') -Raw | ConvertFrom-Json
if($report.version -ne $release.version -or $report.sourceCommit -notmatch '^[a-f0-9]{40}$' -or -not $report.sourceClean -or $report.publicReleaseReady -or @($report.pe).Count -ne 5){throw 'Invalid candidate build report'}
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip=[IO.Compression.ZipFile]::OpenRead($setup)
try{
    $names=@($zip.Entries.FullName)
    foreach($required in @('release.json','SOURCE-AND-NOTICES.md','assets/deployment/manifest.json','tools/Instalar-Penumbra-VR.vbs','tools/Test-PenumbraVrInstallation.ps1','tools/Collect-PenumbraVrDiagnostics.ps1','licenses/OpenALSoft-LICENSE-pffft.txt','sources/openal-soft-1.25.2.tar.bz2')){if($required -notin $names){throw "Required release entry missing: $required"}}
    if(@($names | Where-Object {$_ -match '(\.pdb|\.dmp|\.obj|fltkdll\.dll|msvcr80\.dll)$|(^|/)(work|logs|captures|\.git)/'}).Count){throw 'Developer/private payload included'}
}finally{$zip.Dispose()}
$zip=[IO.Compression.ZipFile]::OpenRead($source)
try{
    foreach($required in @('CMakeLists.txt','release.json','products/overture/PenumbraOverture/Init.cpp','tools/Build-PenumbraVrRelease.ps1','source-inputs/openal-soft-1.25.2.tar.bz2','source-inputs/angelscript_2.7.1b.zip')){if($required -notin @($zip.Entries.FullName)){throw "Required source entry missing: $required"}}
}finally{$zip.Dispose()}
Write-Host 'Release artifacts: identity, setup/source/notices, outer checksums and absence of developer/private payload passed.'
