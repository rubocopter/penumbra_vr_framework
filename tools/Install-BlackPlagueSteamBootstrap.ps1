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
$DeploymentManifestPath = Join-Path $RepoRoot 'assets\deployment\manifest.json'
if (-not (Test-Path -LiteralPath $DeploymentManifestPath -PathType Leaf)) {
    throw "Deployment payload manifest not found: $DeploymentManifestPath"
}
$DeploymentManifest = Get-Content -LiteralPath $DeploymentManifestPath -Raw | ConvertFrom-Json
$BlackPlagueDeployment = @($DeploymentManifest.products | Where-Object { $_.game -eq 'black_plague' })
if ($DeploymentManifest.schemaVersion -ne 1 -or $BlackPlagueDeployment.Count -ne 1) {
    throw 'Deployment payload manifest does not contain one supported Black Plague product definition.'
}

function Get-DeploymentPayload([string]$Id) {
    $payload = @($BlackPlagueDeployment[0].payloads | Where-Object { $_.id -eq $Id })
    if ($payload.Count -ne 1) {
        throw "Deployment payload '$Id' is missing or duplicated."
    }
    return $payload[0]
}

function Get-DeploymentDestination([string]$Id, [string]$Root) {
    $payload = Get-DeploymentPayload $Id
    return Join-Path $Root ([string]$payload.destination -replace '/', '\')
}

$GamePath = [System.IO.Path]::GetFullPath($GamePath)
$GameRoot = Split-Path -Parent $GamePath
$AlutPath = Get-DeploymentDestination 'bootstrap_proxy' $GameRoot
$OriginalAlutPath = Join-Path $GameRoot 'PenumbraVR_alut_original.dll'
$ProbePath = Get-DeploymentDestination 'probe' $GameRoot
$OpenVrPath = Get-DeploymentDestination 'openvr_loader' $GameRoot
$VrAssetsPath = Get-DeploymentDestination 'openvr_actions' $GameRoot
$HandTexturePath = Get-DeploymentDestination 'hand_texture' $GameRoot
$HandAssetsPath = Split-Path -Parent $HandTexturePath
$LocalizationPath = Get-DeploymentDestination 'spanish_localization' $GameRoot
$LocalizationBackupPath = Join-Path $GameRoot 'PenumbraVR_Espanol_original.lang'
$AudioConfigPath = Get-DeploymentDestination 'openal_hrtf_config' $GameRoot
$InstallStatePath = Join-Path $GameRoot 'PenumbraVR.BlackPlague.install.json'

function Get-Sha256([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return $null }
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToUpperInvariant()
}

function Get-OptionalProperty($Object, [string]$Name) {
    if ($null -eq $Object) { return $null }
    $property = $Object.PSObject.Properties[$Name]
    if ($null -eq $property) { return $null }
    return $property.Value
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

    $state = $null
    if (Test-Path -LiteralPath $InstallStatePath -PathType Leaf) {
        $state = Get-Content -LiteralPath $InstallStatePath -Raw | ConvertFrom-Json
        $currentHash = Get-Sha256 $AlutPath
        if ($currentHash -and $state.installedProxySha256 -and
            $currentHash -ne [string]$state.installedProxySha256) {
            throw 'alut.dll changed after Penumbra VR installation; restore aborted to avoid overwriting another modification.'
        }
        if ((Test-Path -LiteralPath $HandTexturePath -PathType Leaf) -and
            (Get-OptionalProperty $state 'handTextureSha256') -and
            (Get-Sha256 $HandTexturePath) -ne [string](Get-OptionalProperty $state 'handTextureSha256')) {
            throw 'The installed Penumbra VR hand texture changed after installation; restore aborted.'
        }
        if ((Test-Path -LiteralPath $LocalizationPath -PathType Leaf) -and
            (Get-OptionalProperty $state 'spanishLocalizationSha256') -and
            (Get-Sha256 $LocalizationPath) -ne [string](Get-OptionalProperty $state 'spanishLocalizationSha256')) {
            throw 'The installed Penumbra VR Spanish localization changed after installation; restore aborted.'
        }
    }

    Copy-Item -LiteralPath $OriginalAlutPath -Destination $AlutPath -Force
    Remove-Item -LiteralPath $OriginalAlutPath -Force
    Remove-Item -LiteralPath $ProbePath -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $OpenVrPath -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $HandTexturePath -Force -ErrorAction SilentlyContinue
    $stateLocalizationSha256 = Get-OptionalProperty $state 'spanishLocalizationSha256'
    if ($stateLocalizationSha256) {
        if ([bool](Get-OptionalProperty $state 'spanishLocalizationHadOriginal')) {
            if (-not (Test-Path -LiteralPath $LocalizationBackupPath -PathType Leaf)) {
                throw 'The managed Spanish localization backup is missing; restore aborted.'
            }
            $stateLocalizationOriginalSha256 = Get-OptionalProperty $state 'spanishLocalizationOriginalSha256'
            if ($stateLocalizationOriginalSha256 -and
                (Get-Sha256 $LocalizationBackupPath) -ne [string]$stateLocalizationOriginalSha256) {
                throw 'The managed Spanish localization backup failed hash validation; restore aborted.'
            }
            Copy-Item -LiteralPath $LocalizationBackupPath -Destination $LocalizationPath -Force
            Remove-Item -LiteralPath $LocalizationBackupPath -Force
        } else {
            Remove-Item -LiteralPath $LocalizationPath -Force -ErrorAction SilentlyContinue
        }
    }
    $stateAudioConfigCreated = [bool](Get-OptionalProperty $state 'audioConfigCreated')
    $stateAudioConfigSha256 = Get-OptionalProperty $state 'audioConfigSha256'
    if ($stateAudioConfigCreated -and
        (Test-Path -LiteralPath $AudioConfigPath -PathType Leaf) -and
        $stateAudioConfigSha256 -and
        (Get-Sha256 $AudioConfigPath) -eq [string]$stateAudioConfigSha256) {
        Remove-Item -LiteralPath $AudioConfigPath -Force
    }
    Remove-Item -LiteralPath $InstallStatePath -Force -ErrorAction SilentlyContinue
    if (Test-Path -LiteralPath $VrAssetsPath -PathType Container) {
        Remove-Item -LiteralPath $VrAssetsPath -Recurse -Force
    }
    Write-Host 'Black Plague normal Steam launch bootstrap restored cleanly.'
    exit 0
}

$PreviousState = $null
if (Test-Path -LiteralPath $InstallStatePath -PathType Leaf) {
    $PreviousState = Get-Content -LiteralPath $InstallStatePath -Raw | ConvertFrom-Json
}

$ReleaseRoot = Join-Path $BuildRoot 'bin\Release'
function Get-DeploymentSource([string]$Id) {
    $payload = Get-DeploymentPayload $Id
    $sourceKind = [string]$payload.source.kind
    if ($sourceKind -eq 'build-output') {
        return Join-Path $ReleaseRoot ([string]$payload.source.path -replace '/', '\')
    }
    if ($sourceKind -eq 'repository-file' -or $sourceKind -eq 'repository-directory') {
        return Join-Path $RepoRoot ([string]$payload.source.path -replace '/', '\')
    }
    throw "Deployment payload '$Id' does not have a filesystem source."
}

$ProxySource = Get-DeploymentSource 'bootstrap_proxy'
$ProbeSource = Get-DeploymentSource 'probe'
$OpenVrSource = Get-DeploymentSource 'openvr_loader'
$VrAssetsSource = Get-DeploymentSource 'openvr_actions'
$HandTextureSource = Get-DeploymentSource 'hand_texture'
$LocalizationSource = Get-DeploymentSource 'spanish_localization'

foreach ($required in @($ProxySource, $ProbeSource, $OpenVrSource)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Required Release artifact not found: $required"
    }
}
if (-not (Test-Path -LiteralPath $VrAssetsSource -PathType Container)) {
    throw "OpenVR assets not found: $VrAssetsSource"
}
if (-not (Test-Path -LiteralPath $HandTextureSource -PathType Leaf)) {
    throw "Rework hand texture not found: $HandTextureSource"
}
if (-not (Test-Path -LiteralPath $LocalizationSource -PathType Leaf)) {
    throw "Spanish localization not found: $LocalizationSource"
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
New-Item -ItemType Directory -Path $HandAssetsPath -Force | Out-Null
Copy-Item -LiteralPath $HandTextureSource -Destination $HandTexturePath -Force

$LocalizationHadOriginal = $false
$LocalizationOriginalSha256 = $null
$previousLocalizationSha256 = Get-OptionalProperty $PreviousState 'spanishLocalizationSha256'
if ($previousLocalizationSha256) {
    if ((Test-Path -LiteralPath $LocalizationPath -PathType Leaf) -and
        (Get-Sha256 $LocalizationPath) -ne [string]$previousLocalizationSha256) {
        throw 'The managed Spanish localization changed after installation; redeploy aborted.'
    }
    $LocalizationHadOriginal = [bool](Get-OptionalProperty $PreviousState 'spanishLocalizationHadOriginal')
    $LocalizationOriginalSha256 = [string](Get-OptionalProperty $PreviousState 'spanishLocalizationOriginalSha256')
    if ($LocalizationHadOriginal) {
        if (-not (Test-Path -LiteralPath $LocalizationBackupPath -PathType Leaf)) {
            throw 'The managed Spanish localization backup is missing; redeploy aborted.'
        }
        if ($LocalizationOriginalSha256 -and
            (Get-Sha256 $LocalizationBackupPath) -ne $LocalizationOriginalSha256) {
            throw 'The managed Spanish localization backup failed hash validation; redeploy aborted.'
        }
    }
} else {
    $LocalizationHadOriginal = Test-Path -LiteralPath $LocalizationPath -PathType Leaf
    if ($LocalizationHadOriginal) {
        if (-not (Test-Path -LiteralPath $LocalizationBackupPath -PathType Leaf)) {
            Copy-Item -LiteralPath $LocalizationPath -Destination $LocalizationBackupPath
        }
        $LocalizationOriginalSha256 = Get-Sha256 $LocalizationBackupPath
    }
}
$LocalizationDirectory = Split-Path -Parent $LocalizationPath
New-Item -ItemType Directory -Path $LocalizationDirectory -Force | Out-Null
Copy-Item -LiteralPath $LocalizationSource -Destination $LocalizationPath -Force

$AudioConfigCreated = [bool](Get-OptionalProperty $PreviousState 'audioConfigCreated')
if (-not (Test-Path -LiteralPath $AudioConfigPath -PathType Leaf)) {
    [System.IO.File]::WriteAllText(
        $AudioConfigPath,
        "[general]`nhrtf = auto`n",
        [System.Text.Encoding]::ASCII)
    $AudioConfigCreated = $true
}

foreach ($copy in @(
    @($ProxySource, $AlutPath),
    @($ProbeSource, $ProbePath),
    @($OpenVrSource, $OpenVrPath),
    @($HandTextureSource, $HandTexturePath),
    @($LocalizationSource, $LocalizationPath)
)) {
    if ((Get-Sha256 $copy[0]) -ne (Get-Sha256 $copy[1])) {
        throw "Deployment verification failed for '$($copy[1])'."
    }
}

$sourceVrFiles = @(Get-ChildItem -LiteralPath $VrAssetsSource -Recurse -File)
$destinationVrFiles = @(Get-ChildItem -LiteralPath $VrAssetsPath -Recurse -File)
if ($sourceVrFiles.Count -ne $destinationVrFiles.Count) {
    throw 'Deployment verification failed for the OpenVR action/binding directory.'
}
foreach ($sourceVrFile in $sourceVrFiles) {
    $relativeVrPath = $sourceVrFile.FullName.Substring($VrAssetsSource.Length).TrimStart('\')
    $destinationVrFile = Join-Path $VrAssetsPath $relativeVrPath
    if (-not (Test-Path -LiteralPath $destinationVrFile -PathType Leaf) -or
        (Get-Sha256 $sourceVrFile.FullName) -ne (Get-Sha256 $destinationVrFile)) {
        throw "Deployment verification failed for OpenVR payload '$relativeVrPath'."
    }
}

$state = [ordered]@{
    schema = 1
    gameExeSha256 = Get-Sha256 $GamePath
    originalAlutSha256 = Get-Sha256 $OriginalAlutPath
    installedProxySha256 = Get-Sha256 $AlutPath
    probeSha256 = Get-Sha256 $ProbePath
    handTextureSha256 = Get-Sha256 $HandTexturePath
    spanishLocalizationSha256 = Get-Sha256 $LocalizationPath
    spanishLocalizationHadOriginal = $LocalizationHadOriginal
    spanishLocalizationOriginalSha256 = $LocalizationOriginalSha256
    audioConfigCreated = $AudioConfigCreated
    audioConfigSha256 = if ($AudioConfigCreated) { Get-Sha256 $AudioConfigPath } else { $null }
}
$state | ConvertTo-Json | Set-Content -LiteralPath $InstallStatePath -Encoding UTF8

Write-Host 'Black Plague normal Steam launch bootstrap installed.'
Write-Host 'Start SteamVR, then use the normal Play button for Penumbra: Black Plague in Steam.'
Write-Host 'Validation scripts and Start-Black-Plague-VR.cmd remain available for diagnostics.'
