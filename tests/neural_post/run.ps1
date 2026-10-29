[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$HostPath,
    [Parameter(Mandatory)][string]$RuntimeDirectory,
    [string]$ProbePath = (Join-Path $PSScriptRoot '../../build/bin/neural_post/Release/pvr_neural_post_roundtrip.exe'),
    [switch]$WithModel
)
$ErrorActionPreference = 'Stop'
$HostPath = (Resolve-Path -LiteralPath $HostPath).Path
$RuntimeDirectory = (Resolve-Path -LiteralPath $RuntimeDirectory).Path
$ProbePath = (Resolve-Path -LiteralPath $ProbePath).Path
$scratch = Join-Path ([IO.Path]::GetTempPath()) ('PenumbraNeuralTests-' + [guid]::NewGuid())
New-Item -ItemType Directory -Path $scratch | Out-Null
Push-Location -LiteralPath $scratch
try {
    $cases = @(
        @{Mode='missinghost'; Diagnostic=3; Height=256; Width=256; Timeout=250},
        @{Mode='missingmodel'; Diagnostic=0; Height=256; Width=256; Timeout=250},
        @{Mode='identity'; Diagnostic=3; Height=256; Width=256; Timeout=250},
        @{Mode='detail'; Diagnostic=3; Height=64; Width=64; Timeout=250},
        @{Mode='detail_srgb'; Diagnostic=3; Height=64; Width=64; Timeout=250},
        @{Mode='detail_edit'; Diagnostic=4; Height=64; Width=64; Timeout=250},
        @{Mode='detail_srgb_edit'; Diagnostic=4; Height=64; Width=64; Timeout=250},
        @{Mode='aspect'; Diagnostic=3; Height=896; Width=0; Timeout=250},
        @{Mode='live'; Diagnostic=4; Height=256; Width=256; Timeout=250},
        @{Mode='glfailure'; Diagnostic=4; Height=256; Width=256; Timeout=250},
        @{Mode='timeout'; Diagnostic=3; Height=256; Width=256; Timeout=50},
        @{Mode='childfault'; Diagnostic=3; Height=256; Width=256; Timeout=50},
        @{Mode='watchdog'; Diagnostic=3; Height=256; Width=256; Timeout=50}
    )
    if ($WithModel) {
        $cases += @{Mode='live'; Diagnostic=0; Height=256; Width=256; Timeout=250}
        $cases += @{Mode='zero'; Diagnostic=0; Height=256; Width=256; Timeout=250}
    }
    foreach ($case in $cases) {
        $ini = Join-Path $scratch ("NeuralPost-$($case.Mode)-$($case.Diagnostic).ini")
        @"
[Neural]
Enabled=1
HostPath=$(if ($case.Mode -eq 'missinghost') {Join-Path $scratch 'absent-helper.exe'} else {$HostPath})
RuntimeDirectory=$(if ($case.Mode -eq 'missingmodel') {Join-Path $scratch 'absent-runtime'} else {$RuntimeDirectory})
DiagnosticMode=$($case.Diagnostic)
InternalWidth=$($case.Width)
InternalHeight=$($case.Height)
TimeoutMs=$($case.Timeout)
ScalePercent=$(if ($case.Diagnostic -eq 0) {25} else {100})
IntensityPercent=$(if ($case.Mode -eq 'zero') {0} else {60})
Passes=1
DiagnosticDelayMs=$(if ($case.Mode -in @('watchdog', 'zero')) {2500} elseif ($case.Mode -eq 'timeout') {100} else {0})
DiagnosticDelayCount=$(if ($case.Mode -in @('timeout', 'watchdog')) {1} else {0})
DiagnosticExitOnRequest3=$(if ($case.Mode -in @('childfault', 'zero')) {1} else {0})
DiagnosticWarmupDelayMs=$(if ($case.Mode -eq 'live') {100} else {0})
"@ | Set-Content -LiteralPath $ini -Encoding Unicode
        & $ProbePath $ini $case.Mode
        if ($LASTEXITCODE -ne 0) { throw "Neural probe failed: $($case.Mode), diagnostic $($case.Diagnostic)" }
    }
} finally {
    Pop-Location
    # Only delete this invocation's explicitly checked temporary test directory.
    $resolvedScratch = [IO.Path]::GetFullPath($scratch)
    $tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
    if ($resolvedScratch.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase) -and
        (Split-Path -Leaf $resolvedScratch).StartsWith('PenumbraNeuralTests-')) {
        # The game path retires GPU workers asynchronously. Its last child can
        # briefly hold this diagnostic working directory during NGX shutdown.
        for ($attempt = 0; $attempt -lt 40; ++$attempt) {
            try {
                Remove-Item -LiteralPath $resolvedScratch -Recurse -Force
                break
            } catch {
                if ($attempt -eq 39) { throw }
                Start-Sleep -Milliseconds 50
            }
        }
    }
}
