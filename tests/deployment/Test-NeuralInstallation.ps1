$ErrorActionPreference='Stop'
$repo=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$install=Join-Path $repo 'tools/Install-PenumbraNeural.ps1'
$temp=Join-Path ([IO.Path]::GetTempPath()) ('PvrNeuralInstall-'+[guid]::NewGuid().ToString('N'))
function Assert($condition,[string]$message){if(-not $condition){throw $message}}
function Reject([scriptblock]$action,[string]$message){$failed=$false;try{& $action | Out-Null}catch{$failed=$true};Assert $failed $message}
function FakePe([string]$path,[System.UInt16]$machine=0x8664){
 $bytes=New-Object byte[] 256;$bytes[0]=0x4D;$bytes[1]=0x5A;$bytes[0x3C]=0x60;$bytes[0x60]=0x50;$bytes[0x61]=0x45
 [BitConverter]::GetBytes($machine).CopyTo($bytes,0x64);[IO.File]::WriteAllBytes($path,$bytes)
}
try{
 New-Item -ItemType Directory -Path $temp | Out-Null
 $game=Join-Path $temp 'Game';$payload=Join-Path $temp 'Package';$modelDir=Join-Path $temp 'Source'
 New-Item -ItemType Directory -Path $game,(Join-Path $payload 'neural'),$modelDir | Out-Null
 [IO.File]::WriteAllText((Join-Path $game 'Penumbra.exe'),'fixture game; never launched')
 FakePe (Join-Path $payload 'neural/PenumbraNeuralHost.exe');FakePe (Join-Path $payload 'neural/nvngx.dll_nr_bridge.dll')
 $model=Join-Path $modelDir 'nvngx_dlssnr.dll';FakePe $model
 $ini=Join-Path $game 'NeuralPost.ini';$state=Join-Path $game '.penumbravr-neural/state.json'
 [IO.File]::WriteAllBytes($ini,[Text.Encoding]::Unicode.GetBytes("[Neural]`r`nEnabled=0`r`n; custom configuration`r`n"))
 $before=(Get-FileHash -LiteralPath $ini).Hash
 $args=@{GamePath=$game;ModelPath=$model;PackageRoot=$payload}
 & $install @args -Plan | Out-Null
 Assert (-not(Test-Path $state)) 'Preflight created state'
 Assert ((Get-FileHash $ini).Hash -eq $before) 'Preflight edited INI'
 & $install @args | Out-Null
 Assert (Test-Path $state) 'No ownership receipt'
 Assert ((Get-FileHash (Join-Path $game 'PenumbraVR-Neural/NeuralRuntime/nvngx_dlssnr.dll')).Hash -eq (Get-FileHash $model).Hash) 'Model copy differs'
 Assert ([IO.File]::ReadAllText($ini).Contains('PenumbraVR-Neural')) 'INI does not configure imported runtime'
 Assert (-not [IO.File]::ReadAllText($ini).Contains('Enabled=1')) 'Installer enabled Neural without in-game choice'
 Assert ((Get-FileHash $model).Hash -eq (Get-FileHash (Join-Path $game 'PenumbraVR-Neural/NeuralRuntime/nvngx_dlssnr.dll')).Hash) 'Source model modified'
 & $install @args | Out-Null
 Assert ((Get-Content $state -Raw | ConvertFrom-Json).Files.Count -eq 4) 'Upgrade duplicated state'
 & $install -GamePath $game -Restore -Plan | Out-Null
 Assert (Test-Path $state) 'Removal preflight wrote files'
 & $install -GamePath $game -Restore | Out-Null
 Assert ((Get-FileHash $ini).Hash -eq $before) 'Removal did not restore exact original INI bytes'
 Assert (-not(Test-Path (Join-Path $game 'PenumbraVR-Neural'))) 'Owned payload directory remains'
 Assert (-not(Test-Path (Join-Path $game '.penumbravr-neural'))) 'Owned state directory remains'
 & $install -GamePath $game -Restore | Out-Null
 Remove-Item -LiteralPath $ini
 & $install @args | Out-Null
 $modelTarget=Join-Path $game 'PenumbraVR-Neural/NeuralRuntime/nvngx_dlssnr.dll'
 [IO.File]::WriteAllText($modelTarget,'user changed model')
 $receipt=(Get-FileHash $state).Hash
 Reject {& $install -GamePath $game -Restore} 'Removal accepted modified model'
 Assert ((Get-FileHash $state).Hash -eq $receipt) 'Rejected removal changed receipt'
 Copy-Item -LiteralPath $model -Destination $modelTarget -Force
 & $install -GamePath $game -Restore | Out-Null
 Assert (-not(Test-Path $ini)) 'Removal left generated INI'
 FakePe $model 0x14C
 Reject {& $install @args} 'Accepted x86 model'
 Assert (-not(Test-Path $state)) 'Rejected model created receipt'
 [IO.File]::WriteAllText($model,'not a PE')
 Reject {& $install @args} 'Accepted invalid model'
 FakePe $model
 New-Item -ItemType Directory -Path (Join-Path $game 'PenumbraVR-Neural') | Out-Null
 Reject {& $install @args} 'Overwrote unowned runtime folder'
 Remove-Item -LiteralPath (Join-Path $game 'PenumbraVR-Neural')
 Reject {& $install -GamePath $game -ModelPath (Join-Path $modelDir 'missing.dll') -PackageRoot $payload} 'Accepted missing model'
 Write-Output 'Neural installer: import/update/plan/exact restore/default-off/invalid x86 and missing model/ownership conflicts passed.'
}finally{
 $resolved=[IO.Path]::GetFullPath($temp);$prefix=[IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\')+'\'
 if($resolved.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase) -and (Split-Path -Leaf $resolved).StartsWith('PvrNeuralInstall-')){Remove-Item -LiteralPath $resolved -Recurse -Force}
}
