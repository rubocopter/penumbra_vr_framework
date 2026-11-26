[CmdletBinding()]
param(
    [string]$BuildRoot,
    [Parameter(Mandatory = $true)][string]$OutputPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
if (-not $BuildRoot) { $BuildRoot = Join-Path $repoRoot 'build' }
$buildRoot = [IO.Path]::GetFullPath($BuildRoot)
$outputPath = [IO.Path]::GetFullPath($OutputPath)
if (Test-Path -LiteralPath $outputPath) { throw "Output archive already exists: $outputPath" }

$tempRoot = Join-Path ([IO.Path]::GetTempPath()) ('PvrBpRqManual-' + [guid]::NewGuid().ToString('N'))
$candidate = Join-Path $tempRoot 'candidate.zip'
$candidateRoot = Join-Path $tempRoot 'candidate'
$stage = Join-Path $tempRoot 'overlay'

try {
    New-Item -ItemType Directory -Path $tempRoot,$stage -Force | Out-Null

    # The canonical candidate builder rebuilds both probes and verifies pinned
    # OpenVR/OpenAL dependencies before we select the portable overlay content.
    & (Join-Path $PSScriptRoot 'Package-BlackPlagueCandidate.ps1') -BuildRoot $buildRoot -OutputPath $candidate
    if (-not (Test-Path -LiteralPath $candidate -PathType Leaf)) {
        throw 'Black Plague/Requiem candidate was not produced.'
    }
    Expand-Archive -LiteralPath $candidate -DestinationPath $candidateRoot

    $deployment = Get-Content -LiteralPath (Join-Path $repoRoot 'assets/deployment/manifest.json') -Raw | ConvertFrom-Json
    $products = @($deployment.products | Where-Object { $_.game -eq 'black_plague' })
    if ($deployment.schemaVersion -ne 1 -or $products.Count -ne 1) {
        throw 'Expected one Black Plague deployment manifest.'
    }
    $requiredIds = @('bootstrap_proxy','openal_proxy','openal_implementation',
                     'probe','requiem_probe','openvr_loader','openvr_actions','hand_texture')
    foreach ($id in $requiredIds) {
        $entries = @($products[0].payloads | Where-Object { $_.id -eq $id })
        if ($entries.Count -ne 1) { throw "Expected one deployment payload: $id" }
        $entry = $entries[0]
        $sourceRelative = [string]$entry.source.path
        $destinationRelative = [string]$entry.destination
        if ([IO.Path]::IsPathRooted($sourceRelative) -or
            $sourceRelative -match '(^|[\\/])\.\.([\\/]|$)' -or
            [IO.Path]::IsPathRooted($destinationRelative) -or
            $destinationRelative -match '(^|[\\/])\.\.([\\/]|$)') {
            throw "Unsafe deployment path for $id"
        }
        switch ([string]$entry.source.kind) {
            'build-output' { $source = Join-Path $candidateRoot (Join-Path 'build/bin/Release' $sourceRelative) }
            'repository-file' { $source = Join-Path $candidateRoot $sourceRelative }
            'repository-directory' { $source = Join-Path $candidateRoot $sourceRelative }
            default { throw "Unexpected source kind for $id" }
        }
        if (-not (Test-Path -LiteralPath $source)) { throw "Missing candidate payload: $id" }
        $target = Join-Path $stage (Join-Path 'redist' $destinationRelative)
        New-Item -ItemType Directory -Path (Split-Path -Parent $target) -Force | Out-Null
        Copy-Item -LiteralPath $source -Destination $target -Recurse
    }

    $pinned = Get-Content -LiteralPath (Join-Path $repoRoot 'assets/deployment/prerequisites.json') -Raw | ConvertFrom-Json
    foreach ($entry in @($pinned.bundledLibraries | Where-Object { $_.name -in @('OpenVR loader','OpenAL Soft') })) {
        $destination = if ($entry.name -eq 'OpenVR loader') { 'openvr_api.dll' }
                       elseif ($entry.path -eq 'PenumbraVR_OpenALSoft.dll') { 'PenumbraVR_OpenALSoft.dll' }
                       else { continue }
        $path = Join-Path $stage (Join-Path 'redist' $destination)
        if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -cne [string]$entry.sha256) {
            throw "Pinned dependency checksum mismatch: $destination"
        }
    }

    $localization = Get-Content -LiteralPath (Join-Path $repoRoot 'assets/localization/manifest.json') -Raw | ConvertFrom-Json
    if ($localization.schemaVersion -ne 1) { throw 'Unsupported localization manifest.' }
    foreach ($gameId in @('black_plague', 'requiem')) {
        $entries = @($localization.translations | Where-Object { $_.game -ceq $gameId })
        if ($entries.Count -ne 1) { throw "Missing/ambiguous localization: $gameId" }
        $entry = $entries[0]
        foreach ($relative in @($entry.sourcePath, $entry.installPath, $entry.noticePath)) {
            if ([IO.Path]::IsPathRooted($relative) -or $relative -match '(^|[\\/])\.\.([\\/]|$)') {
                throw "Unsafe localization path for $gameId"
            }
        }
        $expectedInstallPath = if ($gameId -eq 'black_plague') { 'redist/config/Espanol.lang' }
                               else { 'redist/expansion01/config/Espanol_exp.lang' }
        if ($entry.installPath -cne $expectedInstallPath) { throw "Unexpected localization destination: $gameId" }
        $source = Join-Path $repoRoot $entry.sourcePath
        if ((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -cne [string]$entry.sha256) {
            throw "Localization checksum mismatch: $gameId"
        }
        $target = Join-Path $stage ('optional/spanish/' + $entry.installPath.Substring('redist/'.Length))
        New-Item -ItemType Directory -Path (Split-Path -Parent $target) -Force | Out-Null
        Copy-Item -LiteralPath $source -Destination $target
        Copy-Item -LiteralPath (Join-Path $repoRoot $entry.noticePath) -Destination (Join-Path $stage "optional/spanish/$gameId-leeme.txt")
    }
    [IO.File]::WriteAllText((Join-Path $stage 'package-game.json'),
        "{`"game`":`"black_plague`"}`n", [Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText((Join-Path $stage 'optional/alsoft.ini'),
        "[general]`nhrtf = auto`n", [Text.UTF8Encoding]::new($false))

    # Use Setup's x64 companion build, with notices but no NVIDIA model.
    $neuralBuild = Join-Path $repoRoot 'build/neural-host'
    & cmake -S (Join-Path $repoRoot 'tools/neural_post') -B $neuralBuild -A x64
    if ($LASTEXITCODE -ne 0) { throw 'Neural companion configure failed.' }
    & cmake --build $neuralBuild --config Release
    if ($LASTEXITCODE -ne 0) { throw 'Neural companion build failed.' }
    $neuralRoot = Join-Path $stage 'optional/neural'
    New-Item -ItemType Directory -Path $neuralRoot -Force | Out-Null
    foreach ($name in @('PenumbraNeuralHost.exe', 'nvngx.dll_nr_bridge.dll')) {
        Copy-Item -LiteralPath (Join-Path $neuralBuild "Release/$name") -Destination $neuralRoot
    }
    Copy-Item -LiteralPath (Join-Path $repoRoot 'tools/neural_post/README.md') -Destination $neuralRoot
    Copy-Item -LiteralPath (Join-Path $repoRoot 'tools/neural_post/vendor/nr-bridge/LICENSE') -Destination (Join-Path $neuralRoot 'DirectNR-LICENSE.txt')
    Copy-Item -LiteralPath (Join-Path $repoRoot 'tools/neural_post/NVIDIA-SDK-LICENSE.txt') -Destination $neuralRoot

    $notices = @{
        'OpenALSoft-LICENSE-pffft.txt' = 'assets/licenses/OpenALSoft-LICENSE-pffft.txt'
        'OpenVR-LICENSE.txt' = 'products/overture/dependencies/openvr-2.15.6/LICENSE'
        'COPYING' = 'COPYING'
        'THIRD-PARTY-NOTICES.md' = 'docs/THIRD_PARTY.md'
        'OpenALSoft-COPYING' = 'products/overture/dependencies/bin/win32/OpenALSoft-COPYING'
        'OpenALSoft-readme.txt' = 'products/overture/dependencies/bin/win32/OpenALSoft-readme.txt'
        'MANUAL-INSTALL.md' = 'docs/BLACK_PLAGUE_REQUIEM_MANUAL_LINUX.md'
        'install-linux.sh' = 'tools/install-black-plague-requiem-linux.sh'
        'install-linux.py' = 'tools/install-linux.py'
        'installer-game-profile.json' = 'assets/settings/installer-game-profile.json'
    }
    foreach ($name in $notices.Keys) {
        $source = Join-Path $repoRoot $notices[$name]
        $target = Join-Path $stage $name
        if ($name -eq 'install-linux.sh') {
            [IO.File]::WriteAllText($target, [IO.File]::ReadAllText($source).Replace("`r`n", "`n"),
                [Text.UTF8Encoding]::new($false))
        } else {
            Copy-Item -LiteralPath $source -Destination $target
        }
    }

    $lines = @(
        foreach ($file in Get-ChildItem -LiteralPath $stage -Recurse -File | Sort-Object FullName) {
            $relative = $file.FullName.Substring($stage.Length).TrimStart('\').Replace('\', '/')
            '{0}  {1}' -f (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant(), $relative
        }
    )
    [IO.File]::WriteAllText((Join-Path $stage 'SHA256SUMS.txt'),
        ([string]::Join("`n", [string[]]$lines) + "`n"), [Text.UTF8Encoding]::new($false))

    Import-Module (Join-Path $PSScriptRoot 'PenumbraVrArchive.psm1') -Force
    New-Item -ItemType Directory -Path (Split-Path -Parent $outputPath) -Force | Out-Null
    Write-PvrArchive -Root $stage -OutputPath $outputPath
    Write-Host "Created experimental Black Plague/Requiem manual overlay: $outputPath"
} finally {
    if (Test-Path -LiteralPath $tempRoot -PathType Container) {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force
    }
}
