[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$KnownGameExe,
    [Parameter(Mandatory = $true)][string]$RetailAlut,
    [Parameter(Mandatory = $true)][string]$BuildRoot
)

$ErrorActionPreference = 'Stop'
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
    if ($LASTEXITCODE -ne 0 -or
        (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne
            (Get-FileHash -LiteralPath $defaultArchive -Algorithm SHA256).Hash) {
        throw 'Packager default build path produced a different package.'
    }
    $unpacked = Join-Path $temporaryRoot 'unpacked'
    Expand-Archive -LiteralPath $archive -DestinationPath $unpacked
    $installer = Join-Path $unpacked 'tools/Install-BlackPlagueSteamBootstrap.ps1'
    $steamRoot = Join-Path $temporaryRoot 'Steam'
    $gameRoot = Join-Path $steamRoot 'steamapps/common/Penumbra Black Plague/redist'
    New-Item -ItemType Directory -Path $gameRoot -Force | Out-Null
    $gameExe = Join-Path $gameRoot 'Penumbra.exe'
    Copy-Item -LiteralPath $KnownGameExe -Destination $gameExe
    Copy-Item -LiteralPath $RetailAlut -Destination (Join-Path $gameRoot 'alut.dll')
    $originalHash = (Get-FileHash -LiteralPath (Join-Path $gameRoot 'alut.dll') -Algorithm SHA256).Hash

    & $installer -SteamRoot $steamRoot | Out-Null
    $state = Join-Path $gameRoot 'PenumbraVR.BlackPlague.install.json'
    $proxy = Join-Path $gameRoot 'alut.dll'
    $expectedProxy = Join-Path $unpacked 'build/bin/Release/PenumbraVR.BlackPlague.Bootstrap.dll'
    if (-not (Test-Path -LiteralPath $state -PathType Leaf) -or
        (Get-FileHash -LiteralPath $proxy -Algorithm SHA256).Hash -ne
            (Get-FileHash -LiteralPath $expectedProxy -Algorithm SHA256).Hash) {
        throw 'Package installation did not deploy its own build.'
    }

    & powershell -NoProfile -ExecutionPolicy Bypass -File $installer -SteamRoot $steamRoot -Restore | Out-Null
    if ($LASTEXITCODE -ne 0 -or
        (Get-FileHash -LiteralPath $proxy -Algorithm SHA256).Hash -ne $originalHash -or
        (Test-Path -LiteralPath $state)) {
        throw 'Package uninstall did not restore the game fixture.'
    }
    $secondGameRoot = Join-Path $steamRoot 'steamapps/common/Penumbra Overture/redist'
    New-Item -ItemType Directory -Path $secondGameRoot -Force | Out-Null
    Copy-Item -LiteralPath $KnownGameExe -Destination (Join-Path $secondGameRoot 'Penumbra.exe')
    $ambiguous = $false
    try { & $installer -SteamRoot $steamRoot | Out-Null } catch {
        $ambiguous = $_.Exception.Message -like '*Expected exactly one supported Black Plague installation*'
    }
    if (-not $ambiguous -or (Test-Path -LiteralPath $state)) {
        throw 'Ambiguous installations were not rejected before writes.'
    }
    Write-Host 'Black Plague ZIP installs and restores independently of the source checkout.'
} finally {
    if (Test-Path -LiteralPath $temporaryRoot -PathType Container) {
        Remove-Item -LiteralPath $temporaryRoot -Recurse -Force
    }
}
