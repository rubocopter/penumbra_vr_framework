[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$OutputPath,
    [string]$RuntimeDirectory,
    [string]$SourceDirectory=(Join-Path (Split-Path -Parent $PSScriptRoot) 'work/release-inputs'),
    [string]$PayloadPath
)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$release=Get-Content -LiteralPath (Join-Path $repo 'release.json') -Raw | ConvertFrom-Json
$output=[IO.Path]::GetFullPath($OutputPath)
if(Test-Path -LiteralPath $output){throw 'Choose a new output filename; an existing EXE is never overwritten.'}
if([IO.Path]::GetFileName($output) -ne "PenumbraVR-Setup-$($release.version).exe"){throw 'Setup filename must match release.json.'}
$temp=Join-Path ([IO.Path]::GetTempPath()) ('PvrSetupBuild-'+[guid]::NewGuid().ToString('N'))
try{
    New-Item -ItemType Directory -Path $temp | Out-Null
    if(-not $PayloadPath){
        $PayloadPath=Join-Path $temp 'payload.zip'
        & (Join-Path $PSScriptRoot 'Package-FrameworkCandidate.ps1') -OutputPath $PayloadPath -RuntimeDirectory $RuntimeDirectory -SourceDirectory $SourceDirectory
    }
    $PayloadPath=[IO.Path]::GetFullPath($PayloadPath)
    $check=Join-Path $temp 'check'
    Expand-Archive -LiteralPath $PayloadPath -DestinationPath $check
    & (Join-Path $check 'tools/Install-PenumbraFrameworkCandidate.ps1') -List -SteamRoot (Join-Path $temp 'empty-steam') | Out-Null
    foreach($required in @('SHA256SUMS.txt','assets/banner/penumbra-vr-framework.png','sources/openal-soft-1.25.2.tar.bz2','sources/angelscript_2.7.1b.zip')){
        if(-not(Test-Path -LiteralPath (Join-Path $check $required))){throw "Setup payload missing: $required"}
    }
    $publish=Join-Path $temp 'publish'
    Push-Location (Join-Path $repo 'installer/PenumbraVR.Setup')
    try{
    & dotnet publish (Join-Path $repo 'installer/PenumbraVR.Setup/PenumbraVR.Setup.csproj') -c Release -r win-x64 --self-contained true `
        -p:PublishSingleFile=true -p:DebugType=None "-p:Version=$($release.version)" `
        "-p:PenumbraVrPayloadPath=$PayloadPath" "-p:PenumbraVrBannerPath=$(Join-Path $repo 'assets/banner/penumbra-vr-framework.png')" -o $publish
    if($LASTEXITCODE -ne 0){throw 'Self-contained Setup publish failed.'}
    }finally{Pop-Location}
    $published=Join-Path $publish 'PenumbraVR.Setup.exe'
    $smoke=& $published --smoke-test | Out-String | ConvertFrom-Json
    if($LASTEXITCODE -ne 0 -or -not $smoke.payloadExtracted -or -not $smoke.tempCleaned){throw 'Published Setup smoke failed.'}
    New-Item -ItemType Directory -Path (Split-Path -Parent $output) -Force | Out-Null
    Copy-Item -LiteralPath $published -Destination $output
    [pscustomobject]@{Setup=$output;SHA256=(Get-FileHash -LiteralPath $output).Hash;Version=$release.version;CleanMachineAcceptance='pending';HeadsetAcceptance='pending'}
}finally{
    if(Test-Path -LiteralPath $temp){Remove-Item -LiteralPath $temp -Recurse -Force}
}
