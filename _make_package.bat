@echo off

cd /d "%~dp0"

if not exist Release-mingw-amd64-unicode\NSxfer.dll		echo ERROR: Missing Release-mingw-amd64-unicode\NSxfer.dll && exit /b 2
if not exist Release-mingw-x86-ansi\NSxfer.dll			echo ERROR: Missing Release-mingw-x86-ansi\NSxfer.dll      && exit /b 2
if not exist Release-mingw-x86-unicode\NSxfer.dll		echo ERROR: Missing Release-mingw-x86-unicode\NSxfer.dll   && exit /b 2

set PATH=%PATH%;%PROGRAMFILES%\7-Zip

REM :: Read version from the .rc file
for /f usebackq^ tokens^=3^ delims^=^"^,^  %%f in (`type Resource.rc ^| findstr /r /c:"\s*\"FileVersion\"\s*"`) do set RCVER=%%f

rmdir /S /Q _Package > nul 2> nul
mkdir _Package
mkdir _Package\amd64-unicode
mkdir _Package\x86-unicode
mkdir _Package\x86-ansi

mklink /H _Package\amd64-unicode\NSxfer.dll			Release-mingw-amd64-unicode\NSxfer.dll
mklink /H _Package\x86-unicode\NSxfer.dll			Release-mingw-x86-unicode\NSxfer.dll
mklink /H _Package\x86-ansi\NSxfer.dll				Release-mingw-x86-ansi\NSxfer.dll
mklink /H _Package\NSxfer.Readme.txt				NSxfer.Readme.txt
mklink /H _Package\README.md						README.md
mklink /H _Package\LICENSE.md						LICENSE.md

pushd _Package
7z a "..\NSxfer-%RCVER%.7z" * -r
popd

echo.
rem pause

rmdir /S /Q _Package > nul 2> nul
