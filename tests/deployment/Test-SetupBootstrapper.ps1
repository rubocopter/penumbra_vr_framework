[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$gui = Join-Path $repoRoot 'tools/Install-PenumbraFrameworkGui.ps1'
$project = Join-Path $repoRoot 'installer/PenumbraVR.Setup/PenumbraVR.Setup.csproj'
$banner = Join-Path $repoRoot 'assets/banner/penumbra-vr-framework.png'
$temporaryRoot = Join-Path ([IO.Path]::GetTempPath()) ('PenumbraVrSetupSmoke-' + [guid]::NewGuid().ToString('N'))

try {
    $english = (& powershell -NoProfile -ExecutionPolicy Bypass -File $gui -UiContract -Language en | Out-String).Trim() | ConvertFrom-Json
    if ($english.language -ne 'en' -or
        $english.installText -ne 'Install' -or
        $english.spanishOverture -or
        $english.spanishBlackPlague -or
        $english.spanishRequiem -or
        -not $english.translationsEditable -or
        -not $english.bannerPresent -or -not $english.recommendedSettings -or
        ($english.tabs -join ',') -ne 'Install,Maintenance' -or
        -not $english.bannerFullWidth -or $english.primaryActions -ne 1 -or
        -not $english.primaryVisible) {
        throw 'English UI defaults do not match the v1.0 installer contract.'
    }

    $spanish = (& powershell -NoProfile -ExecutionPolicy Bypass -File $gui -UiContract -Language es | Out-String).Trim() | ConvertFrom-Json
    if ($spanish.language -ne 'es' -or
        $spanish.installText -ne 'Instalar' -or
        -not $spanish.spanishOverture -or
        -not $spanish.spanishBlackPlague -or
        -not $spanish.spanishRequiem -or
        -not $spanish.translationsEditable) {
        throw 'Spanish UI did not auto-select the editable translation options.'
    }

    if (-not (Test-Path -LiteralPath $project -PathType Leaf)) {
        throw "Bootstrapper project is missing: $project"
    }

    New-Item -ItemType Directory -Path $temporaryRoot -Force | Out-Null
    $payloadRoot = Join-Path $temporaryRoot 'payload'
    $toolsRoot = Join-Path $payloadRoot 'tools'
    New-Item -ItemType Directory -Path $toolsRoot -Force | Out-Null
    Copy-Item -LiteralPath $gui -Destination (Join-Path $toolsRoot 'Install-PenumbraFrameworkGui.ps1')
    Copy-Item -LiteralPath (Join-Path $repoRoot 'tools/PenumbraVrInstallerUi.psm1') -Destination $toolsRoot
    [IO.File]::WriteAllText((Join-Path $toolsRoot 'Install-PenumbraFrameworkCandidate.ps1'), '# smoke payload')
    [IO.File]::WriteAllText((Join-Path $toolsRoot 'Get-PenumbraInstallations.ps1'), '# smoke payload')
    $sums=@(Get-ChildItem -LiteralPath $payloadRoot -Recurse -File | ForEach-Object { '{0}  {1}' -f (Get-FileHash $_.FullName).Hash,$_.FullName.Substring($payloadRoot.Length+1).Replace('\','/') })
    [IO.File]::WriteAllLines((Join-Path $payloadRoot 'SHA256SUMS.txt'),[string[]]$sums)
    $payloadZip = Join-Path $temporaryRoot 'payload.zip'
    Compress-Archive -Path (Join-Path $payloadRoot '*') -DestinationPath $payloadZip

    $publish = Join-Path $temporaryRoot 'publish'
    Push-Location (Split-Path -Parent $project)
    try{
    & dotnet publish $project -c Release -r win-x64 --self-contained true `
        -p:PublishSingleFile=true -p:DebugType=None `
        "-p:PenumbraVrPayloadPath=$payloadZip" "-p:PenumbraVrBannerPath=$banner" `
        -o $publish | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Bootstrapper publish failed.' }
    }finally{Pop-Location}

    $exe = Join-Path $publish 'PenumbraVR.Setup.exe'
    if (-not (Test-Path -LiteralPath $exe -PathType Leaf) -or
        @(Get-ChildItem -LiteralPath $publish -File | Where-Object Extension -eq '.dll').Count) {
        throw 'Bootstrapper publish is not a self-contained single executable.'
    }

    $defaultSmoke = (& $exe --smoke-test | Out-String).Trim() | ConvertFrom-Json
    $spanishSmoke = (& $exe --smoke-test --language es | Out-String).Trim() | ConvertFrom-Json
    if ($defaultSmoke.language -ne 'en' -or $defaultSmoke.spanishTranslations -or
        -not $defaultSmoke.payloadEmbedded -or -not $defaultSmoke.bannerEmbedded -or
        -not $defaultSmoke.payloadExtracted -or -not $defaultSmoke.tempCleaned -or
        $spanishSmoke.language -ne 'es' -or -not $spanishSmoke.spanishTranslations) {
        throw 'Bootstrapper smoke contract failed.'
    }

    Write-Host 'Single-EXE bootstrapper English/Spanish defaults, embedded resources and extraction smoke passed.'
} finally {
    if (Test-Path -LiteralPath $temporaryRoot -PathType Container) {
        Remove-Item -LiteralPath $temporaryRoot -Recurse -Force
    }
}
