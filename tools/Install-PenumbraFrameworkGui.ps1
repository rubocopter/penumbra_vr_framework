[CmdletBinding()]
param(
    [switch]$SmokeTest,[switch]$UiContract,[string]$RenderPath,
    [ValidateSet('en','es')][string]$Language='en',
    [ValidateSet('Install','Maintenance')][string]$RenderPage='Install',
    [string]$PreviewDataPath,[string]$SteamRoot,[string[]]$ManualPaths=@()
)
$ErrorActionPreference='Stop'
if($PreviewDataPath -and -not ($RenderPath -or $UiContract -or $SmokeTest)){throw 'Preview data is only allowed for read-only UI previews.'}
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
[Windows.Forms.Application]::EnableVisualStyles()
Import-Module (Join-Path $PSScriptRoot 'PenumbraVrInstallerUi.psm1') -Force
$selector=Join-Path $PSScriptRoot 'Install-PenumbraFrameworkCandidate.ps1'
$discovery=Join-Path $PSScriptRoot 'Get-PenumbraInstallations.ps1'
if(-not (Test-Path $selector) -or -not (Test-Path $discovery)){throw 'Installer package is incomplete.'}
$packageRoot=Split-Path -Parent $PSScriptRoot
$releasePath=Join-Path $packageRoot 'release.json'
$script:version=if(Test-Path $releasePath){(Get-Content $releasePath -Raw | ConvertFrom-Json).version}else{'1.0.0'}
$script:cards=@();$script:views=@();$script:maintenanceViews=@();$script:manualPaths=@($ManualPaths);$script:busy=$false
$script:Language=$Language
$logPath=Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'PenumbraVR/installer.jsonl'
function T([string]$English,[string]$Spanish){if($script:Language -eq 'es'){$Spanish}else{$English}}
function Label([string]$text,[bool]$bold=$false){
    $c=[Windows.Forms.Label]::new();$c.Text=$text;$c.AutoSize=$true;$c.Margin=[Windows.Forms.Padding]::new(0,0,0,2)
    if($bold){$c.Font=[Drawing.Font]::new('Segoe UI',10,[Drawing.FontStyle]::Bold)};$c
}
function CheckBox([string]$text,[bool]$checked=$false){
    $c=[Windows.Forms.CheckBox]::new();$c.Text=$text;$c.AutoSize=$true;$c.Checked=$checked;$c.Margin=[Windows.Forms.Padding]::new(0,2,0,2);$c
}
function Button([string]$text,[int]$width=170){
    $c=[Windows.Forms.Button]::new();$c.Text=$text;$c.Size=[Drawing.Size]::new($width,38);$c.FlatStyle='Flat';$c.FlatAppearance.BorderColor=[Drawing.Color]::FromArgb(200,205,213);$c.Margin=[Windows.Forms.Padding]::new(0,0,10,8);$c
}
function Stack {
    $p=[Windows.Forms.FlowLayoutPanel]::new();$p.FlowDirection='TopDown';$p.WrapContents=$false;$p.AutoSize=$true;$p.AutoSizeMode='GrowAndShrink';$p.Margin=[Windows.Forms.Padding]::new(0);$p
}
function StatusText($card){
    $text=switch($card.StatusKey){
        'Installed' {T 'Installed' 'Instalado'}
        'UpdateAvailable' {T 'Update available' 'Actualización disponible'}
        'NotInstalled' {T 'Not installed' 'No instalado'}
        'NeedsRepair' {T 'Needs repair' 'Necesita reparación'}
        'RecoveryRequired' {T 'Interrupted installation' 'Instalación interrumpida'}
        'Unsupported' {T 'Unsupported version' 'Versión no compatible'}
        default {T 'Problem detected' 'Problema detectado'}
    }
    if($card.Version){$text+=' — v'+$card.Version};$text
}
function InstalledText([bool]$installed){if($installed){T 'Installed ✓' 'Instalado ✓'}else{T 'Not installed' 'No instalado'}}

