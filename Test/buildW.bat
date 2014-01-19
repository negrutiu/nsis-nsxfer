@echo off
set NSIS_PATH=%~dp0\..\..\NSISbin
"%NSIS_PATH%\makensis.exe" /V4 "%~dp0\Test.nsi"
if %ERRORLEVEL% neq 0 pause
