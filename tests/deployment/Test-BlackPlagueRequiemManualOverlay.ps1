[CmdletBinding()]
param(
    [string]$BuildRoot,
    [string]$ArchivePath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if (-not $BuildRoot) { $BuildRoot = Join-Path $repoRoot 'build' }
$buildRoot = [IO.Path]::GetFullPath($BuildRoot)
$packager = Join-Path $repoRoot 'tools/Package-BlackPlagueRequiemManualOverlay.ps1'
$tempRoot = Join-Path ([IO.Path]::GetTempPath()) ('PvrBpRqManualTest-' + [guid]::NewGuid().ToString('N'))

try {
    New-Item -ItemType Directory -Path $tempRoot -Force | Out-Null
    if (-not $ArchivePath) {
        $ArchivePath = Join-Path $tempRoot 'shared.zip'
        $repeatPath = Join-Path $tempRoot 'shared-again.zip'
        & $packager -BuildRoot $BuildRoot -OutputPath $ArchivePath
        & $packager -BuildRoot $BuildRoot -OutputPath $repeatPath
        if ((Get-FileHash $ArchivePath).Hash -cne (Get-FileHash $repeatPath).Hash) {
            throw 'Black Plague/Requiem manual ZIP is not deterministic.'
        }
    }

    Add-Type -AssemblyName System.IO.Compression
    $zip = [IO.Compression.ZipFile]::OpenRead([IO.Path]::GetFullPath($ArchivePath))
    try {
        $names = @($zip.Entries | ForEach-Object { $_.FullName })
        if (@($names | Select-Object -Unique).Count -ne $names.Count) {
            throw 'Duplicate archive entries.'
        }
        foreach ($required in @(
            'redist/alut.dll', 'redist/OpenAL32.dll', 'redist/PenumbraVR_OpenALSoft.dll',
            'redist/PenumbraVR.BlackPlague.Probe.dll', 'redist/PenumbraVR.Requiem.Probe.dll',
            'redist/openvr_api.dll', 'redist/vr/actions.json',
            'redist/vr/bindings/psvr2_sense.json',
            'redist/assets/rework/HAND_Low_C.jpg',
            'MANUAL-INSTALL.md', 'install-linux.sh', 'SHA256SUMS.txt',
            'install-linux.py', 'installer-game-profile.json', 'package-game.json', 'optional/alsoft.ini',
            'optional/spanish/config/Espanol.lang', 'optional/spanish/expansion01/config/Espanol_exp.lang',
            'optional/spanish/black_plague-leeme.txt', 'optional/spanish/requiem-leeme.txt',
            'optional/neural/PenumbraNeuralHost.exe', 'optional/neural/nvngx.dll_nr_bridge.dll',
            'optional/neural/README.md', 'optional/neural/DirectNR-LICENSE.txt', 'optional/neural/NVIDIA-SDK-LICENSE.txt',
            'COPYING', 'THIRD-PARTY-NOTICES.md', 'OpenALSoft-COPYING', 'OpenALSoft-readme.txt',
            'OpenALSoft-LICENSE-pffft.txt', 'OpenVR-LICENSE.txt'
        )) {
            if ($names -cnotcontains $required) { throw "Missing ZIP entry: $required" }
        }
        if (@($names | Where-Object {
            $_ -notmatch '^(redist/|optional/|MANUAL-INSTALL\.md$|install-linux\.(sh|py)$|installer-game-profile\.json$|package-game\.json$|SHA256SUMS\.txt$|COPYING$|THIRD-PARTY-NOTICES\.md$|OpenALSoft-COPYING$|OpenALSoft-readme\.txt$|OpenALSoft-LICENSE-pffft\.txt$|OpenVR-LICENSE\.txt$)' -or
            $_ -match '(?i)(^|/)(\.\.|Penumbra\.exe|Requiem\.exe|PenumbraVR_alut_original\.dll|\.pdb|\.dmp|\.git)(/|$)' -or
            $_ -match '(?i)\.(ps1|psm1|bat|cmd|vbs|cpp|hpp|h|pdb|dmp)$|(^|/)nvngx_dlssnr\.dll$' -or
            $_ -match '^redist/(alsoft\.ini$|config/|expansion01/|tools/|PenumbraVR_Penumbra_original\.exe)'
        }).Count) {
            throw 'Unexpected, retail or developer-only content in manual ZIP.'
        }
    } finally { $zip.Dispose() }

    $unpacked = Join-Path $tempRoot 'extracted'
    Expand-Archive -LiteralPath $ArchivePath -DestinationPath $unpacked
    $manifestPath = Join-Path $unpacked 'SHA256SUMS.txt'
    $manifestBytes = [IO.File]::ReadAllBytes($manifestPath)
    if ($manifestBytes.Length -ge 3 -and $manifestBytes[0] -eq 239 -and
        $manifestBytes[1] -eq 187 -and $manifestBytes[2] -eq 191) {
        throw 'Checksum manifest must not contain a UTF-8 BOM.'
    }
    $manifestText = [IO.File]::ReadAllText($manifestPath)
    if ($manifestText.Contains("`r") -or -not $manifestText.EndsWith("`n")) {
        throw 'Invalid checksum manifest line endings.'
    }
    $seen = @{}
    foreach ($line in Get-Content -LiteralPath $manifestPath) {
        if ($line -cnotmatch '^([a-f0-9]{64})  (.+)$') { throw 'Invalid checksum manifest entry.' }
        $hash = $Matches[1].ToUpperInvariant()
        $relative = $Matches[2]
        if ($seen.ContainsKey($relative)) { throw "Duplicate manifest entry: $relative" }
        $seen[$relative] = $true
        if ((Get-FileHash -LiteralPath (Join-Path $unpacked $relative)).Hash -cne $hash) {
            throw "Invalid checksum for $relative"
        }
    }
    if ($seen.Count -ne (@(Get-ChildItem -LiteralPath $unpacked -Recurse -File).Count - 1)) {
        throw 'Incomplete ZIP checksum manifest.'
    }

    $expectedSources = @{
        'OpenALSoft-LICENSE-pffft.txt' = 'assets/licenses/OpenALSoft-LICENSE-pffft.txt'
        'OpenVR-LICENSE.txt' = 'products/overture/dependencies/openvr-2.15.6/LICENSE'
        'redist/alut.dll' = Join-Path $buildRoot 'bin/Release/PenumbraVR.BlackPlague.Bootstrap.dll'
        'redist/OpenAL32.dll' = Join-Path $buildRoot 'bin/Release/PenumbraVR.BlackPlague.OpenALProxy.dll'
        'redist/PenumbraVR.BlackPlague.Probe.dll' = Join-Path $buildRoot 'bin/Release/PenumbraVR.BlackPlague.Probe.dll'
        'redist/PenumbraVR.Requiem.Probe.dll' = Join-Path $buildRoot 'bin/Release/PenumbraVR.Requiem.Probe.dll'
        'redist/openvr_api.dll' = Join-Path $buildRoot 'bin/Release/openvr_api.dll'
        'redist/PenumbraVR_OpenALSoft.dll' = 'products/overture/dependencies/bin/win32/OpenAL32.dll'
        'install-linux.py' = 'tools/install-linux.py'
        'installer-game-profile.json' = 'assets/settings/installer-game-profile.json'
        'optional/neural/PenumbraNeuralHost.exe' = 'build/neural-host/Release/PenumbraNeuralHost.exe'
        'optional/neural/nvngx.dll_nr_bridge.dll' = 'build/neural-host/Release/nvngx.dll_nr_bridge.dll'
        'optional/neural/README.md' = 'tools/neural_post/README.md'
        'optional/neural/DirectNR-LICENSE.txt' = 'tools/neural_post/vendor/nr-bridge/LICENSE'
        'optional/neural/NVIDIA-SDK-LICENSE.txt' = 'tools/neural_post/NVIDIA-SDK-LICENSE.txt'
    }

    $identity = Get-Content -LiteralPath (Join-Path $unpacked 'package-game.json') -Raw | ConvertFrom-Json
    if ($identity.game -cne 'black_plague' -or @($identity.PSObject.Properties).Count -ne 1) {
        throw 'Invalid shared package identity.'
    }
    $audioBytes = [IO.File]::ReadAllBytes((Join-Path $unpacked 'optional/alsoft.ini'))
    if ([Convert]::ToBase64String($audioBytes) -cne
        [Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes("[general]`nhrtf = auto`n"))) {
        throw 'Optional OpenAL configuration must be exact LF UTF-8 without BOM.'
    }
    $localization = Get-Content -LiteralPath (Join-Path $repoRoot 'assets/localization/manifest.json') -Raw | ConvertFrom-Json
    foreach ($gameId in @('black_plague', 'requiem')) {
        $entries = @($localization.translations | Where-Object { $_.game -ceq $gameId })
        if ($entries.Count -ne 1) { throw "Missing/ambiguous localization: $gameId" }
        $entry = $entries[0]
        $relative = 'optional/spanish/' + $entry.installPath.Substring('redist/'.Length)
        if ((Get-FileHash -LiteralPath (Join-Path $unpacked $relative)).Hash -cne $entry.sha256 -or
            (Get-FileHash -LiteralPath (Join-Path $repoRoot $entry.sourcePath)).Hash -cne $entry.sha256) {
            throw "Translation provenance/hash mismatch: $gameId"
        }
        if ((Get-FileHash -LiteralPath (Join-Path $unpacked "optional/spanish/$gameId-leeme.txt")).Hash -cne
            (Get-FileHash -LiteralPath (Join-Path $repoRoot $entry.noticePath)).Hash) {
            throw "Translation notice mismatch: $gameId"
        }
    }
    foreach ($entry in $expectedSources.GetEnumerator()) {
        $source = if ([IO.Path]::IsPathRooted($entry.Value)) { $entry.Value }
                  else { Join-Path $repoRoot $entry.Value }
        if ((Get-FileHash -LiteralPath (Join-Path $unpacked $entry.Key)).Hash -cne
            (Get-FileHash -LiteralPath $source).Hash) {
            throw "Unexpected overlay binary source: $($entry.Key)"
        }
    }
    $wrapper = [IO.File]::ReadAllText((Join-Path $repoRoot 'tools/install-black-plague-requiem-linux.sh')).Replace("`r`n", "`n")
    if ($wrapper.Contains("`r") -or [Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes($wrapper)) -cne
        [Convert]::ToBase64String([IO.File]::ReadAllBytes((Join-Path $unpacked 'install-linux.sh')))) {
        throw 'Linux wrapper provenance/LF/BOM check failed.'
    }

    # Test the same copy and restore sequence on an isolated synthetic game.
    $game = Join-Path $tempRoot 'fake-game'
    $redist = Join-Path $game 'redist'
    New-Item -ItemType Directory -Path $redist -Force | Out-Null
    Set-Content -LiteralPath (Join-Path $redist 'Penumbra.exe') -Value 'black plague fixture'
    Set-Content -LiteralPath (Join-Path $redist 'Requiem.exe') -Value 'requiem fixture'
    Set-Content -LiteralPath (Join-Path $redist 'alut.dll') -Value 'original ALUT fixture'
    Set-Content -LiteralPath (Join-Path $redist 'OpenAL32.dll') -Value 'original OpenAL fixture'
    Set-Content -LiteralPath (Join-Path $redist 'game-data.bin') -Value 'retail fixture data'
    Set-Content -LiteralPath (Join-Path $redist 'alsoft.ini') -Value 'user audio settings'
    $backup = Join-Path $game 'redist.before-penumbravr'
    Copy-Item -LiteralPath $redist -Destination $backup -Recurse
    Copy-Item -LiteralPath (Join-Path $backup 'alut.dll') -Destination (Join-Path $redist 'PenumbraVR_alut_original.dll')
    Copy-Item -Path (Join-Path $unpacked 'redist/*') -Destination $redist -Recurse -Force
    if ((Get-Content (Join-Path $redist 'PenumbraVR_alut_original.dll') -Raw).Trim() -cne 'original ALUT fixture' -or
        (Get-Content (Join-Path $redist 'Penumbra.exe') -Raw).Trim() -cne 'black plague fixture' -or
        (Get-Content (Join-Path $redist 'Requiem.exe') -Raw).Trim() -cne 'requiem fixture' -or
        -not (Test-Path (Join-Path $redist 'vr/actions.json')) -or
        (Test-Path (Join-Path $redist 'config/Espanol.lang')) -or
        (Test-Path (Join-Path $redist 'expansion01/config/Espanol_exp.lang')) -or
        (Get-Content (Join-Path $redist 'alsoft.ini') -Raw).Trim() -cne 'user audio settings' -or
        (Get-Content (Join-Path $redist 'game-data.bin') -Raw).Trim() -cne 'retail fixture data') {
        throw 'Shared overlay failed synthetic installation verification.'
    }
    Move-Item -LiteralPath $redist -Destination (Join-Path $game 'redist.penumbravr-test')
    Move-Item -LiteralPath $backup -Destination $redist
    if ((Get-Content (Join-Path $redist 'alut.dll') -Raw).Trim() -cne 'original ALUT fixture' -or
        (Test-Path (Join-Path $redist 'PenumbraVR.BlackPlague.Probe.dll'))) {
        throw 'Shared overlay failed synthetic restoration verification.'
    }

    $bash = Get-Command bash -ErrorAction SilentlyContinue
    $bashPath = if ($bash) { $bash.Source } else { 'C:\Program Files\Git\bin\bash.exe' }
    if (Test-Path -LiteralPath $bashPath -PathType Leaf) {
        & $bashPath -n (Join-Path $unpacked 'install-linux.sh')
        if ($LASTEXITCODE -ne 0) { throw 'Linux install helper failed Bash syntax validation.' }
    }
    Write-Host 'Experimental shared ZIP passed integrity, provenance, layout, synthetic install/restore and Bash syntax checks (if available).'
} finally {
    if (Test-Path -LiteralPath $tempRoot -PathType Container) {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force
    }
}
