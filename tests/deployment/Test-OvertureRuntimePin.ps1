[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$packager = Join-Path $repoRoot 'products/overture/scripts/package.ps1'
$package = Join-Path $repoRoot 'products/overture/build/package/Release/PenumbraVR'
$manifest = Join-Path $package 'SHA256SUMS.txt'
$executable = Join-Path $package 'Penumbra_vr.exe'
if (-not (Test-Path -LiteralPath $manifest -PathType Leaf) -or
    -not (Test-Path -LiteralPath $executable -PathType Leaf)) {
    throw 'Build the Overture Release package before testing runtime dependency rejection.'
}
$manifestBefore = (Get-FileHash -LiteralPath $manifest -Algorithm SHA256).Hash
$executableBefore = (Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash

$tempBase = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath()).TrimEnd('\') + '\'
$fixture = [System.IO.Path]::GetFullPath((Join-Path $tempBase ('PenumbraVrRuntimePin-' + [guid]::NewGuid().ToString('N'))))
if (-not $fixture.StartsWith($tempBase, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Unsafe runtime fixture path.'
}
try {
    New-Item -ItemType Directory -Path $fixture | Out-Null
    [System.IO.File]::WriteAllText((Join-Path $fixture 'msvcp140.dll'), 'wrong runtime')
    [System.IO.File]::WriteAllText((Join-Path $fixture 'vcruntime140.dll'), 'wrong runtime')
    $rejected = $false
    try { & $packager -Configuration Release -RuntimeDirectory $fixture 6>$null | Out-Null }
    catch { $rejected = $_.Exception.Message -like '*pinned SHA-256*' }
    if (-not $rejected) { throw 'Overture packager did not reject unpinned Visual C++ runtime DLLs.' }
    if ((Get-FileHash -LiteralPath $manifest -Algorithm SHA256).Hash -ne $manifestBefore -or
        (Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash -ne $executableBefore) {
        throw 'Rejected runtime dependency changed the existing Overture package.'
    }
    Write-Host 'Overture rejects unpinned Visual C++ runtime DLLs before changing its existing package.'
}
finally {
    if (Test-Path -LiteralPath $fixture -PathType Container) {
        $resolved = [System.IO.Path]::GetFullPath($fixture)
        if (-not $resolved.StartsWith($tempBase, [StringComparison]::OrdinalIgnoreCase)) {
            throw "Unsafe runtime fixture cleanup path: $resolved"
        }
        Remove-Item -LiteralPath $resolved -Recurse -Force
    }
}
