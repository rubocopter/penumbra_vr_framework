[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$KnownGameExe,
    [Parameter(Mandatory = $true)][string]$RetailAlut,
    [Parameter(Mandatory = $true)][string]$BuildRoot
)

$ErrorActionPreference = 'Stop'
# Fixture deployments must never edit the real Documents configuration.
$PSDefaultParameterValues = @{ '*:SettingsScope' = 'DefaultFiles' }
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$packager = Join-Path $repoRoot 'tools/Package-BlackPlagueCandidate.ps1'
$temporaryRoot = [System.IO.Path]::GetFullPath(
    (Join-Path ([System.IO.Path]::GetTempPath()) ('PenumbraVrPackageTest-' + [guid]::NewGuid().ToString('N'))))
$tempBase = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath())
if (-not $temporaryRoot.StartsWith($tempBase, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Unsafe test directory.'
}

try {
    New-Item -ItemType Directory -Path $temporaryRoot | Out-Null
    $archive = Join-Path $temporaryRoot 'BlackPlagueCandidate.zip'
    & $packager -BuildRoot $BuildRoot -OutputPath $archive
    if (-not (Test-Path -LiteralPath $archive -PathType Leaf)) {
        throw 'Packager did not create a ZIP archive.'
    }
    $secondArchive = Join-Path $temporaryRoot 'BlackPlagueCandidateAgain.zip'
    & $packager -BuildRoot $BuildRoot -OutputPath $secondArchive
    if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne
        (Get-FileHash -LiteralPath $secondArchive -Algorithm SHA256).Hash) {
        throw 'Identical release inputs produced different ZIP archives.'
    }
    $defaultArchive = Join-Path $temporaryRoot 'BlackPlagueDefault.zip'
    & powershell -NoProfile -ExecutionPolicy Bypass -File $packager -OutputPath $defaultArchive | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw 'Packager default build path failed under Windows PowerShell.'
    }
    $unpacked = Join-Path $temporaryRoot 'unpacked'
    Expand-Archive -LiteralPath $archive -DestinationPath $unpacked
    $defaultUnpacked = Join-Path $temporaryRoot 'default-unpacked'
    Expand-Archive -LiteralPath $defaultArchive -DestinationPath $defaultUnpacked
    $expectedContents = @(Get-ChildItem -LiteralPath $unpacked -Recurse -File | ForEach-Object {
        $relative = $_.FullName.Substring($unpacked.Length).TrimStart('\')
        "$relative $((Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash)"
    } | Sort-Object)
    $defaultContents = @(Get-ChildItem -LiteralPath $defaultUnpacked -Recurse -File | ForEach-Object {
        $relative = $_.FullName.Substring($defaultUnpacked.Length).TrimStart('\')
        "$relative $((Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash)"
    } | Sort-Object)
    if (@(Compare-Object $expectedContents $defaultContents).Count -ne 0) {
        throw 'Packager default build path produced different package contents under Windows PowerShell.'
    }
    $installer = Join-Path $unpacked 'tools/Install-BlackPlagueSteamBootstrap.ps1'
    $steamRoot = Join-Path $temporaryRoot 'Steam'
    $gameRoot = Join-Path $steamRoot 'steamapps/common/Penumbra Black Plague/redist'
    New-Item -ItemType Directory -Path $gameRoot -Force | Out-Null
    $gameExe = Join-Path $gameRoot 'Penumbra.exe'
    Copy-Item -LiteralPath $KnownGameExe -Destination $gameExe
    Copy-Item -LiteralPath $RetailAlut -Destination (Join-Path $gameRoot 'alut.dll')
    $originalHash = (Get-FileHash -LiteralPath (Join-Path $gameRoot 'alut.dll') -Algorithm SHA256).Hash

    & $installer -GamePath $gameExe -LargeAddressAware | Out-Null
    $state = Join-Path $gameRoot 'PenumbraVR.BlackPlague.install.json'
    $proxy = Join-Path $gameRoot 'alut.dll'
    $expectedProxy = Join-Path $unpacked 'build/bin/Release/PenumbraVR.BlackPlague.Bootstrap.dll'
    $openAlProxy = Join-Path $gameRoot 'OpenAL32.dll'
    $openAlImplementation = Join-Path $gameRoot 'PenumbraVR_OpenALSoft.dll'
    $openAlBackup = Join-Path $gameRoot 'PenumbraVR_OpenAL_original.dll'
    $expectedOpenAlProxy = Join-Path $unpacked 'build/bin/Release/PenumbraVR.BlackPlague.OpenALProxy.dll'
    $expectedOpenAlImplementation = Join-Path $unpacked 'products/overture/dependencies/bin/win32/OpenAL32.dll'
    if (-not (Test-Path -LiteralPath $state -PathType Leaf) -or
        (Get-FileHash -LiteralPath $proxy -Algorithm SHA256).Hash -ne
            (Get-FileHash -LiteralPath $expectedProxy -Algorithm SHA256).Hash -or
        (Get-FileHash -LiteralPath $openAlProxy -Algorithm SHA256).Hash -ne
            (Get-FileHash -LiteralPath $expectedOpenAlProxy -Algorithm SHA256).Hash -or
        (Get-FileHash -LiteralPath $openAlImplementation -Algorithm SHA256).Hash -ne
            (Get-FileHash -LiteralPath $expectedOpenAlImplementation -Algorithm SHA256).Hash -or
        (Test-Path -LiteralPath $openAlBackup) -or
        (Get-FileHash -LiteralPath $gameExe -Algorithm SHA256).Hash -ne
            'DB086CC7A4C7B10864DE0FEBBE2D71A3E4EFF1EC8D067811A6A59EDC1C617196') {
        throw 'Package installation did not deploy its own build.'
    }

    [System.IO.File]::WriteAllBytes($openAlImplementation, [byte[]](1, 2, 3, 4))
    & $installer -GamePath $gameExe -Repair | Out-Null
    if ((Get-FileHash -LiteralPath $openAlImplementation -Algorithm SHA256).Hash -ne
        (Get-FileHash -LiteralPath $expectedOpenAlImplementation -Algorithm SHA256).Hash) {
        throw 'Package repair did not restore the private OpenAL implementation.'
    }

    & powershell -NoProfile -ExecutionPolicy Bypass -File $installer -GamePath $gameExe -Restore | Out-Null
    if ($LASTEXITCODE -ne 0 -or
        (Get-FileHash -LiteralPath $proxy -Algorithm SHA256).Hash -ne $originalHash -or
        (Get-FileHash -LiteralPath $gameExe -Algorithm SHA256).Hash -ne
            'FD316F7586737A63EBA989ECE2271280FE6A98582A1319FE2151385A3DF97BFF' -or
        (Test-Path -LiteralPath $openAlProxy) -or
        (Test-Path -LiteralPath $openAlImplementation) -or
        (Test-Path -LiteralPath $openAlBackup) -or
        (Test-Path -LiteralPath $state)) {
        throw 'Package uninstall did not restore the game fixture.'
    }

    Copy-Item -LiteralPath $expectedOpenAlImplementation -Destination $openAlProxy
    $preexistingOpenAlHash = (Get-FileHash -LiteralPath $openAlProxy -Algorithm SHA256).Hash
    & $installer -GamePath $gameExe -LargeAddressAware | Out-Null
    if ((Get-FileHash -LiteralPath $openAlBackup -Algorithm SHA256).Hash -ne $preexistingOpenAlHash -or
        (Get-FileHash -LiteralPath $openAlProxy -Algorithm SHA256).Hash -ne
            (Get-FileHash -LiteralPath $expectedOpenAlProxy -Algorithm SHA256).Hash) {
        throw 'Package installation did not preserve a known pre-existing OpenAL32.dll.'
    }
    & $installer -GamePath $gameExe -Restore | Out-Null
    if ((Get-FileHash -LiteralPath $openAlProxy -Algorithm SHA256).Hash -ne $preexistingOpenAlHash -or
        (Test-Path -LiteralPath $openAlImplementation) -or
        (Test-Path -LiteralPath $openAlBackup)) {
        throw 'Package restore did not return the known pre-existing OpenAL32.dll exactly.'
    }
    Remove-Item -LiteralPath $openAlProxy -Force
    # Upgrade the previous ZIP's direct OpenAL Soft ownership without treating
    # that managed DLL as a new user-owned original.
    foreach($withOriginal in @($false,$true)){
        if($withOriginal){Copy-Item -LiteralPath $expectedOpenAlImplementation -Destination $openAlProxy}
        & $installer -GamePath $gameExe | Out-Null
        $legacyState=Get-Content -LiteralPath $state -Raw | ConvertFrom-Json
        $legacyState | Add-Member openAlSha256 (Get-FileHash -LiteralPath $expectedOpenAlImplementation).Hash
        $legacyState | Add-Member openAlHadOriginal $withOriginal
        $legacyState | Add-Member originalOpenAlSha256 $(if($withOriginal){(Get-FileHash -LiteralPath $expectedOpenAlImplementation).Hash}else{$null})
        foreach($name in @('openAlProxySha256','openAlImplementationSha256','openAlOriginalHadFile','openAlOriginalSha256')){$legacyState.PSObject.Properties.Remove($name)}
        $legacyState | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $state
        Copy-Item -LiteralPath $expectedOpenAlImplementation -Destination $openAlProxy -Force
        Remove-Item -LiteralPath $openAlImplementation -Force
        & $installer -GamePath $gameExe | Out-Null
        $upgraded=Get-Content -LiteralPath $state -Raw | ConvertFrom-Json
        if($upgraded.openAlOriginalHadFile -ne $withOriginal -or (Get-FileHash -LiteralPath $openAlProxy).Hash -ne (Get-FileHash -LiteralPath $expectedOpenAlProxy).Hash){throw 'Legacy OpenAL update changed original ownership.'}
        & $installer -GamePath $gameExe -Restore | Out-Null
        if((Test-Path -LiteralPath $openAlProxy) -ne $withOriginal -or (Test-Path -LiteralPath $openAlImplementation)){throw 'Legacy OpenAL update did not restore original presence exactly.'}
        if($withOriginal){Remove-Item -LiteralPath $openAlProxy -Force}
    }
    Write-Host 'Black Plague ZIP installs, repairs and restores independently of the source checkout.'
} finally {
    if (Test-Path -LiteralPath $temporaryRoot -PathType Container) {
        Remove-Item -LiteralPath $temporaryRoot -Recurse -Force
    }
}
