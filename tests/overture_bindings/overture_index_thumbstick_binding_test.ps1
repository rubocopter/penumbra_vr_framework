[CmdletBinding()]
param([string]$RepositoryRoot)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if (-not $RepositoryRoot) {
    $RepositoryRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
}

$tempBase = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + '\'
$fixture = [IO.Path]::GetFullPath((Join-Path $tempBase ('pvr-index-bindings-' + [guid]::NewGuid().ToString('N'))))
if (-not $fixture.StartsWith($tempBase, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Unsafe Index binding fixture path.'
}

function Assert-IndexThumbsticks([string]$BindingPath) {
    $binding = Get-Content -LiteralPath $BindingPath -Raw | ConvertFrom-Json
    # SteamVR's indexcontroller/resources/input/index_controller_profile.json
    # declares /input/thumbstick, with type joystick, position and click.
    foreach ($layout in @(
        @{ set = 'gameplay'; move = 'left'; turn = 'right' },
        @{ set = 'gameplay_left'; move = 'right'; turn = 'left' }
    )) {
        foreach ($control in @(
            @{ hand = $layout.move; position = 'move'; click = 'sprint' },
            @{ hand = $layout.turn; position = 'turn'; click = 'crouch' }
        )) {
            $set = '/actions/' + $layout.set
            $output = "$set/in/" + $control.position
            $rows = @($binding.bindings.$set.sources | Where-Object {
                $_.inputs.PSObject.Properties.Name -contains 'position' -and
                $_.inputs.position.output -eq $output
            })
            if ($rows.Count -ne 1) { throw "$BindingPath requires one source for $output." }
            $expectedPath = '/user/hand/' + $control.hand + '/input/thumbstick'
            if ($rows[0].path -ne $expectedPath -or $rows[0].mode -ne 'joystick') {
                throw "$BindingPath must bind $output to $expectedPath in joystick mode; got '$($rows[0].path)'."
            }
            if ($rows[0].inputs.click.output -ne "$set/in/$($control.click)") {
                throw "$BindingPath lost the thumbstick click for $output."
            }
        }
    }
    foreach ($layout in @(@{ set = 'ui'; hand = 'right' }, @{ set = 'ui_left'; hand = 'left' })) {
        $set = '/actions/' + $layout.set
        $rows = @($binding.bindings.$set.sources | Where-Object { $_.inputs.click.output -eq "$set/in/drag" })
        if ($rows.Count -ne 1 -or $rows[0].mode -ne 'joystick' -or
            $rows[0].path -ne "/user/hand/$($layout.hand)/input/thumbstick") {
            throw "$BindingPath must bind $set drag to the dominant thumbstick click."
        }
    }
}

try {
    New-Item -ItemType Directory -Path $fixture | Out-Null
    & (Join-Path $RepositoryRoot 'products/overture/scripts/generate-bindings.ps1') -OutputRoot $fixture | Out-Null
    Assert-IndexThumbsticks (Join-Path $fixture 'knuckles.json')
    foreach ($root in @('assets/openvr/overture/bindings', 'assets/openvr/bindings')) {
        Assert-IndexThumbsticks (Join-Path (Join-Path $RepositoryRoot $root) 'knuckles.json')
    }
    # Touch actually exposes /input/joystick; an Index fix must not change it.
    $touch = Get-Content -LiteralPath (Join-Path $fixture 'oculus_touch.json') -Raw | ConvertFrom-Json
    $move = @($touch.bindings.'/actions/gameplay'.sources | Where-Object {
        $_.inputs.PSObject.Properties.Name -contains 'position' -and
        $_.inputs.position.output -eq '/actions/gameplay/in/move'
    })
    if ($move.Count -ne 1 -or $move[0].path -ne '/user/hand/left/input/joystick') {
        throw 'Index thumbstick correction changed the Touch movement source.'
    }
    Write-Host 'Index movement, turning and stick clicks use the physical thumbsticks in both layouts and payload roots.'
}
finally {
    if (Test-Path -LiteralPath $fixture -PathType Container) {
        Remove-Item -LiteralPath $fixture -Recurse -Force
    }
}
