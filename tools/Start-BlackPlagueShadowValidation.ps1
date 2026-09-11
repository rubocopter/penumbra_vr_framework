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
$logRoot = Join-Path $env:LOCALAPPDATA 'PenumbraVR\logs'

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

# --launch-vr delegates the actual game start to an already-running Steam
# process. The launcher can therefore exit before Steam creates penumbra.exe.
# Keep the per-session request object alive for the complete validation run so
# the body adapter cannot race the request during its one-time installation
# sample. Nothing is persisted after this helper exits.
$existingGame = @(Get-Process -Name 'penumbra' -ErrorAction SilentlyContinue)
if ($existingGame.Count -ne 0) {
    throw 'A penumbra.exe process is already running. Close it before starting shadow validation so activation can be proven from a fresh game process.'
}

$shadowRequestName = 'Local\PenumbraVR.BlackPlague.ReconciliationShadow'
$shadowRequest = New-Object System.Threading.Mutex -ArgumentList $false, $shadowRequestName
$gameProcess = $null
$exitCode = 1
try {
    Write-Host 'Exact-build verifier passed. Starting Black Plague with reconciliation shadow requested.'
    Write-Host 'Positional translation remains disabled; this mode is telemetry-only.'
    $launchStartedAt = Get-Date
    & $launcher '--launch-vr' $GamePath
    $launcherExitCode = $LASTEXITCODE
    if ($launcherExitCode -ne 0) {
        throw "Probe launcher failed with exit code $launcherExitCode."
    }

    $deadline = (Get-Date).AddSeconds(60)
    do {
        Start-Sleep -Milliseconds 250
        $gameProcess = Get-Process -Name 'penumbra' -ErrorAction SilentlyContinue |
            Select-Object -First 1
    } while ($null -eq $gameProcess -and (Get-Date) -lt $deadline)

    if ($null -eq $gameProcess) {
        throw 'Black Plague did not create penumbra.exe within 60 seconds. The shadow request was not validated.'
    }

    $probeLog = Join-Path $logRoot "black-plague-probe-$($gameProcess.Id).log"
    $activationDeadline = (Get-Date).AddSeconds(30)
    $activationLine = $null
    do {
        Start-Sleep -Milliseconds 100
        if (Test-Path -LiteralPath $probeLog -PathType Leaf) {
            $candidate = Get-Content -LiteralPath $probeLog |
                Select-String -Pattern 'Black Plague body adapter installed=.*body_reconciliation_shadow enabled=[01] source=(disabled|environment|mutex)' |
                Select-Object -Last 1
            if ($null -ne $candidate -and $candidate.Line.Length -ge 23) {
                $candidateTime = [DateTime]::ParseExact(
                    $candidate.Line.Substring(0, 23),
                    'yyyy-MM-dd HH:mm:ss.fff',
                    [Globalization.CultureInfo]::InvariantCulture)
                if ($candidateTime -ge $launchStartedAt.AddSeconds(-1)) {
                    $activationLine = $candidate.Line
                }
            }
        }
    } while ($null -eq $activationLine -and -not $gameProcess.HasExited -and (Get-Date) -lt $activationDeadline)

    if ($null -eq $activationLine) {
        throw "Black Plague PID $($gameProcess.Id) did not report body-adapter/shadow activation within 30 seconds. Check '$probeLog'."
    }
    if ($activationLine -notmatch 'Black Plague body adapter installed=1 .*body_reconciliation_shadow enabled=1 source=mutex') {
        throw "Black Plague PID $($gameProcess.Id) did not activate the requested shadow. Probe telemetry: $activationLine"
    }

    Write-Host "Black Plague detected (PID $($gameProcess.Id)); probe confirmed body_reconciliation_shadow enabled=1 source=mutex."
    Write-Host 'Shadow request will remain active until the game exits.'
    Write-Host 'Keep this window open during the validation run.'
    $gameProcess.WaitForExit()
    $exitCode = 0
}
finally {
    $shadowRequest.Dispose()
}

exit $exitCode
