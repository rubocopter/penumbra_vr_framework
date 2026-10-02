[CmdletBinding()]
param(
    [string]$RepositoryRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if (-not $RepositoryRoot) {
    $RepositoryRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
}

$generator = Join-Path $RepositoryRoot 'products/overture/scripts/generate-bindings.ps1'
$temporaryRoot = Join-Path ([IO.Path]::GetTempPath()) ('pvr-overture-bindings-' + [guid]::NewGuid().ToString('N'))

try {
    New-Item -ItemType Directory -Path $temporaryRoot | Out-Null
    & $generator -OutputRoot $temporaryRoot | Out-Null

    $bindingPath = Join-Path $temporaryRoot 'oculus_touch.json'
    $document = Get-Content -Raw $bindingPath | ConvertFrom-Json
    $globalBinding = $document.bindings.PSObject.Properties['/actions/global'].Value

    foreach ($hand in @('left', 'right')) {
        $output = "/actions/global/in/${hand}_aim"
        $pose = @($globalBinding.poses | Where-Object { $_.output -eq $output })
        if ($pose.Count -ne 1) {
            throw "Expected exactly one Touch aim pose for $output; found $($pose.Count)."
        }

        $expectedPath = "/user/hand/$hand/pose/tip"
        if ($pose[0].path -ne $expectedPath) {
            throw "Touch aim pose for $hand must use $expectedPath; generated '$($pose[0].path)'."
        }
    }
}
finally {
    if (Test-Path $temporaryRoot) {
        Remove-Item -Recurse -Force $temporaryRoot
    }
}
