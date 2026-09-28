[CmdletBinding()]
param(
    [string]$BuildRoot,
    [Parameter(Mandatory = $true)][string]$OutputPath
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
& (Join-Path $PSScriptRoot 'Test-PenumbraVrMetadata.ps1')
if (-not $BuildRoot) {
    $BuildRoot = Join-Path $repoRoot 'build'
}
$buildRoot = [System.IO.Path]::GetFullPath($BuildRoot)
$outputPath = [System.IO.Path]::GetFullPath($OutputPath)
if (Test-Path -LiteralPath $outputPath) {
    throw "Output archive already exists: $outputPath"
}

$cachePath = Join-Path $buildRoot 'CMakeCache.txt'
if (-not (Test-Path -LiteralPath $cachePath -PathType Leaf) -or
    -not (Select-String -LiteralPath $cachePath -Pattern '^PENUMBRA_VR_OPENVR_SDK:[^=]+=.+$' -Quiet)) {
    throw 'Release build lacks a configured real OpenVR SDK.'
}
$configuredSource = @(Select-String -LiteralPath $cachePath -Pattern '^CMAKE_HOME_DIRECTORY:INTERNAL=(.+)$' |
    ForEach-Object { $_.Matches[0].Groups[1].Value })
if ($configuredSource.Count -ne 1 -or
    [System.IO.Path]::GetFullPath($configuredSource[0]).TrimEnd('\', '/') -ine
        [System.IO.Path]::GetFullPath($repoRoot).TrimEnd('\', '/')) {
    throw 'Black Plague Release build belongs to a different source checkout; configure the current repository before packaging.'
}
$configuredSdk = @(Select-String -LiteralPath $cachePath -Pattern '^PENUMBRA_VR_OPENVR_SDK:[^=]+=(.+)$' |
    ForEach-Object { $_.Matches[0].Groups[1].Value })
$pinnedSdk = Join-Path $repoRoot 'products/overture/dependencies/openvr-2.15.6'
if ($configuredSdk.Count -ne 1 -or
    [System.IO.Path]::GetFullPath($configuredSdk[0]).TrimEnd('\', '/') -ine
        [System.IO.Path]::GetFullPath($pinnedSdk).TrimEnd('\', '/')) {
    throw 'Black Plague Release build must use this checkout pinned OpenVR SDK.'
}

# The candidate contains three compiled outputs. Build their exact CMake
# targets before copying any of them so a source edit cannot silently ship an
# older DLL from a previous release build.
& cmake --build $buildRoot --config Release --target `
    pvr_black_plague_probe pvr_requiem_probe `
    pvr_black_plague_bootstrap pvr_laa_transform --parallel 4
if ($LASTEXITCODE -ne 0) {
    throw 'Black Plague Release build failed; candidate package was not created.'
}

$deployment = Get-Content -LiteralPath (Join-Path $repoRoot 'assets/deployment/manifest.json') -Raw | ConvertFrom-Json
$product = @($deployment.products | Where-Object { $_.game -eq 'black_plague' })
if ($deployment.schemaVersion -ne 1 -or $product.Count -ne 1) {
    throw 'Black Plague deployment manifest is missing or ambiguous.'
}

$stage = Join-Path ([System.IO.Path]::GetTempPath()) ('PenumbraVrBpPackage-' + [guid]::NewGuid().ToString('N'))
$stage = [System.IO.Path]::GetFullPath($stage)
$tempRoot = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath())
if (-not $stage.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Unsafe package staging path.'
}

function Copy-PackageItem([string]$Source, [string]$RelativeDestination) {
    if (-not (Test-Path -LiteralPath $Source)) {
        throw "Required package source is missing: $Source"
    }
    if ([System.IO.Path]::IsPathRooted($RelativeDestination) -or
        $RelativeDestination -match '(^|[\\/])\.\.([\\/]|$)') {
        throw "Unsafe package destination: $RelativeDestination"
    }
    $destination = Join-Path $stage $RelativeDestination
    New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
    Copy-Item -LiteralPath $Source -Destination $destination -Recurse
}

try {
    New-Item -ItemType Directory -Path $stage | Out-Null
    foreach ($payload in @($product[0].payloads)) {
        $kind = [string]$payload.source.kind
        if ($kind -eq 'generated') { continue }
        $relative = [string]$payload.source.path
        if ($kind -eq 'build-output') {
            if ($relative.Contains('/') -or $relative.Contains('\')) {
                throw "Unsafe build output path: $relative"
            }
            $source = Join-Path $buildRoot (Join-Path 'bin/Release' $relative)
            $target = Join-Path 'build/bin/Release' $relative
        } elseif ($kind -eq 'repository-file' -or $kind -eq 'repository-directory') {
            if ([System.IO.Path]::IsPathRooted($relative) -or
                $relative -match '(^|[\\/])\.\.([\\/]|$)') {
                throw "Unsafe repository source path: $relative"
            }
            $source = Join-Path $repoRoot $relative
            $target = $relative
        } else {
            throw "Unsupported package source kind: $kind"
        }
        Copy-PackageItem $source $target
    }

    $supportFiles = @(
        'assets/deployment/manifest.json',
        'assets/localization/manifest.json',
        'assets/localization/black_plague/leeme.txt',
        'assets/localization/requiem/leeme.txt',
        'tools/Install-BlackPlagueSteamBootstrap.ps1',
        'tools/Install-Black-Plague-VR.cmd',
        'tools/Get-PenumbraInstallations.ps1',
        'tools/Get-PenumbraBuildInfo.ps1',
        'COPYING',
        'docs/THIRD_PARTY.md',
        'products/overture/dependencies/openvr-2.15.6/LICENSE',
        'products/overture/dependencies/openvr-2.15.6/bin/win32/openvr_api.dll'
    )
    foreach ($relative in $supportFiles) {
        Copy-PackageItem (Join-Path $repoRoot $relative) $relative
    }
    Copy-PackageItem (Join-Path $buildRoot 'bin/Release/PenumbraVR.LaaTransform.exe') `
        'build/bin/Release/PenumbraVR.LaaTransform.exe'
    $licenseLoader = Join-Path $stage 'products/overture/dependencies/openvr-2.15.6/bin/win32/openvr_api.dll'
    $builtLoader = Join-Path $stage 'build/bin/Release/openvr_api.dll'
    if ((Get-FileHash -LiteralPath $licenseLoader -Algorithm SHA256).Hash -ne
        (Get-FileHash -LiteralPath $builtLoader -Algorithm SHA256).Hash) {
        throw 'Built OpenVR loader does not match the pinned SDK.'
    }

    @'
# Black Plague VR candidate

This package contains a development candidate for the allowlisted Steam x86
Black Plague build. It is not the final three-game Framework release.

Run `tools\Install-Black-Plague-VR.cmd` to detect a supported Steam installation.
For another Steam folder, pass `-SteamRoot "C:\path\to\Steam"`; if several
supported copies are found, pass `-GamePath "C:\path\to\redist\Penumbra.exe"`.
The installer checks the executable hash before changing the game. Use the same
command with `-Restore` to uninstall. It refuses unexpected changes to managed
files and reports the path that needs attention. SteamVR is required to play.
After an interrupted install or uninstall, pass `-Recover` with an explicit
`-GamePath` before another operation; the executable may be missing at recovery.
For a damaged installed payload, run `-Repair -GamePath <executable>`.
Repair requires its install record and verified original backups; unexpected
files in the managed OpenVR directory and changed audio settings are rejected.
A damaged managed LAA executable can be rebuilt from its canonical backup.
`-LargeAddressAware` optionally applies the recorded exact-build PE transform
inside the managed installation and restores the canonical executable on uninstall.

See `COPYING` and `docs/THIRD_PARTY.md` for licenses.
'@ | Set-Content -LiteralPath (Join-Path $stage 'PACKAGE-README.md') -Encoding UTF8

    Add-Type -AssemblyName System.IO.Compression
    $outputDirectory = Split-Path -Parent $outputPath
    New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
    $stream = [System.IO.File]::Open($outputPath, [System.IO.FileMode]::CreateNew)
    try {
        $zip = [System.IO.Compression.ZipArchive]::new(
            $stream, [System.IO.Compression.ZipArchiveMode]::Create, $false)
        try {
            $fixedTimestamp = [datetimeoffset]::new(2000, 1, 1, 0, 0, 0, [timespan]::Zero)
            foreach ($file in @(Get-ChildItem -LiteralPath $stage -Recurse -File | Sort-Object FullName)) {
                $name = $file.FullName.Substring($stage.Length).TrimStart('\').Replace('\', '/')
                $entry = $zip.CreateEntry($name, [System.IO.Compression.CompressionLevel]::Optimal)
                $entry.LastWriteTime = $fixedTimestamp
                $entryStream = $entry.Open()
                $sourceStream = [System.IO.File]::OpenRead($file.FullName)
                try { $sourceStream.CopyTo($entryStream) }
                finally {
                    $sourceStream.Dispose()
                    $entryStream.Dispose()
                }
            }
        } finally { $zip.Dispose() }
    } finally { $stream.Dispose() }
    Write-Host "Created Black Plague candidate package: $outputPath"
} finally {
    if (Test-Path -LiteralPath $stage -PathType Container) {
        Remove-Item -LiteralPath $stage -Recurse -Force
    }
}
