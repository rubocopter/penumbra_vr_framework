[CmdletBinding()]
param([Parameter(Mandatory = $true)][string]$ImagePath)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$inputRoot = Split-Path -Parent $PSScriptRoot
$inputSource = Get-Content -LiteralPath (Join-Path $inputRoot 'src/backends/black_plague/native_input_bridge.cpp') -Raw
$inputImage = [System.IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $ImagePath).Path)
if ($inputImage.Length -lt 0x272A88) { throw 'Capture is too short for the mapped virtual image.' }
function Assert-Call([int]$Site, [int]$Target) {
    if ($inputImage[$Site] -ne 0xE8 -or
        ($Site + 5 + [BitConverter]::ToInt32($inputImage, $Site + 1)) -ne $Target) {
        throw ('Native call mismatch at RVA 0x{0:X}' -f $Site)
    }
}
$inputQueries = [regex]::Matches($inputSource, '\{0x([0-9A-Fa-f]+),A::[a-z_]+(?:,Q::(held|released))?\}')
if ($inputQueries.Count -ne 76) { throw 'Review the verifier when changing the native query table.' }
foreach ($inputMatch in $inputQueries) {
    $inputRva = [Convert]::ToInt32($inputMatch.Groups[1].Value, 16)
    $inputTarget = switch ($inputMatch.Groups[2].Value) {
        held { 0xDA470 }; released { 0xDA510 }; default { 0xDA5B0 }
    }
    Assert-Call $inputRva $inputTarget
}
Assert-Call 0x51CD 0x9CBC0
Assert-Call 0x5227 0x9CC60
Assert-Call 0x4477 0x797B0
Assert-Call 0x4C70 0x945F0
Assert-Call 0x4FF2 0x6C7C0
if ([BitConverter]::ToUInt32($inputImage, 0x272A84) -ne 0x403BF0) { throw 'Update vtable mismatch.' }
Write-Output 'Verified 76 query calls, 2 movement calls, 3 menu cursor calls and Update vtable. No process was modified.'
if ($inputImage.Length -lt 0x292CC8) { throw 'Capture is too short for spatial vtables.' }
$spatialSlots = @{
    0x291BE8 = 0x189E30; 0x27D0D4 = 0xABA90; 0x27D12C = 0xAC900; 0x27D130 = 0xAA4C0
    0x27D0E4 = 0xA9FD0; 0x27D13C = 0xAD6C0
    0x292C3C = 0x19C2A0; 0x292C44 = 0x19C2C0
    0x292C5C = 0x19C360; 0x292C64 = 0x19C380; 0x292CC4 = 0x19C590
}
foreach ($spatialSlot in $spatialSlots.GetEnumerator()) {
    if ([BitConverter]::ToUInt32($inputImage, $spatialSlot.Key) -ne (0x400000 + $spatialSlot.Value)) {
        throw ('Spatial vtable mismatch at RVA 0x{0:X}' -f $spatialSlot.Key)
    }
}
function Assert-Bytes([int]$At, [byte[]]$Expected) {
    for ($i = 0; $i -lt $Expected.Length; ++$i) {
        if ($inputImage[$At + $i] -ne $Expected[$i]) { throw ('Spatial instruction mismatch at RVA 0x{0:X}' -f $At) }
    }
}
Assert-Bytes 0xCA120 @(0x56,0x8B,0x74,0x24,0x08,0x57,0x8B,0xC1)
Assert-Bytes 0xCCF00 @(0x8B,0x91,0x54,0x03,0,0,0x85,0xD2)
Assert-Bytes 0xAD84F @(0xFF,0x57,0x68)
Assert-Bytes 0xACBF0 @(0x89,0x4E,0x14) # native local contact x store
Assert-Bytes 0xACBF6 @(0x89,0x56,0x18) # y
Assert-Bytes 0xACBFF @(0x89,0x46,0x1C) # z
Write-Output 'Verified 11 spatial method slots, SetMatrix/GetJointNum entries, picking call and local contact stores.'
