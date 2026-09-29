[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Import-Module (Join-Path $repo 'tools/PenumbraVrPrerequisites.psm1') -Force
$temp=Join-Path ([IO.Path]::GetTempPath()) ('PvrPrerequisites-'+[guid]::NewGuid().ToString('N'))
function Assert($condition,[string]$message) { if (-not $condition) { throw $message } }
function Write-Pe([string]$path,[uint16]$machine=0x14c,[string]$ImportName) {
    $bytes=New-Object byte[] 512
    $bytes[0]=0x4d; $bytes[1]=0x5a
    [BitConverter]::GetBytes([uint32]128).CopyTo($bytes,60)
    $bytes[128]=0x50; $bytes[129]=0x45
    [BitConverter]::GetBytes($machine).CopyTo($bytes,132)
    [BitConverter]::GetBytes([uint16]224).CopyTo($bytes,148)
    [BitConverter]::GetBytes([uint16]0x10b).CopyTo($bytes,152)
    if($ImportName){
        [BitConverter]::GetBytes([uint32]512).CopyTo($bytes,212)
        [BitConverter]::GetBytes([uint32]400).CopyTo($bytes,256)
        [BitConverter]::GetBytes([uint32]40).CopyTo($bytes,260)
        [BitConverter]::GetBytes([uint32]480).CopyTo($bytes,412)
        [Text.Encoding]::ASCII.GetBytes($ImportName).CopyTo($bytes,480)
    }
    New-Item -ItemType Directory -Path (Split-Path -Parent $path) -Force | Out-Null
    [IO.File]::WriteAllBytes($path,$bytes)
}
try {
    New-Item -ItemType Directory -Path $temp | Out-Null
    $x86=Join-Path $temp 'x86.dll'; $x64=Join-Path $temp 'x64.dll'
    Write-Pe $x86; Write-Pe $x64 0x8664
    Assert ((Get-PvrPeInfo $x86).Architecture -eq 'x86') 'x86 detection failed'
    Assert ((Get-PvrPeInfo $x64).Architecture -eq 'x64') 'x64 detection failed'
    [IO.File]::WriteAllBytes((Join-Path $temp 'bad.dll'),[byte[]]@(1,2,3))
    $bad=$false; try { Get-PvrPeInfo (Join-Path $temp 'bad.dll') | Out-Null } catch { $bad=$true }
    Assert $bad 'Malformed PE accepted'
    $empty=@(Test-PvrGameContent -Game requiem -RedistRoot $temp)
    Assert (@($empty | Where-Object {$_.Status -ne 'present'}).Count -gt 0) 'Empty expansion accepted'
    $manifest=Get-Content (Join-Path $repo 'assets/deployment/prerequisites.json') -Raw | ConvertFrom-Json
    foreach ($dll in $manifest.gameLibraries) { Write-Pe (Join-Path $temp $dll.path) }
    $checks=@(Get-PvrPrerequisites -Game black_plague -RedistRoot $temp -SkipSteamVr)
    Assert (@($checks | Where-Object {$_.Name -eq 'OpenAL Soft' -and $_.Status -eq 'missing'}).Count -eq 1) 'Absent app-local OpenAL hidden by global runtime'
    Write-Pe (Join-Path $temp 'SDL.dll') 0x14c 'fixture.dll'
    $checks=@(Get-PvrPrerequisites -Game black_plague -RedistRoot $temp -SkipSteamVr)
    Assert (@($checks | Where-Object {$_.Name -like '*fixture.dll*' -and $_.Status -eq 'missing'}).Count -gt 0) 'Transitive import closure omitted'
    Write-Pe (Join-Path $temp 'fixture.dll') 0x8664
    $checks=@(Get-PvrPrerequisites -Game black_plague -RedistRoot $temp -SkipSteamVr)
    Assert (@($checks | Where-Object {$_.Name -like '*fixture.dll*' -and $_.Status -eq 'invalid'}).Count -gt 0) 'Transitive import architecture omitted'
    Write-Pe (Join-Path $temp 'SDL.dll')
    Remove-Item (Join-Path $temp 'libpng12.dll')
    $checks=@(Get-PvrPrerequisites -Game black_plague -RedistRoot $temp -SkipSteamVr)
    Assert (@($checks | Where-Object {$_.Name -eq 'PNG decoder' -and $_.Status -eq 'missing'}).Count -eq 1) 'Dynamic PNG dependency omitted'
    Write-Pe (Join-Path $temp 'SDL.dll') 0x8664
    $checks=@(Get-PvrPrerequisites -Game black_plague -RedistRoot $temp -SkipSteamVr)
    Assert (@($checks | Where-Object {$_.Name -eq 'SDL' -and $_.Status -eq 'invalid'}).Count -eq 1) 'Wrong architecture accepted'
    $checks=@(Get-PvrPrerequisites -Game black_plague -RedistRoot $temp -OpenVrPathsPath (Join-Path $temp 'absent.vrpath'))
    Assert (@($checks | Where-Object {$_.Name -eq 'SteamVR' -and $_.Status -eq 'missing'}).Count -eq 1) 'Missing runtime hidden'
    Assert (-not @($checks | Where-Object {$_.Name -match 'TIFF|FLTK|Theora|OpenXR'}).Count) 'Unnecessary dependency required'
    Write-Host 'Release prerequisites: architecture, malformed files, expansion data, OpenAL, dynamic codecs and missing runtime passed.'
} finally { if (Test-Path $temp) { Remove-Item -LiteralPath $temp -Recurse -Force } }
