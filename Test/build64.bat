@echo off
set NSIS_PATH=%~dp0\..\..\NSISbin
"%NSIS_PATH%\makensis.exe" /DAMD64 /V4 "%~dp0\Test.nsi"
if %ERRORLEVEL% neq 0 pause

echo ----------------------------------------------------------
set /P answer=Execute NSxfer64.exe (y/N)? 
if /I "%answer%" equ "y" "%~dp0\NSxfer64.exe"
