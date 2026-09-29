param([string]$BuildRoot,[switch]$X86,[string]$FixtureRoot)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if(-not $BuildRoot){$BuildRoot=Join-Path $repo 'build'}
if(-not $X86){
    $fixture=Join-Path ([IO.Path]::GetTempPath()) ('PvrOpenAl-'+[guid]::NewGuid().ToString('N'))
    try{
        & (Join-Path $env:WINDIR 'SysWOW64/WindowsPowerShell/v1.0/powershell.exe') -NoProfile -ExecutionPolicy Bypass -File $PSCommandPath -BuildRoot ([IO.Path]::GetFullPath($BuildRoot)) -FixtureRoot $fixture -X86
        if($LASTEXITCODE -ne 0){throw 'x86 OpenAL proxy loader test failed.'}
    }finally{if(Test-Path -LiteralPath $fixture){Remove-Item -LiteralPath $fixture -Recurse -Force}}
    return
}
$temp=[IO.Path]::GetFullPath($FixtureRoot)
if(-not $temp.StartsWith([IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\')+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Unsafe audio fixture path.'}
try{
    New-Item -ItemType Directory -Path $temp | Out-Null
    Copy-Item -LiteralPath (Join-Path $BuildRoot 'bin/Release/PenumbraVR.BlackPlague.OpenALProxy.dll') -Destination (Join-Path $temp 'OpenAL32.dll')
    Copy-Item -LiteralPath (Join-Path $repo 'products/overture/dependencies/bin/win32/OpenAL32.dll') -Destination (Join-Path $temp 'PenumbraVR_OpenALSoft.dll')
    # Null audio exercises the actual x86 loader/API without requiring a headset
    # or sound device; this does not claim audible/gameplay validation.
    $env:ALSOFT_DRIVERS='null'
    Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class AudioProxyTest {
  [DllImport("kernel32", CharSet=CharSet.Unicode)] public static extern bool SetDllDirectory(string path);
  [DllImport("kernel32", CharSet=CharSet.Unicode)] public static extern IntPtr LoadLibrary(string path);
  [DllImport("kernel32", CharSet=CharSet.Ansi)] public static extern IntPtr GetProcAddress(IntPtr module,string name);
  [DllImport("kernel32")] public static extern bool FreeLibrary(IntPtr module);
  [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate IntPtr Open([MarshalAs(UnmanagedType.LPStr)] string name);
  [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate bool Close(IntPtr device);
  [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate IntPtr Proc(IntPtr device,[MarshalAs(UnmanagedType.LPStr)] string name);
}
'@
    # Game executables live in this same directory. Reproduce that app-local
    # search boundary inside the dedicated PowerShell host.
    if(-not [AudioProxyTest]::SetDllDirectory($temp)){throw 'Could not set fixture DLL directory.'}
    $module=[AudioProxyTest]::LoadLibrary((Join-Path $temp 'OpenAL32.dll'))
    if($module -eq [IntPtr]::Zero){throw 'Could not load the app-local x86 proxy.'}
    try{
        $openPointer=[AudioProxyTest]::GetProcAddress($module,'alcOpenDevice')
        $open=[Runtime.InteropServices.Marshal]::GetDelegateForFunctionPointer($openPointer,[AudioProxyTest+Open])
        $close=[Runtime.InteropServices.Marshal]::GetDelegateForFunctionPointer([AudioProxyTest]::GetProcAddress($module,'alcCloseDevice'),[AudioProxyTest+Close])
        $proc=[Runtime.InteropServices.Marshal]::GetDelegateForFunctionPointer([AudioProxyTest]::GetProcAddress($module,'alcGetProcAddress'),[AudioProxyTest+Proc])
        foreach($name in @('Generic Software','Generic Hardware','')){
            $device=$open.Invoke($name)
            if($device -eq [IntPtr]::Zero){throw "Legacy device name failed: $name"}
            try{if($proc.Invoke($device,'alcOpenDevice') -ne $openPointer){throw 'Dynamic lookup bypassed device compatibility.'}}
            finally{[void]$close.Invoke($device)}
        }
        foreach($name in @('alGenSources','alBufferData','alGenEffects','alcCreateContext')){
            if([AudioProxyTest]::GetProcAddress($module,$name) -eq [IntPtr]::Zero){throw "Forwarded OpenAL symbol missing: $name"}
        }
    }finally{[void][AudioProxyTest]::FreeLibrary($module)}
    Write-Host 'Actual app-local x86 OpenAL proxy: legacy device names, dynamic lookup and forwarded symbols passed (null audio).'
}finally{ # The parent removes the fixture after this x86 process exits.
}
