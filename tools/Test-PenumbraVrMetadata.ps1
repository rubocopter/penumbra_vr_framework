[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$manifestRoot = Join-Path $repositoryRoot 'manifests'
$openVrAssetRoot = Join-Path $repositoryRoot 'assets\openvr'
$localizationAssetRoot = Join-Path $repositoryRoot 'assets\localization'

function Read-JsonFile([string]$Path) {
    try {
        return Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
    } catch {
        throw "Invalid JSON in $Path`: $($_.Exception.Message)"
    }
}

$jsonFiles = @(
    Get-ChildItem -LiteralPath $manifestRoot, $openVrAssetRoot, $localizationAssetRoot -Recurse -File -Filter '*.json'
)
foreach ($jsonFile in $jsonFiles) {
    $null = Read-JsonFile $jsonFile.FullName
}

$catalogSourcePath = Join-Path $repositoryRoot 'src\common\build_catalog.cpp'
$catalogSource = Get-Content -LiteralPath $catalogSourcePath -Raw
$catalogPattern = [regex]::new(
    '\{GameId::(?<game>[a-z_]+),\s*"(?<id>[^"]+)",\s*"(?<sha>[A-F0-9]{64})",\s*BuildVariant::(?<variant>[a-z_]+),\s*"(?<canonical>[A-F0-9]{64})",\s*(?<probe>true|false)\}',
    [System.Text.RegularExpressions.RegexOptions]::Singleline)
$catalogEntries = @($catalogPattern.Matches($catalogSource) | ForEach-Object {
    [pscustomobject]@{
        Game = $_.Groups['game'].Value
        Id = $_.Groups['id'].Value
        Sha256 = $_.Groups['sha'].Value
        Variant = $_.Groups['variant'].Value
        CanonicalSha256 = $_.Groups['canonical'].Value
        ProbeAllowed = $_.Groups['probe'].Value -eq 'true'
    }
})

if (-not $catalogEntries) {
    throw "No known-build entries could be read from $catalogSourcePath."
}
if (($catalogEntries.Sha256 | Sort-Object -Unique).Count -ne $catalogEntries.Count) {
    throw 'The compiled known-build catalogue contains duplicate SHA-256 values.'
}
$catalogIdentityVariants = @($catalogEntries | ForEach-Object { "$($_.Game)|$($_.Id)|$($_.Variant)" })
if (($catalogIdentityVariants | Sort-Object -Unique).Count -ne $catalogIdentityVariants.Count) {
    throw 'The compiled known-build catalogue contains duplicate game/build/variant identities.'
}
foreach ($catalogEntry in $catalogEntries) {
    if ($catalogEntry.Variant -eq 'observed') {
        if ($catalogEntry.Sha256 -cne $catalogEntry.CanonicalSha256) {
            throw "Observed build '$($catalogEntry.Id)' does not use its own SHA-256 as canonical identity."
        }
        continue
    }
    if ($catalogEntry.Variant -ne 'large_address_aware') {
        throw "Known build '$($catalogEntry.Id)' uses unsupported variant '$($catalogEntry.Variant)'."
    }
    if ($catalogEntry.Sha256 -ceq $catalogEntry.CanonicalSha256) {
        throw "Large Address Aware variant '$($catalogEntry.Id)' must differ from its canonical SHA-256."
    }
    $canonicalEntry = @($catalogEntries | Where-Object {
        $_.Game -eq $catalogEntry.Game -and
        $_.Id -eq $catalogEntry.Id -and
        $_.Variant -eq 'observed' -and
        $_.Sha256 -ceq $catalogEntry.CanonicalSha256
    })
    if ($canonicalEntry.Count -ne 1) {
        throw "Large Address Aware variant '$($catalogEntry.Id)' does not resolve to exactly one canonical observed build."
    }
    if ($canonicalEntry[0].ProbeAllowed -ne $catalogEntry.ProbeAllowed) {
        throw "Large Address Aware variant '$($catalogEntry.Id)' changes probe authorization relative to its canonical build."
    }
}

$manifestFiles = @(Get-ChildItem -LiteralPath $manifestRoot -Recurse -File -Filter '*.json')
$manifestHashes = @()
$manifestVariantHashes = @()
foreach ($manifestFile in $manifestFiles) {
    $manifest = Read-JsonFile $manifestFile.FullName
    $relativePath = $manifestFile.FullName.Substring($repositoryRoot.Length + 1)
    $gameFolder = Split-Path $manifestFile.DirectoryName -Leaf
    $fileHash = [System.IO.Path]::GetFileNameWithoutExtension($manifestFile.Name)

    if ($manifest.schemaVersion -ne 1) {
        throw "$relativePath has unsupported schemaVersion '$($manifest.schemaVersion)'."
    }
    if ($manifest.game -ne $gameFolder) {
        throw "$relativePath declares game '$($manifest.game)' but is stored under '$gameFolder'."
    }
    if ($manifest.executable.sha256 -cne $fileHash -or $fileHash -cnotmatch '^[A-F0-9]{64}$') {
        throw "$relativePath must use its canonical uppercase executable SHA-256 as its filename."
    }
    if ($manifest.executable.machine -ne 'x86') {
        throw "$relativePath does not describe the required x86 executable."
    }

    $catalogEntry = @($catalogEntries | Where-Object { $_.Sha256 -ceq $fileHash })
    if ($catalogEntry.Count -ne 1) {
        throw "$relativePath does not have exactly one matching compiled catalogue entry."
    }
    if ($catalogEntry[0].Game -ne $manifest.game -or
        $catalogEntry[0].Id -ne $manifest.buildId -or
        $catalogEntry[0].Variant -ne 'observed' -or
        $catalogEntry[0].CanonicalSha256 -cne $fileHash) {
        throw "$relativePath disagrees with the compiled catalogue game or build ID."
    }
    if ($manifest.game -eq 'black_plague' -and -not $catalogEntry[0].ProbeAllowed) {
        throw "$relativePath is the Black Plague research manifest but the probe is not allowlisted."
    }
    if ($manifest.game -ne 'black_plague' -and $catalogEntry[0].ProbeAllowed) {
        throw "$relativePath unexpectedly allows the Black Plague probe."
    }
    $manifestHashes += $fileHash

    if ($manifest.executable.PSObject.Properties.Name -contains 'transformedVariants') {
        foreach ($variant in @($manifest.executable.transformedVariants)) {
            if ($variant.kind -ne 'large-address-aware') {
                throw "$relativePath declares unsupported transformed variant '$($variant.kind)'."
            }
            if ($variant.sha256 -cnotmatch '^[A-F0-9]{64}$') {
                throw "$relativePath transformed variant must use a canonical uppercase SHA-256."
            }
            if ($variant.largeAddressAware -ne $true) {
                throw "$relativePath Large Address Aware variant must declare largeAddressAware=true."
            }

            $variantCatalogEntry = @($catalogEntries | Where-Object { $_.Sha256 -ceq $variant.sha256 })
            if ($variantCatalogEntry.Count -ne 1) {
                throw "$relativePath transformed variant does not have exactly one matching compiled catalogue entry."
            }
            if ($variantCatalogEntry[0].Game -ne $manifest.game -or
                $variantCatalogEntry[0].Id -ne $manifest.buildId -or
                $variantCatalogEntry[0].Variant -ne 'large_address_aware' -or
                $variantCatalogEntry[0].CanonicalSha256 -cne $fileHash -or
                $variantCatalogEntry[0].ProbeAllowed -ne $catalogEntry[0].ProbeAllowed) {
                throw "$relativePath transformed variant disagrees with its canonical build identity."
            }
            $manifestVariantHashes += $variant.sha256
        }
    }
}

if ((@($manifestHashes + $manifestVariantHashes) | Sort-Object -Unique).Count -ne
    @($manifestHashes + $manifestVariantHashes).Count) {
    throw 'Exact-build manifests contain duplicate executable hashes.'
}

$catalogLaaHashes = @($catalogEntries | Where-Object { $_.Variant -eq 'large_address_aware' } | ForEach-Object { $_.Sha256 } | Sort-Object)
$documentedLaaHashes = @($manifestVariantHashes | Sort-Object)
if (@(Compare-Object -ReferenceObject $catalogLaaHashes -DifferenceObject $documentedLaaHashes).Count -ne 0) {
    throw 'The compiled Large Address Aware build variants and manifest transformed variants disagree.'
}

$localizationManifestPath = Join-Path $localizationAssetRoot 'manifest.json'
$localizationManifest = Read-JsonFile $localizationManifestPath
if ($localizationManifest.schemaVersion -ne 1) {
    throw "assets\localization\manifest.json has unsupported schemaVersion '$($localizationManifest.schemaVersion)'."
}

$expectedLocalizationTargets = @{
    black_plague = 'redist/config/Espanol.lang'
    requiem = 'redist/expansion01/config/Espanol_exp.lang'
}
$localizationEntries = @($localizationManifest.translations)
$localizationGames = @($localizationEntries | ForEach-Object { $_.game })
if (($localizationGames | Sort-Object -Unique).Count -ne $localizationGames.Count) {
    throw 'Spanish localization metadata contains duplicate game entries.'
}
if (@(Compare-Object -ReferenceObject @($expectedLocalizationTargets.Keys | Sort-Object) -DifferenceObject @($localizationGames | Sort-Object)).Count -ne 0) {
    throw 'Spanish localization metadata must contain exactly the Black Plague and Requiem payloads.'
}

foreach ($entry in $localizationEntries) {
    if ($entry.installPath -cne $expectedLocalizationTargets[$entry.game]) {
        throw "Spanish localization '$($entry.game)' has unexpected install path '$($entry.installPath)'."
    }
    if ([System.IO.Path]::IsPathRooted([string]$entry.sourcePath) -or
        [System.IO.Path]::IsPathRooted([string]$entry.noticePath)) {
        throw "Spanish localization '$($entry.game)' must use repository-relative source and notice paths."
    }
    if ($entry.sha256 -cnotmatch '^[A-F0-9]{64}$') {
        throw "Spanish localization '$($entry.game)' must declare an uppercase SHA-256."
    }

    $payloadPath = Join-Path $repositoryRoot ([string]$entry.sourcePath -replace '/', '\')
    $noticePath = Join-Path $repositoryRoot ([string]$entry.noticePath -replace '/', '\')
    if (-not (Test-Path -LiteralPath $payloadPath -PathType Leaf)) {
        throw "Spanish localization payload is missing: $($entry.sourcePath)"
    }
    if (-not (Test-Path -LiteralPath $noticePath -PathType Leaf)) {
        throw "Spanish localization attribution notice is missing: $($entry.noticePath)"
    }

    $actualHash = (Get-FileHash -LiteralPath $payloadPath -Algorithm SHA256).Hash
    if ($actualHash -cne $entry.sha256) {
        throw "Spanish localization '$($entry.game)' hash '$actualHash' does not match metadata '$($entry.sha256)'."
    }
}

$actionManifestPath = Join-Path $openVrAssetRoot 'actions.json'
$actionManifest = Read-JsonFile $actionManifestPath
$actionNames = @($actionManifest.actions | ForEach-Object { $_.name })
$actionSetNames = @($actionManifest.action_sets | ForEach-Object { $_.name })

if (($actionNames | Sort-Object -Unique).Count -ne $actionNames.Count) {
    throw 'Duplicate SteamVR action names were found in assets\openvr\actions.json.'
}
if (($actionSetNames | Sort-Object -Unique).Count -ne $actionSetNames.Count) {
    throw 'Duplicate SteamVR action-set names were found in assets\openvr\actions.json.'
}

foreach ($skeletonAction in @($actionManifest.actions | Where-Object { $_.type -eq 'skeleton' })) {
    $expectedSkeleton = if ($skeletonAction.name -match '/in/left_') {
        '/skeleton/hand/left'
    } else {
        '/skeleton/hand/right'
    }
    if ($skeletonAction.skeleton -ne $expectedSkeleton) {
        throw "SteamVR action '$($skeletonAction.name)' must declare skeleton '$expectedSkeleton'."
    }
}

foreach ($localization in $actionManifest.localization) {
    $localizedNames = @($localization.PSObject.Properties.Name)
    foreach ($requiredName in @($actionSetNames + $actionNames)) {
        if ($localizedNames -notcontains $requiredName) {
            throw "SteamVR localization '$($localization.language_tag)' is missing '$requiredName'."
        }
    }
}

$bindingControllerTypes = @($actionManifest.default_bindings | ForEach-Object { $_.controller_type })
$bindingUrls = @($actionManifest.default_bindings | ForEach-Object { $_.binding_url })
if (($bindingControllerTypes | Sort-Object -Unique).Count -ne $bindingControllerTypes.Count) {
    throw 'Duplicate SteamVR default-binding controller types were found.'
}
if (($bindingUrls | Sort-Object -Unique).Count -ne $bindingUrls.Count) {
    throw 'Duplicate SteamVR default-binding paths were found.'
}

$mandatoryActions = @($actionManifest.actions | Where-Object {
    $_.PSObject.Properties.Name -contains 'requirement' -and $_.requirement -eq 'mandatory'
} | ForEach-Object { $_.name })
$skeletalControllerTypes = @(
    'playstation_vr2_sense',
    'knuckles',
    'oculus_touch',
    'pico4_controller',
    'pico_neo3_controller'
)

foreach ($defaultBinding in $actionManifest.default_bindings) {
    $bindingRelativePath = $defaultBinding.binding_url.Replace(
        '/', [System.IO.Path]::DirectorySeparatorChar)
    $bindingPath = Join-Path $openVrAssetRoot $bindingRelativePath
    if (-not (Test-Path -LiteralPath $bindingPath)) {
        throw "SteamVR default binding does not exist: $bindingRelativePath"
    }

    $binding = Read-JsonFile $bindingPath
    if ($binding.controller_type -ne $defaultBinding.controller_type) {
        throw "SteamVR controller type mismatch in $bindingRelativePath."
    }

    foreach ($bindingSetProperty in $binding.bindings.PSObject.Properties) {
        if ($actionSetNames -notcontains $bindingSetProperty.Name) {
            throw "Unknown SteamVR action set '$($bindingSetProperty.Name)' in $bindingRelativePath."
        }

        $bindingSet = $bindingSetProperty.Value
        foreach ($directCollection in @('haptics', 'poses', 'skeleton')) {
            foreach ($directBinding in @($bindingSet.$directCollection)) {
                if ($null -eq $directBinding) {
                    continue
                }
                if ($actionNames -notcontains $directBinding.output) {
                    throw "Unknown SteamVR action '$($directBinding.output)' in $bindingRelativePath."
                }
                if ($directCollection -eq 'skeleton') {
                    $hand = if ($directBinding.output -match '/in/(left|right)_skeleton$') {
                        $Matches[1]
                    } else {
                        ''
                    }
                    $expectedPath = if ($hand) {
                        "/user/hand/$hand/input/skeleton/$hand"
                    } else {
                        $null
                    }
                    if ($directBinding.path -ne $expectedPath) {
                        throw "Invalid skeleton path '$($directBinding.path)' in $bindingRelativePath."
                    }
                }
            }
        }

        foreach ($source in @($bindingSet.sources)) {
            if ($null -eq $source) {
                continue
            }
            foreach ($inputProperty in $source.inputs.PSObject.Properties) {
                if ($actionNames -notcontains $inputProperty.Value.output) {
                    throw "Unknown SteamVR action '$($inputProperty.Value.output)' in $bindingRelativePath."
                }
            }
        }
    }

    $boundOutputs = @(
        @($binding.bindings.PSObject.Properties | ForEach-Object { $_.Value.haptics }) |
            ForEach-Object { if ($null -ne $_) { $_.output } }
    ) + @(
        @($binding.bindings.PSObject.Properties | ForEach-Object { $_.Value.poses }) |
            ForEach-Object { if ($null -ne $_) { $_.output } }
    ) + @(
        @($binding.bindings.PSObject.Properties | ForEach-Object { $_.Value.sources }) |
            ForEach-Object {
                if ($null -ne $_ -and $null -ne $_.inputs) {
                    $_.inputs.PSObject.Properties.Value.output
                }
            }
    )
    foreach ($mandatoryAction in $mandatoryActions) {
        if ($boundOutputs -notcontains $mandatoryAction) {
            throw "Mandatory SteamVR action '$mandatoryAction' has no binding in $bindingRelativePath."
        }
    }

    if ($defaultBinding.controller_type -in $skeletalControllerTypes) {
        $skeletonOutputs = @(
            $binding.bindings.PSObject.Properties |
                ForEach-Object { $_.Value.skeleton } |
                ForEach-Object { if ($null -ne $_) { $_.output } }
        )
        foreach ($skeletonAction in @(
            '/actions/global/in/left_skeleton',
            '/actions/global/in/right_skeleton'
        )) {
            if ($skeletonOutputs -notcontains $skeletonAction) {
                throw "Skeletal controller '$($defaultBinding.controller_type)' lacks '$skeletonAction'."
            }
        }
    }
}

Write-Host (
    ('Metadata validation passed: {0} catalogue entries, {1} exact-build manifests, ' +
    '{2} actions, {3} action sets and {4} controller bindings.') -f
        $catalogEntries.Count,
        $manifestFiles.Count,
        $actionNames.Count,
        $actionSetNames.Count,
        $bindingUrls.Count) -ForegroundColor Green
