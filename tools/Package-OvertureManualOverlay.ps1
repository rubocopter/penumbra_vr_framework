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
                            'config/English.lang')) {
        if (-not $sourcePaths.ContainsKey($required)) {
            throw "Manual overlay is missing required runtime payload: $required"
        }
    }

    foreach ($file in Get-ChildItem -LiteralPath $packageRoot -Recurse -File | Sort-Object FullName) {
        $relative = $file.FullName.Substring($packageRoot.Length).TrimStart('\').Replace('\', '/')
        # These are Windows installer-only resources and opt-in localization.
        if ($relative -in @('SHA256SUMS.txt', 'Install-PenumbraVR.ps1',
                           'Install-PenumbraVR.bat', 'config/Espanol.lang') -or
            $relative -match '^(tools|assets)/') {
            continue
        }
        $target = Join-Path $stage ('redist/' + $relative)
        New-Item -ItemType Directory -Path (Split-Path -Parent $target) -Force | Out-Null
        Copy-Item -LiteralPath $file.FullName -Destination $target
    }

    # The retail Steam entry point is Penumbra.exe, so an overlay must provide
    # this filename as well as the developer-facing Penumbra_vr.exe.
    Copy-Item -LiteralPath (Join-Path $packageRoot 'Penumbra_vr.exe') -Destination (Join-Path $stage 'redist/Penumbra.exe')
    Copy-Item -LiteralPath (Join-Path $repoRoot 'docs/OVERTURE_MANUAL_LINUX.md') -Destination (Join-Path $stage 'MANUAL-INSTALL.md')

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
