$ErrorActionPreference='Stop'
$repo=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$scriptPath=Join-Path $repo 'products/overture/scripts/build.ps1'
$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile($scriptPath,[ref]$tokens,[ref]$errors)
$function=$ast.Find({param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Get-BuildInputFingerprint'},$true)
if(-not $function -or $errors.Count){throw 'Cannot read build fingerprint function.'}
. ([scriptblock]::Create($function.Extent.Text))
$temp=Join-Path ([IO.Path]::GetTempPath()) ('PvrBuildGuard-'+[guid]::NewGuid().ToString('N'))
try{
    foreach($folder in @('product/build','src/runtime','src/backends/overture','src/adapters/overture_source')){New-Item -ItemType Directory -Path (Join-Path $temp $folder) -Force | Out-Null}
    $header=Join-Path $temp 'product/layout.h';$body=Join-Path $temp 'product/body.cpp';$generated=Join-Path $temp 'product/build/generated.h';$props=Join-Path $temp 'src/adapters/overture_source/bridge.props'
    foreach($file in @($header,$body,$generated,$props)){[IO.File]::WriteAllText($file,'initial')}
    $product=Join-Path $temp 'product'
    $before=Get-BuildInputFingerprint $product $temp
    [IO.File]::SetLastWriteTimeUtc($body,[DateTime]::UtcNow.AddMinutes(1))
    [IO.File]::SetLastWriteTimeUtc($generated,[DateTime]::UtcNow.AddMinutes(1))
    if((Get-BuildInputFingerprint $product $temp) -cne $before){throw 'Source body/generated files spuriously force full rebuild.'}
    [IO.File]::SetLastWriteTimeUtc($header,[DateTime]::UtcNow.AddMinutes(1))
    $after=Get-BuildInputFingerprint $product $temp
    if($after -ceq $before){throw 'Header layout change did not force full rebuild.'}
    [IO.File]::SetLastWriteTimeUtc($props,[DateTime]::UtcNow.AddMinutes(1))
    if((Get-BuildInputFingerprint $product $temp) -ceq $after){throw 'Bridge compiler flags did not force full rebuild.'}
    Write-Host 'Overture build guard: body/generated files ignored; headers and compiler properties invalidate full-build stamp.'
}finally{if(Test-Path -LiteralPath $temp){Remove-Item -LiteralPath $temp -Recurse -Force}}
