$ErrorActionPreference='Stop'
$script:Prerequisites=Get-Content -LiteralPath (Join-Path (Split-Path -Parent $PSScriptRoot) 'assets/deployment/prerequisites.json') -Raw | ConvertFrom-Json
if ($script:Prerequisites.schemaVersion -ne 1) { throw 'Unsupported prerequisites manifest.' }

function Get-PvrPeInfo {
    [CmdletBinding()]param([Parameter(Mandatory=$true,Position=0)][string]$Path)
    if((Get-Item -LiteralPath $Path).Length -gt 134217728){throw 'PE exceeds the 128 MiB inspection limit.'}
    $b=[IO.File]::ReadAllBytes($Path)
    function U16([int64]$offset) { if ($offset -lt 0 -or $offset+2 -gt $b.Length) { throw 'Truncated PE.' }; [BitConverter]::ToUInt16($b,[int]$offset) }
    function U32([int64]$offset) { if ($offset -lt 0 -or $offset+4 -gt $b.Length) { throw 'Truncated PE.' }; [BitConverter]::ToUInt32($b,[int]$offset) }
    if ((U16 0) -ne 0x5a4d) { throw 'Missing MZ header.' }
    $pe=U32 60
    if ((U32 $pe) -ne 0x4550) { throw 'Missing PE signature.' }
    $machine=U16 ($pe+4); $n=U16 ($pe+6); $optSize=U16 ($pe+20); $opt=$pe+24
    $magic=U16 $opt
    if ($magic -notin @(0x10b,0x20b) -or $optSize -lt 96 -or $opt+$optSize -gt $b.Length -or $n -gt 96) { throw 'Invalid PE optional header.' }
    $arch=switch($machine){0x14c {'x86'} 0x8664 {'x64'} default {'unknown'}}
    $dir=if ($magic -eq 0x10b) {96} else {112}
    $sections=@()
    for($i=0;$i -lt $n;$i++) {
        $s=$opt+$optSize+$i*40
        $sections+=@{Rva=(U32 ($s+12));Size=(U32 ($s+16));Offset=(U32 ($s+20))}
    }
    function Offset([uint32]$rva) {
        if ($rva -lt (U32 ($opt+60)) -and $rva -lt $b.Length) { return [int64]$rva }
        foreach($s in $sections) { if ($rva -ge $s.Rva -and ([int64]$rva-$s.Rva) -lt $s.Size) { $o=[int64]$s.Offset+$rva-$s.Rva; if ($o -lt $b.Length) { return $o } } }
        throw 'PE RVA outside file.'
    }
    function Name([uint32]$rva) {
        $o=Offset $rva; $end=$o
        while($end -lt $b.Length -and $end-$o -lt 260 -and $b[$end] -ne 0) { $end++ }
        if ($end -ge $b.Length -or $end-$o -ge 260) { throw 'Invalid PE import name.' }
        $name=[Text.Encoding]::ASCII.GetString($b,[int]$o,[int]($end-$o))
        if ($name -notmatch '^[a-zA-Z0-9_.-]+\.dll$') { throw 'Unsafe PE import name.' }
        $name
    }
    $imports=@(); $delays=@()
    foreach($entry in @(@(1,20,12),@(13,32,4))) {
        if ($optSize -lt $dir+($entry[0]+1)*8) { continue }
        $rva=U32 ($opt+$dir+$entry[0]*8); $size=U32 ($opt+$dir+$entry[0]*8+4)
        if (-not $rva) { continue }
        if ($size -gt 1048576 -or $size -lt $entry[1]) { throw 'Invalid PE import directory.' }
        $start=Offset $rva
        for($i=0;$i -lt [Math]::Min(1024,[Math]::Floor($size/$entry[1]));$i++) {
            $o=$start+$i*$entry[1]; $nameRva=U32 ($o+$entry[2])
            if (-not $nameRva) { break }
            if ($entry[0] -eq 13 -and (U32 $o) -ne 1) { throw 'Unsupported delay import address format.' }
            $name=Name $nameRva
            if ($entry[0] -eq 1) { $imports+=$name } else { $delays+=$name }
        }
    }
    [pscustomobject]@{Architecture=$arch;Imports=@($imports | Sort-Object -Unique);DelayImports=@($delays | Sort-Object -Unique)}
}

