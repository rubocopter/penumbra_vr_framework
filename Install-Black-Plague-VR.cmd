@echo off
setlocal
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\Install-BlackPlagueSteamBootstrap.ps1" %*
set "PVR_RESULT=%ERRORLEVEL%"
if not "%PVR_RESULT%"=="0" pause
exit /b %PVR_RESULT%
