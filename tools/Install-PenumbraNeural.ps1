[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$GamePath,
    [string]$ModelPath,
    [string]$PackageRoot=(Split-Path -Parent $PSScriptRoot),
    [switch]$Restore,
    [switch]$Plan
)
$ErrorActionPreference='Stop'
if($Restore -and $ModelPath){throw 'Choose import or removal.'}
$root=[IO.Path]::GetFullPath($GamePath)
if(Test-Path -LiteralPath $root -PathType Leaf){$root=Split-Path -Parent $root}
if(Test-Path -LiteralPath (Join-Path $root 'redist') -PathType Container){$root=Join-Path $root 'redist'}
if(-not(Test-Path -LiteralPath $root -PathType Container)){throw 'Game directory not found.'}
if(-not((Test-Path -LiteralPath (Join-Path $root 'Penumbra.exe')) -or (Test-Path -LiteralPath (Join-Path $root 'Requiem.exe')) -or (Test-Path -LiteralPath (Join-Path $root 'Penumbra_vr.exe')))){throw 'Select a Penumbra game directory.'}
function Safe([string]$relative){
 if([IO.Path]::IsPathRooted($relative) -or $relative -match '(^|[\\/])\.\.([\\/]|$)'){throw 'Unsafe Neural path.'}
 $target=[IO.Path]::GetFullPath((Join-Path $root $relative))
 if(-not $target.StartsWith($root.TrimEnd('\')+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Neural path escapes game directory.'}
 for($cursor=$target;$cursor;$cursor=Split-Path -Parent $cursor){
  $item=Get-Item -LiteralPath $cursor -Force -ErrorAction SilentlyContinue
  if($item -and ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Neural path traverses a reparse point.'}
 }
 $target
}
function Hash([string]$path){if(Test-Path -LiteralPath $path -PathType Leaf){(Get-FileHash -LiteralPath $path).Hash}else{$null}}
function Assert-X64([string]$path){
 $stream=[IO.File]::OpenRead($path)
 try{
  $reader=[IO.BinaryReader]::new($stream)
  if($stream.Length -lt 128 -or $reader.ReadUInt16() -ne 0x5A4D){throw 'Not a Windows DLL/EXE.'}
  $stream.Position=0x3C;$pe=$reader.ReadUInt32()
  if($pe -gt $stream.Length-24){throw 'Invalid PE header.'}
  $stream.Position=$pe
  if($reader.ReadUInt32() -ne 0x4550 -or $reader.ReadUInt16() -ne 0x8664){throw 'Neural requires an x64 model/helper.'}
 }finally{$stream.Dispose()}
}
$names=@('NeuralPost.ini','PenumbraVR-Neural/PenumbraNeuralHost.exe',
 'PenumbraVR-Neural/NeuralRuntime/nvngx.dll_nr_bridge.dll','PenumbraVR-Neural/NeuralRuntime/nvngx_dlssnr.dll')
$statePath=Safe '.penumbravr-neural/state.json'
$state=$null
if(Test-Path -LiteralPath $statePath -PathType Leaf){
 $state=Get-Content -LiteralPath $statePath -Raw | ConvertFrom-Json
 if($state.Version -ne 1 -or @($state.Files).Count -ne 4 -or @($state.Files.Path | Select-Object -Unique).Count -ne 4){throw 'Invalid Neural ownership receipt.'}
 foreach($entry in $state.Files){
  if($entry.Path -notin $names -or (Hash (Safe $entry.Path)) -cne $entry.Hash){throw "Neural file changed or missing: $($entry.Path). Re-import/restore from your backup before removal."}
  if($entry.Backup){
   if($entry.Path -ne 'NeuralPost.ini' -or $entry.Backup -ne '.penumbravr-neural/original.ini' -or (Hash (Safe $entry.Backup)) -cne $entry.OriginalHash){throw 'Neural INI backup is missing or changed.'}
  }
 }
}elseif(Test-Path -LiteralPath (Safe '.penumbravr-neural')){throw 'Unrecognized Neural state directory; no files changed.'}
if($Restore -and -not $state){return}
if(-not $Restore){
 if(-not $ModelPath -or [IO.Path]::GetFileName($ModelPath) -ine 'nvngx_dlssnr.dll'){throw 'Select your local nvngx_dlssnr.dll. See https://github.com/lowpoplabs/nr-bridge#prerequisites'}
 $model=(Resolve-Path -LiteralPath $ModelPath).Path;Assert-X64 $model
 $payload=Join-Path $PackageRoot 'neural'
 if(-not(Test-Path -LiteralPath $payload)){ $payload=Join-Path $PackageRoot 'build/neural-host/Release' }
 foreach($name in @('PenumbraNeuralHost.exe','nvngx.dll_nr_bridge.dll')){Assert-X64 (Join-Path $payload $name)}
 if(-not $state -and (Test-Path -LiteralPath (Safe 'PenumbraVR-Neural'))){throw 'Unowned Neural directory exists; no files changed.'}
 if(Test-Path -LiteralPath (Safe 'NeuralPost.ini') -PathType Container){throw 'NeuralPost.ini is a directory.'}
}
$rootPrefix=$root.TrimEnd('\')+'\'
foreach($process in Get-Process -Name Penumbra,Penumbra_vr,Requiem,PenumbraNeuralHost -ErrorAction SilentlyContinue){
 try{$processPath=$process.Path}catch{throw 'Cannot verify running game; close it first.'}
 if($processPath -and $processPath.StartsWith($rootPrefix,[StringComparison]::OrdinalIgnoreCase)){throw 'Close the game and its Neural companion before changing Neural files.'}
}
if($Plan){[pscustomobject]@{Operation=$(if($Restore){'remove-neural'}else{'import-experimental-neural'});Directory=$root;ModelSupplied=[bool]$ModelPath;EnabledByDefault=$false};return}
$stage=Safe ('.penumbravr-neural-tmp-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $stage | Out-Null
$saved=@();$written=@();$stateExisted=Test-Path -LiteralPath $statePath
try{
 foreach($relative in $names+@('.penumbravr-neural/state.json','.penumbravr-neural/original.ini')){
  $path=Safe $relative
  if(Test-Path -LiteralPath $path -PathType Leaf){$backup=Join-Path $stage ($saved.Count.ToString()+'.bak');Copy-Item -LiteralPath $path -Destination $backup;$saved+=@{Path=$path;Backup=$backup}}
 }
 if($Restore){
  foreach($entry in $state.Files){
   $target=Safe $entry.Path
   if($entry.Backup){Copy-Item -LiteralPath (Safe $entry.Backup) -Destination $target -Force}
   else{Remove-Item -LiteralPath $target -Force}
  }
  Remove-Item -LiteralPath $statePath -Force
  if(Test-Path -LiteralPath (Safe '.penumbravr-neural/original.ini')){Remove-Item -LiteralPath (Safe '.penumbravr-neural/original.ini') -Force}
 }else{
  $stateRoot=Safe '.penumbravr-neural';New-Item -ItemType Directory -Path $stateRoot -Force | Out-Null
  $originalHash=$null
  if($state){$originalHash=($state.Files | Where-Object {$_.Path -eq 'NeuralPost.ini'}).OriginalHash}
  elseif(Test-Path -LiteralPath (Safe 'NeuralPost.ini')){
   $originalHash=Hash (Safe 'NeuralPost.ini')
   Copy-Item -LiteralPath (Safe 'NeuralPost.ini') -Destination (Safe '.penumbravr-neural/original.ini')
  }
  $sources=@('',(Join-Path $payload 'PenumbraNeuralHost.exe'),(Join-Path $payload 'nvngx.dll_nr_bridge.dll'),$model)
  $ini=Join-Path $stage 'NeuralPost.ini'
  $hostPath=Safe $names[1];$runtime=Safe 'PenumbraVR-Neural/NeuralRuntime'
  [IO.File]::WriteAllText($ini,"[Neural]`r`nHostPath=$hostPath`r`nRuntimeDirectory=$runtime`r`nTimeoutMs=20`r`n",[Text.Encoding]::Unicode)
  $sources[0]=$ini;$files=@()
  for($i=0;$i -lt $names.Count;$i++){
   $target=Safe $names[$i];New-Item -ItemType Directory -Path (Split-Path -Parent $target) -Force | Out-Null
   $staged=Join-Path $stage ('new-'+$i);Copy-Item -LiteralPath $sources[$i] -Destination $staged
   $expected=Hash $sources[$i]
   if((Hash $staged) -cne $expected){throw 'Neural staged copy hash mismatch.'}
   Copy-Item -LiteralPath $staged -Destination $target -Force;$written+=$target
   $files+=[pscustomobject]@{Path=$names[$i];Hash=$expected;Backup=$(if($i -eq 0 -and $originalHash){'.penumbravr-neural/original.ini'}else{$null});OriginalHash=$(if($i -eq 0){$originalHash}else{$null})}
  }
  $newState=[pscustomobject]@{Version=1;Experimental=$true;Files=$files}
  $newState | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $stage 'state.json') -Encoding UTF8
  Copy-Item -LiteralPath (Join-Path $stage 'state.json') -Destination $statePath -Force
 }
}catch{
 foreach($path in $written){if(-not @($saved | Where-Object {$_.Path -eq $path}).Count -and (Test-Path -LiteralPath $path)){Remove-Item -LiteralPath $path -Force}}
 foreach($entry in $saved){New-Item -ItemType Directory -Path (Split-Path -Parent $entry.Path) -Force | Out-Null;Copy-Item -LiteralPath $entry.Backup -Destination $entry.Path -Force}
 if(-not $stateExisted -and (Test-Path -LiteralPath $statePath)){Remove-Item -LiteralPath $statePath -Force}
 if(-not $stateExisted -and (Test-Path -LiteralPath (Safe '.penumbravr-neural/original.ini'))){Remove-Item -LiteralPath (Safe '.penumbravr-neural/original.ini') -Force}
 throw
}finally{
 # Only this invocation's verified private stage, never a supplied directory.
 if($stage.StartsWith($rootPrefix,[StringComparison]::OrdinalIgnoreCase) -and (Split-Path -Leaf $stage).StartsWith('.penumbravr-neural-tmp-')){Remove-Item -LiteralPath $stage -Recurse -Force}
 foreach($relative in @('PenumbraVR-Neural/NeuralRuntime','PenumbraVR-Neural','.penumbravr-neural')){
  $directory=Safe $relative
  if((Test-Path -LiteralPath $directory -PathType Container) -and -not @(Get-ChildItem -LiteralPath $directory -Force).Count){Remove-Item -LiteralPath $directory}
 }
}
Write-Output $(if($Restore){'Experimental Neural removed; previous configuration restored.'}else{'Experimental Neural imported. Enable it in VR Settings > Neural. Preferences remain independent per game.'})
