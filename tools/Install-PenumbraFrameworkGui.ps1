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
$form.Size = [System.Drawing.Size]::new(680, 430)
$form.MinimumSize = [System.Drawing.Size]::new(620, 380)
$form.StartPosition = 'CenterScreen'
$form.Font = [System.Drawing.Font]::new('Segoe UI', 10)

$heading = [System.Windows.Forms.Label]::new()
$heading.Text = 'Juegos encontrados'
$heading.Location = [System.Drawing.Point]::new(18, 17)
$heading.AutoSize = $true
$form.Controls.Add($heading)

$list = [System.Windows.Forms.CheckedListBox]::new()
$list.Location = [System.Drawing.Point]::new(18, 48)
$list.Size = [System.Drawing.Size]::new(625, 205)
$list.Anchor = 'Top,Left,Right,Bottom'
$list.CheckOnClick = $true
$list.HorizontalScrollbar = $true
$form.Controls.Add($list)

$status = [System.Windows.Forms.Label]::new()
$status.Text = 'Se comprobarán las versiones exactas antes de modificar archivos.'
$status.Location = [System.Drawing.Point]::new(18, 264)
$status.Size = [System.Drawing.Size]::new(625, 43)
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
        if (-not $entry.Installable -or $entry.Game -eq 'Requiem') { continue }
        if ($entry.Game -notin @('Overture', 'Black Plague')) { continue }
        $label = $entry.Game
        if ($entry.Game -eq 'Black Plague' -and
            @($found | Where-Object {
                $_.Game -eq 'Requiem' -and $_.Installable -and
                (Split-Path -Parent $_.Path) -ieq (Split-Path -Parent $entry.Path)
            }).Count -eq 1) {
            $label = 'Black Plague + Requiem'
        }
        $script:groups += [PSCustomObject]@{
            Game = if ($entry.Game -eq 'Overture') { 'Overture' } else { 'BlackPlague' }
            Path = $entry.Path
            Label = $label
        }
        [void]$list.Items.Add("$label  —  $($entry.InstallRoot)", $true)
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
    $verb = switch ($Operation) {
        'Restore' { 'desinstalar' }
        'Repair' { 'reparar' }
        default { 'instalar' }
    }
    $names = ($selected | ForEach-Object { $_.Label }) -join ', '
    $answer = [System.Windows.Forms.MessageBox]::Show(
        "¿Quieres $verb $names?", 'Penumbra VR',
        [System.Windows.Forms.MessageBoxButtons]::YesNo,
        [System.Windows.Forms.MessageBoxIcon]::Question)
    if ($answer -ne [System.Windows.Forms.DialogResult]::Yes) { return }
    $controls = @($list, $installButton, $repairButton, $restoreButton,
        $refreshButton, $browseButton)
    foreach ($control in $controls) { $control.Enabled = $false }
    $form.UseWaitCursor = $true
    $status.Text = "Procesando $names..."
    [System.Windows.Forms.Application]::DoEvents()
    try {
        foreach ($item in $selected) {
            $arguments = @{ Game = $item.Game; GamePath = $item.Path }
            if ($Operation -eq 'Restore') { $arguments.Restore = $true }
            if ($Operation -eq 'Repair') { $arguments.Repair = $true }
            & $selector @arguments | Out-Null
        }
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

$installButton.Add_Click({ Invoke-Selected 'Install' })
$repairButton.Add_Click({ Invoke-Selected 'Repair' })
$restoreButton.Add_Click({ Invoke-Selected 'Restore' })
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
