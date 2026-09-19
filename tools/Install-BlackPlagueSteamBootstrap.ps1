[CmdletBinding()]
param(
    [string]$GamePath = (Join-Path ${env:ProgramFiles(x86)} 'Steam\steamapps\common\Penumbra Black Plague\redist\Penumbra.exe'),
    [string]$BuildRoot = (Join-Path (Split-Path -Parent $PSScriptRoot) 'build'),
    [switch]$Restore
)

$ErrorActionPreference = 'Stop'

$ExpectedGameHashes = @(
    'FD316F7586737A63EBA989ECE2271280FE6A98582A1319FE2151385A3DF97BFF',
    'DB086CC7A4C7B10864DE0FEBBE2D71A3E4EFF1EC8D067811A6A59EDC1C617196'
)
$ExpectedOriginalAlutHash = 'D81DEA8E88E35C319F7F2D8AAEB14C63A4986131492D3DF860D1F2C18B844590'

$RepoRoot = Split-Path -Parent $PSScriptRoot
$GamePath = [System.IO.Path]::GetFullPath($GamePath)
$GameRoot = Split-Path -Parent $GamePath
$AlutPath = Join-Path $GameRoot 'alut.dll'
$OriginalAlutPath = Join-Path $GameRoot 'PenumbraVR_alut_original.dll'
$ProbePath = Join-Path $GameRoot 'PenumbraVR.BlackPlague.Probe.dll'
$OpenVrPath = Join-Path $GameRoot 'openvr_api.dll'
$VrAssetsPath = Join-Path $GameRoot 'vr'
$InstallStatePath = Join-Path $GameRoot 'PenumbraVR.BlackPlague.install.json'

function Get-Sha256([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return $null }
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToUpperInvariant()
}

function Assert-SupportedGame {
    if (-not (Test-Path -LiteralPath $GamePath -PathType Leaf)) {
        throw "Black Plague executable not found: $GamePath"
    }
    $hash = Get-Sha256 $GamePath
    if ($ExpectedGameHashes -notcontains $hash) {
        throw "Unsupported Black Plague executable SHA-256: $hash"
    }
}

Assert-SupportedGame

if ($Restore) {
    if (-not (Test-Path -LiteralPath $OriginalAlutPath -PathType Leaf)) {
        throw "No managed ALUT backup exists at $OriginalAlutPath"
    }
    if ((Get-Sha256 $OriginalAlutPath) -ne $ExpectedOriginalAlutHash) {
        throw 'The managed ALUT backup does not match the known retail DLL; restore aborted.'
    }

    if (Test-Path -LiteralPath $InstallStatePath -PathType Leaf) {
        $state = Get-Content -LiteralPath $InstallStatePath -Raw | ConvertFrom-Json
        $currentHash = Get-Sha256 $AlutPath
        if ($currentHash -and $state.installedProxySha256 -and
            $currentHash -ne [string]$state.installedProxySha256) {
            throw 'alut.dll changed after Penumbra VR installation; restore aborted to avoid overwriting another modification.'
        }
    }

    Copy-Item -LiteralPath $OriginalAlutPath -Destination $AlutPath -Force
    Remove-Item -LiteralPath $OriginalAlutPath -Force
    Remove-Item -LiteralPath $ProbePath -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $OpenVrPath -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $InstallStatePath -Force -ErrorAction SilentlyContinue
    if (Test-Path -LiteralPath $VrAssetsPath -PathType Container) {
        Remove-Item -LiteralPath $VrAssetsPath -Recurse -Force
    }
    Write-Host 'Black Plague normal Steam launch bootstrap restored cleanly.'
    exit 0
}

$ReleaseRoot = Join-Path $BuildRoot 'bin\Release'
$ProxySource = Join-Path $ReleaseRoot 'PenumbraVR.BlackPlague.Bootstrap.dll'
$ProbeSource = Join-Path $ReleaseRoot 'PenumbraVR.BlackPlague.Probe.dll'
$OpenVrSource = Join-Path $ReleaseRoot 'openvr_api.dll'
$VrAssetsSource = Join-Path $RepoRoot 'assets\openvr'

foreach ($required in @($ProxySource, $ProbeSource, $OpenVrSource)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Required Release artifact not found: $required"
    }
}
if (-not (Test-Path -LiteralPath $VrAssetsSource -PathType Container)) {
    throw "OpenVR assets not found: $VrAssetsSource"
}

if (-not (Test-Path -LiteralPath $OriginalAlutPath -PathType Leaf)) {
    $currentAlutHash = Get-Sha256 $AlutPath
    if ($currentAlutHash -ne $ExpectedOriginalAlutHash) {
        throw "Refusing to replace unknown alut.dll SHA-256: $currentAlutHash"
    }
    Copy-Item -LiteralPath $AlutPath -Destination $OriginalAlutPath
} elseif ((Get-Sha256 $OriginalAlutPath) -ne $ExpectedOriginalAlutHash) {
    throw 'Existing managed ALUT backup does not match the known retail DLL.'
}

Copy-Item -LiteralPath $ProxySource -Destination $AlutPath -Force
Copy-Item -LiteralPath $ProbeSource -Destination $ProbePath -Force
Copy-Item -LiteralPath $OpenVrSource -Destination $OpenVrPath -Force
if (Test-Path -LiteralPath $VrAssetsPath -PathType Container) {
    Remove-Item -LiteralPath $VrAssetsPath -Recurse -Force
}
Copy-Item -LiteralPath $VrAssetsSource -Destination $VrAssetsPath -Recurse

$state = [ordered]@{
    schema = 1
    gameExeSha256 = Get-Sha256 $GamePath
    originalAlutSha256 = Get-Sha256 $OriginalAlutPath
    installedProxySha256 = Get-Sha256 $AlutPath
    probeSha256 = Get-Sha256 $ProbePath
}
$state | ConvertTo-Json | Set-Content -LiteralPath $InstallStatePath -Encoding UTF8

Write-Host 'Black Plague normal Steam launch bootstrap installed.'
Write-Host 'Start SteamVR, then use the normal Play button for Penumbra: Black Plague in Steam.'
Write-Host 'Validation scripts and Start-Black-Plague-VR.cmd remain available for diagnostics.'
