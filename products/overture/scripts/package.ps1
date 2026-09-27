[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',
    [string]$RuntimeDirectory
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$frameworkRoot = [System.IO.Path]::GetFullPath((Join-Path $repositoryRoot '..\..'))
$buildRoot = [System.IO.Path]::GetFullPath((Join-Path $repositoryRoot 'build'))
$packageRoot = [System.IO.Path]::GetFullPath((Join-Path $buildRoot "package\$Configuration\PenumbraVR"))
$expectedPrefix = $buildRoot.TrimEnd([System.IO.Path]::DirectorySeparatorChar) + [System.IO.Path]::DirectorySeparatorChar

if (-not $packageRoot.StartsWith($expectedPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to package outside the build directory: $packageRoot"
}

$executablePath = Join-Path $buildRoot "bin\$Configuration\Penumbra_vr.exe"
if (-not (Test-Path -LiteralPath $executablePath)) {
    throw "Build output not found: $executablePath"
}

# The app-local CRT is a release input, not an arbitrary property of the
# packaging machine. Select only the recorded x86 binaries before touching an
# existing package so a VS update cannot silently change release contents.
$runtimeManifestPath = Join-Path $repositoryRoot 'runtime-dependencies.json'
$runtimeManifest = Get-Content -LiteralPath $runtimeManifestPath -Raw | ConvertFrom-Json
if ($runtimeManifest.schemaVersion -ne 1 -or $runtimeManifest.architecture -ne 'x86' -or
    $runtimeManifest.redistributable -ne 'Microsoft.VC143.CRT') {
    throw "Unsupported Overture Visual C++ runtime manifest: $runtimeManifestPath"
}
$runtimeNames = @('msvcp140.dll', 'vcruntime140.dll')
foreach ($name in $runtimeNames) {
    if ([string]$runtimeManifest.files.PSObject.Properties[$name].Value -notmatch '^[0-9A-Fa-f]{64}$') {
        throw "Missing pinned SHA-256 for Visual C++ runtime $name"
    }
}
if ($RuntimeDirectory) {
    $runtimeCandidates = @([System.IO.Path]::GetFullPath($RuntimeDirectory))
} else {
    $vswherePath = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswherePath -PathType Leaf)) {
        throw 'Visual Studio runtime discovery is unavailable; pass -RuntimeDirectory with the pinned x86 CRT.'
    }
    $runtimeCandidates = @(& $vswherePath -products * -find 'VC\Redist\MSVC\*\x86\Microsoft.VC143.CRT\msvcp140.dll' |
        ForEach-Object { Split-Path -Parent $_ } | Select-Object -Unique)
}
$crtDirectory = $null
foreach ($candidate in $runtimeCandidates) {
    $matchesPin = $true
    foreach ($name in $runtimeNames) {
        $path = Join-Path $candidate $name
        $item = Get-Item -LiteralPath $path -Force -ErrorAction SilentlyContinue
        if (-not $item -or $item.PSIsContainer -or
            ($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -or
            (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ine
                [string]$runtimeManifest.files.PSObject.Properties[$name].Value) {
            $matchesPin = $false
            break
        }
    }
    if ($matchesPin) { $crtDirectory = $candidate; break }
}
if (-not $crtDirectory) {
    throw "No Visual C++ x86 runtime directory matches the pinned SHA-256 hashes in $runtimeManifestPath. Pass -RuntimeDirectory with the recorded CRT."
}

foreach ($candidate in @($buildRoot, (Join-Path $buildRoot 'package'),
        (Join-Path $buildRoot "package\$Configuration"), $packageRoot)) {
    $item = Get-Item -LiteralPath $candidate -Force -ErrorAction SilentlyContinue
    if ($item -and ($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint)) {
        throw "Refusing to package through a reparse point: $candidate"
    }
}

if (Test-Path -LiteralPath $packageRoot) {
    Remove-Item -LiteralPath $packageRoot -Recurse -Force
}
New-Item -ItemType Directory -Path $packageRoot -Force | Out-Null

Copy-Item -LiteralPath $executablePath -Destination $packageRoot
Copy-Item -LiteralPath (Join-Path $repositoryRoot 'dependencies\openvr-2.15.6\bin\win32\openvr_api.dll') -Destination $packageRoot
# App-local OpenAL Soft: guarantees EFX (filters/reverb) and HRTF support on
# every machine regardless of which legacy OpenAL runtime is installed. The
# DLL search order picks this copy up before any system-wide router.
Copy-Item -LiteralPath (Join-Path $repositoryRoot 'dependencies\bin\win32\OpenAL32.dll') -Destination $packageRoot
# App-local Visual C++ runtime: the executable links the DLL CRT, so ship the
# redistributable DLLs next to it instead of asking players to install the
# VC++ redist (officially supported app-local deployment). SteamVR already
# requires Windows 10/11, where only these two files are ever missing.
foreach ($crtDll in $runtimeNames) {
    Copy-Item -LiteralPath (Join-Path $crtDirectory $crtDll) -Destination $packageRoot
}
New-Item -ItemType Directory -Path (Join-Path $packageRoot 'licenses') -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $repositoryRoot 'dependencies\bin\win32\OpenALSoft-COPYING') -Destination (Join-Path $packageRoot 'licenses\OpenALSoft-COPYING.txt')
Copy-Item -LiteralPath (Join-Path $repositoryRoot 'dependencies\bin\win32\OpenALSoft-readme.txt') -Destination (Join-Path $packageRoot 'licenses\OpenALSoft-readme.txt')
$dataRoot = Join-Path $repositoryRoot 'data'
Get-ChildItem -LiteralPath $dataRoot -Force | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination $packageRoot -Recurse
}
$vrRoot = Join-Path $packageRoot 'vr'
Copy-Item -LiteralPath (Join-Path $frameworkRoot 'assets\openvr\overture') -Destination $vrRoot -Recurse
# The stock game supplies the existing core shaders, but VR-specific programs
# live with the engine source and must be overlaid into the redist explicitly.
$programRoot = Join-Path $packageRoot 'core\programs'
New-Item -ItemType Directory -Path $programRoot -Force | Out-Null
$vrPrograms = @(
    'Ambient_Hemisphere_vp.cg',
    'Ambient_Hemisphere_fp.cg',
    'VR_Enhanced_Final_vp.cg',
    'VR_Enhanced_Final_fp.cg',
	'VR_Diffuse_Light_fp.cg',
	'VR_Bump_Light_fp.cg',
	'VR_DiffuseSpec_Light_fp.cg',
	'VR_BumpSpec_Light_fp.cg',
	'VR_BumpColorSpec_Light_fp.cg',
    'VR_Diffuse_Light_Spot_fp.cg',
    'VR_Bump_Light_Spot_fp.cg',
    'VR_DiffuseSpec_Light_Spot_fp.cg',
    'VR_BumpSpec_Light_Spot_fp.cg',
    'VR_BumpColorSpec_Light_Spot_fp.cg',
    'VR_Glowstick_Halo_fp.cg',
    'VR_Glowstick_Halo_Fog_fp.cg'
)
foreach ($vrProgram in $vrPrograms) {
    Copy-Item -LiteralPath (Join-Path $repositoryRoot "HPL1Engine\assets\core\programs\$vrProgram") -Destination $programRoot
}
# Overlay the flashlight-specific light entity so its shorter projection near
# plane ships with the corrected HUD-model bulb and lens positions.
$textureRoot = Join-Path $packageRoot 'textures'
New-Item -ItemType Directory -Path $textureRoot -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $repositoryRoot 'HPL1Engine\assets\textures\light_player_flashlight_spot.lnt') -Destination $textureRoot
Copy-Item -LiteralPath (Join-Path $repositoryRoot 'readme.md') -Destination (Join-Path $packageRoot 'README.md')
Copy-Item -LiteralPath (Join-Path $repositoryRoot 'PenumbraOverture\COPYING') -Destination (Join-Path $packageRoot 'COPYING.txt')
Copy-Item -LiteralPath (Join-Path $repositoryRoot 'HPL1Engine\COPYING') -Destination (Join-Path $packageRoot 'licenses\HPL1-COPYING.txt')
Copy-Item -LiteralPath (Join-Path $repositoryRoot 'HPL1Engine\LICENSE-assets') -Destination (Join-Path $packageRoot 'licenses\HPL1-assets.txt')
Copy-Item -LiteralPath (Join-Path $repositoryRoot 'HPL1Engine\LICENSE-shaders') -Destination (Join-Path $packageRoot 'licenses\HPL1-shaders.txt')
Copy-Item -LiteralPath (Join-Path $repositoryRoot 'OALWrapper\LICENSE') -Destination (Join-Path $packageRoot 'licenses\OALWrapper-LICENSE.txt')
Copy-Item -LiteralPath (Join-Path $repositoryRoot 'dependencies\openvr-2.15.6\LICENSE') -Destination (Join-Path $packageRoot 'licenses\OpenVR-LICENSE.txt')
Copy-Item -LiteralPath (Join-Path $repositoryRoot 'SOURCE_PROVENANCE.md') -Destination (Join-Path $packageRoot 'licenses\SOURCE_PROVENANCE.md')
Copy-Item -LiteralPath (Join-Path $frameworkRoot 'docs\THIRD_PARTY.md') -Destination (Join-Path $packageRoot 'licenses\THIRD_PARTY.md')
Copy-Item -LiteralPath (Join-Path $repositoryRoot 'docs') -Destination $packageRoot -Recurse
Copy-Item -LiteralPath (Join-Path $repositoryRoot 'scripts\deploy.ps1') -Destination (Join-Path $packageRoot 'Install-PenumbraVR.ps1')
Copy-Item -LiteralPath (Join-Path $repositoryRoot 'scripts\Install-PenumbraVR.bat') -Destination (Join-Path $packageRoot 'Install-PenumbraVR.bat')

