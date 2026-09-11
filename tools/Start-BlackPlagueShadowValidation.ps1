[CmdletBinding()]
param(
    [string]$GamePath = (Join-Path ${env:ProgramFiles(x86)} 'Steam\steamapps\common\Penumbra Black Plague\redist\penumbra.exe'),
    [string]$ImagePath = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$launcher = Join-Path $repoRoot 'build\bin\Release\PenumbraVR.ProbeLauncher.exe'
$verifier = Join-Path $PSScriptRoot 'Test-BlackPlagueInputMap.ps1'

if (-not (Test-Path -LiteralPath $launcher -PathType Leaf)) {
    throw "Release launcher not found at '$launcher'. Build the framework first."
}
if (-not (Test-Path -LiteralPath $GamePath -PathType Leaf)) {
    throw "Black Plague executable not found at '$GamePath'."
}

if ([string]::IsNullOrWhiteSpace($ImagePath)) {
    $defaultImage = Join-Path $repoRoot 'artifacts\black-plague-22000-live.bin'
    if (-not (Test-Path -LiteralPath $defaultImage -PathType Leaf)) {
        throw "Initialized exact-build image not found at '$defaultImage'. Pass -ImagePath <capture> explicitly; shadow validation must not skip the binary-evidence gate."
    }
    $ImagePath = $defaultImage
}

Write-Host "Validating exact-build initialized image before shadow launch..."
& $verifier -ImagePath $ImagePath

# The game is started through an already-running Steam process, so launcher-shell
# environment variables are not a reliable way to reach the game. Hold a named
# object only while --launch-vr waits for and initializes the probe. The body
# adapter samples this request once during installation; disposing the mutex
# leaves no persistent setting for the next run.
$shadowRequestName = 'Local\PenumbraVR.BlackPlague.ReconciliationShadow'
$shadowRequest = New-Object System.Threading.Mutex -ArgumentList $false, $shadowRequestName
$exitCode = 1
try {
    Write-Host 'Exact-build verifier passed. Starting Black Plague with reconciliation shadow requested.'
    Write-Host 'Positional translation remains disabled; this mode is telemetry-only.'
    & $launcher '--launch-vr' $GamePath
    $exitCode = $LASTEXITCODE
}
finally {
    $shadowRequest.Dispose()
}

exit $exitCode
