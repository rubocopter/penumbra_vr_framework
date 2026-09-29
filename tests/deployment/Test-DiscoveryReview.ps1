[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$BlackPlagueExe,
    [Parameter(Mandatory=$true)][string]$RetailAlut,
    [ValidateSet('All','Recovery','Prerequisites')][string]$Check='All'
)

$ErrorActionPreference='Stop'
$repo=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$discover=Join-Path $repo 'tools/Get-PenumbraInstallations.ps1'
$tempBase=[IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\')+'\'
$fixture=[IO.Path]::GetFullPath((Join-Path $tempBase ('PvrDiscoveryReview-'+[guid]::NewGuid().ToString('N'))))
if(-not $fixture.StartsWith($tempBase,[StringComparison]::OrdinalIgnoreCase)){throw 'Unsafe fixture root.'}
$failures=[Collections.Generic.List[string]]::new()
function Assert-Discovery([bool]$condition,[string]$message){
    if(-not $condition){$failures.Add($message);Write-Host "FAIL: $message"}else{Write-Host "PASS: $message"}
}
function Scan([string]$path){@(& $discover -SteamRoot (Join-Path $fixture 'NoSteam') -ManualPaths @($path))}
function Save-Journal($record,[string]$path){$record | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $path -Encoding UTF8}
try {
    New-Item -ItemType Directory -Path $fixture | Out-Null
    if($Check -in @('All','Recovery')){
        foreach($product in @('Overture','Black Plague')){
            $installRoot=Join-Path $fixture $product
            $redist=Join-Path $installRoot 'redist'
            $journal=if($product -eq 'Overture'){Join-Path $installRoot '.penumbravr-journal'}else{Join-Path $redist '.penumbravr-bp-journal'}
            New-Item -ItemType Directory -Path $redist,$journal -Force | Out-Null
            $paths=@(if($product -eq 'Overture'){@('redist/Penumbra.exe')}else{@(
                'Penumbra.exe','PenumbraVR_Penumbra_original.exe','alut.dll','PenumbraVR_alut_original.dll',
                'PenumbraVR.BlackPlague.Probe.dll','openvr_api.dll','vr','assets/rework/HAND_Low_C.jpg',
                'config/Espanol.lang','PenumbraVR_Espanol_original.lang','alsoft.ini','PenumbraVR.BlackPlague.install.json',
                'PenumbraVR.Requiem.Probe.dll','expansion01/config/Espanol_exp.lang','PenumbraVR_Espanol_exp_original.lang',
                'OpenAL32.dll','PenumbraVR_OpenAL_original.dll'
            )})
            $root=if($product -eq 'Overture'){$installRoot}else{$redist}
            $entries=@(for($i=0;$i -lt $paths.Count;$i++){
                $source=if($i -eq 0){$BlackPlagueExe}elseif($product -eq 'Black Plague' -and $i -eq 2){$RetailAlut}else{$null}
                $copy=Join-Path $journal "item-$i"
                if($source){Copy-Item -LiteralPath $source -Destination $copy}
                [pscustomobject]@{Target=Join-Path $root $paths[$i];Copy=$copy;Kind=$(if($source){'file'}else{'absent'});SnapshotHash=$(if($source){(Get-FileHash -LiteralPath $copy).Hash}else{$null});SnapshotFiles=@();SnapshotDirectory=$null;MissingParents=@()}
            })
            $record=[pscustomobject]@{Version=1;GameRoot=$root;Entries=$entries}
            $manifest=Join-Path $journal 'journal.json'
            Save-Journal $record $manifest
            $row=@(Scan $installRoot)
            Assert-Discovery ($row.Count -eq 1 -and $row[0].ManagedProduct -eq $product -and $row[0].Managed -and $row[0].NeedsRecovery -and $row[0].Status -eq 'recovery-required' -and -not $row[0].Installable -and -not $row[0].KnownBuild -and $row[0].Game -eq 'Unknown' -and $row[0].Path -eq (Join-Path $redist 'Penumbra.exe')) "$product journal-only missing EXE has a recovery owner without executable recognition"
            $direct=@(Scan $redist)
            Assert-Discovery ($direct.Count -eq 1 -and $direct[0].ManagedProduct -eq $product) "$product missing EXE is discoverable from manual redist"
            foreach($damage in @('version','root','target','copy','kind','hash','empty','json')){
                $bad=$record | ConvertTo-Json -Depth 10 | ConvertFrom-Json
                switch($damage){
                    'version'{$bad.Version=2}
                    'root'{$bad.GameRoot=Join-Path $fixture 'other-root'}
                    'target'{$bad.Entries[0].Target=Join-Path $fixture 'outside.exe'}
                    'copy'{$bad.Entries[0].Copy=Join-Path $fixture 'outside-copy'}
                    'kind'{$bad.Entries[0].Kind='untouched'}
                    'hash'{$bad.Entries[0].SnapshotHash=('0'*64)}
                    'empty'{$bad.Entries=@()}
                }
                if($damage -eq 'json'){[IO.File]::WriteAllText($manifest,'{invalid')}else{Save-Journal $bad $manifest}
                $row=@(Scan $installRoot)
                Assert-Discovery ($row.Count -eq 1 -and -not $row[0].ManagedProduct -and -not $row[0].Managed -and $row[0].NeedsRecovery -and -not $row[0].Installable) "$product rejects $damage journal ownership while blocking writes"
            }
            if($product -eq 'Black Plague'){
                foreach($count in @(12,15,17)){
                    $legacy=[pscustomobject]@{Version=1;GameRoot=$root;Entries=@($entries | Select-Object -First $count)}
                    Save-Journal $legacy $manifest
                    Assert-Discovery ((Scan $installRoot)[0].ManagedProduct -eq $product) "Black Plague accepts legacy $count-entry journal"
                }
                Import-Module (Join-Path $repo 'tools/PenumbraVrConfiguration.psm1') -Force
                $settings=@(Get-PvrConfigurationPaths -Game black_plague -RedistRoot $redist)+@(Get-PvrConfigurationPaths -Game requiem -RedistRoot $redist)
                $settingsEntries=@(for($i=0;$i -lt 4;$i++){
                    [pscustomobject]@{Target=$settings[$i];Copy=Join-Path $journal ('item-'+($i+17));Kind='untouched';SnapshotHash=$null;SnapshotDirectory=$null}
                })
                $current=[pscustomobject]@{Version=1;GameRoot=$root;Entries=@($entries)+$settingsEntries}
                Save-Journal $current $manifest
                Assert-Discovery ((Scan $installRoot)[0].ManagedProduct -eq $product) 'Black Plague accepts untouched only for final four known settings paths'
                $current.Entries[18].Target=Join-Path $fixture 'foreign-settings.cfg'
                Save-Journal $current $manifest
                Assert-Discovery (-not (Scan $installRoot)[0].ManagedProduct) 'Black Plague rejects foreign untouched settings path'
                $current.Entries[18].Target=$settings[1]
                [IO.File]::WriteAllText((Join-Path $journal 'item-18'),'unexpected copy')
                Save-Journal $current $manifest
                Assert-Discovery (-not (Scan $installRoot)[0].ManagedProduct) 'Black Plague rejects snapshot copy for untouched settings'
            }
            $directoryRecord=$record | ConvertTo-Json -Depth 10 | ConvertFrom-Json
            $index=if($product -eq 'Overture'){0}else{6}
            $directoryCopy=Join-Path $journal "item-$index"
            if(Test-Path -LiteralPath $directoryCopy){Remove-Item -LiteralPath $directoryCopy}
            New-Item -ItemType Directory -Path $directoryCopy | Out-Null
            [IO.File]::WriteAllText((Join-Path $directoryCopy 'actions.json'),'{}')
            $hash=(Get-FileHash -LiteralPath (Join-Path $directoryCopy 'actions.json')).Hash
            $directoryRecord.Entries[$index].Kind='directory'
            if($product -eq 'Overture'){
                $directoryRecord.Entries[$index].Target=Join-Path $redist 'vr'
                $directoryRecord.Entries[$index].SnapshotFiles=@('actions.json:'+$hash.ToLowerInvariant())
            }else{
                $directoryRecord.Entries[$index].SnapshotDirectory=[pscustomobject]@{files=@([ordered]@{path='actions.json';sha256=$hash});directories=@()}
            }
            Save-Journal $directoryRecord $manifest
            Assert-Discovery ((Scan $installRoot)[0].ManagedProduct -eq $product) "$product validates directory snapshot ownership"
            [IO.File]::WriteAllText((Join-Path $directoryCopy 'actions.json'),'damaged')
            Assert-Discovery (-not (Scan $installRoot)[0].ManagedProduct) "$product rejects damaged directory snapshot ownership"
            Remove-Item -LiteralPath $manifest
            Assert-Discovery (-not (Scan $installRoot)[0].ManagedProduct) "$product journal directory alone is not ownership evidence"
        }
    }
    if($Check -in @('All','Prerequisites')){
        $redist=Join-Path $fixture 'runtime/redist'
        New-Item -ItemType Directory -Path $redist -Force | Out-Null
        Copy-Item -LiteralPath $BlackPlagueExe -Destination (Join-Path $redist 'Penumbra.exe')
        & (Join-Path $PSScriptRoot 'New-GameContentFixture.ps1') -Root $redist -Games black_plague
        & (Join-Path $PSScriptRoot 'New-GameRuntimeFixture.ps1') -Root $redist -Game black_plague
        Copy-Item -LiteralPath $RetailAlut -Destination (Join-Path $redist 'alut.dll') -Force
        $row=(Scan $redist)[0]
        Assert-Discovery ($row.Installable -and $row.Status -eq 'available' -and @($row.Issues).Count -eq 0 -and -not (Test-Path (Join-Path $redist 'openvr_api.dll'))) 'Scan accepts pinned installer-provided OpenVR/OpenAL without SteamVR or target copies'
        foreach($dll in @('SDL.dll','msvcr71.dll','libpng12.dll','alut.dll')){
            $path=Join-Path $redist $dll
            $bytes=[IO.File]::ReadAllBytes($path)
            Remove-Item -LiteralPath $path
            $row=(Scan $redist)[0]
            Assert-Discovery (-not $row.Installable -and $row.Status -eq 'incomplete' -and @($row.Issues | Where-Object {$_ -like '*missing*' -and $_ -like '*Steam*'}).Count -gt 0) "Missing game library $dll blocks availability with remediation"
            [IO.File]::WriteAllBytes($path,$bytes)
        }
        $path=Join-Path $redist 'SDL.dll'
        $bytes=[IO.File]::ReadAllBytes($path)
        $pe=[BitConverter]::ToInt32($bytes,60)
        $bytes[$pe+4]=0x64;$bytes[$pe+5]=0x86
        [IO.File]::WriteAllBytes($path,$bytes)
        $row=(Scan $redist)[0]
        Assert-Discovery (-not $row.Installable -and $row.Status -eq 'incomplete' -and @($row.Issues | Where-Object {$_ -like '*SDL*invalid*'}).Count -gt 0) 'Wrong architecture game library blocks availability'
        [IO.File]::WriteAllText($path,'not a PE')
        $row=(Scan $redist)[0]
        Assert-Discovery (-not $row.Installable -and $row.Status -eq 'incomplete') 'Malformed game library blocks availability'
        Remove-Item -LiteralPath $path
        & (Join-Path $PSScriptRoot 'New-GameRuntimeFixture.ps1') -Root $redist -Game black_plague
        $steam=Join-Path $fixture 'Steam'
        $custom=Join-Path $steam 'steamapps/common/Custom BP folder with spaces'
        New-Item -ItemType Directory -Path (Split-Path -Parent $custom) -Force | Out-Null
        Move-Item -LiteralPath (Split-Path -Parent $redist) -Destination $custom
        [IO.File]::WriteAllText((Join-Path $steam 'steamapps/appmanifest_22120.acf'),'"AppState" { "appid" "22120" "installdir" "Custom BP folder with spaces" }')
        $found=@(& $discover -SteamRoot $steam -ManualPaths @((Join-Path $custom 'redist')))
        Assert-Discovery ($found.Count -eq 1 -and $found[0].Source -eq 'Steam manifest' -and $found[0].Game -eq 'Black Plague' -and $found[0].Installable) 'Steam manifest custom folder discovery and manual deduplication remain unchanged'
        $redist=Join-Path $custom 'redist'
        $package=Join-Path $fixture 'package'
        New-Item -ItemType Directory -Path (Join-Path $package 'tools') -Force | Out-Null
        foreach($name in @('Get-PenumbraInstallations.ps1','Get-PenumbraBuildInfo.ps1','PenumbraVrPrerequisites.psm1','PenumbraVrConfiguration.psm1')){
            Copy-Item -LiteralPath (Join-Path $repo "tools/$name") -Destination (Join-Path $package "tools/$name")
        }
        Copy-Item -LiteralPath (Join-Path $repo 'assets/deployment') -Destination (Join-Path $package 'assets/deployment') -Recurse -Force
        Copy-Item -LiteralPath (Join-Path $repo 'assets/settings') -Destination (Join-Path $package 'assets/settings') -Recurse -Force
        $prerequisites=Get-Content -LiteralPath (Join-Path $repo 'assets/deployment/prerequisites.json') -Raw | ConvertFrom-Json
        foreach($library in $prerequisites.bundledLibraries){
            $source=Join-Path $repo $library.source
            if(-not(Test-Path -LiteralPath $source)){$source=Join-Path $repo ('products/overture/build/package/Release/PenumbraVR/'+$library.path)}
            $destination=Join-Path $package ('products/overture/'+$library.path)
            New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
            Copy-Item -LiteralPath $source -Destination $destination
        }
        $discover=Join-Path $package 'tools/Get-PenumbraInstallations.ps1'
        Assert-Discovery ((Scan $redist)[0].Installable) 'Packaged discovery resolves pinned installer payload source roots'
        $loader=Join-Path $package 'products/overture/openvr_api.dll'
        $loaderBytes=[IO.File]::ReadAllBytes($loader)
        Remove-Item -LiteralPath $loader
        $row=(Scan $redist)[0]
        Assert-Discovery (-not $row.Installable -and $row.Status -eq 'incomplete' -and @($row.Issues | Where-Object {$_ -like '*OpenVR*missing*'}).Count -eq 1) 'Package missing pinned bundled loader blocks availability'
        [IO.File]::WriteAllText($loader,'untrusted bundled loader')
        Assert-Discovery (-not (Scan $redist)[0].Installable) 'Unpinned bundled loader cannot satisfy prerequisites'
        [IO.File]::WriteAllBytes($loader,$loaderBytes)
        $oRoot=Join-Path $fixture 'managed-overture'
        $oRedist=Join-Path $oRoot 'redist'
        New-Item -ItemType Directory -Path $oRedist,(Join-Path $oRoot '.penumbravr') -Force | Out-Null
        Copy-Item -LiteralPath (Join-Path $repo 'products/overture/build/package/Release/PenumbraVR/Penumbra_vr.exe') -Destination (Join-Path $oRedist 'Penumbra.exe')
        & (Join-Path $PSScriptRoot 'New-GameContentFixture.ps1') -Root $oRedist -Games overture
        & (Join-Path $PSScriptRoot 'New-GameRuntimeFixture.ps1') -Root $oRedist -Game overture
        @{Version=1;RedistRoot=$oRedist;Files=@(@{Path='Penumbra.exe';DeployedHash=(Get-FileHash -LiteralPath (Join-Path $oRedist 'Penumbra.exe')).Hash})} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $oRoot '.penumbravr/deploy-state.json') -Encoding UTF8
        $row=(Scan $oRoot)[0]
        Assert-Discovery ($row.Game -eq 'Overture' -and $row.Installable -and $row.Status -eq 'available') 'Managed Overture accepts pinned package CRT without target CRT copies'
        Remove-Item -LiteralPath (Join-Path $package 'products/overture/msvcp140.dll')
        $row=(Scan $oRoot)[0]
        Assert-Discovery (-not $row.Installable -and $row.Status -eq 'incomplete') 'Managed Overture missing package CRT blocks availability'
    }
    if($failures.Count){throw ($failures.Count.ToString()+' discovery regression(s) failed: '+($failures -join '; '))}
    Write-Host 'Discovery review IMPORTANT7/8 fixture checks passed.'
} finally {
    if(Test-Path -LiteralPath $fixture -PathType Container){Remove-Item -LiteralPath $fixture -Recurse -Force}
}
