[CmdletBinding()]
param(
    [string]$GamePath = (Join-Path ${env:ProgramFiles(x86)} 'Steam\steamapps\common\Penumbra Black Plague\redist\penumbra.exe'),
    [string]$ImagePath = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$validation = Join-Path $PSScriptRoot 'Start-BlackPlaguePhysicalDisplacementValidation.ps1'
& $validation -GamePath $GamePath -ImagePath $ImagePath -EnableRoomScale
exit $LASTEXITCODE
