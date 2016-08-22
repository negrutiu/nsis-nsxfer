@echo off
set NSIS_PATH=%~dp0\..\..\NSISbin
"%NSIS_PATH%\makensis.exe" /V4 "%~dp0\HttpFlood.nsi"
if %ERRORLEVEL% neq 0 pause && goto :EOF

echo ----------------------------------------------------------
set /P answer=Execute HttpFlood.exe (y/N)? 
if /I "%answer%" equ "y" "%~dp0\HttpFlood.exe"