$form=[Windows.Forms.Form]::new();$form.Text='Penumbra VR Framework';$form.ClientSize=[Drawing.Size]::new(900,930)
$form.MinimumSize=[Drawing.Size]::new(820,760);$form.StartPosition='CenterScreen';$form.Font=[Drawing.Font]::new('Segoe UI',10)
$form.BackColor=[Drawing.Color]::FromArgb(246,247,249);$form.AutoScaleMode='Dpi'
$shell=[Windows.Forms.TableLayoutPanel]::new();$shell.Dock='Fill';$shell.ColumnCount=1;$shell.RowCount=4
[void]$shell.ColumnStyles.Add([Windows.Forms.ColumnStyle]::new([Windows.Forms.SizeType]::Percent,100))
foreach($height in @(58,128)){[void]$shell.RowStyles.Add([Windows.Forms.RowStyle]::new([Windows.Forms.SizeType]::Absolute,$height))}
[void]$shell.RowStyles.Add([Windows.Forms.RowStyle]::new([Windows.Forms.SizeType]::Percent,100))
[void]$shell.RowStyles.Add([Windows.Forms.RowStyle]::new([Windows.Forms.SizeType]::Absolute,72));$form.Controls.Add($shell)
$header=[Windows.Forms.TableLayoutPanel]::new();$header.Dock='Fill';$header.ColumnCount=2;$header.Padding=[Windows.Forms.Padding]::new(22,12,18,0);$header.Margin=[Windows.Forms.Padding]::new(0)
[void]$header.ColumnStyles.Add([Windows.Forms.ColumnStyle]::new([Windows.Forms.SizeType]::Percent,100));[void]$header.ColumnStyles.Add([Windows.Forms.ColumnStyle]::new([Windows.Forms.SizeType]::AutoSize))
$title=Label 'Penumbra VR Framework';$title.Font=[Drawing.Font]::new('Segoe UI',17,[Drawing.FontStyle]::Bold);$header.Controls.Add($title,0,0)
$languageRow=[Windows.Forms.FlowLayoutPanel]::new();$languageRow.AutoSize=$true;$languageRow.WrapContents=$false
$languageLabel=Label 'Language:';$languageLabel.Margin=[Windows.Forms.Padding]::new(0,5,8,0);$languageRow.Controls.Add($languageLabel)
$languageBox=[Windows.Forms.ComboBox]::new();$languageBox.DropDownStyle='DropDownList';$languageBox.Width=115;[void]$languageBox.Items.AddRange([object[]]@('English','Español'));$languageRow.Controls.Add($languageBox);$header.Controls.Add($languageRow,1,0);$shell.Controls.Add($header,0,0)
$banner=[Windows.Forms.PictureBox]::new();$banner.Dock='Fill';$banner.Margin=[Windows.Forms.Padding]::new(0);$banner.SizeMode='Normal'
$bannerPath=Join-Path $packageRoot 'assets/banner/penumbra-vr-framework.png'
if(Test-Path $bannerPath){$banner.Image=[Drawing.Image]::FromFile($bannerPath)};$shell.Controls.Add($banner,0,1)
$banner.Add_Paint({param($sender,$paint)
    if(-not $sender.Image){return}
    # Fill the strip without distorting the artwork; keep the lower logo visible.
    $scale=[Math]::Max($sender.Width/[double]$sender.Image.Width,$sender.Height/[double]$sender.Image.Height)
    $width=[int][Math]::Ceiling($sender.Image.Width*$scale);$height=[int][Math]::Ceiling($sender.Image.Height*$scale)
    $paint.Graphics.InterpolationMode='HighQualityBicubic'
    $paint.Graphics.DrawImage($sender.Image,[Drawing.Rectangle]::new([int](($sender.Width-$width)/2),$sender.Height-$height,$width,$height))
})
$tabs=[Windows.Forms.TabControl]::new();$tabs.Dock='Fill';$tabs.Margin=[Windows.Forms.Padding]::new(16,12,16,0);$tabs.Padding=[Drawing.Point]::new(20,8)
$installPage=[Windows.Forms.TabPage]::new();$maintenancePage=[Windows.Forms.TabPage]::new();foreach($page in @($installPage,$maintenancePage)){$page.BackColor=$form.BackColor;$page.Padding=[Windows.Forms.Padding]::new(14);$page.AutoScroll=$true;$tabs.TabPages.Add($page)};$shell.Controls.Add($tabs,0,2)
$installBody=Stack;$installPage.Controls.Add($installBody)
$detectedHeading=Label '' $true;$installBody.Controls.Add($detectedHeading)
$gamesPanel=Stack;$installBody.Controls.Add($gamesPanel)
$optionsHeading=Label '' $true;$optionsHeading.Margin=[Windows.Forms.Padding]::new(0,14,0,5);$installBody.Controls.Add($optionsHeading)
$settingsOption=CheckBox '' $true;$installBody.Controls.Add($settingsOption)
$settingsHelp=Label '';$settingsHelp.ForeColor=[Drawing.Color]::DimGray;$settingsHelp.MaximumSize=[Drawing.Size]::new(760,0);$settingsHelp.Margin=[Windows.Forms.Padding]::new(23,0,0,5);$installBody.Controls.Add($settingsHelp)
$advancedButton=Button '' 190;$advancedButton.Height=30;$advancedButton.Margin=[Windows.Forms.Padding]::new(0,6,0,0);$installBody.Controls.Add($advancedButton)
$advancedPanel=Stack;$advancedPanel.Visible=$false;$scopeOption=CheckBox '' $true;$advancedPanel.Controls.Add($scopeOption);$scopeHelp=Label '';$scopeHelp.MaximumSize=[Drawing.Size]::new(740,0);$advancedPanel.Controls.Add($scopeHelp);$installBody.Controls.Add($advancedPanel)
$maintenanceBody=Stack;$maintenancePage.Controls.Add($maintenanceBody)
$installedHeading=Label '' $true;$maintenanceBody.Controls.Add($installedHeading)
$installedPanel=Stack;$maintenanceBody.Controls.Add($installedPanel)
$maintenanceActions=[Windows.Forms.FlowLayoutPanel]::new();$maintenanceActions.AutoSize=$true;$maintenanceActions.WrapContents=$true;$maintenanceActions.Margin=[Windows.Forms.Padding]::new(0,16,0,0)
$modifyButton=Button '' 175;$verifyButton=Button '' 185;$repairButton=Button '' 175;$uninstallButton=Button '' 125
foreach($b in @($modifyButton,$verifyButton,$repairButton,$uninstallButton)){$maintenanceActions.Controls.Add($b)};$maintenanceBody.Controls.Add($maintenanceActions)
$troubleHeading=Label '' $true;$troubleHeading.Margin=[Windows.Forms.Padding]::new(0,16,0,8);$maintenanceBody.Controls.Add($troubleHeading)
$troubleActions=[Windows.Forms.FlowLayoutPanel]::new();$troubleActions.AutoSize=$true;$troubleActions.WrapContents=$true
$diagnosticsButton=Button '' 215;$recoverButton=Button '' 240;$logButton=Button '' 115
foreach($b in @($diagnosticsButton,$recoverButton,$logButton)){$troubleActions.Controls.Add($b)};$maintenanceBody.Controls.Add($troubleActions)
$recoveryHelp=Label '';$recoveryHelp.MaximumSize=[Drawing.Size]::new(750,0);$recoveryHelp.ForeColor=[Drawing.Color]::DimGray;$maintenanceBody.Controls.Add($recoveryHelp)
$footer=[Windows.Forms.TableLayoutPanel]::new();$footer.Dock='Fill';$footer.ColumnCount=2;$footer.Padding=[Windows.Forms.Padding]::new(22,12,22,8);$footer.Margin=[Windows.Forms.Padding]::new(0)
[void]$footer.ColumnStyles.Add([Windows.Forms.ColumnStyle]::new([Windows.Forms.SizeType]::Percent,100));[void]$footer.ColumnStyles.Add([Windows.Forms.ColumnStyle]::new([Windows.Forms.SizeType]::AutoSize))
$leftActions=[Windows.Forms.FlowLayoutPanel]::new();$leftActions.AutoSize=$true;$leftActions.WrapContents=$false
$refreshButton=Button '' 105;$browseButton=Button '' 160;$leftActions.Controls.Add($refreshButton);$leftActions.Controls.Add($browseButton);$footer.Controls.Add($leftActions,0,0)
$installButton=Button '' 220;$installButton.Height=44;$installButton.BackColor=[Drawing.Color]::FromArgb(38,78,107);$installButton.ForeColor=[Drawing.Color]::White;$installButton.Font=[Drawing.Font]::new('Segoe UI',11,[Drawing.FontStyle]::Bold);$installButton.Margin=[Windows.Forms.Padding]::new(0);$footer.Controls.Add($installButton,1,0);$shell.Controls.Add($footer,0,3)
$tips=[Windows.Forms.ToolTip]::new();$tips.AutoPopDelay=15000

