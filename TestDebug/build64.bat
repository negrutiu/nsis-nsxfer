@echo off
setlocal enabledelayedexpansion

if not exist "%NSIS%\makensis.exe" pushd "%~dp0..\.." && set NSIS=!cd!&& popd
if not exist "%NSIS%\makensis.exe" set NSIS=%PROGRAMFILES%\NSIS
if not exist "%NSIS%\makensis.exe" set NSIS=%PROGRAMFILES(X86)%\NSIS
if not exist "%NSIS%\makensis.exe" for /f "delims=*" %%f in ('where makensis.exe 2^> nul') do pushd "%%~dpf" && set NSIS=!cd!&& popd
if not exist "%NSIS%\makensis.exe" echo ERROR: NSIS not found&& pause && exit /b 2

echo ********************************************************************************
echo %NSIS%\makensis.exe
echo ********************************************************************************

"%NSIS%\makensis.exe" /V4 /DAMD64 "%~dp0\NSxfer-Debug.nsi" || pause && exit /b !errorlevel!
