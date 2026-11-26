[CmdletBinding()]
param(
    [string]$PackageRoot,
    [string]$ArchivePath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$packager = Join-Path $repoRoot 'tools/Package-OvertureManualOverlay.ps1'
$tempRoot = Join-Path ([IO.Path]::GetTempPath()) ('PvrManualOverlayTest-' + [guid]::NewGuid().ToString('N'))

try {
    New-Item -ItemType Directory -Path $tempRoot -Force | Out-Null
    if (-not $ArchivePath) {
        if (-not $PackageRoot) {
            $PackageRoot = Join-Path $repoRoot 'products/overture/build/package/Release/PenumbraVR'
        }
        $ArchivePath = Join-Path $tempRoot 'manual.zip'
        $archiveAgain = Join-Path $tempRoot 'manual-again.zip'
        & $packager -PackageRoot $PackageRoot -OutputPath $ArchivePath 6>$null
        & $packager -PackageRoot $PackageRoot -OutputPath $archiveAgain 6>$null
        if ((Get-FileHash -LiteralPath $ArchivePath -Algorithm SHA256).Hash -cne
            (Get-FileHash -LiteralPath $archiveAgain -Algorithm SHA256).Hash) {
            throw 'Manual overlay archive is not deterministic.'
        }
    }

    Add-Type -AssemblyName System.IO.Compression
    $zip = [IO.Compression.ZipFile]::OpenRead([IO.Path]::GetFullPath($ArchivePath))
    try {
        $names = @($zip.Entries | ForEach-Object { $_.FullName })
        if (@($names | Select-Object -Unique).Count -ne $names.Count) {
            throw 'Manual overlay has duplicate ZIP entries.'
        }
        foreach ($required in @('redist/Penumbra.exe', 'redist/Penumbra_vr.exe',
                                'redist/openvr_api.dll', 'redist/OpenAL32.dll',
                                'redist/msvcp140.dll', 'redist/vcruntime140.dll',
                                'redist/vr/actions.json', 'redist/config/English.lang',
                                'MANUAL-INSTALL.md', 'SHA256SUMS.txt')) {
            if ($names -cnotcontains $required) { throw "Manual overlay lacks $required" }
        }
        if (@($names | Where-Object {
            $_ -match '(?i)(^|/)(tools|assets)/|(^|/)Install-PenumbraVR|config/Espanol.lang|(^|/)\.penumbravr|^redist/SHA256SUMS.txt$' -or
            $_ -notmatch '^(redist/|MANUAL-INSTALL\.md$|SHA256SUMS\.txt$)' -or
            $_ -match '(^|/)\.\.(/|$)'
        }).Count) {
            throw 'Manual overlay contains an unexpected/unsafe or installer-only path.'
        }
    } finally { $zip.Dispose() }

    $unpacked = Join-Path $tempRoot 'unpacked'
    Expand-Archive -LiteralPath $ArchivePath -DestinationPath $unpacked
    $manifestPath = Join-Path $unpacked 'SHA256SUMS.txt'
    $manifestText = [IO.File]::ReadAllText($manifestPath)
    if ($manifestText.Contains("`r") -or -not $manifestText.EndsWith("`n")) {
        throw 'Manual overlay checksums must use LF line endings.'
    }
    $checks = @{}
    foreach ($line in Get-Content -LiteralPath $manifestPath) {
        if ($line -cnotmatch '^([a-f0-9]{64})  (.+)$') { throw 'Invalid overlay checksum line.' }
        $expectedHash = $Matches[1].ToUpperInvariant()
        $relative = $Matches[2]
        if ($checks.ContainsKey($relative)) { throw 'Duplicate overlay checksum.' }
        $checks[$relative] = $true
        $actual = Get-FileHash -LiteralPath (Join-Path $unpacked $relative) -Algorithm SHA256
        if ($actual.Hash -cne $expectedHash) { throw "Overlay checksum mismatch: $relative" }
    }
    $files = @(Get-ChildItem -LiteralPath $unpacked -Recurse -File)
    if ($checks.Count -ne ($files.Count - 1)) { throw 'Overlay checksum count mismatch.' }
    if ((Get-FileHash (Join-Path $unpacked 'redist/Penumbra.exe')).Hash -cne
        (Get-FileHash (Join-Path $unpacked 'redist/Penumbra_vr.exe')).Hash) {
        throw 'Overture Steam launcher executable does not match the VR executable.'
    }

    # Exercise the documented file overlay and restore against an isolated
    # synthetic game tree. This never touches an installed game.
    $game = Join-Path $tempRoot 'game'
    $redist = Join-Path $game 'redist'
    New-Item -ItemType Directory -Path (Join-Path $redist 'config') -Force | Out-Null
    Set-Content -LiteralPath (Join-Path $redist 'Penumbra.exe') -Value 'retail fixture'
    Set-Content -LiteralPath (Join-Path $redist 'config/English.lang') -Value 'original English fixture'
    Set-Content -LiteralPath (Join-Path $redist 'game-owned.txt') -Value 'untouched retail data'
    $backup = Join-Path $game 'redist.before-penumbravr'
    Copy-Item -LiteralPath $redist -Destination $backup -Recurse
    Copy-Item -Path (Join-Path $unpacked 'redist/*') -Destination $redist -Recurse -Force
    if ((Get-FileHash (Join-Path $redist 'Penumbra.exe')).Hash -cne
        (Get-FileHash (Join-Path $unpacked 'redist/Penumbra_vr.exe')).Hash -or
        -not (Test-Path (Join-Path $redist 'vr/actions.json')) -or
        (Get-Content (Join-Path $redist 'game-owned.txt') -Raw).Trim() -cne 'untouched retail data') {
        throw 'Documented overlay did not preserve retail files and apply the VR payload.'
    }
    Move-Item -LiteralPath $redist -Destination (Join-Path $game 'redist.penumbravr-test')
    Move-Item -LiteralPath $backup -Destination $redist
    if ((Get-Content (Join-Path $redist 'Penumbra.exe') -Raw).Trim() -cne 'retail fixture') {
        throw 'Manual overlay restore did not recover the retail executable.'
    }
    Write-Host 'Experimental manual Overture ZIP passed layout, integrity, overlay and restore fixture checks.'
} finally {
    if (Test-Path -LiteralPath $tempRoot -PathType Container) {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force
    }
}
