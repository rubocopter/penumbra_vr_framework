[CmdletBinding()]
param([Parameter(Mandatory=$true)][ValidateSet('overture','black_plague','requiem')][string]$Game,
      [Parameter(Mandatory=$true)][string]$GamePath,[Parameter(Mandatory=$true)][string]$OutputPath,
      [string]$PackageRoot,[string]$DataRoot)
$ErrorActionPreference='Stop'
if(-not $PackageRoot){$PackageRoot=Split-Path -Parent $PSScriptRoot}
if(-not $DataRoot){$DataRoot=Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'PenumbraVR'}
$output=[IO.Path]::GetFullPath($OutputPath)
if(Test-Path -LiteralPath $output){throw 'Diagnostic output already exists.'}
function Assert-NoLink([string]$path){
    for($cursor=[IO.Path]::GetFullPath($path);$cursor;$cursor=Split-Path -Parent $cursor){
        $item=Get-Item -LiteralPath $cursor -Force -ErrorAction SilentlyContinue
        if($item -and ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Diagnostic input/output traverses a reparse point.'}
    }
}
Assert-NoLink $GamePath;Assert-NoLink $DataRoot;Assert-NoLink $output
function Redact([string]$text){
    foreach($path in @([IO.Path]::GetFullPath($GamePath),$DataRoot,$env:USERPROFILE,[Environment]::GetFolderPath('MyDocuments')) | Sort-Object Length -Descending){if($path){$text=[regex]::Replace($text,[regex]::Escape($path),'[path]',[Text.RegularExpressions.RegexOptions]::IgnoreCase)}}
    # Credentials/identifiers may be separated from their labels by any punctuation or whitespace.
    $text=[regex]::Replace($text,'(?im)^.*(?:password|passwd|token|secret|authorization|cookie|bearer|api[\W_]*key|steam[\W_]*id(?:64)?|email).*$','[sensitive line removed]')
    # Values also appear without labels, including native Steam account representations.
    $text=[regex]::Replace($text,'\b7656119[0-9]{10}\b|(?i)\bSTEAM_[0-5]:[01]:[0-9]+\b|\[[A-Za-z]:[0-9]+:[0-9]+(?::[0-9]+)?\]','[steam id]')
    $text=[regex]::Replace($text,'(?i)[a-z0-9._%+''-]+@[a-z0-9.-]+\.[a-z]{2,}','[email]')
    $text=[regex]::Replace($text,'(?i)(?:[a-z]:[\\/]|\\\\)[^\r\n"<>]*','[path]')
    if($env:USERNAME){$text=[regex]::Replace($text,[regex]::Escape($env:USERNAME),'[user]',[Text.RegularExpressions.RegexOptions]::IgnoreCase)}
    $text
}
function Limit-ReportText($value,[int]$limit=512){
    # Redact before truncation and JSON encoding; retain neither partial secrets nor invalid JSON.
    $text=Redact ([string]$value)
    if($text.Length -gt $limit){
        $keep=$limit-'[truncated]'.Length
        if([char]::IsHighSurrogate($text[$keep-1])){$keep--}
        $omitted.textFields++;$omitted.textCharacters+=($text.Length-$keep)
        $text=$text.Substring(0,$keep)+'[truncated]'
    }
    return $text
}
function Limit-ReportRecords($records,[int]$limit,[string]$countName){
    $items=@($records | Where-Object {$null -ne $_})
    $omitted[$countName]=[Math]::Max(0,$items.Count-$limit)
    return ,@($items | Select-Object -First $limit)
}
$release=Get-Content -LiteralPath (Join-Path $PackageRoot 'release.json') -Raw | ConvertFrom-Json
$verification=& (Join-Path $PSScriptRoot 'Test-PenumbraVrInstallation.ps1') -Game $Game -GamePath $GamePath -PackageRoot $PackageRoot
$settings=@{}
$settingsPath=Join-Path $DataRoot 'settings.ini'
if(Test-Path -LiteralPath $settingsPath -PathType Leaf){
    Assert-NoLink $settingsPath
    if((Get-Item -LiteralPath $settingsPath).Length -le 65536){
        foreach($line in [IO.File]::ReadAllLines($settingsPath)){
            if($line -match '^\s*(TurnMode|Handedness|PlayMode|HRTF|CrouchMode)\s*=\s*([A-Za-z]{1,16})\s*$' -or $line -match '^\s*(MoveSpeed|MoveDeadZone|HeightOffset|PlayerHeight|RenderScale|SnapTurnAngle|SmoothTurnSpeed|TurnDeadZone|UiDistance|UiScale|SubtitleScale|PhysicalCrouchDepth)\s*=\s*([-+]?[0-9]{1,5}(?:\.[0-9]{1,8})?)\s*$'){$settings[$Matches[1]]=$Matches[2]}
        }
    }else{$settings['collection']='settings exceeded 64 KiB; omitted'}
}
$gpu=@();try{$gpu=@(Get-CimInstance Win32_VideoController -ErrorAction Stop | ForEach-Object {[pscustomobject]@{name=$_.Name;driverVersion=$_.DriverVersion}})}catch{}
$omitted=[ordered]@{verificationChecks=0;components=0;gpu=0;textFields=0;textCharacters=0L}
$boundedChecks=@(foreach($check in (Limit-ReportRecords $verification.Checks 128 'verificationChecks')){
    [ordered]@{Name=(Limit-ReportText $check.Name 128);Required=$check.Required;Status=(Limit-ReportText $check.Status 32);
        Detail=(Limit-ReportText $check.Detail);Remediation=(Limit-ReportText $check.Remediation)}
})
$boundedComponents=@(foreach($component in (Limit-ReportRecords $verification.Components 32 'components')){Limit-ReportText $component 128})
$boundedGpu=@(foreach($adapter in (Limit-ReportRecords $gpu 8 'gpu')){
    [ordered]@{name=(Limit-ReportText $adapter.name 256);driverVersion=(Limit-ReportText $adapter.driverVersion 128)}
})
foreach($key in @($settings.Keys)){$settings[$key]=Limit-ReportText $settings[$key]}
$report=[ordered]@{frameworkVersion=(Limit-ReportText $release.version 128);channel=(Limit-ReportText $release.channel 128);
    windows=[Environment]::OSVersion.VersionString;gpu=$boundedGpu;headset='unknown';controllers='unknown';vrSettings=$settings;
    verification=[ordered]@{Game=(Limit-ReportText $verification.Game 32);Version=(Limit-ReportText $verification.Version 128);
        Components=$boundedComponents;Checks=$boundedChecks;InstallationVerified=$verification.InstallationVerified;HeadsetValidated=$verification.HeadsetValidated};
    omitted=$omitted}
# Text/record limits alone do not bound encoded UTF-8, especially with JSON escaping.
# Re-serialize after each omission, including the counters, and preserve the full verifier summary.
do{
    $reportJson=$report | ConvertTo-Json -Depth 12
    if([Text.Encoding]::UTF8.GetByteCount($reportJson) -le 262144){break}
    $count=$report.verification.Checks.Count
    if(-not $count){throw 'Diagnostic report exceeds 256 KiB after bounding records and text.'}
    $report.verification.Checks=@($report.verification.Checks | Select-Object -First ($count-1))
    $omitted.verificationChecks++
}while($true)
Add-Type -AssemblyName System.IO.Compression
New-Item -ItemType Directory -Path (Split-Path -Parent $output) -Force | Out-Null
$stream=[IO.File]::Open($output,[IO.FileMode]::CreateNew)
try{
    $zip=[IO.Compression.ZipArchive]::new($stream,[IO.Compression.ZipArchiveMode]::Create,$false)
    try{
        function Entry([string]$name,[string]$text){$entry=$zip.CreateEntry($name);$s=$entry.Open();try{$b=[Text.Encoding]::UTF8.GetBytes($text);$s.Write($b,0,$b.Length)}finally{$s.Dispose()}}
        Entry 'report.json' $reportJson
        $logs=Join-Path $DataRoot 'logs'
        if(Test-Path -LiteralPath $logs -PathType Container){
            Assert-NoLink $logs
            $prefix=if($Game -eq 'overture'){'overture'}elseif($Game -eq 'requiem'){'requiem'}else{'black_plague'}
            $files=@(Get-ChildItem -LiteralPath $logs -File | Where-Object {$_.Name -match ('^'+$prefix+'-probe-[0-9]+\.log$')} | Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 2)
            $index=0
            foreach($file in $files){
                Assert-NoLink $file.FullName
                $s=[IO.File]::Open($file.FullName,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::ReadWrite)
                try{
                    $size=[int][Math]::Min(65536,$s.Length);$partial=($s.Length -gt $size)
                    $s.Seek(-$size,[IO.SeekOrigin]::End)|Out-Null;$b=New-Object byte[] $size;$read=$s.Read($b,0,$size);$tail=[Text.Encoding]::UTF8.GetString($b,0,$read)
                    # A cut first line has lost its context (possibly the credential label).
                    if($partial){$newline=$tail.IndexOf("`n");$tail=if($newline -ge 0){$tail.Substring($newline+1)}else{''}}
                }finally{$s.Dispose()}
                $tail=Redact $tail
                # Redaction can expand tiny secrets. Bound the final encoded tail too.
                if([Text.Encoding]::UTF8.GetByteCount($tail) -gt 65536){$tail=$tail.Substring([Math]::Max(0,$tail.Length-16000))}
                Entry ("logs/framework-$index.log") $tail;$index++
            }
        }
    }finally{$zip.Dispose()}
}finally{$stream.Dispose()}
Write-Output $output
