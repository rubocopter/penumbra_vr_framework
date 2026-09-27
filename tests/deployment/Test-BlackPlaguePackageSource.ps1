[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$packager = Join-Path $repoRoot 'tools/Package-BlackPlagueCandidate.ps1'
$buildRoot = Join-Path $repoRoot 'build'
$tempBase = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath()).TrimEnd('\') + '\'
$fixture = [System.IO.Path]::GetFullPath((Join-Path $tempBase ('PenumbraVrForeignBuild-' + [guid]::NewGuid().ToString('N'))))
if (-not $fixture.StartsWith($tempBase, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Unsafe foreign-build fixture path.'
}

try {
    $foreignBuild = Join-Path $fixture 'build'
    $foreignRelease = Join-Path $foreignBuild 'bin/Release'
    New-Item -ItemType Directory -Path $foreignRelease -Force | Out-Null
    $cache = Get-Content -LiteralPath (Join-Path $buildRoot 'CMakeCache.txt') -Raw
    $foreignSource = (Join-Path $fixture 'different-source').Replace('\', '/')
    $changedCache = [regex]::Replace($cache, '(?m)^CMAKE_HOME_DIRECTORY:INTERNAL=.*$',
        'CMAKE_HOME_DIRECTORY:INTERNAL=' + $foreignSource)
    if ($changedCache -eq $cache) { throw 'Fixture CMake cache did not contain a source directory.' }
    [System.IO.File]::WriteAllText((Join-Path $foreignBuild 'CMakeCache.txt'), $changedCache)
    foreach ($name in @('PenumbraVR.BlackPlague.Bootstrap.dll',
                        'PenumbraVR.BlackPlague.Probe.dll', 'openvr_api.dll',
                        'PenumbraVR.LaaTransform.exe')) {
        Copy-Item -LiteralPath (Join-Path $buildRoot "bin/Release/$name") -Destination $foreignRelease
    }
    $archive = Join-Path $fixture 'ForeignBuild.zip'
    $rejected = $false
    try { & $packager -BuildRoot $foreignBuild -OutputPath $archive 6>$null | Out-Null }
    catch { $rejected = $_.Exception.Message -like '*different source checkout*' }
    if (-not $rejected -or (Test-Path -LiteralPath $archive)) {
        throw 'Black Plague packager accepted Release binaries from another source checkout.'
    }
    $ownSource = [System.IO.Path]::GetFullPath($repoRoot).Replace('\', '/')
    $ownCache = [regex]::Replace($changedCache, '(?m)^CMAKE_HOME_DIRECTORY:INTERNAL=.*$',
        'CMAKE_HOME_DIRECTORY:INTERNAL=' + $ownSource)
    $foreignSdk = (Join-Path $fixture 'unrelated-openvr-sdk').Replace('\', '/')
    $wrongSdkCache = [regex]::Replace($ownCache, '(?m)^PENUMBRA_VR_OPENVR_SDK:PATH=.*$',
        'PENUMBRA_VR_OPENVR_SDK:PATH=' + $foreignSdk)
    if ($wrongSdkCache -eq $ownCache) { throw 'Fixture CMake cache did not contain an OpenVR SDK path.' }
    [System.IO.File]::WriteAllText((Join-Path $foreignBuild 'CMakeCache.txt'), $wrongSdkCache)
    $rejectedSdk = $false
    try { & $packager -BuildRoot $foreignBuild -OutputPath $archive 6>$null | Out-Null }
    catch { $rejectedSdk = $_.Exception.Message -like '*pinned OpenVR SDK*' }
    if (-not $rejectedSdk -or (Test-Path -LiteralPath $archive)) {
        throw 'Black Plague packager accepted a build configured against a different OpenVR SDK.'
    }
    Write-Host 'Black Plague package rejects foreign source checkouts and OpenVR SDKs.'
}
finally {
    if (Test-Path -LiteralPath $fixture -PathType Container) {
        $resolved = [System.IO.Path]::GetFullPath($fixture)
        if (-not $resolved.StartsWith($tempBase, [StringComparison]::OrdinalIgnoreCase)) {
            throw "Unsafe foreign-build fixture cleanup path: $resolved"
        }
        Remove-Item -LiteralPath $resolved -Recurse -Force
    }
}
