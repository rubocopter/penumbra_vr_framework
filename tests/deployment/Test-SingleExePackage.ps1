[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$ReleaseDirectory)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
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
            & $exe --extract-to $payload | Out-Null
            if($LASTEXITCODE -ne 0){throw 'Standalone EXE extraction failed.'}
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
