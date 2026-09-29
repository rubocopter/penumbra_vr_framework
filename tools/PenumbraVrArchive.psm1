$ErrorActionPreference='Stop'
function Write-PvrArchive {
    param([string]$Root,[string]$OutputPath)
    Add-Type -AssemblyName System.IO.Compression
    $rootPath=[IO.Path]::GetFullPath($Root).TrimEnd('\')
    foreach($item in @((Get-Item -LiteralPath $rootPath))+@(Get-ChildItem -LiteralPath $rootPath -Recurse -Force)){if($item.Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Archive input contains a reparse point.'}}
    $stream=[IO.File]::Open([IO.Path]::GetFullPath($OutputPath),[IO.FileMode]::CreateNew)
    try{
        $zip=[IO.Compression.ZipArchive]::new($stream,[IO.Compression.ZipArchiveMode]::Create,$false)
        try{
            foreach($file in Get-ChildItem -LiteralPath $rootPath -Recurse -File | Sort-Object FullName){
                $name=$file.FullName.Substring($rootPath.Length).TrimStart('\').Replace('\','/')
                $entry=$zip.CreateEntry($name,[IO.Compression.CompressionLevel]::Optimal)
                $entry.LastWriteTime=[DateTimeOffset]::new(2000,1,1,0,0,0,[TimeSpan]::Zero)
                $target=$entry.Open();$source=[IO.File]::OpenRead($file.FullName)
                try{$source.CopyTo($target)}finally{$source.Dispose();$target.Dispose()}
            }
        }finally{$zip.Dispose()}
    }finally{$stream.Dispose()}
}
Export-ModuleMember -Function Write-PvrArchive
