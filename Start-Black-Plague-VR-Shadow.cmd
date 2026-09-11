@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\Start-BlackPlagueShadowValidation.ps1" %*
set "PVR_RESULT=%ERRORLEVEL%"
if not "%PVR_RESULT%"=="0" pause
exit /b %PVR_RESULT%
