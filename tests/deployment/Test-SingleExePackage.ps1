[CmdletBinding()]
param(
    [Parameter(Mandatory=$true,ParameterSetName='Release')][string]$ReleaseDirectory,
    [Parameter(Mandatory=$true,ParameterSetName='Fixture')][switch]$FixtureOnly
)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)

function Invoke-Extract([string]$Exe,[string]$Destination) {
    # Windows PowerShell's native argument quoting loses a trailing backslash
    # before a closing quote. Escape it explicitly so Setup receives the path.
    $info=New-Object Diagnostics.ProcessStartInfo
    $info.FileName=$Exe
    $info.Arguments='--extract-to "'+[regex]::Replace($Destination,'(\\+)$','$1$1')+'"'
    $info.UseShellExecute=$false
    $info.CreateNoWindow=$true
    $info.RedirectStandardOutput=$true
    $info.RedirectStandardError=$true
    $process=[Diagnostics.Process]::Start($info)
    try{
        $output=$process.StandardOutput.ReadToEndAsync()
        $errorOutput=$process.StandardError.ReadToEndAsync()
        $process.WaitForExit()
        [pscustomobject]@{ExitCode=$process.ExitCode;Output=$output.Result+$errorOutput.Result}
    }finally{$process.Dispose()}
}

function Assert-NewExtraction([string]$Exe,[string]$Destination) {
    if(Test-Path -LiteralPath $Destination){throw "Fixture destination must be new: $Destination"}
    $result=Invoke-Extract $Exe $Destination
    if($result.ExitCode -ne 0){throw "Standalone EXE extraction failed for '$Destination': $($result.Output)"}
    if(-not(Test-Path -LiteralPath (Join-Path $Destination 'SHA256SUMS.txt') -PathType Leaf)){
        throw "Extracted checksums missing: $Destination"
    }
}