$requiredPackagePaths = @(
    'Penumbra_vr.exe', 'openvr_api.dll', 'OpenAL32.dll', 'msvcp140.dll', 'vcruntime140.dll',
    'licenses\OpenALSoft-COPYING.txt', 'licenses\HPL1-COPYING.txt',
    'licenses\HPL1-assets.txt', 'licenses\HPL1-shaders.txt',
    'licenses\OALWrapper-LICENSE.txt', 'licenses\OpenVR-LICENSE.txt',
    'licenses\SOURCE_PROVENANCE.md', 'licenses\THIRD_PARTY.md',
    'Install-PenumbraVR.ps1', 'Install-PenumbraVR.bat',
    'config\English.lang', 'config\Espanol.lang', 'textures\light_player_flashlight_spot.lnt',
    'docs\INPUT.md', 'docs\LIGHTING.md', 'docs\TROUBLESHOOTING.md', 'docs\ROADMAP.md',
    'docs\TEXTURE_CREDITS.md', 'docs\TEXTURES.md', 'docs\TEXTURE_SELECTION.json',
    'docs\TEXTURE_POLICY.json', 'docs\TEXTURE_AUDIT.json',
    'maps', 'models', 'vr\actions.json', 'vr\bindings\psvr2_sense.json',
    'vr\bindings\vive_controller.json', 'vr\bindings\knuckles.json',
    'vr\bindings\oculus_touch.json', 'vr\bindings\microsoft_motion_controller.json',
    'vr\bindings\pico4_controller.json', 'vr\bindings\pico_neo3_controller.json',
    'vr\bindings\holographic_controller.json'
)
$requiredPackagePaths += $vrPrograms | ForEach-Object { "core\programs\$_" }
foreach ($requiredPath in $requiredPackagePaths) {
    $packagedPath = Join-Path $packageRoot $requiredPath
    if (-not (Test-Path -LiteralPath $packagedPath)) {
        throw "Required package entry was not created: $packagedPath"
    }
}

