param([ValidateSet('All','Redaction','Bounds')][string]$Case='All')
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$temp=Join-Path ([IO.Path]::GetTempPath()) ('PvrDiagnostics-'+[guid]::NewGuid().ToString('N'))
try{
    $data=Join-Path $temp 'data';$game=Join-Path $temp 'game';$logs=Join-Path $data 'logs'
    New-Item -ItemType Directory -Path $logs,$game -Force | Out-Null
    [IO.File]::WriteAllText((Join-Path $logs 'black_plague-probe-123.log'),('frame ok '+$game+' '+$env:USERNAME+"`n")*4000+"token=private-secret`n")
    [IO.File]::WriteAllText((Join-Path $logs 'unrelated.log'),'private-secret')
    [IO.File]::WriteAllText((Join-Path $game 'save.bin'),'private-secret')
    [IO.File]::WriteAllText((Join-Path $data 'settings.ini'),"TurnMode=Snap`nHeightOffset=0.12`nPassword=private-secret`n")
    $zip=Join-Path $temp 'diagnostics.zip'
    & (Join-Path $repo 'tools/Collect-PenumbraVrDiagnostics.ps1') -Game black_plague -GamePath $game -DataRoot $data -OutputPath $zip | Out-Null
    Expand-Archive $zip (Join-Path $temp 'unpacked')
    $files=@(Get-ChildItem (Join-Path $temp 'unpacked') -File -Recurse)
    if($files.Count -gt 3 -or -not $files.Count){throw 'Unbounded collection'}
    foreach($file in $files){$text=[IO.File]::ReadAllText($file.FullName);if($file.Length -gt 262144 -or $text.Contains('private-secret') -or $text.Contains($game) -or $text.Contains($env:USERNAME)){throw 'Private information or oversized data included'}}
    $report=Get-Content (Join-Path $temp 'unpacked/report.json') -Raw | ConvertFrom-Json
    if($report.headset -ne 'unknown' -or $report.controllers -ne 'unknown' -or $report.vrSettings.Password){throw 'Unknown hardware fabricated or secrets included'}
    Write-Host 'Diagnostics: bounded redacted tails, settings allowlist, no saves/unrelated logs and unknown hardware passed.'

    if($Case -in @('All','Redaction')){
        $secrets=@('password-space-value','token-space-value','secret-arrow-value','cookie-tab-value','api-key-space-value','bearer-credential-value','punctuation-password-value',
                   'legacy-underscore-value','legacy-combined-value','76561198012345678','STEAM_0:1:123456789','[U:1:246913578]','private.person+vr@example.test','bare.person@example.test','cut-sensitive-value')
        $lines=@('frame ready',
                 'password password-space-value',
                 'token token-space-value',
                 'secret -> secret-arrow-value',
                 "cookie`tcookie-tab-value",
                 'api key api-key-space-value',
                 'Bearer bearer-credential-value',
                 'PASSWORD | punctuation-password-value',
                 'access_token=legacy-underscore-value',
                 'authToken: legacy-combined-value',
                 'SteamID 76561198012345678',
                 'account 76561198012345678',
                 'account STEAM_0:1:123456789',
                 'account [U:1:246913578]',
                 'email private.person+vr@example.test',
                 'contact bare.person@example.test',
                 ('frame path '+$game+' user '+$env:USERNAME),
                 'frame finished') -join "`n"
        [IO.File]::WriteAllText((Join-Path $logs 'black_plague-probe-123.log'),$lines)
        # The 64 KiB read starts after the credential keyword, inside its value.
        $cutLine='password '+('q'*70000)+'cut-sensitive-value'+"`nframe after cut`n"
        [IO.File]::WriteAllText((Join-Path $logs 'black_plague-probe-124.log'),$cutLine)
        [IO.File]::WriteAllText((Join-Path $logs 'black_plague-probe-125.log'),'excluded-third-log')
        [IO.File]::SetLastWriteTimeUtc((Join-Path $logs 'black_plague-probe-125.log'),[datetime]'2000-01-01')
        [IO.File]::WriteAllText((Join-Path $logs 'requiem-probe-123.log'),'excluded-other-game')
        # A verifier scalar must be redacted before JSON encoding, without breaking JSON.
        $state=@{schema=1;frameworkVersion='token report-secret-value';components=@('shared','black_plague','authorization=report-equals-value');vrAssetsSnapshot=@{files=@()}}
        [IO.File]::WriteAllText((Join-Path $game 'PenumbraVR.BlackPlague.install.json'),($state | ConvertTo-Json -Depth 8))
        $before=@(Get-ChildItem $game,$data -File -Recurse | Sort-Object FullName | ForEach-Object {$_.FullName+':'+(Get-FileHash -LiteralPath $_.FullName).Hash}) -join "`n"
        $privacyZip=Join-Path $temp 'privacy.zip'
        & (Join-Path $repo 'tools/Collect-PenumbraVrDiagnostics.ps1') -Game black_plague -GamePath $game -DataRoot $data -OutputPath $privacyZip | Out-Null
        Expand-Archive $privacyZip (Join-Path $temp 'privacy')
        $allText=@(Get-ChildItem (Join-Path $temp 'privacy') -File -Recurse | ForEach-Object {[IO.File]::ReadAllText($_.FullName)}) -join "`n"
        $leaked=@($secrets+@('report-secret-value','report-equals-value','excluded-third-log','excluded-other-game','private-secret') | Where-Object {$allText.Contains($_)})
        if($leaked.Count){throw ('Punctuation-independent redaction or cut-line privacy failed: '+($leaked -join ', '))}
        $privacyReport=Get-Content (Join-Path $temp 'privacy/report.json') -Raw | ConvertFrom-Json
        if($privacyReport.verification.Version.Contains('report-secret-value')){throw 'Report scalar was not redacted'}
        if(-not $allText.Contains('frame ready') -or -not $allText.Contains('frame finished') -or -not $allText.Contains('frame after cut')){throw 'Useful complete log lines lost'}
        foreach($log in Get-ChildItem (Join-Path $temp 'privacy/logs') -File){if($log.Length -gt 65536){throw 'Encoded log exceeded 64 KiB'}}
        $after=@(Get-ChildItem $game,$data -File -Recurse | Sort-Object FullName | ForEach-Object {$_.FullName+':'+(Get-FileHash -LiteralPath $_.FullName).Hash}) -join "`n"
        if($before -cne $after){throw 'Diagnostics changed input files'}
        Write-Host 'Diagnostics privacy: whitespace/punctuation credentials, standalone IDs/emails/Bearer, cut first line, valid JSON and read-only inputs passed.'
    }

    if($Case -in @('All','Bounds')){
        $entries=@(for($i=0;$i -lt 2000;$i++){@{path=('binding-{0:D4}-' -f $i)+('x'*64)+'.json';sha256=('A'*64)}})
        $state=@{schema=1;frameworkVersion='1.0.0';components=@('shared','black_plague');vrAssetsSnapshot=@{files=$entries}}
        $statePath=Join-Path $game 'PenumbraVR.BlackPlague.install.json'
        [IO.File]::WriteAllText($statePath,($state | ConvertTo-Json -Depth 8 -Compress))
        $source=& (Join-Path $repo 'tools/Test-PenumbraVrInstallation.ps1') -Game black_plague -GamePath $game -PackageRoot $repo
        $before=(Get-FileHash -LiteralPath $statePath).Hash
        $boundedZip=Join-Path $temp 'bounded.zip'
        & (Join-Path $repo 'tools/Collect-PenumbraVrDiagnostics.ps1') -Game black_plague -GamePath $game -DataRoot $data -OutputPath $boundedZip | Out-Null
        Expand-Archive $boundedZip (Join-Path $temp 'bounded')
        $reportFile=Get-Item (Join-Path $temp 'bounded/report.json')
        $sizeFailure=if($reportFile.Length -gt 262144){"UTF-8 report exceeded 256 KiB: $($reportFile.Length) bytes for 2000 fixture state entries"}else{$null}
        # Isolate large verifier details/metadata without changing the real verifier.
        $mockTools=Join-Path $temp 'mock-tools';New-Item -ItemType Directory -Path $mockTools | Out-Null
        Copy-Item -LiteralPath (Join-Path $repo 'tools/Collect-PenumbraVrDiagnostics.ps1') -Destination $mockTools
        [IO.File]::WriteAllText((Join-Path $mockTools 'Test-PenumbraVrInstallation.ps1'),@'
param($Game,$GamePath,$PackageRoot)
Get-Content -LiteralPath (Join-Path $PSScriptRoot 'verification.json') -Raw -Encoding UTF8 | ConvertFrom-Json
'@)
        $wide=([string][char]0x6F22+'"\'+[string][char]1)*3000
        $stressSource=@{Game='black_plague';Version=$wide;Components=@(1..80 | ForEach-Object {$wide});InstallationVerified=$false;HeadsetValidated=$null;
            Checks=@(1..128 | ForEach-Object {@{Name=$wide;Required=$true;Status='invalid';Detail=$wide;Remediation=$wide}})}
        [IO.File]::WriteAllText((Join-Path $mockTools 'verification.json'),($stressSource | ConvertTo-Json -Depth 8),[Text.Encoding]::UTF8)
        $stressZip=Join-Path $temp 'stress.zip'
        & (Join-Path $mockTools 'Collect-PenumbraVrDiagnostics.ps1') -Game black_plague -GamePath $game -DataRoot $data -OutputPath $stressZip -PackageRoot $repo | Out-Null
        Expand-Archive $stressZip (Join-Path $temp 'stress')
        $stressFile=Get-Item (Join-Path $temp 'stress/report.json')
        if($sizeFailure){throw $sizeFailure}
        if($stressFile.Length -gt 262144){throw "Escaped/multibyte UTF-8 report exceeded 256 KiB: $($stressFile.Length) bytes"}
        $stressReport=Get-Content -LiteralPath $stressFile.FullName -Raw -Encoding UTF8 | ConvertFrom-Json
        if($stressReport.omitted.textFields -le 0 -or $stressReport.omitted.textCharacters -le 0){throw 'Truncated field counts missing'}
        if((@($stressReport.verification.Components).Count+$stressReport.omitted.components) -ne 80){throw 'Omitted component count inaccurate'}
        if($stressReport.omitted.verificationChecks -le 0 -or (@($stressReport.verification.Checks).Count+$stressReport.omitted.verificationChecks) -ne 128){throw 'Final byte-limit omission count inaccurate'}
        foreach($check in $stressReport.verification.Checks){if($check.Name.Length -gt 1024 -or $check.Detail.Length -gt 1024 -or $check.Remediation.Length -gt 1024){throw 'Verifier text fields are unbounded'}}
        if($stressReport.verification.InstallationVerified -ne $false -or $null -ne $stressReport.verification.HeadsetValidated){throw 'Truncation changed verification summary'}
        $boundedReport=Get-Content -LiteralPath $reportFile.FullName -Raw -Encoding UTF8 | ConvertFrom-Json
        if($boundedReport.omitted.verificationChecks -le 0 -or (@($boundedReport.verification.Checks).Count+$boundedReport.omitted.verificationChecks) -ne @($source.Checks).Count){throw 'Omitted verification record count missing or inaccurate'}
        if($boundedReport.verification.InstallationVerified -ne $source.InstallationVerified -or $null -ne $boundedReport.verification.HeadsetValidated){throw 'Bounded report changed verification result or fabricated headset validation'}
        if((Get-FileHash -LiteralPath $statePath).Hash -cne $before){throw 'Size limiting changed fixture state'}
        Write-Host "Diagnostics size: $($reportFile.Length) UTF-8 bytes; $($boundedReport.omitted.verificationChecks) verifier checks explicitly omitted from 2000-entry state fixture."
        Write-Host "Diagnostics escaped/multibyte size: $($stressFile.Length) UTF-8 bytes; $($stressReport.omitted.verificationChecks) checks, $($stressReport.omitted.components) components and $($stressReport.omitted.textCharacters) text characters explicitly omitted."
    }
}finally{if(Test-Path $temp){Remove-Item -LiteralPath $temp -Recurse -Force}}
