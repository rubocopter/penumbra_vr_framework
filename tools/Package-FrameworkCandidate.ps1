[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$OutputPath
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
& (Join-Path $PSScriptRoot 'Test-PenumbraVrMetadata.ps1')
$outputPath = [System.IO.Path]::GetFullPath($OutputPath)
if (Test-Path -LiteralPath $outputPath) {
    throw "Output archive already exists: $outputPath"
}
$stageName = 'PenumbraVrFrameworkPackage-' + [guid]::NewGuid().ToString('N')
$tempRoot = [System.IO.Path]::GetFullPath((Join-Path ([System.IO.Path]::GetTempPath()) $stageName))
$tempPrefix = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath()).TrimEnd('\') + '\'
if (-not $tempRoot.StartsWith($tempPrefix, [StringComparison]::OrdinalIgnoreCase)) {
    throw "Unsafe package staging path: $tempRoot"
}
$stage = Join-Path $tempRoot 'stage'
$inputs = Join-Path $tempRoot 'inputs'
try {
    New-Item -ItemType Directory -Path $stage, $inputs -Force | Out-Null
    $overtureZip = Join-Path $inputs 'Overture.zip'
    $blackPlagueZip = Join-Path $inputs 'BlackPlague.zip'
    & (Join-Path $PSScriptRoot 'Package-OvertureCandidate.ps1') -OutputPath $overtureZip 6>$null
    & (Join-Path $PSScriptRoot 'Package-BlackPlagueCandidate.ps1') -OutputPath $blackPlagueZip 6>$null
    Expand-Archive -LiteralPath $overtureZip -DestinationPath (Join-Path $stage 'products/overture')
    Expand-Archive -LiteralPath $blackPlagueZip -DestinationPath (Join-Path $stage 'products/black_plague')
    $toolsRoot = Join-Path $stage 'tools'
    New-Item -ItemType Directory -Path $toolsRoot | Out-Null
    foreach ($name in @('Install-PenumbraFrameworkCandidate.ps1',
                        'Install-PenumbraFrameworkCandidate.cmd',
                        'Install-PenumbraFrameworkGui.ps1',
                        'Instalar-Penumbra-VR.vbs',
                        'Get-PenumbraInstallations.ps1', 'Get-PenumbraBuildInfo.ps1')) {
        Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination $toolsRoot
    }
    @'
# Penumbra VR Framework candidate

This archive contains host-tested Overture and shared Black Plague/Requiem
deployment candidates. Requiem's runtime has representative headset evidence;
the full game and this installer are not yet headset validated or supported.

Double-click `tools\Instalar-Penumbra-VR.vbs` for the graphical installer.
It hides the PowerShell console, detects exact builds, and presents install,
repair and uninstall actions. For diagnostics, run
`tools\Install-PenumbraFrameworkCandidate.cmd` to list installations and
select one or more compatible games by entering their numbers separated by commas.
Use `-Selections 1,3` to select displayed numbers without a prompt, `-List` for
read-only discovery, `-GamePath` for one game folder or executable,
`-ManualPaths` for non-Steam folders and `-Restore` to undo selections.
Black Plague and Requiem share one redist transaction. Overture has its own;
if a later root fails, earlier completed roots remain installed or restored.
Exit selected games before making changes.
Install, repair, recovery and restore write JSONL events to
`%LOCALAPPDATA%\PenumbraVR\installer.jsonl` with the package checksum, game,
path and result. Use `-LogPath <file>` to choose another location; `-List` does
not create a log.
If a deployment was interrupted, run the selector with `-Recover -Game
Overture`, `-Recover -Game BlackPlague` or `-Recover -Game Requiem` with `-GamePath <game folder or
executable>` before another install. Recovery works when the executable is
missing.
Use `-Repair -Game Overture`, `-Repair -Game BlackPlague` or `-Repair -Game Requiem` with an explicit
`-GamePath <game folder or executable>` to replace damaged recorded mod files
from this verified package. Repair requires the recorded original backups;
Black Plague can reconstruct its managed LAA executable from the verified
canonical backup. User audio settings remain under strict checks.
For the shared redist, `-LargeAddressAware` applies the verified Black Plague
PE transform with a managed canonical backup. Requiem's executable is unchanged.

The product subfolders retain installer and license notices. SteamVR is
required for gameplay.
'@ | Set-Content -LiteralPath (Join-Path $stage 'PACKAGE-README.md') -Encoding UTF8

    $checksums = @(
        foreach ($file in @(Get-ChildItem -LiteralPath $stage -Recurse -File | Sort-Object FullName)) {
            $relative = $file.FullName.Substring($stage.Length).TrimStart('\').Replace('\', '/')
            '{0}  {1}' -f (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash, $relative
        }
    )
    [System.IO.File]::WriteAllLines((Join-Path $stage 'SHA256SUMS.txt'),
        [string[]]$checksums, [System.Text.UTF8Encoding]::new($false))

    Add-Type -AssemblyName System.IO.Compression
    New-Item -ItemType Directory -Path (Split-Path -Parent $outputPath) -Force | Out-Null
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
    Write-Host "Created Framework candidate package: $outputPath"
} finally {
    if (Test-Path -LiteralPath $tempRoot -PathType Container) {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force
    }
}
