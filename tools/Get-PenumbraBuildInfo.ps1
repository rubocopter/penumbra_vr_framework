[CmdletBinding()]
param(
    [Parameter(Mandatory = $true, Position = 0)]
    [ValidateNotNullOrEmpty()]
    [string[]] $Path,

    [switch] $AsJson
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$knownBuilds = @{
    '95ACB863441A17E701AF2CD1B1EF301C55C1AC620269A167275580EB6954A448' = @{
        Game = 'Overture'
        BuildId = 'overture-retail-observed'
        Variant = 'observed'
        CanonicalSHA256 = '95ACB863441A17E701AF2CD1B1EF301C55C1AC620269A167275580EB6954A448'
        Status = 'recognized-only'
    }
    'A88F605CE01D5E1F053B2F8450E7EFA77F8C6E50622303AC2D6736C2694DBC71' = @{
        Game = 'Overture'
        BuildId = 'overture-vr-rework-v0.1.0'
        Variant = 'observed'
        CanonicalSHA256 = 'A88F605CE01D5E1F053B2F8450E7EFA77F8C6E50622303AC2D6736C2694DBC71'
        Status = 'external-rework'
    }
    'FD316F7586737A63EBA989ECE2271280FE6A98582A1319FE2151385A3DF97BFF' = @{
        Game = 'Black Plague'
        BuildId = 'black-plague-steam-observed'
        Variant = 'observed'
        CanonicalSHA256 = 'FD316F7586737A63EBA989ECE2271280FE6A98582A1319FE2151385A3DF97BFF'
        Status = 'research-target'
    }
    'DB086CC7A4C7B10864DE0FEBBE2D71A3E4EFF1EC8D067811A6A59EDC1C617196' = @{
        Game = 'Black Plague'
        BuildId = 'black-plague-steam-observed'
        Variant = 'large-address-aware'
        CanonicalSHA256 = 'FD316F7586737A63EBA989ECE2271280FE6A98582A1319FE2151385A3DF97BFF'
        Status = 'research-target'
    }
    'B64232D751CEE376E1384CFE5A4A81DBD7DEDDF03983CC11D0D0A34D5825EEA2' = @{
        Game = 'Requiem'
        BuildId = 'requiem-steam-observed'
        Variant = 'observed'
        CanonicalSHA256 = 'B64232D751CEE376E1384CFE5A4A81DBD7DEDDF03983CC11D0D0A34D5825EEA2'
        Status = 'research-target'
    }
    '577D1D7780872CD6C5B99B45759CDC48FEE486A1CCBF319E8F6CF0EAED54E955' = @{
        Game = 'Requiem'
        BuildId = 'requiem-steam-observed'
        Variant = 'large-address-aware'
        CanonicalSHA256 = 'B64232D751CEE376E1384CFE5A4A81DBD7DEDDF03983CC11D0D0A34D5825EEA2'
        Status = 'research-target'
    }
}

function Read-PeMachine {
    param(
        [Parameter(Mandatory = $true)]
        [string] $LiteralPath
    )

    $stream = [System.IO.File]::Open(
        $LiteralPath,
        [System.IO.FileMode]::Open,
        [System.IO.FileAccess]::Read,
        [System.IO.FileShare]::ReadWrite
    )
    $reader = [System.IO.BinaryReader]::new($stream)

    try {
        if ($reader.ReadUInt16() -ne 0x5A4D) {
            throw "Not a PE executable (missing MZ header): $LiteralPath"
        }

        $stream.Position = 0x3C
        $peOffset = $reader.ReadUInt32()
        if ($peOffset -gt ($stream.Length - 6)) {
            throw "Invalid PE header offset: $LiteralPath"
        }

        $stream.Position = $peOffset
        if ($reader.ReadUInt32() -ne 0x00004550) {
            throw "Not a PE executable (missing PE signature): $LiteralPath"
        }

        $machine = $reader.ReadUInt16()
        switch ($machine) {
            0x014C { return 'x86' }
            0x8664 { return 'x64' }
            0x01C0 { return 'ARM' }
            0xAA64 { return 'ARM64' }
            default { return ('unknown-0x{0:X4}' -f $machine) }
        }
    }
    finally {
        $reader.Dispose()
        $stream.Dispose()
    }
}

$results = foreach ($candidatePath in $Path) {
    $resolvedPath = (Resolve-Path -LiteralPath $candidatePath).Path
    $file = Get-Item -LiteralPath $resolvedPath
    if ($file.PSIsContainer) {
        throw "Expected an executable file, received a directory: $resolvedPath"
    }

    $sha256 = (Get-FileHash -LiteralPath $resolvedPath -Algorithm SHA256).Hash.ToUpperInvariant()
    $known = $knownBuilds[$sha256]

    [PSCustomObject][ordered]@{
        SchemaVersion = 1
        Path = $file.FullName
        FileName = $file.Name
        SizeBytes = $file.Length
        Architecture = Read-PeMachine -LiteralPath $file.FullName
        SHA256 = $sha256
        KnownBuild = ($null -ne $known)
        Game = if ($null -ne $known) { $known.Game } else { 'Unknown' }
        BuildId = if ($null -ne $known) { $known.BuildId } else { 'unknown' }
        Variant = if ($null -ne $known) { $known.Variant } else { 'unknown' }
        CanonicalSHA256 = if ($null -ne $known) { $known.CanonicalSHA256 } else { $null }
        Status = if ($null -ne $known) { $known.Status } else { 'unknown' }
        SupportedByThisRepository = $false
    }
}

if ($AsJson) {
    $results | ConvertTo-Json -Depth 4
}
else {
    $results
}
