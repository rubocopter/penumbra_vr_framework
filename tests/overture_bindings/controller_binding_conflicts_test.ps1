[CmdletBinding()]
param([string]$RepositoryRoot)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if (-not $RepositoryRoot) { $RepositoryRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot) }
$fixture = Join-Path ([IO.Path]::GetTempPath()) ('pvr-binding-conflicts-' + [guid]::NewGuid().ToString('N'))
try {
    New-Item -ItemType Directory -Path $fixture | Out-Null
    & (Join-Path $RepositoryRoot 'products/overture/scripts/generate-bindings.ps1') -OutputRoot $fixture | Out-Null
    foreach ($root in @($fixture, "$RepositoryRoot/assets/openvr/bindings", "$RepositoryRoot/assets/openvr/overture/bindings")) {
        foreach ($file in @('knuckles.json', 'oculus_touch.json', 'pico4_controller.json', 'pico_neo3_controller.json', 'psvr2_sense.json', 'microsoft_motion_controller.json', 'holographic_controller.json')) {
            $graph = Get-Content -LiteralPath (Join-Path $root $file) -Raw | ConvertFrom-Json
            foreach ($set in @('gameplay', 'gameplay_left')) {
                $keys = @{}
                foreach ($row in $graph.bindings."/actions/$set".sources) {
                    foreach ($event in $row.inputs.PSObject.Properties) {
                        $key = $row.path + ':' + $event.Name
                        if ($keys.ContainsKey($key)) { throw "$file $set routes $key to two actions." }
                        $keys[$key] = $event.Value.output
                    }
                    if ($graph.controller_type -eq 'knuckles' -and $row.inputs.PSObject.Properties.Value.output -match '/in/quick_light$') {
                        if ($row.path -notmatch '/input/grip$') { throw 'Index quick light shares the offhand interact trigger.' }
                    }
                }
            }
            foreach ($set in $graph.bindings.PSObject.Properties) {
                foreach ($row in $set.Value.sources) {
                    if ($graph.controller_type -in @('oculus_touch','pico4_controller','pico_neo3_controller') -and
                        ($row.path -match '/hand/right/input/[xy]$' -or $row.path -match '/hand/left/input/[ab]$')) {
                        throw "$file binds a face button absent from that hand: $($row.path)."
                    }
                    if ($graph.controller_type -eq 'knuckles' -and $row.path -match '/input/system$') {
                        throw 'Index pause must not depend on the reserved SteamVR system button.'
                    }
                    if ($graph.controller_type -eq 'knuckles' -and $row.path -match '/input/(grip|trackpad)$') {
                        if ($row.mode -ne 'button' -or
                            $row.PSObject.Properties.Name -notcontains 'parameters' -or
                            $row.parameters.force_input -ne 'force' -or
                            $row.parameters.click_activate_threshold -ne '0.65' -or
                            $row.parameters.click_deactivate_threshold -ne '0.6') {
                            throw 'Index grip/trackpad must use the explicit force-click contract.'
                        }
                    }
                    foreach ($event in $row.inputs.PSObject.Properties) {
                        if ($event.Value.output -match '/in/crouch$' -and
                            ($row.mode -ne 'joystick' -or $event.Name -ne 'click')) {
                            throw "$file crouch must use a real joystick click, never position/touch."
                        }
                    }
                }
            }
            if ($graph.controller_type -eq 'knuckles') {
                foreach ($pose in $graph.bindings.'/actions/global'.poses | Where-Object { $_.output -match '_aim$' }) {
                    if ($pose.path -notmatch '/pose/tip$') { throw 'Index aim must use its declared tip pose.' }
                }
            }
        }
    }
    Write-Host 'Modern layouts separate gameplay clicks and keep crouch off analog position.'
}
finally {
    if (Test-Path -LiteralPath $fixture) { Remove-Item -LiteralPath $fixture -Recurse -Force }
}
