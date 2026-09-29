[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$OutputDirectory,
      [Parameter(Mandatory=$true)][string]$RuntimeDirectory,
      [Parameter(Mandatory=$true)][string]$SourceDirectory,
      [switch]$PackageOnly)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$release=Get-Content -LiteralPath (Join-Path $repo 'release.json') -Raw | ConvertFrom-Json
if($release.schemaVersion -ne 1 -or $release.version -notmatch '^\d+\.\d+\.\d+$' -or $release.channel -ne 'release-candidate'){throw 'Release identity is not a versioned candidate.'}
$output=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path -LiteralPath $output){throw 'Use a new output directory; existing artifacts are never overwritten.'}
$dirty=@(& git -C $repo status --porcelain)
if($LASTEXITCODE -ne 0 -or $dirty.Count){throw 'Commit the intended release source before building; source archives must describe the binaries.'}
$commit=(& git -C $repo rev-parse HEAD).Trim()
$sources=(Get-Content -LiteralPath (Join-Path $repo 'assets/deployment/redistribution.json') -Raw | ConvertFrom-Json).sources
foreach($source in $sources){$path=Join-Path $SourceDirectory $source.filename;if(-not(Test-Path -LiteralPath $path -PathType Leaf) -or (Get-FileHash -LiteralPath $path).Hash -ine $source.sha256){throw "Required pinned source input is missing or changed: $($source.filename). Download from $($source.url)"}}
$crt=Get-Content -LiteralPath (Join-Path $repo 'products/overture/runtime-dependencies.json') -Raw | ConvertFrom-Json
foreach($entry in $crt.files.PSObject.Properties){$path=Join-Path $RuntimeDirectory $entry.Name;if(-not(Test-Path -LiteralPath $path -PathType Leaf) -or (Get-FileHash -LiteralPath $path).Hash -ine $entry.Value){throw "Pinned official x86 CRT input missing or changed: $($entry.Name)"}}
if(-not $PackageOnly){
    & cmake -S $repo -B (Join-Path $repo 'build') -G 'Visual Studio 17 2022' -A Win32 "-DPENUMBRA_VR_OPENVR_SDK=$repo/products/overture/dependencies/openvr-2.15.6" | Out-Host
    if($LASTEXITCODE -ne 0){throw 'Root configure failed.'}
    & cmake --build (Join-Path $repo 'build') --config Release --parallel 4 | Out-Host
    if($LASTEXITCODE -ne 0){throw 'Root Release build failed.'}
    & ctest --test-dir (Join-Path $repo 'build') -C Release --output-on-failure | Out-Host
    if($LASTEXITCODE -ne 0){throw 'Root test suite failed.'}
}
$temp=[IO.Path]::GetFullPath((Join-Path ([IO.Path]::GetTempPath()) ('PvrRelease-'+[guid]::NewGuid().ToString('N'))))
$tempPrefix=[IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\')+'\'
if(-not $temp.StartsWith($tempPrefix,[StringComparison]::OrdinalIgnoreCase)){throw 'Unsafe release staging path.'}
Import-Module (Join-Path $PSScriptRoot 'PenumbraVrArchive.psm1') -Force
Import-Module (Join-Path $PSScriptRoot 'PenumbraVrPrerequisites.psm1') -Force
try{
    New-Item -ItemType Directory -Path $temp | Out-Null
    $payload=Join-Path $temp 'payload.zip'
    & (Join-Path $PSScriptRoot 'Package-FrameworkCandidate.ps1') -OutputPath $payload -RuntimeDirectory $RuntimeDirectory -SourceDirectory $SourceDirectory | Out-Host
    $setup=Join-Path $temp "PenumbraVR-Setup-$($release.version).exe"
    & (Join-Path $PSScriptRoot 'Package-PenumbraVrSetup.ps1') -OutputPath $setup -PayloadPath $payload | Out-Host
    & (Join-Path $repo 'tests/deployment/Test-ReleaseIdentity.ps1') -Built
    $baseSource=Join-Path $temp 'committed-source.zip'
    & git -C $repo archive --format=zip "--output=$baseSource" HEAD
    if($LASTEXITCODE -ne 0){throw 'Committed source export failed.'}
    $sourceStage=Join-Path $temp 'source'
    Expand-Archive -LiteralPath $baseSource -DestinationPath $sourceStage
    New-Item -ItemType Directory -Path (Join-Path $sourceStage 'source-inputs') | Out-Null
    foreach($source in $sources){Copy-Item -LiteralPath (Join-Path $SourceDirectory $source.filename) -Destination (Join-Path $sourceStage 'source-inputs')}
    $sourceZip=Join-Path $temp "PenumbraVR-Source-$($release.version).zip"
    Write-PvrArchive -Root $sourceStage -OutputPath $sourceZip
    $pe=@()
    foreach($relative in @('build/bin/Release/PenumbraVR.BlackPlague.Bootstrap.dll','build/bin/Release/PenumbraVR.BlackPlague.OpenALProxy.dll','build/bin/Release/PenumbraVR.BlackPlague.Probe.dll','build/bin/Release/PenumbraVR.Requiem.Probe.dll','build/bin/Release/PenumbraVR.LaaTransform.exe','products/overture/build/bin/Release/Penumbra_vr.exe')){
        $path=Join-Path $repo $relative;$info=Get-PvrPeInfo $path;$versionInfo=(Get-Item -LiteralPath $path).VersionInfo
        if($info.Architecture -ne 'x86' -or $versionInfo.ProductVersion -ne $release.version){throw "Shipped PE identity mismatch: $relative"}
        $pe+=[pscustomobject]@{file=$relative;architecture=$info.Architecture;productVersion=$versionInfo.ProductVersion;sha256=(Get-FileHash -LiteralPath $path).Hash;imports=$info.Imports;delayImports=$info.DelayImports}
    }
    $vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $vsVersion=(& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationVersion | Select-Object -First 1)
    $compilerRecord=Get-ChildItem -LiteralPath (Join-Path $repo 'build/CMakeFiles') -Recurse -Filter CMakeCXXCompiler.cmake | Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
    $compilerVersion='unknown'
    if($compilerRecord -and [IO.File]::ReadAllText($compilerRecord.FullName) -match 'set\(CMAKE_CXX_COMPILER_VERSION "([^"]+)"\)'){$compilerVersion=$Matches[1]}
    $toolchain=[ordered]@{generator='Visual Studio 17 2022';platform='Win32';visualStudioVersion=$vsVersion;msvcCompilerVersion=$compilerVersion}
    $report=[ordered]@{schemaVersion=1;version=$release.version;channel=$release.channel;sourceCommit=$commit;sourceClean=$true;configuration='Release';architecture='x86';windows=[Environment]::OSVersion.VersionString;powerShell=$PSVersionTable.PSVersion.ToString();cmake=(@(& cmake --version))[0];toolchain=$toolchain;rootBuildAndTests=$(if($PackageOnly){'not repeated; package-only mode'}else{'passed in this invocation'});overtureBuildAndTests='passed by product packager';crtVersion=$crt.fileVersion;crtFiles=$crt.files;sourceInputs=$sources;pe=$pe;fixedInputArchivePolicy='sorted entries; ZIP timestamps 2000-01-01; byte determinism tested separately';fullBinaryReproducibility='not demonstrated: historical static-library build flags and toolchain timestamps remain outside this evidence';cleanMachineAcceptance='pending';currentCandidateHeadsetAcceptance='pending';publicReleaseReady=$false}
    New-Item -ItemType Directory -Path $output | Out-Null
    Copy-Item -LiteralPath $setup,$sourceZip -Destination $output
    $report | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath (Join-Path $output 'build-report.json') -Encoding UTF8
    $lines=@(Get-ChildItem -LiteralPath $output -File | Sort-Object Name | ForEach-Object {'{0}  {1}' -f (Get-FileHash -LiteralPath $_.FullName).Hash,$_.Name})
    [IO.File]::WriteAllLines((Join-Path $output 'SHA256SUMS.txt'),[string[]]$lines,[Text.UTF8Encoding]::new($false))
    & (Join-Path $repo 'tests/deployment/Test-ReleasePackage.ps1') -ReleaseDirectory $output
    [pscustomobject]@{Version=$release.version;SourceCommit=$commit;OutputDirectory=$output;Setup=(Join-Path $output ([IO.Path]::GetFileName($setup)));Source=(Join-Path $output ([IO.Path]::GetFileName($sourceZip)));PublicReleaseReady=$false}
}finally{
    if(Test-Path -LiteralPath $temp -PathType Container){Remove-Item -LiteralPath $temp -Recurse -Force}
}
