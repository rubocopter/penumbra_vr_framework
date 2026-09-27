[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$packager = Join-Path $repoRoot 'tools/Package-OvertureCandidate.ps1'
$source = Join-Path $repoRoot 'products/overture/PenumbraOverture/VRSettings.cpp'
$builtExe = Join-Path $repoRoot 'products/overture/build/bin/Release/Penumbra_vr.exe'
$packagedExe = Join-Path $repoRoot 'products/overture/build/package/Release/PenumbraVR/Penumbra_vr.exe'
if (-not (Test-Path -LiteralPath $source -PathType Leaf) -or
    -not (Test-Path -LiteralPath $builtExe -PathType Leaf)) {
    throw 'Build Overture Release before testing package freshness.'
}
$originalSourceTime = (Get-Item -LiteralPath $source).LastWriteTimeUtc
$builtBeforeTime = (Get-Item -LiteralPath $builtExe).LastWriteTimeUtc
$tempBase = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath()).TrimEnd('\') + '\'
$fixture = [System.IO.Path]::GetFullPath((Join-Path $tempBase ('PenumbraVrOvertureBuildGate-' + [guid]::NewGuid().ToString('N'))))
if (-not $fixture.StartsWith($tempBase, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Unsafe Overture build-gate fixture path.'
}
try {
    New-Item -ItemType Directory -Path $fixture | Out-Null
    (Get-Item -LiteralPath $source).LastWriteTimeUtc = [DateTime]::UtcNow.AddSeconds(2)
    & $packager -OutputPath (Join-Path $fixture 'Candidate.zip') 6>$null | Out-Null
    if ((Get-Item -LiteralPath $builtExe).LastWriteTimeUtc -le $builtBeforeTime) {
        throw 'Overture packager shipped an executable older than an edited source file.'
    }
    if ((Get-FileHash -LiteralPath $builtExe -Algorithm SHA256).Hash -ne
        (Get-FileHash -LiteralPath $packagedExe -Algorithm SHA256).Hash) {
        throw 'Overture packager shipped an executable different from the current Release build.'
    }
    Write-Host 'Overture packager rebuilt an edited source before assembling its ZIP.'
}
finally {
    (Get-Item -LiteralPath $source).LastWriteTimeUtc = $originalSourceTime
    if (Test-Path -LiteralPath $fixture -PathType Container) {
        $resolved = [System.IO.Path]::GetFullPath($fixture)
        if (-not $resolved.StartsWith($tempBase, [StringComparison]::OrdinalIgnoreCase)) {
            throw "Unsafe Overture build-gate fixture cleanup path: $resolved"
        }
        Remove-Item -LiteralPath $resolved -Recurse -Force
    }
}
