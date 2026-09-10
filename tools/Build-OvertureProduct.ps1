[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',

    [switch]$Package,

    [switch]$Deploy,

    [string]$InstallRoot,

    [switch]$NoSteamLauncher,

    [switch]$Full
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$frameworkRoot = Split-Path -Parent $PSScriptRoot
$productBuild = Join-Path $frameworkRoot 'products\overture\scripts\build.ps1'
if (-not (Test-Path -LiteralPath $productBuild)) {
    throw "The Framework-owned Overture build script is missing: $productBuild"
}

$arguments = @{
    Configuration = $Configuration
}
foreach ($switchName in @('Package', 'Deploy', 'NoSteamLauncher', 'Full')) {
    if ($PSBoundParameters.ContainsKey($switchName)) {
        $arguments[$switchName] = $true
    }
}
if ($PSBoundParameters.ContainsKey('InstallRoot')) {
    $arguments.InstallRoot = $InstallRoot
}

& $productBuild @arguments
