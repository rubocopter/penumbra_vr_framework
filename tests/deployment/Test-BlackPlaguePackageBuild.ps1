[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$packager = Join-Path $repoRoot 'tools/Package-BlackPlagueCandidate.ps1'
$source = Join-Path $repoRoot 'src/probe/log.cpp'
$probe = Join-Path $repoRoot 'build/bin/Release/PenumbraVR.BlackPlague.Probe.dll'
if (-not (Test-Path -LiteralPath $source -PathType Leaf) -or
    -not (Test-Path -LiteralPath $probe -PathType Leaf)) {
    throw 'Configure and build Black Plague Release before testing package freshness.'
}
$originalSourceTime = (Get-Item -LiteralPath $source).LastWriteTimeUtc
$probeBeforeTime = (Get-Item -LiteralPath $probe).LastWriteTimeUtc
$tempBase = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath()).TrimEnd('\') + '\'
$fixture = [System.IO.Path]::GetFullPath((Join-Path $tempBase ('PenumbraVrBpBuildGate-' + [guid]::NewGuid().ToString('N'))))
if (-not $fixture.StartsWith($tempBase, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Unsafe build-gate fixture path.'
}
try {
    New-Item -ItemType Directory -Path $fixture | Out-Null
    (Get-Item -LiteralPath $source).LastWriteTimeUtc = [DateTime]::UtcNow.AddSeconds(2)
    & $packager -OutputPath (Join-Path $fixture 'Candidate.zip') 6>$null | Out-Null
    $probeAfterTime = (Get-Item -LiteralPath $probe).LastWriteTimeUtc
    if ($probeAfterTime -le $probeBeforeTime) {
        throw 'Black Plague packager shipped a probe older than an edited source file.'
    }
    Write-Host 'Black Plague packager rebuilt the changed probe before assembling its ZIP.'
}
finally {
    (Get-Item -LiteralPath $source).LastWriteTimeUtc = $originalSourceTime
    if (Test-Path -LiteralPath $fixture -PathType Container) {
        $resolved = [System.IO.Path]::GetFullPath($fixture)
        if (-not $resolved.StartsWith($tempBase, [StringComparison]::OrdinalIgnoreCase)) {
            throw "Unsafe build-gate fixture cleanup path: $resolved"
        }
        Remove-Item -LiteralPath $resolved -Recurse -Force
    }
}