function Read-PvrConfig {
    param([string]$Path)
    if ((Get-Item -LiteralPath $Path).Length -gt 4194304) { throw 'Configuration exceeds 4 MiB.' }
    $settings=[Xml.XmlReaderSettings]::new(); $settings.DtdProcessing='Prohibit'; $settings.XmlResolver=$null
    $text=[IO.File]::ReadAllText($Path) -replace '^\s*<\?xml[^?]*\?>',''
    $reader=[Xml.XmlReader]::Create([IO.StringReader]::new('<PvrRoot>'+$text+'</PvrRoot>'),$settings)
    try { $doc=[Xml.XmlDocument]::new(); $doc.PreserveWhitespace=$true; $doc.XmlResolver=$null; $doc.Load($reader); $doc } finally { $reader.Dispose() }
}
function New-PvrCheck([string]$name,[string]$status,[string]$detail,[string]$remediation='Verify the game files in Steam, then retry.') {
    [pscustomobject]@{Name=$name;Required=$true;Status=$status;Detail=$detail;Remediation=$remediation}
}
function Test-PvrGameContent {
    [CmdletBinding()]param([ValidateSet('overture','black_plague','requiem')][string]$Game,[string]$RedistRoot)
    foreach($relative in $script:Prerequisites.gameFiles.$Game) {
        $path=Join-Path $RedistRoot $relative
        $status=if(Test-Path -LiteralPath $path -PathType Leaf){'present'}else{'missing'}
        if ($status -eq 'present' -and $relative.EndsWith('.cfg')) {
            try {
                $doc=Read-PvrConfig $path
                if ($relative.EndsWith('default_settings.cfg')) {
                    $map=$doc.SelectSingleNode('/PvrRoot/Map')
                    if (-not $map -or -not $map.GetAttribute('File')) { throw 'Default start map is not declared.' }
                    $mapName=$map.GetAttribute('File')
                    if ([IO.Path]::IsPathRooted($mapName) -or $mapName -match '(^|[\\/])\.\.([\\/]|$)') { throw 'Invalid start map path.' }
                    $maps=if($Game -eq 'requiem'){'expansion01/maps'}else{'maps'}
                    if (-not (Test-Path -LiteralPath (Join-Path $RedistRoot (Join-Path $maps $mapName)) -PathType Leaf)) { throw 'Declared start map is missing.' }
                }
            } catch { $status='invalid' }
        }
        New-PvrCheck $relative $status $path
    }
}
function Get-PvrPrerequisites {
    [CmdletBinding()]param([ValidateSet('overture','black_plague','requiem')][string]$Game,[string]$RedistRoot,[string]$PackageRoot,[string]$OpenVrPathsPath,[switch]$SkipSteamVr,[switch]$ForInstall)
    $resolved=@{}
    foreach($library in @($script:Prerequisites.gameLibraries)+@($script:Prerequisites.bundledLibraries)) {
        if ($library.games -and $Game -notin $library.games) { continue }
        $path=Join-Path $RedistRoot $library.path; $status='missing'
        if (Test-Path -LiteralPath $path -PathType Leaf) {
            try {
                $info=Get-PvrPeInfo $path
                $status=if($info.Architecture -eq 'x86'){'present'}else{'invalid'}
                if ($library.sha256 -and (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ine $library.sha256) { $status='invalid' }
            } catch { $status='invalid' }
        }
        if ($ForInstall -and $library.source -and $PackageRoot) {
            $sources=@($library.source,('products/overture/'+$library.path),('products/black_plague/'+$library.source),('products/overture/build/package/Release/PenumbraVR/'+$library.path))
            foreach($relativeSource in $sources) {
            $source=Join-Path $PackageRoot $relativeSource
            $validSource=$false
            if(Test-Path -LiteralPath $source -PathType Leaf){
                try{$validSource=(Get-PvrPeInfo $source).Architecture -eq 'x86' -and (-not $library.sha256 -or (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -ieq $library.sha256)}catch{}
            }
            if ($validSource) {
                # Ownership/conflict policy is enforced by the transaction, not relaxed here.
                $status='provided'
                $path=$source
                break
            }
            }
        }
        $remedy=if($library.source){'Repair using the complete Penumbra VR installer package.'}else{'Verify the game files in Steam to restore its x86 libraries.'}
        New-PvrCheck $library.name $status $library.path $remedy
        if($status -in @('present','provided')){$resolved[$library.path.ToLowerInvariant()]=$path}
    }
    # Inspect direct and delay imports recursively without loading code or
    # searching PATH/global third-party DLL installations. Dynamic codecs are
    # listed separately in the manifest; OS APIs stay owned by Windows.
    $exeName=if($Game -eq 'requiem'){'Requiem.exe'}else{'Penumbra.exe'}
    $exe=Join-Path $RedistRoot $exeName
    if($ForInstall -and $Game -eq 'black_plague' -and (Test-Path -LiteralPath (Join-Path $RedistRoot 'PenumbraVR_Penumbra_original.exe') -PathType Leaf)){$exe=Join-Path $RedistRoot 'PenumbraVR_Penumbra_original.exe'}
    if($ForInstall -and $Game -eq 'overture' -and $PackageRoot){
        foreach($relative in @('products/overture/Penumbra_vr.exe','products/overture/build/bin/Release/Penumbra_vr.exe')){if(Test-Path -LiteralPath (Join-Path $PackageRoot $relative)){$exe=Join-Path $PackageRoot $relative;break}}
    }
    if(Test-Path -LiteralPath $exe -PathType Leaf){$resolved[$exeName.ToLowerInvariant()]=$exe}
    $originalAlut=Join-Path $RedistRoot 'PenumbraVR_alut_original.dll'
    if($Game -ne 'overture' -and (Test-Path -LiteralPath $originalAlut -PathType Leaf)){$resolved['original-alut.dll']=$originalAlut}
    $queue=[Collections.Generic.Queue[string]]::new()
    foreach($path in $resolved.Values){$queue.Enqueue($path)}
    $seen=@{};$reported=@{}
    $osNames=@('kernel32.dll','user32.dll','gdi32.dll','advapi32.dll','shell32.dll','ole32.dll','oleaut32.dll','comdlg32.dll','comctl32.dll','winmm.dll','msvcrt.dll','opengl32.dll','glu32.dll','ws2_32.dll','wsock32.dll','version.dll','dsound.dll','msacm32.dll','imm32.dll','setupapi.dll','winspool.drv','shlwapi.dll','rpcrt4.dll','ntdll.dll','crypt32.dll','bcrypt.dll','bcryptprimitives.dll','winhttp.dll','dwmapi.dll','uxtheme.dll','secur32.dll','iphlpapi.dll','powrprof.dll','cfgmgr32.dll','avrt.dll','hid.dll','psapi.dll','dbghelp.dll','ucrtbase.dll')
    while($queue.Count){
        $path=$queue.Dequeue();if($seen.ContainsKey($path)){continue};$seen[$path]=$true
        if($seen.Count -gt 128){New-PvrCheck 'Import closure' 'invalid' 'More than 128 dependency files.';break}
        try{$peInfo=Get-PvrPeInfo $path}catch{New-PvrCheck ('Import closure '+(Split-Path -Leaf $path)) 'invalid' 'Cannot inspect dependency.';continue}
        foreach($name in @($peInfo.Imports)+@($peInfo.DelayImports)){
            $key=$name.ToLowerInvariant();if($reported.ContainsKey($key)){continue};$reported[$key]=$true
            if($key -in $osNames -or $key -match '^(api|ext)-ms-'){
                $systemDir=if([Environment]::Is64BitOperatingSystem){Join-Path $env:WINDIR 'SysWOW64'}else{Join-Path $env:WINDIR 'System32'}
                $osPath=Join-Path $systemDir $(if($key -match '^(api|ext)-ms-'){'ucrtbase.dll'}else{$name})
                New-PvrCheck ('Windows API '+$name) $(if(Test-Path -LiteralPath $osPath -PathType Leaf){'present'}else{'missing'}) 'Windows x86 system component' 'Use supported Windows 10/11 and repair Windows components; never copy system DLLs into the game.'
                continue
            }
            $child=if($resolved.ContainsKey($key)){$resolved[$key]}else{Join-Path $RedistRoot $name}
            $status='missing'
            if(Test-Path -LiteralPath $child -PathType Leaf){try{$childInfo=Get-PvrPeInfo $child;$status=if($childInfo.Architecture -eq 'x86'){'present'}else{'invalid'}}catch{$status='invalid'}}
            New-PvrCheck ('Import '+$name) $status ('Required by '+(Split-Path -Leaf $path)) 'Verify game files in Steam or repair the pinned installer payload; do not download individual DLLs.'
            if($status -eq 'present'){$queue.Enqueue($child)}
        }
    }
    if (-not $SkipSteamVr) {
        if (-not $OpenVrPathsPath) { $OpenVrPathsPath=Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'openvr/openvrpaths.vrpath' }
        $status='missing'
        try {
            $paths=Get-Content -LiteralPath $OpenVrPathsPath -Raw | ConvertFrom-Json
            foreach($runtime in @($paths.runtime)) { if (Test-Path -LiteralPath (Join-Path $runtime 'bin/win64/vrserver.exe') -PathType Leaf) { $status='present' } }
        } catch { }
        New-PvrCheck 'SteamVR' $status 'OpenVR runtime registration' 'Install SteamVR through Steam and complete its headset setup. No OpenXR runtime change is needed.'
    }
}
Export-ModuleMember -Function Get-PvrPeInfo,Read-PvrConfig,Test-PvrGameContent,Get-PvrPrerequisites
