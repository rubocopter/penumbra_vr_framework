[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$gui=Join-Path $repo 'tools/Install-PenumbraFrameworkGui.ps1'
$temp=Join-Path ([IO.Path]::GetTempPath()) ('PvrGui-'+[guid]::NewGuid().ToString('N'))
function Assert($ok,[string]$message){if(-not $ok){throw $message}}
try{
    New-Item -ItemType Directory -Path $temp | Out-Null
    $entries=@();foreach($game in @('Overture','Black Plague','Requiem')){
        $root=if($game -eq 'Overture'){'C:\Games\Overture'}else{'C:\Games\BP'}
        $exe=if($game -eq 'Requiem'){'Requiem.exe'}else{'Penumbra.exe'}
        $entries+=[pscustomobject]@{Game=$game;Path="$root\redist\$exe";InstallRoot=$root;Managed=$false;ManagedProduct=$null;Installable=$true;KnownBuild=$true;Status='available';NeedsRecovery=$false;RequiemPresent=($game -ne 'Overture');Issues=@()}
    }
    $fixture=Join-Path $temp 'preview.json';[pscustomobject]@{Entries=$entries;Reports=@()} | ConvertTo-Json -Depth 8 | Set-Content $fixture -Encoding UTF8
    # Load the real WinForms construction/events without its final modal entry
    # point. Keep this harness in tests, never add write-capable preview actions.
    $tokens=$null;$errors=$null;$ast=[Management.Automation.Language.Parser]::ParseFile($gui,[ref]$tokens,[ref]$errors)
    Assert (-not $errors.Count) 'GUI must parse on Windows PowerShell.'
    $entryPoint=$ast.EndBlock.Statements[-1]
    Assert ($entryPoint -is [Management.Automation.Language.TryStatementAst]) 'GUI entry point must own disposal in finally.'
    $previewTools=Join-Path $temp 'tools';New-Item -ItemType Directory -Path $previewTools | Out-Null
    foreach($name in @('PenumbraVrInstallerUi.psm1','Install-PenumbraFrameworkCandidate.ps1','Get-PenumbraInstallations.ps1')){Copy-Item -LiteralPath (Join-Path $repo ('tools/'+$name)) -Destination $previewTools}
    $construction=Join-Path $previewTools 'Initialize-Gui.ps1'
    [IO.File]::WriteAllText($construction,[IO.File]::ReadAllText($gui).Substring(0,$entryPoint.Extent.StartOffset),[Text.UTF8Encoding]::new($true))
    . $construction -UiContract -PreviewDataPath $fixture
    Refresh-Games
    $form.StartPosition='Manual';$form.Location=[Drawing.Point]::new(-32000,-32000);$form.Show();[Windows.Forms.Application]::DoEvents();Resize-Content;Update-Actions
    Assert ($script:views.Count -eq 2 -and $script:views[1].Requiem.Parent -eq $script:views[1].Spanish.Parent) 'Requiem must be inside its Black Plague card.'
    Assert ($banner.Width -eq $form.ClientSize.Width -and $installButton.Visible -and $installButton.Enabled) 'Full-width banner and visible primary action required.'
    Assert ($script:views[0].Panel.Width -gt 750 -and $script:views[1].Panel.Width -eq $script:views[0].Panel.Width) 'Cards must span the page width.'
    Assert (-not $script:views[0].Spanish.Checked -and $settingsOption.Checked) 'Fresh English selections must exclude translations and include recommended settings.'
    $languageBox.SelectedIndex=1
    Assert ($script:views[0].Spanish.Checked -and $script:views[1].Spanish.Checked -and $script:views[1].SpanishRequiem.Checked -and $installButton.Text -eq 'Instalar') 'Language switch must update live controls and translation choices.'
    $script:views[0].Spanish.Checked=$false;$script:views[0].Textures.Checked=$true
    Refresh-Games
    Assert (-not $script:views[0].Spanish.Checked -and $script:views[0].Textures.Checked) 'Refresh must preserve explicit component choices.'
    $script:views[1].Requiem.Checked=$false
    Assert (-not $script:views[1].SpanishRequiem.Enabled) 'Requiem translation must depend on expansion selection.'
    Sync-Cards;$calls=@(New-PvrUiRequests -Cards $script:cards -Operation Install)
    Assert ($calls.Count -eq 2 -and $calls[1].Arguments.RequiemMode -eq 'Disable' -and -not $calls[0].Arguments.InstallSpanishOverture -and $calls[0].Arguments.TextureEnhancements) 'Actual control selections must reach the selector requests.'
    $script:views[0].Select.Checked=$false;Sync-Cards
    Assert (@(New-PvrUiRequests -Cards $script:cards -Operation Install).Count -eq 1) 'Unchecked game must leave the write plan.'
    $tabs.SelectedIndex=1
    Assert (-not $installButton.Visible -and $modifyButton.Visible -and -not $repairButton.Enabled) 'Maintenance must hide the install action and disable repair for pristine games.'
    $script:maintenanceViews[1].Select.Checked=$true
    $modifyButton.PerformClick()
    Assert ($tabs.SelectedIndex -eq 0 -and $installButton.Visible) 'Modify components must return to Install.'
    $reports=@();foreach($e in $entries){$e.Managed=$true;if($e.Game -ne 'Requiem'){$reports+=[pscustomobject]@{Path=$e.Path;Report=[pscustomobject]@{Version='1.0.0';Components=$(if($e.Game -eq 'Overture'){@('shared','overture','texture_enhancements')}else{@('shared','black_plague','requiem','spanish_black_plague')});InstallationVerified=$true;Checks=@()}}}}
    [pscustomobject]@{Entries=$entries;Reports=$reports} | ConvertTo-Json -Depth 8 | Set-Content $fixture -Encoding UTF8
    $script:cards=@();Refresh-Games
    Assert ($script:views[0].Textures.Checked -and $script:views[1].Spanish.Checked -and $script:views[1].Requiem.Checked) 'Owned components must populate the actual controls.'
    $script:maintenanceViews[0].Select.Checked=$false
    Refresh-Games
    Assert (-not $script:maintenanceViews[0].Select.Checked -and $script:maintenanceViews[1].Select.Checked) 'Refresh must retain excluded maintenance roots.'
    $languageBox.SelectedIndex=0
    Assert (-not $script:maintenanceViews[0].Select.Checked -and $script:views[1].Spanish.Checked) 'Changing interface language must retain maintenance exclusions and owned translations.'
    $languageBox.SelectedIndex=1
    $languageBox.SelectedIndex=0
    Assert ($script:views[1].Spanish.Checked -and $script:views[0].Spanish.Checked) 'Returning to English must preserve both installed and explicitly selected translations.'
    $tabs.SelectedIndex=1;$modifyButton.PerformClick();Sync-Cards
    Assert (-not $script:views[0].Select.Checked -and $script:views[1].Select.Checked -and @(New-PvrUiRequests -Cards $script:cards -Operation Install).Count -eq 1) 'Modify components must use only the selected maintenance root.'
    $timer=[Windows.Forms.Timer]::new();$timer.Interval=100
    $timer.Add_Tick({
        foreach($dialog in @([Windows.Forms.Application]::OpenForms)){
            if($dialog -eq $form){continue}
            $bodyPanel=@($dialog.Controls | Where-Object {$_.Dock -eq 'Fill'})[0]
            $checks=@($bodyPanel.Controls | Where-Object {$_ -is [Windows.Forms.CheckBox]})
            if($checks.Count -ne 3){continue}
            $checks[0].Checked=$false;$checks[1].Checked=$false;$checks[2].Checked=$true
            $timer.Stop();$dialog.DialogResult='OK'
        }
    })
    try{$timer.Start();$removeCards=@(Select-Removal)}finally{$timer.Stop();$timer.Dispose()}
    $remove=@(New-PvrUiRequests -Cards $removeCards -Operation Uninstall)
    Assert ($remove.Count -eq 1 -and $remove[0].Arguments.Game -eq 'Requiem' -and $remove[0].Arguments.Restore) 'The real component removal dialog must route Requiem-only removal without removing BP or Overture.'
    # Replace only the external write boundary. The real GUI still builds its
    # requests, preflights every root and handles the partial failure.
    $trace=Join-Path $temp 'selector-trace.txt';$reject=Join-Path $temp 'reject-plan'
    $stub=Join-Path $previewTools 'Selector-Stub.ps1'
    $stubText=@'
param([string]$Game,[string]$GamePath,[switch]$Plan,[switch]$List,[string]$SteamRoot,[string[]]$ManualPaths,[switch]$RecommendedSettings,[string]$SettingsScope,[switch]$InstallSpanishOverture,[switch]$TextureEnhancements,[string]$RequiemMode,[switch]$InstallSpanishBlackPlague,[switch]$InstallSpanishRequiem)
if($List){throw 'No fixture discovery after an injected write failure.'}
$verb=if($Plan){'Plan'}else{'Apply'}
Add-Content -LiteralPath '__TRACE__' -Value ($verb+' '+$Game)
if($Game -eq 'BlackPlague' -and ((-not $Plan) -or (Test-Path -LiteralPath '__REJECT__'))){throw 'Injected later-root failure.'}
if($Plan){[pscustomobject]@{Game=$Game;Path=$GamePath;Payload=[pscustomobject]@{Files=@();Configuration=@()}}}
'@
    [IO.File]::WriteAllText($stub,$stubText.Replace('__TRACE__',$trace.Replace("'","''")).Replace('__REJECT__',$reject.Replace("'","''")))
    function Show-Text([string]$title,[string]$text,[switch]$Review){$script:lastDialog=[pscustomobject]@{Title=$title;Text=$text};'Cancel'}
    $selector=$stub;$PreviewDataPath=$null
    foreach($v in $script:views){$v.Select.Checked=$true}
    Invoke-Operation 'Install'
    Assert ((@(Get-Content $trace) -join ',') -eq 'Plan Overture,Plan BlackPlague,Apply Overture,Apply BlackPlague') 'All roots must pass preflight before the first write.'
    Assert ($script:lastDialog.Title -eq 'Operation stopped' -and $script:lastDialog.Text.Contains('C:\Games\Overture') -and $script:lastDialog.Text.Contains('C:\Games\BP')) 'Partial failure must identify both the completed root and the failed root.'
    $PreviewDataPath=$fixture;$script:cards=@();Refresh-Games;$PreviewDataPath=$null
    [IO.File]::WriteAllText($reject,'reject');[IO.File]::WriteAllText($trace,'')
    Invoke-Operation 'Install'
    Assert ((@(Get-Content $trace) -join ',') -eq 'Plan Overture,Plan BlackPlague' -and $script:lastDialog.Title -eq 'Check failed — no games changed') 'A later preflight failure must perform zero writes and report no changed games.'
    'Live WinForms language, selection, refresh, layout and maintenance checks passed.'
}finally{
    if($banner -and $banner.Image){$banner.Image.Dispose()};if($tips){$tips.Dispose()};if($form){$form.Dispose()}
    $absolute=[IO.Path]::GetFullPath($temp);$prefix=[IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\')+'\'
    if($absolute.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase) -and (Test-Path -LiteralPath $absolute)){Remove-Item -LiteralPath $absolute -Recurse -Force}
}
