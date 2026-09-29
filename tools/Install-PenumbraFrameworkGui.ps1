[CmdletBinding()]
param(
    [switch]$SmokeTest,
    [string]$SteamRoot,
    [string[]]$ManualPaths = @()
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
[System.Windows.Forms.Application]::EnableVisualStyles()

$selector = Join-Path $PSScriptRoot 'Install-PenumbraFrameworkCandidate.ps1'
$discovery = Join-Path $PSScriptRoot 'Get-PenumbraInstallations.ps1'
if (-not (Test-Path -LiteralPath $selector -PathType Leaf) -or
    -not (Test-Path -LiteralPath $discovery -PathType Leaf)) {
    throw 'Installer package is incomplete.'
}

$form = [System.Windows.Forms.Form]::new()
$form.Text = 'Penumbra VR Framework'
$form.Size = [System.Drawing.Size]::new(780, 635)
$form.MinimumSize = [System.Drawing.Size]::new(760, 610)
$form.StartPosition = 'CenterScreen'
$form.Font = [System.Drawing.Font]::new('Segoe UI', 10)

$heading = [System.Windows.Forms.Label]::new()
$heading.Text = 'Juegos encontrados'
$heading.Location = [System.Drawing.Point]::new(18, 17)
$heading.AutoSize = $true
$form.Controls.Add($heading)

$list = [System.Windows.Forms.CheckedListBox]::new()
$list.Location = [System.Drawing.Point]::new(18, 48)
$list.Size = [System.Drawing.Size]::new(725, 205)
$list.Anchor = 'Top,Left,Right'
$list.CheckOnClick = $true
$list.HorizontalScrollbar = $true
$form.Controls.Add($list)

$status = [System.Windows.Forms.Label]::new()
$status.Text = 'Se comprobarán las versiones exactas antes de modificar archivos.'
$status.Location = [System.Drawing.Point]::new(18, 409)
$status.Size = [System.Drawing.Size]::new(725, 70)
$status.Anchor = 'Left,Right,Bottom'
$form.Controls.Add($status)

$installButton = [System.Windows.Forms.Button]::new()
$installButton.Text = 'Instalar / actualizar'
$installButton.Location = [System.Drawing.Point]::new(18, 322)
$installButton.Size = [System.Drawing.Size]::new(155, 34)
$installButton.Anchor = 'Left,Bottom'
$form.Controls.Add($installButton)

$repairButton = [System.Windows.Forms.Button]::new()
$repairButton.Text = 'Reparar'
$repairButton.Location = [System.Drawing.Point]::new(184, 322)
$repairButton.Size = [System.Drawing.Size]::new(100, 34)
$repairButton.Anchor = 'Left,Bottom'
$form.Controls.Add($repairButton)

$restoreButton = [System.Windows.Forms.Button]::new()
$restoreButton.Text = 'Desinstalar'
$restoreButton.Location = [System.Drawing.Point]::new(295, 322)
$restoreButton.Size = [System.Drawing.Size]::new(110, 34)
$restoreButton.Anchor = 'Left,Bottom'
$form.Controls.Add($restoreButton)

$refreshButton = [System.Windows.Forms.Button]::new()
$refreshButton.Text = 'Actualizar lista'
$refreshButton.Location = [System.Drawing.Point]::new(416, 322)
$refreshButton.Size = [System.Drawing.Size]::new(135, 34)
$refreshButton.Anchor = 'Left,Bottom'
$form.Controls.Add($refreshButton)

$browseButton = [System.Windows.Forms.Button]::new()
$browseButton.Text = 'Carpeta...'
$browseButton.Location = [System.Drawing.Point]::new(562, 322)
$browseButton.Size = [System.Drawing.Size]::new(90, 34)
$browseButton.Anchor = 'Left,Bottom'
$form.Controls.Add($browseButton)
foreach($button in @($installButton,$repairButton,$restoreButton,$refreshButton,$browseButton)){$button.Location=[Drawing.Point]::new($button.Location.X,490)}
function Option([string]$text,[int]$y,[bool]$checked=$false){$c=[Windows.Forms.CheckBox]::new();$c.Text=$text;$c.Location=[Drawing.Point]::new(18,$y);$c.Size=[Drawing.Size]::new(725,26);$c.Checked=$checked;$form.Controls.Add($c);$c}
$requiemOption=Option 'Incluir soporte de Requiem donde exista una expansion valida' 268 $true
$translationOption=Option 'Traducciones comunitarias al espanol (opcional)' 296
$textureOption=Option 'Texturas mejoradas de Overture (opcional)' 324
foreach($option in @($translationOption,$textureOption)){$option.ThreeState=$true;$option.CheckState='Indeterminate';$option.Text+='; gris: conservar seleccion anterior'}
$settingsOption=Option 'Ajustes graficos recomendados: cambios concretos en defaults y perfil actual' 352
function Action([string]$text,[int]$x){$b=[Windows.Forms.Button]::new();$b.Text=$text;$b.Location=[Drawing.Point]::new($x,537);$b.Size=[Drawing.Size]::new(155,34);$b.Anchor='Left,Bottom';$form.Controls.Add($b);$b}
$recoverButton=Action 'Recuperar' 18
$verifyButton=Action 'Verificar archivos' 184
$diagnosticsButton=Action 'Diagnostico ZIP' 350
$removeRequiemButton=Action 'Quitar solo Requiem' 516

$script:groups = @()
$script:manualPaths = @($ManualPaths)
function Refresh-Games {
    # -List verifies every package checksum before displaying installable rows.
    $searchArgs = @{}
    if ($SteamRoot) { $searchArgs.SteamRoot = $SteamRoot }
    if ($script:manualPaths.Count) { $searchArgs.ManualPaths = $script:manualPaths }
    & $selector -List @searchArgs | Out-Null
    $found = @(& $discovery @searchArgs)
    $list.Items.Clear()
    $script:groups = @()
    foreach ($entry in $found) {
        if ($entry.Game -eq 'Requiem') { continue }
        $product=if($entry.Game -in @('Overture','Black Plague')){$entry.Game}elseif($entry.ManagedProduct){$entry.ManagedProduct}else{'Unknown'}
        $label = $product
        if ($entry.Game -eq 'Black Plague' -and
            @($found | Where-Object {
                $_.Game -eq 'Requiem' -and $_.Installable -and
                (Split-Path -Parent $_.Path) -ieq (Split-Path -Parent $entry.Path)
            }).Count -eq 1) {
            $label = 'Black Plague + Requiem'
        }
        $script:groups += [PSCustomObject]@{
            Game = if ($product -eq 'Overture') { 'Overture' } elseif($product -eq 'Black Plague'){'BlackPlague'}else{$null}
            Path = $entry.Path
            Label = $label
            Installable = $entry.Installable
            Managed = $entry.Managed
            NeedsRecovery = $entry.NeedsRecovery
            Issues = @($entry.Issues)
            Status = $entry.Status
        }
        [void]$list.Items.Add("$label [$($entry.Status)] - $($entry.InstallRoot)", [bool]$entry.Installable)
    }
    if (-not $script:groups.Count) {
        $status.Text = 'No se encontró ninguna versión compatible. No se ha modificado ningún juego.'
    } else {
        $status.Text = 'Selecciona los juegos y pulsa Instalar. Cierra los juegos antes de continuar.'
    }
}

function Invoke-Selected([string]$Operation) {
    $selected = @($list.CheckedIndices | ForEach-Object { $script:groups[[int]$_] })
    if (-not $selected.Count) {
        [void][System.Windows.Forms.MessageBox]::Show(
            'Selecciona al menos un juego.', 'Penumbra VR')
        return
    }
    if(@($selected | Where-Object {-not $_.Game}).Count){[void][Windows.Forms.MessageBox]::Show('Version desconocida: no se puede modificar. Revisa la ruta y los archivos en Steam.','Penumbra VR');return}
    if($Operation -eq 'Install' -and @($selected | Where-Object {-not $_.Installable}).Count){[void][Windows.Forms.MessageBox]::Show(($selected.Issues -join "`n"),'Destino incompleto');return}
    $verb = switch ($Operation) {
        'Restore' { 'desinstalar' }
        'Repair' { 'reparar' }
        'Recover' { 'recuperar' }
        'RemoveRequiem' { 'quitar solo Requiem de' }
        default { 'instalar' }
    }
    $names = ($selected | ForEach-Object { $_.Label }) -join ', '
    if($Operation -in @('Verify','Diagnostics')){
        try{
            foreach($item in $selected){
                if($Operation -eq 'Verify'){$report=& $selector -Verify -Game $item.Game -GamePath $item.Path;[void][Windows.Forms.MessageBox]::Show(($report.Checks | ForEach-Object {"$($_.Name): $($_.Status). $($_.Remediation)"}) -join "`n",'Verificacion de archivos')}
                else{$dialog=[Windows.Forms.SaveFileDialog]::new();$dialog.Filter='ZIP (*.zip)|*.zip';$dialog.FileName='PenumbraVR-diagnostics.zip';try{if($dialog.ShowDialog($form) -eq 'OK'){& $selector -Game $item.Game -GamePath $item.Path -DiagnosticOutputPath $dialog.FileName | Out-Null}}finally{$dialog.Dispose()}}
            }
        }catch{[void][Windows.Forms.MessageBox]::Show($_.Exception.Message,'Penumbra VR')}
        return
    }
    $batchArgs=$null;$preview=@()
    try{
        if($Operation -in @('Install','Restore')){
            $manual=@($selected.Path)
            $isolated=Join-Path $PSScriptRoot 'no-steam-discovery'
            $rows=@(& $discovery -SteamRoot $isolated -ManualPaths $manual)
            $indices=@();for($i=0;$i -lt $rows.Count;$i++){if($rows[$i].Path -in $manual){$indices+=([string]($i+1))}}
            $batchArgs=@{SteamRoot=$isolated;ManualPaths=$manual;Selections=$indices}
            if($Operation -eq 'Restore'){$batchArgs.Restore=$true}else{
                $batchArgs.RequiemMode=if($requiemOption.Checked){'Auto'}else{'Disable'}
                if($translationOption.CheckState -ne 'Indeterminate'){$batchArgs.CommunityTranslations=$translationOption.Checked}
                if($textureOption.CheckState -ne 'Indeterminate'){$batchArgs.TextureEnhancements=$textureOption.Checked}
                $batchArgs.RecommendedSettings=$settingsOption.Checked
            }
            $preview=@(& $selector @batchArgs -Plan)
        }elseif($Operation -ne 'Recover'){
            foreach($item in $selected){$a=@{Game=$item.Game;GamePath=$item.Path;Plan=$true};if($Operation -eq 'Repair'){$a.Repair=$true}else{if($item.Game -ne 'BlackPlague'){throw 'Quitar Requiem requiere seleccionar su base Black Plague.'};$a.Game='Requiem';$a.Restore=$true};$preview+=@(& $selector @a)}
        }
    }catch{[void][Windows.Forms.MessageBox]::Show($_.Exception.Message,'Preflight: no se modifico ningun juego');return}
    $review=[Windows.Forms.Form]::new();$review.Text="Revisar: $verb $names";$review.Size=[Drawing.Size]::new(760,540);$review.StartPosition='CenterParent'
    $details=[Windows.Forms.TextBox]::new();$details.Multiline=$true;$details.ReadOnly=$true;$details.ScrollBars='Both';$details.WordWrap=$false;$details.Dock='Fill'
    $lines=@('Archivos y cambios previstos. Partidas, idioma, resolucion y calibracion quedan fuera del ajuste recomendado.','')
    foreach($p in $preview){$lines+="$($p.Game): $($p.Path)";$lines+=@($p.Payload.Files | ForEach-Object {"  $_"});foreach($cfg in @($p.Payload.Configuration)){$lines+="  Ajustes: $($cfg.Path)";$lines+=@($cfg.Changes | ForEach-Object {"    $($_.Section).$($_.Attribute): $($_.Original) -> $($_.Applied)"})};$lines+=''}
    if($Operation -eq 'Recover'){$lines+='Restaurar la instantanea verificada de la operacion interrumpida.'}
    $details.Text=$lines -join "`r`n";$review.Controls.Add($details)
    $buttons=[Windows.Forms.FlowLayoutPanel]::new();$buttons.Dock='Bottom';$buttons.Height=45
    foreach($choice in @('OK','Cancel')){$b=[Windows.Forms.Button]::new();$b.Text=if($choice -eq 'OK'){'Aplicar'}else{'Cancelar'};$b.DialogResult=$choice;$buttons.Controls.Add($b)}
    $review.Controls.Add($buttons);try{$answer=$review.ShowDialog($form)}finally{$review.Dispose()}
    if($answer -ne 'OK'){return}
    $controls = @($list, $installButton, $repairButton, $restoreButton,
        $refreshButton, $browseButton,$requiemOption,$translationOption,$textureOption,$settingsOption,$recoverButton,$verifyButton,$diagnosticsButton,$removeRequiemButton)
    foreach ($control in $controls) { $control.Enabled = $false }
    $form.UseWaitCursor = $true
    $status.Text = "Procesando $names..."
    [System.Windows.Forms.Application]::DoEvents()
    try {
        if($batchArgs){& $selector @batchArgs | Out-Null}else{foreach ($item in $selected) {
            $arguments = @{ Game = $item.Game; GamePath = $item.Path }
            if ($Operation -eq 'Restore') { $arguments.Restore = $true }
            if ($Operation -eq 'Repair') { $arguments.Repair = $true }
            if ($Operation -eq 'Recover') { $arguments.Recover = $true }
            if ($Operation -eq 'RemoveRequiem') { $arguments.Game='Requiem';$arguments.Restore=$true }
            & $selector @arguments | Out-Null
        }}
        Refresh-Games
        $status.Text = "Operación completada: $names."
        [void][System.Windows.Forms.MessageBox]::Show(
            'Operación completada. Ya puedes iniciar el juego desde Steam.',
            'Penumbra VR', [System.Windows.Forms.MessageBoxButtons]::OK,
            [System.Windows.Forms.MessageBoxIcon]::Information)
    } catch {
        $status.Text = 'La operación se detuvo. Consulta el detalle y el log del instalador.'
        [void][System.Windows.Forms.MessageBox]::Show(
            $_.Exception.Message, 'Penumbra VR',
            [System.Windows.Forms.MessageBoxButtons]::OK,
            [System.Windows.Forms.MessageBoxIcon]::Error)
    } finally {
        foreach ($control in $controls) { $control.Enabled = $true }
        $form.UseWaitCursor = $false
    }
}

$list.Add_SelectedIndexChanged({if($list.SelectedIndex -ge 0){$row=$script:groups[$list.SelectedIndex];$status.Text=($row.Issues -join ' ');if(-not $status.Text){$status.Text=$row.Status}}})
$list.Add_ItemCheck({param($sender,$eventArgs) if($eventArgs.Index -lt $script:groups.Count){$row=$script:groups[$eventArgs.Index];if(-not $row.Installable -and -not $row.Managed -and -not $row.NeedsRecovery){$eventArgs.NewValue='Unchecked'}}})
$installButton.Add_Click({ Invoke-Selected 'Install' })
$repairButton.Add_Click({ Invoke-Selected 'Repair' })
$restoreButton.Add_Click({ Invoke-Selected 'Restore' })
$recoverButton.Add_Click({Invoke-Selected 'Recover'})
$verifyButton.Add_Click({Invoke-Selected 'Verify'})
$diagnosticsButton.Add_Click({Invoke-Selected 'Diagnostics'})
$removeRequiemButton.Add_Click({Invoke-Selected 'RemoveRequiem'})
$refreshButton.Add_Click({
    try { Refresh-Games }
    catch {
        [void][System.Windows.Forms.MessageBox]::Show(
            $_.Exception.Message, 'Penumbra VR',
            [System.Windows.Forms.MessageBoxButtons]::OK,
            [System.Windows.Forms.MessageBoxIcon]::Error)
    }
})
$browseButton.Add_Click({
    $folder = [System.Windows.Forms.FolderBrowserDialog]::new()
    $folder.Description = 'Selecciona la carpeta del juego o su directorio redist.'
    try {
        if ($folder.ShowDialog($form) -eq [System.Windows.Forms.DialogResult]::OK) {
            $script:manualPaths += $folder.SelectedPath
            Refresh-Games
        }
    } catch {
        [void][System.Windows.Forms.MessageBox]::Show(
            $_.Exception.Message, 'Penumbra VR',
            [System.Windows.Forms.MessageBoxButtons]::OK,
            [System.Windows.Forms.MessageBoxIcon]::Error)
    } finally { $folder.Dispose() }
})

if ($SmokeTest) {
    if ($form.Controls.Count -lt 6 -or -not $installButton.Text -or
        -not $restoreButton.Text) { throw 'Installer GUI controls were not created.' }
    if ($SteamRoot -or $ManualPaths.Count) {
        Refresh-Games
        $script:groups | ForEach-Object { Write-Output $_.Label }
    }
    $form.Dispose()
    return
}
try { Refresh-Games }
catch {
    [void][System.Windows.Forms.MessageBox]::Show(
        $_.Exception.Message, 'Penumbra VR',
        [System.Windows.Forms.MessageBoxButtons]::OK,
        [System.Windows.Forms.MessageBoxIcon]::Error)
    $form.Dispose()
    return
}
[void]$form.ShowDialog()
$form.Dispose()
