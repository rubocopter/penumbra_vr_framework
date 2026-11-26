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
                                'MANUAL-INSTALL.md', 'SHA256SUMS.txt',
                                'OpenALSoft-LICENSE-pffft.txt', 'redist/licenses/OpenVR-LICENSE.txt',
                                'install-linux.py', 'install-linux.sh', 'installer-game-profile.json',
                                'package-game.json', 'texture-selection.json', 'optional/alsoft.ini',
                                'optional/spanish/config/Espanol.lang',
                                'optional/spanish/THIRD-PARTY-NOTICES.md', 'optional/textures/TEXTURE_CREDITS.md',
                                'optional/neural/PenumbraNeuralHost.exe', 'optional/neural/nvngx.dll_nr_bridge.dll',
                                'optional/neural/README.md', 'optional/neural/DirectNR-LICENSE.txt',
                                'optional/neural/NVIDIA-SDK-LICENSE.txt')) {
            if ($names -cnotcontains $required) { throw "Manual overlay lacks $required" }
        }
        if (@($names | Where-Object {
            $_ -match '(?i)(^|/)(tools|assets)/|(^|/)Install-PenumbraVR|(^|/)\.penumbravr|^redist/(SHA256SUMS.txt|config/Espanol.lang|alsoft.ini)$|(^|/)nvngx_dlssnr\.dll$|\.(ps1|psm1|bat|cmd|vbs|cpp|hpp|h|pdb|dmp)$' -or
            $_ -notmatch '^(redist/|optional/|OpenALSoft-LICENSE-pffft\.txt$|MANUAL-INSTALL\.md$|SHA256SUMS\.txt$|install-linux\.(py|sh)$|installer-game-profile\.json$|package-game\.json$|texture-selection\.json$)' -or
            $_ -match '(^|/)\.\.(/|$)'
        }).Count) {
            throw 'Manual overlay contains an unexpected/unsafe or installer-only path.'
        }
    } finally { $zip.Dispose() }

    $unpacked = Join-Path $tempRoot 'unpacked'
    Expand-Archive -LiteralPath $ArchivePath -DestinationPath $unpacked
    $manifestPath = Join-Path $unpacked 'SHA256SUMS.txt'
    $manifestBytes = [IO.File]::ReadAllBytes($manifestPath)
    if ($manifestBytes.Length -ge 3 -and $manifestBytes[0] -eq 239 -and
        $manifestBytes[1] -eq 187 -and $manifestBytes[2] -eq 191) {
        throw 'Manual overlay checksums must not contain a UTF-8 BOM.'
    }
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

    $identity = Get-Content -LiteralPath (Join-Path $unpacked 'package-game.json') -Raw | ConvertFrom-Json
    if ($identity.game -cne 'overture' -or @($identity.PSObject.Properties).Count -ne 1) {
        throw 'Invalid Overture package identity.'
    }
    $audioBytes = [IO.File]::ReadAllBytes((Join-Path $unpacked 'optional/alsoft.ini'))
    if ([Convert]::ToBase64String($audioBytes) -cne
        [Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes("[general]`nhrtf = auto`n"))) {
        throw 'Optional OpenAL configuration must be exact LF UTF-8 without BOM.'
    }
    $expectedSources = @{
        'OpenALSoft-LICENSE-pffft.txt' = 'assets/licenses/OpenALSoft-LICENSE-pffft.txt'
        'redist/licenses/OpenVR-LICENSE.txt' = 'products/overture/dependencies/openvr-2.15.6/LICENSE'
        'install-linux.py' = 'tools/install-linux.py'
        'installer-game-profile.json' = 'assets/settings/installer-game-profile.json'
        'texture-selection.json' = 'products/overture/docs/TEXTURE_SELECTION.json'
        'optional/textures/TEXTURE_CREDITS.md' = 'products/overture/docs/TEXTURE_CREDITS.md'
        'optional/spanish/THIRD-PARTY-NOTICES.md' = 'docs/THIRD_PARTY.md'
        'optional/neural/PenumbraNeuralHost.exe' = 'build/neural-host/Release/PenumbraNeuralHost.exe'
        'optional/neural/nvngx.dll_nr_bridge.dll' = 'build/neural-host/Release/nvngx.dll_nr_bridge.dll'
        'optional/neural/README.md' = 'tools/neural_post/README.md'
        'optional/neural/DirectNR-LICENSE.txt' = 'tools/neural_post/vendor/nr-bridge/LICENSE'
        'optional/neural/NVIDIA-SDK-LICENSE.txt' = 'tools/neural_post/NVIDIA-SDK-LICENSE.txt'
    }
    foreach ($entry in $expectedSources.GetEnumerator()) {
        if ((Get-FileHash -LiteralPath (Join-Path $unpacked $entry.Key)).Hash -cne
            (Get-FileHash -LiteralPath (Join-Path $repoRoot $entry.Value)).Hash) {
            throw "Unexpected overlay source: $($entry.Key)"
        }
    }
    $wrapper = [IO.File]::ReadAllText((Join-Path $repoRoot 'tools/install-black-plague-requiem-linux.sh')).Replace("`r`n", "`n")
    if ($wrapper.Contains("`r") -or [Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes($wrapper)) -cne
        [Convert]::ToBase64String([IO.File]::ReadAllBytes((Join-Path $unpacked 'install-linux.sh')))) {
        throw 'Linux wrapper provenance/LF/BOM check failed.'
    }
    $selection = Get-Content -LiteralPath (Join-Path $repoRoot 'products/overture/docs/TEXTURE_SELECTION.json') -Raw | ConvertFrom-Json
    foreach ($texture in $selection.Files) {
        $path = Join-Path $unpacked ('optional/textures/' + $texture.Path)
        if ((Get-FileHash -LiteralPath $path).Hash -cne $texture.SHA256 -or
            (Get-Item -LiteralPath $path).Length -ne $texture.Bytes -or
            (Test-Path -LiteralPath (Join-Path $unpacked ('redist/' + $texture.Path)))) {
            throw "Texture is missing, invalid or still in core: $($texture.Path)"
        }
    }
    if (@(Get-ChildItem -LiteralPath (Join-Path $unpacked 'optional/textures') -Recurse -File).Count -ne
        (@($selection.Files).Count + 1)) { throw 'Unexpected optional texture payload.' }
    if (-not $PackageRoot) { $PackageRoot = Join-Path $repoRoot 'products/overture/build/package/Release/PenumbraVR' }
    if ((Get-FileHash -LiteralPath (Join-Path $unpacked 'optional/spanish/config/Espanol.lang')).Hash -cne
        (Get-FileHash -LiteralPath (Join-Path $PackageRoot 'config/Espanol.lang')).Hash) {
        throw 'Overture Spanish differs from the verified product package.'
    }
    # Every non-optional product resource must remain available to the VR core.
    $texturePaths = @{}
    foreach ($texture in $selection.Files) { $texturePaths[[string]$texture.Path] = $true }
    $sourceRoot = [IO.Path]::GetFullPath($PackageRoot).TrimEnd('\', '/')
    foreach ($file in Get-ChildItem -LiteralPath $sourceRoot -Recurse -File) {
        $relative = $file.FullName.Substring($sourceRoot.Length).TrimStart('\', '/').Replace('\', '/')
        if ($relative -in @('SHA256SUMS.txt', 'Install-PenumbraVR.ps1', 'Install-PenumbraVR.bat',
                           'config/Espanol.lang', 'alsoft.ini') -or
            $relative -match '^(tools|assets)/' -or $texturePaths.ContainsKey($relative)) { continue }
        if ((Get-FileHash -LiteralPath (Join-Path $unpacked ('redist/' + $relative))).Hash -cne
            (Get-FileHash -LiteralPath $file.FullName).Hash) { throw "Core resource changed or missing: $relative" }
    }

    # Exercise the documented file overlay and restore against an isolated
    # synthetic game tree. This never touches an installed game.
    $game = Join-Path $tempRoot 'game'
    $redist = Join-Path $game 'redist'
    New-Item -ItemType Directory -Path (Join-Path $redist 'config') -Force | Out-Null
    Set-Content -LiteralPath (Join-Path $redist 'Penumbra.exe') -Value 'retail fixture'
    Set-Content -LiteralPath (Join-Path $redist 'config/English.lang') -Value 'original English fixture'
    Set-Content -LiteralPath (Join-Path $redist 'game-owned.txt') -Value 'untouched retail data'
    Set-Content -LiteralPath (Join-Path $redist 'alsoft.ini') -Value 'user audio settings'
    $backup = Join-Path $game 'redist.before-penumbravr'
    Copy-Item -LiteralPath $redist -Destination $backup -Recurse
    Copy-Item -Path (Join-Path $unpacked 'redist/*') -Destination $redist -Recurse -Force
    if ((Get-FileHash (Join-Path $redist 'Penumbra.exe')).Hash -cne
        (Get-FileHash (Join-Path $unpacked 'redist/Penumbra_vr.exe')).Hash -or
        -not (Test-Path (Join-Path $redist 'vr/actions.json')) -or
        (Test-Path (Join-Path $redist 'config/Espanol.lang')) -or
        (Get-Content (Join-Path $redist 'alsoft.ini') -Raw).Trim() -cne 'user audio settings' -or
        (Get-Content (Join-Path $redist 'game-owned.txt') -Raw).Trim() -cne 'untouched retail data') {
        throw 'Documented overlay did not preserve retail files and apply the VR payload.'
    }
    Move-Item -LiteralPath $redist -Destination (Join-Path $game 'redist.penumbravr-test')
    Move-Item -LiteralPath $backup -Destination $redist
    if ((Get-Content (Join-Path $redist 'Penumbra.exe') -Raw).Trim() -cne 'retail fixture') {
        throw 'Manual overlay restore did not recover the retail executable.'
    }
    $bash = Get-Command bash -ErrorAction SilentlyContinue
    $bashPath = if ($bash) { $bash.Source } else { 'C:\Program Files\Git\bin\bash.exe' }
    if (Test-Path -LiteralPath $bashPath -PathType Leaf) {
        & $bashPath -n (Join-Path $unpacked 'install-linux.sh')
        if ($LASTEXITCODE -ne 0) { throw 'Linux install helper failed Bash syntax validation.' }
    }
    Write-Host 'Experimental manual Overture ZIP passed layout, integrity, overlay and restore fixture checks.'
} finally {
    if (Test-Path -LiteralPath $tempRoot -PathType Container) {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force
    }
}
