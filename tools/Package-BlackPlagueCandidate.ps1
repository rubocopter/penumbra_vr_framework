[CmdletBinding()]
param(
    [string]$BuildRoot,
    [Parameter(Mandatory = $true)][string]$OutputPath
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
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
