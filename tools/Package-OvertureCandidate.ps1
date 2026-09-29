[CmdletBinding()]
param(
    [string]$PackageRoot,
    [Parameter(Mandatory = $true)][string]$OutputPath,
    [string]$RuntimeDirectory
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
& (Join-Path $PSScriptRoot 'Test-PenumbraVrMetadata.ps1')
$buildCurrentProduct = -not $PackageRoot
if ($buildCurrentProduct) {
    $PackageRoot = Join-Path $repoRoot 'products/overture/build/package/Release/PenumbraVR'
}
$packageRoot = [System.IO.Path]::GetFullPath($PackageRoot).TrimEnd('\')
$outputPath = [System.IO.Path]::GetFullPath($OutputPath)
if (Test-Path -LiteralPath $outputPath) {
    throw "Output archive already exists: $outputPath"
}
if ($buildCurrentProduct) {
    & (Join-Path $PSScriptRoot 'Build-OvertureProduct.ps1') -Package -RuntimeDirectory $RuntimeDirectory
}

$manifest = Join-Path $packageRoot 'SHA256SUMS.txt'
if (-not (Test-Path -LiteralPath $manifest -PathType Leaf)) {
    throw "Overture package manifest not found: $manifest"
}
$expected = [System.Collections.Generic.Dictionary[string,string]]::new([StringComparer]::OrdinalIgnoreCase)
foreach ($line in @(Get-Content -LiteralPath $manifest)) {
    if ($line -notmatch '^([0-9a-fA-F]{64})  (.+)$') {
        throw "Invalid Overture package manifest line: $line"
    }
    $relative = $Matches[2]
    if ([System.IO.Path]::IsPathRooted($relative) -or
        $relative -match '(^|[\\/])\.\.([\\/]|$)') {
        throw "Unsafe Overture package path: $relative"
    }
    $relative = $relative.Replace('\', '/')
    if ($expected.ContainsKey($relative)) {
        throw "Duplicate Overture package path: $relative"
    }
    $expected.Add($relative, $Matches[1].ToUpperInvariant())
}
$files = @(Get-ChildItem -LiteralPath $packageRoot -Recurse -File | Sort-Object FullName)
if ($expected.Count -ne ($files.Count - 1)) {
    throw 'Overture package file count differs from SHA256SUMS.txt.'
}
foreach ($file in $files) {
    if ($file.FullName -eq $manifest) { continue }
    $relative = $file.FullName.Substring($packageRoot.Length).TrimStart('\').Replace('\', '/')
    $hash = $null
    if (-not $expected.TryGetValue($relative, [ref]$hash) -or
        $hash -ne (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash) {
        throw "Overture package hash mismatch: $relative"
    }
}
foreach ($required in @('Penumbra_vr.exe', 'openvr_api.dll', 'OpenAL32.dll',
                        'Install-PenumbraVR.ps1', 'vr/actions.json')) {
    if (-not $expected.ContainsKey($required)) {
        throw "Required Overture package entry missing: $required"
    }
}

Add-Type -AssemblyName System.IO.Compression
New-Item -ItemType Directory -Path (Split-Path -Parent $outputPath) -Force | Out-Null
$stream = [System.IO.File]::Open($outputPath, [System.IO.FileMode]::CreateNew)
try {
    $zip = [System.IO.Compression.ZipArchive]::new(
        $stream, [System.IO.Compression.ZipArchiveMode]::Create, $false)
    try {
        $fixedTimestamp = [datetimeoffset]::new(2000, 1, 1, 0, 0, 0, [timespan]::Zero)
        foreach ($file in $files) {
            $name = $file.FullName.Substring($packageRoot.Length).TrimStart('\').Replace('\', '/')
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
Write-Host "Created Overture candidate package: $outputPath"
