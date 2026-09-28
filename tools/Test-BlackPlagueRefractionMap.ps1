[CmdletBinding()]
param([Parameter(Mandatory = $true)][string]$ImagePath)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$manifest = Get-Content -LiteralPath (Join-Path $repoRoot 'manifests/black_plague/FD316F7586737A63EBA989ECE2271280FE6A98582A1319FE2151385A3DF97BFF.json') -Raw | ConvertFrom-Json
$copy = $manifest.symbols.'hpl.cLowLevelGraphicsSDL.CopyContextToTexure'
if ((Get-FileHash -LiteralPath $ImagePath -Algorithm SHA256).Hash -ne $copy.sourceEvidence.captureSha256) {
    throw 'The refraction verifier requires the recorded initialized Black Plague capture.'
}
$image = [System.IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $ImagePath).Path)
function Rva([string]$Value) { return [Convert]::ToInt32($Value.Substring(2), 16) }
function Assert-Bytes([int]$At, [byte[]]$Expected) {
    for ($i = 0; $i -lt $Expected.Length; ++$i) {
        if ($image[$At + $i] -ne $Expected[$i]) {
            throw ('Refraction ABI mismatch at RVA 0x{0:X}' -f ($At + $i))
        }
    }
}
$entry = Rva $copy.rva
$signature = @($copy.signature.bytes.Split(' ') | ForEach-Object { [Convert]::ToByte($_, 16) })
Assert-Bytes $entry $signature
$base = Rva $manifest.executable.preferredImageBase
if ([BitConverter]::ToUInt32($image, (Rva $copy.vtableSlotRva)) -ne ($base + $entry)) {
    throw 'The native graphics vtable no longer points to the mapped screen-copy function.'
}
Assert-Bytes ($entry + 0x6A) @(0xFF, 0x15, 0xE0, 0x25, 0x67, 0x00) # glCopyTexSubImage2D
Assert-Bytes ($entry + 0x74) @(0xC2, 0x10, 0x00) # four thiscall stack arguments
foreach ($site in $copy.refractionCallSites) {
    $at = Rva $site.rva
    Assert-Bytes $at @(0xFF, 0x92, 0x74, 0x01, 0x00, 0x00)
    if ((Rva $site.returnRva) -ne ($at + 6)) { throw 'Invalid refraction return boundary.' }
}
foreach ($site in $copy.otherScreenCopyReturnRvas) {
    Assert-Bytes ((Rva $site) - 6) @(0xFF, 0x92, 0x74, 0x01, 0x00, 0x00)
}
Write-Output 'Black Plague initialized refraction vtable, copy ABI and six caller boundaries verified.'
