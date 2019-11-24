@echo off

cd /d "%~dp0"

if not exist Release-mingw-amd64-unicode\NSxfer.dll		echo ERROR: Missing Release-mingw-amd64-unicode\NSxfer.dll && pause && exit /B 2
if not exist Release-mingw-x86-ansi\NSxfer.dll			echo ERROR: Missing Release-mingw-x86-ansi\NSxfer.dll      && pause && exit /B 2
if not exist Release-mingw-x86-unicode\NSxfer.dll		echo ERROR: Missing Release-mingw-x86-unicode\NSxfer.dll   && pause && exit /B 2

set Z7=%PROGRAMFILES%\7-Zip\7z.exe
if not exist "%Z7%" echo ERROR: Missing %Z7% && pause && exit /B 2

REM :: Read version from the .rc file
for /f usebackq^ tokens^=3^ delims^=^"^,^  %%f in (`type Resource.rc ^| findstr /r /c:"\s*\"FileVersion\"\s*"`) do set RCVER=%%f

rmdir /S /Q _Package > NUL 2> NUL
mkdir _Package
mkdir _Package\amd64-unicode
mkdir _Package\x86-unicode
mkdir _Package\x86-ansi

mklink /H _Package\amd64-unicode\NSxfer.dll			Release-mingw-amd64-unicode\NSxfer.dll
mklink /H _Package\x86-unicode\NSxfer.dll			Release-mingw-x86-unicode\NSxfer.dll
mklink /H _Package\x86-ansi\NSxfer.dll				Release-mingw-x86-ansi\NSxfer.dll
mklink /H _Package\NSxfer.Readme.txt				NSxfer.Readme.txt

pushd _Package
"%Z7%" a "..\NSxfer-%RCVER%.7z" * -r
popd

echo.
pause

rmdir /S /Q _Package > NUL 2> NUL
