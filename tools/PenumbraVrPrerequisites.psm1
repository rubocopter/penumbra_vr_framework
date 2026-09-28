$ErrorActionPreference='Stop'
$script:Prerequisites=Get-Content -LiteralPath (Join-Path (Split-Path -Parent $PSScriptRoot) 'assets/deployment/prerequisites.json') -Raw | ConvertFrom-Json
if ($script:Prerequisites.schemaVersion -ne 1) { throw 'Unsupported prerequisites manifest.' }

function Get-PvrPeInfo {
    [CmdletBinding()]param([Parameter(Mandatory=$true,Position=0)][string]$Path)
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
            $source=Join-Path $PackageRoot $library.source
            if ((Test-Path -LiteralPath $source -PathType Leaf) -and (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -ieq $library.sha256) {
                # Ownership/conflict policy is enforced by the transaction, not relaxed here.
                $status='provided'
            }
        }
        $remedy=if($library.source){'Repair using the complete Penumbra VR installer package.'}else{'Verify the game files in Steam to restore its x86 libraries.'}
        New-PvrCheck $library.name $status $library.path $remedy
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