function Resize-Content {
    $width=[Math]::Max(650,$installPage.ClientSize.Width-46)
    $installBody.Width=$width;$maintenanceBody.Width=$width;$gamesPanel.Width=$width;$installedPanel.Width=$width
    $maintenanceActions.MaximumSize=[Drawing.Size]::new($width,0);$troubleActions.MaximumSize=[Drawing.Size]::new($width,0)
    foreach($view in @($script:views)+@($script:maintenanceViews)){
        $view.Panel.MinimumSize=[Drawing.Size]::new($width,0);$view.Panel.MaximumSize=[Drawing.Size]::new($width,0)
        $view.Body.MinimumSize=[Drawing.Size]::new($width-32,0);$view.Body.MaximumSize=[Drawing.Size]::new($width-32,0)
        foreach($c in @($view.Body.Controls)){if($c -is [Windows.Forms.Label]){$c.MaximumSize=[Drawing.Size]::new($width-48,0)}}
    }
}
function Sync-Cards {
    foreach($view in $script:views){
        $c=$view.Card;$c.Selected=$view.Select.Checked;$c.SpanishBase=$view.Spanish.Checked
        if($view.Textures){$c.Textures=$view.Textures.Checked}
        if($view.Requiem){$c.RequiemSelected=$view.Requiem.Checked;$c.SpanishRequiem=$view.SpanishRequiem.Checked}
    }
    foreach($view in $script:maintenanceViews){$view.Card.MaintenanceSelected=$view.Select.Checked}
}
function Update-Actions {
    if($script:busy){return};Sync-Cards
    $action=Get-PvrUiAction -Cards $script:cards -RecommendedSettings $settingsOption.Checked
    $installButton.Text=switch($action.Key){'Repair'{T 'Repair installation' 'Reparar instalación'} 'Update'{T 'Update' 'Actualizar'} 'ApplyChanges'{T 'Apply changes' 'Aplicar cambios'} default{T 'Install' 'Instalar'}}
    if(-not $script:cards.Count){$installButton.Text=T 'Install' 'Instalar'}
    $installButton.Enabled=$action.Enabled;$installButton.Visible=$tabs.SelectedIndex -eq 0
    foreach($view in $script:views){
        $editable=$view.Select.Checked -and $view.Card.StatusKey -notin @('NeedsRepair','RecoveryRequired','Unsupported','Incomplete')
        $view.Spanish.Enabled=$editable
        if($view.Textures){$view.Textures.Enabled=$editable}
        if($view.Requiem){$view.Requiem.Enabled=$editable -and $view.Card.RequiemAvailable;$view.SpanishRequiem.Enabled=$editable -and $view.Requiem.Checked -and $view.Card.RequiemAvailable}
    }
    $managed=@($script:maintenanceViews | Where-Object {$_.Select.Checked -and $_.Card.Managed -and -not $_.Card.NeedsRecovery})
    $verifyButton.Enabled=$managed.Count -gt 0;$repairButton.Enabled=$managed.Count -gt 0;$uninstallButton.Enabled=$managed.Count -gt 0
    $recoverButton.Enabled=@($script:maintenanceViews | Where-Object {$_.Select.Checked -and $_.Card.NeedsRecovery}).Count -gt 0
    $diagnosticsButton.Enabled=@($script:maintenanceViews | Where-Object {$_.Select.Checked -and $_.Card.Game -ne 'Unknown'}).Count -gt 0
    $modifyButton.Enabled=$script:cards.Count -gt 0
}
function New-CardPanel {
    $p=[Windows.Forms.Panel]::new();$p.BackColor=[Drawing.Color]::White;$p.BorderStyle='FixedSingle';$p.AutoSize=$true;$p.AutoSizeMode='GrowAndShrink';$p.Padding=[Windows.Forms.Padding]::new(15,10,15,10);$p.Margin=[Windows.Forms.Padding]::new(0,7,0,5)
    $body=Stack;$body.Location=[Drawing.Point]::new(15,10);$p.Controls.Add($body)
    [pscustomobject]@{Panel=$p;Body=$body}
}
function Draw-Cards {
    foreach($hostPanel in @($gamesPanel,$installedPanel)){foreach($c in @($hostPanel.Controls)){$hostPanel.Controls.Remove($c);$c.Dispose()}}
    $script:views=@();$script:maintenanceViews=@()
    foreach($card in $script:cards){
        $v=New-CardPanel;$v | Add-Member Card $card
        $select=CheckBox $card.Title $card.Selected;$select.Font=[Drawing.Font]::new('Segoe UI',11,[Drawing.FontStyle]::Bold);$select.Enabled=$card.Selectable;$v.Body.Controls.Add($select);$v | Add-Member Select $select
        $detected=if($card.StatusKey -eq 'Unsupported'){T 'Unsupported version' 'Versión no compatible'}elseif($card.StatusKey -eq 'Incomplete'){T 'Detected — incomplete game files' 'Detectado — faltan archivos del juego'}else{T 'Detected ✓' 'Detectado ✓'}
        $path=Label ($detected+' — '+$card.InstallRoot);$path.ForeColor=[Drawing.Color]::DimGray;$v.Body.Controls.Add($path);$tips.SetToolTip($path,$card.Path)
        $state=Label ('VR Framework: '+(StatusText $card));$state.ForeColor=if($card.StatusKey -in @('NeedsRepair','RecoveryRequired','Unsupported','Incomplete')){[Drawing.Color]::Firebrick}else{[Drawing.Color]::FromArgb(36,91,74)};$v.Body.Controls.Add($state)
        if($card.StatusKey -eq 'NeedsRepair'){$v.Body.Controls.Add((Label (T 'Repair restores the recorded components first. You can modify them afterwards.' 'La reparación restaura primero los componentes registrados. Después puedes modificarlos.')))}
        if($card.Issues.Count){$issue=Label ($card.Issues -join ' ');$issue.ForeColor=[Drawing.Color]::Firebrick;$v.Body.Controls.Add($issue)}
        $reqOption=$null;$reqSpanish=$null;$textures=$null
        if($card.Game -eq 'BlackPlague'){
            $reqText=if($card.RequiemAvailable){T 'Requiem expansion detected ✓' 'Expansión Requiem detectada ✓'}elseif($card.RequiemPresent -or $card.RequiemInstalled){T 'Requiem expansion: problem detected' 'Expansión Requiem: problema detectado'}else{T 'Requiem expansion not detected' 'Expansión Requiem no detectada'}
            $reqLabel=Label $reqText $true;$reqLabel.Margin=[Windows.Forms.Padding]::new(23,10,0,4);$v.Body.Controls.Add($reqLabel)
            $reqOption=CheckBox (T 'Install Requiem VR support' 'Instalar soporte VR de Requiem') $card.RequiemSelected;$reqOption.Margin=[Windows.Forms.Padding]::new(23,0,0,4);$v.Body.Controls.Add($reqOption)
            if($card.RequiemInstalled){$reqState=Label ('Requiem VR: '+(InstalledText $true));$reqState.Margin=[Windows.Forms.Padding]::new(23,0,0,3);$v.Body.Controls.Add($reqState)}
        }
        $optional=Label (T 'Optional components' 'Componentes opcionales') $true;$optional.Margin=[Windows.Forms.Padding]::new(23,5,0,3);$v.Body.Controls.Add($optional)
        $baseText=if($card.Game -eq 'BlackPlague'){T 'Black Plague Spanish translation' 'Traducción al español de Black Plague'}else{T 'Spanish translation' 'Traducción al español'}
        $spanish=CheckBox $baseText $card.SpanishBase;$spanish.Margin=[Windows.Forms.Padding]::new(23,0,0,3);$v.Body.Controls.Add($spanish);$v | Add-Member Spanish $spanish
        if($card.Game -eq 'Overture'){$textures=CheckBox (T 'Enhanced textures' 'Texturas mejoradas') $card.Textures;$textures.Margin=[Windows.Forms.Padding]::new(23,0,0,3);$v.Body.Controls.Add($textures)}
        if($card.Game -eq 'BlackPlague'){$reqSpanish=CheckBox (T 'Requiem Spanish translation' 'Traducción al español de Requiem') $card.SpanishRequiem;$reqSpanish.Margin=[Windows.Forms.Padding]::new(23,0,0,3);$v.Body.Controls.Add($reqSpanish)}
        $v | Add-Member Textures $textures;$v | Add-Member Requiem $reqOption;$v | Add-Member SpanishRequiem $reqSpanish
        foreach($c in @($select,$spanish,$textures,$reqOption,$reqSpanish) | Where-Object {$null -ne $_}){$c.Add_CheckedChanged({Update-Actions})}
        $script:views+= $v;$gamesPanel.Controls.Add($v.Panel)
        $m=New-CardPanel;$m | Add-Member Card $card
        $ms=CheckBox $card.Title $card.MaintenanceSelected;$ms.Enabled=$card.Game -ne 'Unknown';$ms.Font=$select.Font;$m.Body.Controls.Add($ms);$m | Add-Member Select $ms
        $m.Body.Controls.Add((Label $card.InstallRoot));$m.Body.Controls.Add((Label ('VR Framework: '+(StatusText $card))))
        $m.Body.Controls.Add((Label ($baseText+': '+(InstalledText $card.OriginalSpanishBase))))
        if($card.Game -eq 'Overture'){$m.Body.Controls.Add((Label ((T 'Enhanced textures: ' 'Texturas mejoradas: ')+(InstalledText $card.OriginalTextures))))}
        if($card.Game -eq 'BlackPlague'){$m.Body.Controls.Add((Label ('Requiem VR: '+(InstalledText $card.RequiemInstalled))));$m.Body.Controls.Add((Label ((T 'Requiem Spanish translation: ' 'Traducción de Requiem: ')+(InstalledText $card.OriginalSpanishRequiem))))}
        $ms.Add_CheckedChanged({Update-Actions});$script:maintenanceViews+=$m;$installedPanel.Controls.Add($m.Panel)
    }
    if(-not $script:cards.Count){$gamesPanel.Controls.Add((Label (T 'No games detected. Install the games through Steam or choose their folder.' 'No se detectaron juegos. Instala los juegos desde Steam o selecciona su carpeta.')));$installedPanel.Controls.Add((Label (T 'No VR installations detected.' 'No se detectaron instalaciones VR.')))}
    Resize-Content;Update-Actions
}
function Set-UiLanguage([bool]$selectTranslations=$false){
    Sync-Cards;$script:Language=if($languageBox.SelectedIndex -eq 1){'es'}else{'en'}
    if($selectTranslations -and $script:Language -eq 'es'){foreach($c in $script:cards | Where-Object {$_.StatusKey -notin @('NeedsRepair','RecoveryRequired','Unsupported','Incomplete')}){$c.SpanishBase=$true;if($c.RequiemAvailable){$c.SpanishRequiem=$true}}}
    $languageLabel.Text=T 'Language:' 'Idioma:';$installPage.Text=T 'Install' 'Instalar';$maintenancePage.Text=T 'Maintenance' 'Mantenimiento'
    $detectedHeading.Text=T 'Detected games and installation status' 'Juegos detectados y estado de instalación'
    $optionsHeading.Text=T 'Installation options' 'Opciones de instalación'
    $settingsOption.Text=T 'Apply recommended VR graphics settings' 'Aplicar los ajustes gráficos recomendados para VR'
    $settingsHelp.Text=T 'Applies the tested settings recommended for Penumbra VR. Existing configuration will be backed up.' 'Aplica los ajustes probados para Penumbra VR. Se guardará una copia de la configuración existente.'
    $advancedButton.Text=(T 'Advanced options' 'Opciones avanzadas')+$(if($advancedPanel.Visible){' ▴'}else{' ▾'})
    $scopeOption.Text=T 'Also apply settings to the current Windows user profile' 'Aplicar también los ajustes al perfil actual de Windows'
    $scopeHelp.Text=T 'Disable to change only the game defaults. Saved games and VR calibration are preserved.' 'Desmarca esta opción para modificar solo la configuración del juego. Se conservan las partidas y la calibración VR.'
    $installedHeading.Text=(T 'Installed components' 'Componentes instalados')+' — v'+$script:version
    $modifyButton.Text=T 'Modify components' 'Modificar componentes';$verifyButton.Text=T 'Verify VR installation' 'Comprobar instalación VR';$repairButton.Text=T 'Repair installation' 'Reparar instalación';$uninstallButton.Text=T 'Uninstall…' 'Desinstalar…'
    $troubleHeading.Text=T 'Troubleshooting' 'Solución de problemas';$diagnosticsButton.Text=T 'Generate diagnostics ZIP' 'Generar ZIP de diagnóstico'
    $recoverButton.Text=T 'Restore interrupted installation' 'Restaurar instalación interrumpida';$logButton.Text=T 'View log' 'Ver registro'
    $recoveryHelp.Text=T 'Restores the verified snapshot from before an interrupted operation. Available only when an interrupted installation is detected.' 'Restaura la copia verificada anterior a una operación interrumpida. Solo está disponible si se detecta una instalación interrumpida.'
    $refreshButton.Text=T 'Refresh' 'Actualizar';$browseButton.Text=T 'Locate game…' 'Buscar juego…'
    Draw-Cards
}
function Refresh-Games {
    Sync-Cards;$previous=@{};foreach($c in $script:cards){$previous[$c.Path]=$c}
    if($PreviewDataPath){$preview=Get-Content -LiteralPath $PreviewDataPath -Raw | ConvertFrom-Json;$found=@($preview.Entries)}else{
        $search=@{};if($SteamRoot){$search.SteamRoot=$SteamRoot};if($script:manualPaths.Count){$search.ManualPaths=$script:manualPaths}
        & $selector -List @search | Out-Null;$found=@(& $discovery @search)
    }
    $script:cards=@()
    foreach($entry in $found){
        if((Split-Path -Leaf $entry.Path) -ieq 'Requiem.exe'){continue}
        $expansion=@($found | Where-Object {(Split-Path -Leaf $_.Path) -ieq 'Requiem.exe' -and (Split-Path -Parent $_.Path) -ieq (Split-Path -Parent $entry.Path)}) | Select-Object -First 1
        $report=$null
        if($entry.Managed -and -not $entry.NeedsRecovery){
            if($PreviewDataPath){$report=($preview.Reports | Where-Object {$_.Path -ieq $entry.Path} | Select-Object -First 1).Report}else{
                $owner=if($entry.ManagedProduct){$entry.ManagedProduct}else{$entry.Game}
                $product=if($owner -eq 'Overture'){'overture'}else{'black_plague'}
                try{$report=& (Join-Path $PSScriptRoot 'Test-PenumbraVrInstallation.ps1') -Game $product -GamePath $entry.Path -SkipSteamVr}catch{$report=[pscustomobject]@{Version='';Components=@();InstallationVerified=$false;Checks=@()}}
            }
        }
        $card=ConvertTo-PvrUiGame -Entry $entry -Expansion $expansion -Report $report -PackageVersion $script:version -Language $script:Language
        if($previous.ContainsKey($card.Path)){$old=$previous[$card.Path];foreach($field in @('Selected','MaintenanceSelected','SpanishBase','SpanishRequiem','Textures','RequiemSelected')){$card.$field=$old.$field}}
        $script:cards+=$card
    }
    Draw-Cards
}
function Open-Log {if(Test-Path -LiteralPath $logPath){Start-Process -FilePath 'notepad.exe' -ArgumentList ('"'+$logPath+'"')}else{[void][Windows.Forms.MessageBox]::Show((T 'No installation log has been created yet.' 'Todavía no se ha creado un registro de instalación.'),'Penumbra VR')}}
function Show-Text([string]$title,[string]$text,[switch]$Review){
    $dialog=[Windows.Forms.Form]::new();$dialog.Text=$title;$dialog.ClientSize=[Drawing.Size]::new(730,440);$dialog.MinimumSize=[Drawing.Size]::new(650,370);$dialog.StartPosition='CenterParent';$dialog.Font=$form.Font
    $body=[Windows.Forms.TextBox]::new();$body.Multiline=$true;$body.ReadOnly=$true;$body.ScrollBars='Vertical';$body.Dock='Fill';$body.Text=$text;$body.BorderStyle='None';$body.BackColor=[Drawing.Color]::White;$dialog.Controls.Add($body)
    $buttons=[Windows.Forms.FlowLayoutPanel]::new();$buttons.Dock='Bottom';$buttons.Height=58;$buttons.Padding=[Windows.Forms.Padding]::new(12,10,0,0)
    $open=Button (T 'Open installation log' 'Abrir registro de instalación') 225;$open.Add_Click({Open-Log});$buttons.Controls.Add($open)
    if($Review){$apply=Button (T 'Apply' 'Aplicar') 110;$apply.DialogResult='OK';$buttons.Controls.Add($apply);$dialog.AcceptButton=$apply}
    $close=Button $(if($Review){T 'Cancel' 'Cancelar'}else{T 'Close' 'Cerrar'}) 110;$close.DialogResult='Cancel';$buttons.Controls.Add($close);$dialog.CancelButton=$close
    $dialog.Controls.Add($buttons);try{$dialog.ShowDialog($form)}finally{$dialog.Dispose()}
}
function Maintenance-Cards([string]$operation){
    foreach($v in $script:maintenanceViews){
        if(-not $v.Select.Checked){continue}
        if($operation -eq 'Recover' -and -not $v.Card.NeedsRecovery){continue}
        if($operation -in @('Repair','Verify') -and (-not $v.Card.Managed -or $v.Card.NeedsRecovery)){continue}
        $copy=$v.Card.PSObject.Copy();$copy.Selected=$true;$copy
    }
}
function Select-Removal {
    $dialog=[Windows.Forms.Form]::new();$dialog.Text=T 'Remove Penumbra VR components' 'Quitar componentes de Penumbra VR';$dialog.ClientSize=[Drawing.Size]::new(700,460);$dialog.StartPosition='CenterParent';$dialog.Font=$form.Font
    $body=Stack;$body.Dock='Fill';$body.AutoScroll=$true;$body.Padding=[Windows.Forms.Padding]::new(18);$dialog.Controls.Add($body)
    $help=Label (T 'Select the VR components to remove. Original game files are restored; saves are preserved.' 'Selecciona los componentes VR que quieres quitar. Se restauran los archivos originales y se conservan las partidas.');$help.MaximumSize=[Drawing.Size]::new(640,0);$body.Controls.Add($help)
    $items=@()
    foreach($v in $script:maintenanceViews | Where-Object {$_.Card.Managed -and -not $_.Card.NeedsRecovery}){
        $c=$v.Card.PSObject.Copy();$c.RemoveBase=$false;$c.RemoveRequiem=$false
        $base=CheckBox ($c.Title+' — '+$c.InstallRoot) $v.Select.Checked;$base.MaximumSize=[Drawing.Size]::new(640,0);$body.Controls.Add($base)
        $req=$null;if($c.RequiemInstalled){$req=CheckBox 'Requiem' $base.Checked;$req.Margin=[Windows.Forms.Padding]::new(24,0,0,8);$body.Controls.Add($req);$base.Tag=$req;$base.Add_CheckedChanged({$this.Tag.Checked=$this.Checked;$this.Tag.Enabled=-not $this.Checked});$req.Enabled=-not $base.Checked}
        $items+=[pscustomobject]@{Card=$c;Base=$base;Requiem=$req}
    }
    $footer=[Windows.Forms.FlowLayoutPanel]::new();$footer.Dock='Bottom';$footer.Height=58;$footer.Padding=[Windows.Forms.Padding]::new(18,8,0,0)
    $remove=Button (T 'Remove selected' 'Quitar seleccionados') 185;$remove.DialogResult='OK';$cancel=Button (T 'Cancel' 'Cancelar') 110;$cancel.DialogResult='Cancel';$footer.Controls.Add($remove);$footer.Controls.Add($cancel);$dialog.Controls.Add($footer);$dialog.CancelButton=$cancel
    try{if($dialog.ShowDialog($form) -ne 'OK'){return};foreach($item in $items){$item.Card.RemoveBase=$item.Base.Checked;$item.Card.RemoveRequiem=$item.Requiem -and $item.Requiem.Checked;if($item.Card.RemoveBase -or $item.Card.RemoveRequiem){$item.Card}}}finally{$dialog.Dispose()}
}
function Check-Summary($report){
    $lines=@()
    foreach($group in @(Get-PvrUiCheckGroups -Report $report)){
        $name=switch($group.Key){
            'Executable' {T 'Supported game executable' 'Ejecutable compatible'}
            'Framework' {T 'VR Framework files and backups present' 'Archivos VR y copias de seguridad presentes'}
            'Bindings' {T 'Controller bindings installed' 'Asignaciones de controles instaladas'}
            'Configuration' {T 'Configuration valid' 'Configuración válida'}
            'Dependencies' {T 'Required dependencies available' 'Dependencias necesarias disponibles'}
        }
        $lines+= $(if($group.Passed){'✓ '}else{'✗ '})+$name
        foreach($check in @($group.Checks | Where-Object {$_.Required -and $_.Status -notin @('present','provided')})){$lines+='  '+$check.Name+': '+$check.Status+'. '+$check.Remediation;if($check.Detail){$lines+='  '+$check.Detail}}
    }
    $lines+='';$lines+=if($report.InstallationVerified){T 'No problems found.' 'No se encontraron problemas.'}else{T 'Problems found. Review the items above.' 'Se encontraron problemas. Revisa los elementos anteriores.'};$lines -join "`r`n"
}
function Invoke-Operation([string]$operation){
    if($script:busy -or $PreviewDataPath){return}
    Sync-Cards
    try{
        $cards=if($operation -eq 'Install'){@($script:cards)}elseif($operation -eq 'Uninstall'){@(Select-Removal)}else{@(Maintenance-Cards $operation)}
        $scope=if($scopeOption.Checked){'DefaultAndUserFiles'}else{'DefaultFiles'}
        $requests=@(New-PvrUiRequests -Cards $cards -Operation $operation -RecommendedSettings $settingsOption.Checked -SettingsScope $scope)
        if(-not $requests.Count){return}
        if($operation -eq 'Verify'){
            $text=@();foreach($r in $requests){$a=$r.Arguments;$report=& $selector @a;$text+=(Format-PvrUiOutcomes -Requests @($r));$text+=(Check-Summary $report);$text+=''}
            [void](Show-Text (T 'VR installation check' 'Comprobación de la instalación VR') ($text -join "`r`n"));return
        }
        if($operation -eq 'Diagnostics'){
            foreach($r in $requests){$save=[Windows.Forms.SaveFileDialog]::new();$save.Filter='ZIP (*.zip)|*.zip';$save.FileName='PenumbraVR-'+$r.Card.Game+'-diagnostics.zip';try{if($save.ShowDialog($form) -eq 'OK'){$a=$r.Arguments;$a.DiagnosticOutputPath=$save.FileName;& $selector @a | Out-Null}}finally{$save.Dispose()}};return
        }
        # Preflight every root before the first write, even when optional choices differ.
        $previews=@();if($operation -ne 'Recover'){foreach($r in $requests){$a=$r.Arguments;$previews+=@(& $selector @a -Plan)}}
        if($operation -ne 'Install' -or @($requests | Where-Object {$_.RequiresReview}).Count){
            $review=@((T 'Close the selected games before continuing.' 'Cierra los juegos seleccionados antes de continuar.'),'')
            if($operation -eq 'Recover'){$review+=(T 'Restore the verified snapshot from before the interrupted operation.' 'Restaurar la copia verificada anterior a la operación interrumpida.')}
            foreach($r in $requests){$review+=$r.Card.Title+' — '+$r.Card.InstallRoot;if($r.Arguments.Game -eq 'Requiem'){$review+=(T 'Remove Requiem VR support; keep Black Plague VR.' 'Quitar el soporte VR de Requiem y conservar Black Plague VR.')}}
            foreach($p in $previews){$review+='';$review+=@($p.Payload.Files | ForEach-Object {'  '+$_});foreach($cfg in @($p.Payload.Configuration)){$review+='  '+$cfg.Path;$review+=@($cfg.Changes | ForEach-Object {'    '+$_.Section+'.'+$_.Attribute+': '+$_.Original+' → '+$_.Applied})}}
            if((Show-Text (T 'Review changes' 'Revisar cambios') ($review -join "`r`n") -Review) -ne 'OK'){return}
        }
    }catch{[void](Show-Text (T 'Check failed — no games changed' 'Comprobación fallida — ningún juego modificado') $_.Exception.Message);return}
    $script:busy=$true;$shell.Enabled=$false;$form.UseWaitCursor=$true;$form.Text=T 'Penumbra VR Framework — Processing…' 'Penumbra VR Framework — Procesando…';[Windows.Forms.Application]::DoEvents()
    $completed=@();$current=$null
    try{
        foreach($r in $requests){$current=$r;$a=$r.Arguments;& $selector @a | Out-Null;$completed+=$r;$current=$null}
        $script:cards=@();Refresh-Games
        $message=@();foreach($r in $completed){$result=if($operation -eq 'Install'){if($r.Operation -eq 'Repair'){T 'VR repaired' 'VR reparado'}else{T 'VR installed / updated' 'VR instalado / actualizado'}}elseif($operation -eq 'Uninstall'){if($r.Arguments.Game -eq 'Requiem'){T 'Requiem VR removed' 'VR de Requiem eliminado'}else{T 'VR components removed' 'Componentes VR eliminados'}}elseif($operation -eq 'Recover'){T 'Previous installation restored' 'Instalación anterior restaurada'}else{T 'VR repaired' 'VR reparado'}
            $message+='✓ '+(Format-PvrUiOutcomes -Requests @($r) -Label $result)
            if($operation -eq 'Install' -and $r.Operation -ne 'Repair'){
                if($r.Card.RequiemSelected){$message+='✓ '+(T 'Requiem VR support installed' 'Soporte VR de Requiem instalado')}
                if($settingsOption.Checked){$message+='✓ '+(T 'Recommended settings applied' 'Ajustes recomendados aplicados')}
            }
        }
        if($operation -in @('Install','Repair')){
            $verified=$true
            foreach($r in $completed){$current=$r;$a=@{Game=$r.Card.Game;GamePath=$r.Card.Path;Verify=$true};$report=& $selector @a;if(-not $report.InstallationVerified){$verified=$false;$message+=(Format-PvrUiOutcomes -Requests @($r) -Label (T 'Check needs attention' 'Revisa la comprobación'));$message+=(Check-Summary $report)};$current=$null}
            if($verified){$message+='✓ '+(T 'Controller bindings and required dependencies verified' 'Asignaciones de controles y dependencias necesarias verificadas');$message+='';$message+=(T 'Start SteamVR. You can now launch the games normally through Steam.' 'Inicia SteamVR. Ya puedes abrir los juegos normalmente desde Steam.')}
            $caption=if($verified){T 'Installation complete' 'Instalación completada'}else{T 'Changes applied — installation needs attention' 'Cambios aplicados — revisa la instalación'}
        }else{$caption=T 'Operation complete' 'Operación completada'}
        [void](Show-Text $caption ($message -join "`r`n"))
    }catch{
        $detail=$_.Exception.Message
        if($current){$detail=(Format-PvrUiOutcomes -Requests @($current) -Label (T 'Operation stopped here' 'La operación se detuvo aquí'))+"`r`n`r`n"+$detail}
        if($completed.Count){$detail=(T 'These installations completed before the operation stopped:' 'Estas instalaciones se completaron antes de detenerse la operación:')+"`r`n"+(Format-PvrUiOutcomes -Requests $completed -Label (T 'Completed' 'Completado'))+"`r`n`r`n"+$detail}
        [void](Show-Text (T 'Operation stopped' 'Operación detenida') $detail)
        try{$script:cards=@();Refresh-Games}catch{}
    }finally{$script:busy=$false;$shell.Enabled=$true;$form.UseWaitCursor=$false;$form.Text='Penumbra VR Framework';Update-Actions}
}
$languageBox.Add_SelectedIndexChanged({Set-UiLanguage $true})
$languageBox.SelectedIndex=if($Language -eq 'es'){1}else{0}
$advancedButton.Add_Click({$advancedPanel.Visible=-not $advancedPanel.Visible;$advancedButton.Text=(T 'Advanced options' 'Opciones avanzadas')+$(if($advancedPanel.Visible){' ▴'}else{' ▾'})})
$settingsOption.Add_CheckedChanged({Update-Actions});$tabs.Add_SelectedIndexChanged({Update-Actions});$form.Add_Resize({Resize-Content})
$installButton.Add_Click({Invoke-Operation 'Install'});$verifyButton.Add_Click({Invoke-Operation 'Verify'});$repairButton.Add_Click({Invoke-Operation 'Repair'});$uninstallButton.Add_Click({Invoke-Operation 'Uninstall'});$recoverButton.Add_Click({Invoke-Operation 'Recover'});$diagnosticsButton.Add_Click({Invoke-Operation 'Diagnostics'});$logButton.Add_Click({Open-Log})
$modifyButton.Add_Click({
    foreach($v in $script:views){$v.Select.Checked=$v.Card.Selectable -and $v.Card.MaintenanceSelected}
    $tabs.SelectedIndex=0;Update-Actions
})
$refreshButton.Add_Click({try{Refresh-Games}catch{[void][Windows.Forms.MessageBox]::Show($_.Exception.Message,'Penumbra VR')}})
$browseButton.Add_Click({$folder=[Windows.Forms.FolderBrowserDialog]::new();$folder.Description=T 'Select the game folder or its redist folder.' 'Selecciona la carpeta del juego o su directorio redist.';try{if($folder.ShowDialog($form) -eq 'OK'){$script:manualPaths+=$folder.SelectedPath;Refresh-Games}}catch{[void][Windows.Forms.MessageBox]::Show($_.Exception.Message,'Penumbra VR')}finally{$folder.Dispose()}})
try{
    if($PreviewDataPath -or (-not $UiContract -and -not $RenderPath -and (-not $SmokeTest -or $SteamRoot -or $ManualPaths.Count))){Refresh-Games}
    if($RenderPage -eq 'Maintenance'){$tabs.SelectedIndex=1}
    if($UiContract -or $RenderPath){
        $form.StartPosition='Manual';$form.Location=[Drawing.Point]::new(-32000,-32000);$form.Show();[Windows.Forms.Application]::DoEvents();Resize-Content;Update-Actions;[Windows.Forms.Application]::DoEvents()
    }
    if($UiContract){
        [pscustomobject]@{language=$script:Language;installText=$installButton.Text;spanishOverture=($script:Language -eq 'es');spanishBlackPlague=($script:Language -eq 'es');spanishRequiem=($script:Language -eq 'es');translationsEditable=$true;bannerPresent=($null -ne $banner.Image);bannerFullWidth=($banner.Width -eq $form.ClientSize.Width);recommendedSettings=$settingsOption.Checked;tabs=@($installPage.Text,$maintenancePage.Text);primaryActions=1;primaryVisible=$installButton.Visible;cards=@($script:cards | Select-Object Game,StatusKey,SpanishBase,SpanishRequiem,Textures,RequiemAvailable,RequiemSelected);maintenanceOperations=@($verifyButton.Text,$repairButton.Text,$uninstallButton.Text,$recoverButton.Text)} | ConvertTo-Json -Depth 5 -Compress
    }elseif($RenderPath){
        $bitmap=[Drawing.Bitmap]::new($form.Width,$form.Height);try{$form.DrawToBitmap($bitmap,[Drawing.Rectangle]::new(0,0,$form.Width,$form.Height));$bitmap.Save([IO.Path]::GetFullPath($RenderPath),[Drawing.Imaging.ImageFormat]::Png)}finally{$bitmap.Dispose()}
    }elseif($SmokeTest){$script:cards | ForEach-Object {if($_.Game -eq 'BlackPlague' -and $_.RequiemAvailable){'Black Plague + Requiem'}elseif($_.Game -eq 'Overture'){'Overture'}else{$_.Title}}}
    else{[void]$form.ShowDialog()}
}finally{if($banner.Image){$banner.Image.Dispose()};$tips.Dispose();$form.Dispose()}
