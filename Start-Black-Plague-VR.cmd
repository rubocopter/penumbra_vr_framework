@echo off
setlocal
set "PVR_GAME=%ProgramFiles(x86)%\Steam\steamapps\common\Penumbra Black Plague\redist\penumbra.exe"
if not "%~1"=="" set "PVR_GAME=%~1"
set "PVR_LAUNCHER=%~dp0build\bin\Release\PenumbraVR.ProbeLauncher.exe"
if not exist "%PVR_LAUNCHER%" (
  echo No se encuentra el launcher Release. Compila el framework primero.
  pause
  exit /b 5
)
"%PVR_LAUNCHER%" --launch-vr "%PVR_GAME%"
set "PVR_RESULT=%ERRORLEVEL%"
if not "%PVR_RESULT%"=="0" pause
exit /b %PVR_RESULT%
