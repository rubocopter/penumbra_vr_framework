[CmdletBinding()]
param(
    [string]$PackageRoot,
    [Parameter(Mandatory = $true)][string]$OutputPath,
    [string]$RuntimeDirectory
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$outputPath = [IO.Path]::GetFullPath($OutputPath)
if (Test-Path -LiteralPath $outputPath) {
    throw "Output archive already exists: $outputPath"
}

if (-not $PackageRoot) {
    & (Join-Path $PSScriptRoot 'Build-OvertureProduct.ps1') -Package -RuntimeDirectory $RuntimeDirectory
    $PackageRoot = Join-Path $repoRoot 'products/overture/build/package/Release/PenumbraVR'
}
$packageRoot = [IO.Path]::GetFullPath($PackageRoot).TrimEnd('\')
$tempRoot = Join-Path ([IO.Path]::GetTempPath()) ('PvrManualOverlay-' + [guid]::NewGuid().ToString('N'))
$stage = Join-Path $tempRoot 'stage'

try {
    New-Item -ItemType Directory -Path $stage -Force | Out-Null

    # The existing Overture packager checks every packaged file against the
    # source manifest before we turn the product package into an overlay.
    $verifiedZip = Join-Path $tempRoot 'verified-overture.zip'
    & (Join-Path $PSScriptRoot 'Package-OvertureCandidate.ps1') -PackageRoot $packageRoot -OutputPath $verifiedZip 6>$null

    $manifestPath = Join-Path $packageRoot 'SHA256SUMS.txt'
    $sourcePaths = @{}
    foreach ($line in Get-Content -LiteralPath $manifestPath) {
        if ($line -notmatch '^[a-fA-F0-9]{64}  (.+)$') {
            throw "Invalid Overture package manifest line: $line"
        }
        $relative = $Matches[1].Replace('\', '/')
        $sourcePaths[$relative] = $true
    }
    foreach ($required in @('Penumbra_vr.exe', 'openvr_api.dll', 'OpenAL32.dll',
                            'msvcp140.dll', 'vcruntime140.dll', 'vr/actions.json',
                            'config/English.lang', 'config/Espanol.lang')) {
        if (-not $sourcePaths.ContainsKey($required)) {
            throw "Manual overlay is missing required runtime payload: $required"
        }
    }

    $selectionPath = Join-Path $repoRoot 'products/overture/docs/TEXTURE_SELECTION.json'
    $selection = Get-Content -LiteralPath $selectionPath -Raw | ConvertFrom-Json
    $textures = @{}
    foreach ($entry in $selection.Files) {
        $relative = [string]$entry.Path
        if ([IO.Path]::IsPathRooted($relative) -or $relative -match '(^|[\\/])\.\.([\\/]|$)' -or
            $relative.Contains('\') -or $textures.ContainsKey($relative)) {
            throw "Unsafe or duplicate selected texture path: $relative"
        }
        if (-not $sourcePaths.ContainsKey($relative) -or
            (Get-FileHash -LiteralPath (Join-Path $packageRoot $relative) -Algorithm SHA256).Hash -cne $entry.SHA256 -or
            (Get-Item -LiteralPath (Join-Path $packageRoot $relative)).Length -ne $entry.Bytes) {
            throw "Selected texture failed package provenance/hash validation: $relative"
        }
        $textures[$relative] = $true
    }

    foreach ($file in Get-ChildItem -LiteralPath $packageRoot -Recurse -File | Sort-Object FullName) {
        $relative = $file.FullName.Substring($packageRoot.Length).TrimStart('\').Replace('\', '/')
        # Keep required VR resources in core; only manifest-selected textures
        # and the translation are opt-in, irrespective of their original folder.
        if ($relative -in @('SHA256SUMS.txt', 'Install-PenumbraVR.ps1',
                           'Install-PenumbraVR.bat', 'alsoft.ini') -or
            $relative -match '^(tools|assets)/') {
            continue
        }
        $destination = if ($relative -eq 'config/Espanol.lang') { 'optional/spanish/' + $relative }
                       elseif ($textures.ContainsKey($relative)) { 'optional/textures/' + $relative }
                       else { 'redist/' + $relative }
        $target = Join-Path $stage $destination
        New-Item -ItemType Directory -Path (Split-Path -Parent $target) -Force | Out-Null
        Copy-Item -LiteralPath $file.FullName -Destination $target
    }

    # The retail Steam entry point is Penumbra.exe, so an overlay must provide
    # this filename as well as the developer-facing Penumbra_vr.exe.
    Copy-Item -LiteralPath (Join-Path $packageRoot 'Penumbra_vr.exe') -Destination (Join-Path $stage 'redist/Penumbra.exe')

    $supportFiles = @{
        'OpenALSoft-LICENSE-pffft.txt' = 'assets/licenses/OpenALSoft-LICENSE-pffft.txt'
        'MANUAL-INSTALL.md' = 'docs/OVERTURE_MANUAL_LINUX.md'
        'install-linux.py' = 'tools/install-linux.py'
        'install-linux.sh' = 'tools/install-black-plague-requiem-linux.sh'
        'installer-game-profile.json' = 'assets/settings/installer-game-profile.json'
        'texture-selection.json' = 'products/overture/docs/TEXTURE_SELECTION.json'
        'optional/textures/TEXTURE_CREDITS.md' = 'products/overture/docs/TEXTURE_CREDITS.md'
        'optional/spanish/THIRD-PARTY-NOTICES.md' = 'docs/THIRD_PARTY.md'
    }
    foreach ($name in $supportFiles.Keys) {
        $target = Join-Path $stage $name
        New-Item -ItemType Directory -Path (Split-Path -Parent $target) -Force | Out-Null
        $source = Join-Path $repoRoot $supportFiles[$name]
        if ($name -eq 'install-linux.sh') {
            [IO.File]::WriteAllText($target, [IO.File]::ReadAllText($source).Replace("`r`n", "`n"),
                [Text.UTF8Encoding]::new($false))
        } else {
            Copy-Item -LiteralPath $source -Destination $target
        }
    }
    [IO.File]::WriteAllText((Join-Path $stage 'package-game.json'),
        "{`"game`":`"overture`"}`n", [Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText((Join-Path $stage 'optional/alsoft.ini'),
        "[general]`nhrtf = auto`n", [Text.UTF8Encoding]::new($false))

    # Match Setup's optional x64 companion; the user supplies NVIDIA's model.
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

    $lines = @(
        foreach ($file in Get-ChildItem -LiteralPath $stage -Recurse -File | Sort-Object FullName) {
            $relative = $file.FullName.Substring($stage.Length).TrimStart('\').Replace('\', '/')
            '{0}  {1}' -f (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant(), $relative
        }
    )
    # GNU sha256sum -c expects a standard LF-delimited, BOM-free manifest.
    [IO.File]::WriteAllText((Join-Path $stage 'SHA256SUMS.txt'),
        ([string]::Join("`n", [string[]]$lines) + "`n"), [Text.UTF8Encoding]::new($false))

    Import-Module (Join-Path $PSScriptRoot 'PenumbraVrArchive.psm1') -Force
    New-Item -ItemType Directory -Path (Split-Path -Parent $outputPath) -Force | Out-Null
    Write-PvrArchive -Root $stage -OutputPath $outputPath
    Write-Host "Created experimental Overture manual overlay: $outputPath"
} finally {
    if (Test-Path -LiteralPath $tempRoot -PathType Container) {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force
    }
}
