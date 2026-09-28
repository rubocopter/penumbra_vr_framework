param([string]$Root,[string[]]$Games=@('black_plague','requiem'))
$repo=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$manifest=Get-Content (Join-Path $repo 'assets/deployment/prerequisites.json') -Raw | ConvertFrom-Json
foreach($game in $Games) {
    foreach($relative in $manifest.gameFiles.$game) {
        $path=Join-Path $Root $relative
        New-Item -ItemType Directory -Path (Split-Path -Parent $path) -Force | Out-Null
        $text=if($relative.EndsWith('default_settings.cfg')){'<Map File="fixture.dae" />'}elseif($relative.EndsWith('.cfg')){'<Main />'}else{'<LANGUAGE />'}
        [IO.File]::WriteAllText($path,$text)
    }
    $maps=if($game -eq 'requiem'){'expansion01/maps'}else{'maps'}
    New-Item -ItemType Directory -Path (Join-Path $Root $maps) -Force | Out-Null
    [IO.File]::WriteAllText((Join-Path $Root "$maps/fixture.dae"),'<COLLADA />')
}