if($FixtureOnly){
    $temp=Join-Path ([IO.Path]::GetTempPath()) ('PvrSingleExe-fixture '+[guid]::NewGuid().ToString('N'))
    try{
        $project=Join-Path $temp 'project'
        New-Item -ItemType Directory -Path $project | Out-Null
        # Build the real Setup entry point with tiny embedded resources, without
        # rebuilding game products or publishing a self-contained release bundle.
        Copy-Item -LiteralPath (Join-Path $repo 'installer/PenumbraVR.Setup/Program.cs') -Destination $project
        Copy-Item -LiteralPath (Join-Path $repo 'installer/PenumbraVR.Setup/PenumbraVR.Setup.csproj') -Destination $project
        $banner=Join-Path $repo 'assets/banner/penumbra-vr-framework.png'
        Add-Type -AssemblyName System.IO.Compression
        $data='extraction fixture'
        $gui='# extraction only; never execute this fixture'
        $sha=[Security.Cryptography.SHA256]::Create()
        try{
            $dataHash=([BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($data)))).Replace('-','')
            $guiHash=([BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($gui)))).Replace('-','')
        }finally{$sha.Dispose()}
        foreach($case in @('valid','checksum-traversal','duplicate','mismatch','unlisted','zip-traversal')){
            $destination=Join-Path $temp "extract-$case"
            $manifest="$dataHash  fixture.txt`n$guiHash  tools/Install-PenumbraFrameworkGui.ps1`n"
            if($case -eq 'checksum-traversal'){
                $sibling=New-Item -ItemType Directory -Path "$destination-sibling"
                [IO.File]::WriteAllText((Join-Path $sibling.FullName 'sentinel.txt'),$data)
                $manifest="$dataHash  ../extract-checksum-traversal-sibling/sentinel.txt`n$guiHash  tools/Install-PenumbraFrameworkGui.ps1`n"
            }
            if($case -eq 'duplicate'){$manifest+="$dataHash  ./fixture.txt`n"}
            if($case -eq 'mismatch'){$manifest=$manifest.Replace($dataHash,('0'*64))}
            $entries=@{'fixture.txt'=$data;'tools/Install-PenumbraFrameworkGui.ps1'=$gui;'SHA256SUMS.txt'=$manifest}
            if($case -eq 'unlisted'){$entries['unlisted.txt']='not in checksum manifest'}
            if($case -eq 'zip-traversal'){$entries['../zip-escape.txt']='must not escape'}
            $zip=Join-Path $temp "$case.zip"
            $stream=[IO.File]::Create($zip)
            $archive=New-Object IO.Compression.ZipArchive($stream,[IO.Compression.ZipArchiveMode]::Create)
            try{
                foreach($name in $entries.Keys){
                    $writer=New-Object IO.StreamWriter($archive.CreateEntry($name).Open())
                    try{$writer.Write($entries[$name])}finally{$writer.Dispose()}
                }
            }finally{$archive.Dispose();$stream.Dispose()}
            $publish=Join-Path $temp "publish-$case"
            & dotnet publish (Join-Path $project 'PenumbraVR.Setup.csproj') -c Release -r win-x64 --self-contained false `
                -p:PublishSingleFile=false "-p:PenumbraVrPayloadPath=$zip" "-p:PenumbraVrBannerPath=$banner" -o $publish | Out-Host
            if($LASTEXITCODE -ne 0){throw "Setup fixture build failed: $case"}
            $exe=Join-Path $publish 'PenumbraVR.Setup.exe'
            if($case -eq 'valid'){
                foreach($suffix in @('plain','backslash','slash')){
                    $target=Join-Path $temp "new destination $suffix"
                    $argument=$target
                    if($suffix -eq 'backslash'){$argument+='\'}
                    if($suffix -eq 'slash'){$argument+='/'}
                    Assert-NewExtraction $exe $argument
                    if([IO.File]::ReadAllText((Join-Path $target 'fixture.txt')) -cne $data -or
                        [IO.File]::ReadAllText((Join-Path $target 'tools/Install-PenumbraFrameworkGui.ps1')) -cne $gui -or
                        (Get-FileHash -LiteralPath (Join-Path $target 'assets/banner/penumbra-vr-framework.png')).Hash -ne
                        (Get-FileHash -LiteralPath $banner).Hash){throw "Fixture content mismatch: $suffix"}
                    $again=Invoke-Extract $exe $argument
                    if($again.ExitCode -eq 0 -or $again.Output -notmatch 'Use a new extraction directory'){
                        throw "Existing extraction directory accepted: $suffix"
                    }
                    Write-Host "PASS: new extraction directory ($suffix), payload bytes and existing-directory refusal."
                }
            }else{
                $result=Invoke-Extract $exe ($destination+'\')
                if($result.ExitCode -eq 0){throw "Unsafe fixture accepted: $case"}
                $expected=switch($case){
                    'checksum-traversal' {'Unsafe or duplicate payload checksum'}
                    'duplicate' {'Unsafe or duplicate payload checksum'}
                    'mismatch' {'Installer payload checksum mismatch'}
                    'unlisted' {'Installer payload file count mismatch'}
                    'zip-traversal' {'outside the specified destination directory'}
                }
                if($result.Output -notmatch [regex]::Escape($expected)){throw "Wrong rejection for ${case}: $($result.Output)"}
                if(Test-Path -LiteralPath (Join-Path $temp 'zip-escape.txt')){throw 'ZIP entry escaped extraction root.'}
                if($case -eq 'checksum-traversal' -and
                    [IO.File]::ReadAllText((Join-Path $sibling.FullName 'sentinel.txt')) -cne $data){throw 'Sibling sentinel changed.'}
                Write-Host "PASS: $case rejected by Setup."
            }
        }
    }finally{
        $resolved=[IO.Path]::GetFullPath($temp)
        $tempPrefix=[IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\','/')+[IO.Path]::DirectorySeparatorChar
        if(-not $resolved.StartsWith($tempPrefix,[StringComparison]::OrdinalIgnoreCase) -or
            [IO.Path]::GetFileName($resolved) -notlike 'PvrSingleExe-fixture *'){throw 'Unsafe fixture cleanup path.'}
        if(Test-Path -LiteralPath $resolved){Remove-Item -LiteralPath $resolved -Recurse -Force}
    }
    return
}

$release=Get-Content -LiteralPath (Join-Path $repo 'release.json') -Raw | ConvertFrom-Json
$files=@(Get-ChildItem -LiteralPath $ReleaseDirectory -File)
if($files.Count -ne 1 -or $files[0].Name -ne "PenumbraVR-Setup-$($release.version).exe"){throw 'Public download directory must contain exactly the single Setup EXE.'}
$temp=Join-Path ([IO.Path]::GetTempPath()) ('PvrSingleExe-'+[guid]::NewGuid().ToString('N'))
try{
    New-Item -ItemType Directory -Path $temp | Out-Null
    $exe=Join-Path $temp 'Setup.exe'
    Copy-Item -LiteralPath $files[0].FullName -Destination $exe
    $previousDotnetRoot=$env:DOTNET_ROOT
    try{
        $env:DOTNET_ROOT=Join-Path $temp 'no-dotnet-runtime'
        Push-Location $temp
        try{
            $report=& $exe --smoke-test | Out-String | ConvertFrom-Json
            if($LASTEXITCODE -ne 0 -or -not $report.payloadExtracted -or -not $report.tempCleaned){throw 'Standalone EXE smoke failed.'}
            $payload=Join-Path $temp 'payload'
            Assert-NewExtraction $exe $payload
            Assert-NewExtraction $exe ((Join-Path $temp 'payload-trailing-separator')+'\')
            & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $payload 'tools/Install-PenumbraFrameworkGui.ps1') -UiContract | Out-Null
            if($LASTEXITCODE -ne 0){throw 'Standalone embedded GUI failed.'}
            & (Join-Path $payload 'tools/Install-PenumbraFrameworkCandidate.ps1') -List -SteamRoot (Join-Path $temp 'no-steam') | Out-Null
            foreach($required in @('products/black_plague/build/bin/Release/PenumbraVR.BlackPlague.OpenALProxy.dll','products/black_plague/products/overture/dependencies/bin/win32/OpenAL32.dll','products/overture/msvcp140.dll','products/overture/vcruntime140.dll','assets/banner/penumbra-vr-framework.png','sources/openal-soft-1.25.2.tar.bz2')){
                if(-not(Test-Path -LiteralPath (Join-Path $payload $required))){throw "Embedded dependency missing: $required"}
            }
        }finally{Pop-Location}
    }finally{$env:DOTNET_ROOT=$previousDotnetRoot}
    Write-Host 'One public EXE: standalone startup, package integrity, bilingual GUI and dependency payload passed.'
}finally{if(Test-Path -LiteralPath $temp){Remove-Item -LiteralPath $temp -Recurse -Force}}
