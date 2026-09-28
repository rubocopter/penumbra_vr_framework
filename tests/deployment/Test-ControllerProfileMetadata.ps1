[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$tempBase = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + '\'
$fixture = [IO.Path]::GetFullPath((Join-Path $tempBase ('PenumbraVrProfiles-' + [guid]::NewGuid().ToString('N'))))
if (-not $fixture.StartsWith($tempBase, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Unsafe controller-profile fixture path.'
}

try {
    foreach ($relative in @('assets/openvr', 'assets/localization', 'assets/deployment', 'manifests')) {
        $destination = Join-Path $fixture $relative
        New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
        Copy-Item -LiteralPath (Join-Path $repoRoot $relative) -Destination $destination -Recurse
    }
    foreach ($relative in @('tools/Test-PenumbraVrMetadata.ps1', 'src/common/build_catalog.cpp',
                           'products/overture/data/models/hud_objects/HAND_Low_C.jpg')) {
        $destination = Join-Path $fixture $relative
        New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
        Copy-Item -LiteralPath (Join-Path $repoRoot $relative) -Destination $destination
    }
    $validator = Join-Path $fixture 'tools/Test-PenumbraVrMetadata.ps1'
    & $validator
    $roots = @('assets/openvr', 'assets/openvr/overture')
    function Assert-Rejected([string]$Name, [scriptblock]$Mutation, [string]$Expected) {
        foreach ($root in $roots) {
            foreach ($relative in @('actions.json', 'bindings/psvr2_sense.json')) {
                Copy-Item -LiteralPath (Join-Path (Join-Path $repoRoot $root) $relative) `
                    -Destination (Join-Path (Join-Path $fixture $root) $relative) -Force
            }
        }
        foreach ($root in $roots) { & $Mutation (Join-Path $fixture $root) }
        $rejected = $false
        try { & $validator }
        catch {
            if ($_.Exception.Message -notlike $Expected) { throw }
            $rejected = $true
        }
        if (-not $rejected) { throw "Controller-profile validator accepted: $Name" }
        Write-Host "Rejected controller-profile regression: $Name"
    }
    Assert-Rejected 'missing controller family in both manifests' {
        param($root)
        $path = Join-Path $root 'actions.json'
        $json = Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
        $json.default_bindings = @($json.default_bindings | Where-Object { $_.controller_type -ne 'knuckles' })
        $json | ConvertTo-Json -Depth 100 | Set-Content -LiteralPath $path -Encoding UTF8
    } '*eight bundled controller profiles*'
    Assert-Rejected 'relative binding path drift' {
        param($root)
        $path = Join-Path $root 'actions.json'
        $json = Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
        $json.default_bindings[0].binding_url = 'bindings/../bindings/psvr2_sense.json'
        $json | ConvertTo-Json -Depth 100 | Set-Content -LiteralPath $path -Encoding UTF8
    } '*unexpected binding path*'
    Assert-Rejected 'lost turn in both dominant-hand layouts' {
        param($root)
        $path = Join-Path $root 'bindings/psvr2_sense.json'
        $json = Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
        foreach ($set in $json.bindings.PSObject.Properties) {
            foreach ($source in $set.Value.sources) {
                foreach ($name in @($source.inputs.PSObject.Properties.Name)) {
                    if ($source.inputs.$name.output -like '*/in/turn') {
                        $source.inputs.PSObject.Properties.Remove($name)
                    }
                }
            }
        }
        $json | ConvertTo-Json -Depth 100 | Set-Content -LiteralPath $path -Encoding UTF8
    } '*has lost implemented action*'
    Assert-Rejected 'haptic bound to a movement action' {
        param($root)
        $path = Join-Path $root 'bindings/psvr2_sense.json'
        $json = Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
        $json.bindings.'/actions/global'.haptics[0].output = '/actions/gameplay/in/move'
        $json | ConvertTo-Json -Depth 100 | Set-Content -LiteralPath $path -Encoding UTF8
    } '*incompatible type*'
} finally {
    if (Test-Path -LiteralPath $fixture -PathType Container) {
        Remove-Item -LiteralPath $fixture -Recurse -Force
    }
}
