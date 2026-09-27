[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$BlackPlagueExe,
    [Parameter(Mandatory = $true)][string]$RequiemExe,
    [Parameter(Mandatory = $true)][string]$BuildRoot
)

$ErrorActionPreference = 'Stop'
$tool = Join-Path $BuildRoot 'bin/Release/PenumbraVR.LaaTransform.exe'
if (-not (Test-Path -LiteralPath $tool -PathType Leaf)) { throw "LAA tool is missing: $tool" }
$canonicalHash = 'FD316F7586737A63EBA989ECE2271280FE6A98582A1319FE2151385A3DF97BFF'
$transformedHash = 'DB086CC7A4C7B10864DE0FEBBE2D71A3E4EFF1EC8D067811A6A59EDC1C617196'
if ((Get-FileHash -LiteralPath $BlackPlagueExe -Algorithm SHA256).Hash -ne $canonicalHash) {
    throw 'Test requires the known canonical Black Plague executable.'
}
$temporaryRoot = [System.IO.Path]::GetFullPath(
    (Join-Path ([System.IO.Path]::GetTempPath()) ('PenumbraVrLaaTest-' + [guid]::NewGuid().ToString('N'))))
$tempPrefix = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath()).TrimEnd('\') + '\'
if (-not $temporaryRoot.StartsWith($tempPrefix, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Unsafe LAA fixture path.'
}
try {
    New-Item -ItemType Directory -Path $temporaryRoot | Out-Null
    $original = Join-Path $temporaryRoot 'Penumbra.exe'
    $patched = Join-Path $temporaryRoot 'Patched.exe'
    Copy-Item -LiteralPath $BlackPlagueExe -Destination $original
    & $tool $original $patched
    if ($LASTEXITCODE -ne 0 -or
        (Get-FileHash -LiteralPath $patched -Algorithm SHA256).Hash -ne $transformedHash -or
        (Get-FileHash -LiteralPath $original -Algorithm SHA256).Hash -ne $canonicalHash) {
        throw 'LAA tool did not produce the exact recorded transformed build.'
    }
    try { & $tool $original $patched 2>$null | Out-Null } catch { }
    if ($LASTEXITCODE -eq 0) { throw 'LAA tool overwrote an existing output.' }
    $unknown = Join-Path $temporaryRoot 'Unknown.exe'
    Copy-Item -LiteralPath $original -Destination $unknown
    $bytes = [System.IO.File]::ReadAllBytes($unknown)
    $bytes[$bytes.Length - 1] = $bytes[$bytes.Length - 1] -bxor 1
    [System.IO.File]::WriteAllBytes($unknown, $bytes)
    $rejected = Join-Path $temporaryRoot 'Rejected.exe'
    try { & $tool $unknown $rejected 2>$null | Out-Null } catch { }
    if ($LASTEXITCODE -eq 0 -or (Test-Path -LiteralPath $rejected)) {
        throw 'LAA tool accepted an unknown executable.'
    }
    $requiemHash = 'B64232D751CEE376E1384CFE5A4A81DBD7DEDDF03983CC11D0D0A34D5825EEA2'
    $requiemLaaHash = '577D1D7780872CD6C5B99B45759CDC48FEE486A1CCBF319E8F6CF0EAED54E955'
    $requiemOutput = Join-Path $temporaryRoot 'RequiemLaa.exe'
    if ((Get-FileHash -LiteralPath $RequiemExe -Algorithm SHA256).Hash -ne $requiemHash) {
        throw 'Test requires the known canonical Requiem executable.'
    }
    & $tool $RequiemExe $requiemOutput | Out-Null
    if ($LASTEXITCODE -ne 0 -or
        (Get-FileHash -LiteralPath $requiemOutput -Algorithm SHA256).Hash -ne $requiemLaaHash -or
        (Get-FileHash -LiteralPath $RequiemExe -Algorithm SHA256).Hash -ne $requiemHash) {
        throw 'LAA tool did not produce the exact recorded Requiem variant.'
    }
    Write-Host 'LAA tool transforms both exact canonical builds and preserves their sources.'
} finally {
    if (Test-Path -LiteralPath $temporaryRoot -PathType Container) {
        Remove-Item -LiteralPath $temporaryRoot -Recurse -Force
    }
}