if (Test-Path -LiteralPath (Join-Path $packageRoot 'data')) {
    throw "Invalid package layout: data must be merged into the game redist root."
}

& (Join-Path $PSScriptRoot 'check-texture-selection.ps1') -ContentRoot $packageRoot
$manifestPath = Join-Path $packageRoot 'SHA256SUMS.txt'
function Get-RelativePath([string]$basePath, [string]$fullPath) {
    # Path.GetRelativePath only exists on .NET Framework 4.7.1+; Uri works
    # since .NET Framework 4.0 and keeps package.ps1 usable on any runtime.
    $base = [System.IO.Path]::GetFullPath($basePath).TrimEnd([System.IO.Path]::DirectorySeparatorChar)
    $baseUri = [System.Uri]($base + [System.IO.Path]::DirectorySeparatorChar)
    $fullUri = [System.Uri]([System.IO.Path]::GetFullPath($fullPath))
    return [System.Uri]::UnescapeDataString($baseUri.MakeRelativeUri($fullUri).ToString()).Replace('/', [System.IO.Path]::DirectorySeparatorChar)
}
$manifestLines = Get-ChildItem -LiteralPath $packageRoot -File -Recurse |
    Where-Object { $_.FullName -ne $manifestPath } |
    Sort-Object FullName |
    ForEach-Object {
        $relativePath = (Get-RelativePath $packageRoot $_.FullName).Replace('\', '/')
        $hash = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        "$hash  $relativePath"
    }
$manifestLines | Set-Content -LiteralPath $manifestPath -Encoding utf8

Write-Host "Package created at $packageRoot" -ForegroundColor Green
