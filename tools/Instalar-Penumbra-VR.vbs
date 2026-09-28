Option Explicit
Dim shell, filesystem, script, command
Set shell = CreateObject("WScript.Shell")
Set filesystem = CreateObject("Scripting.FileSystemObject")
script = filesystem.BuildPath(filesystem.GetParentFolderName(WScript.ScriptFullName), "Install-PenumbraFrameworkGui.ps1")
command = "powershell.exe -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File " & Chr(34) & script & Chr(34)
shell.Run command, 0, False
