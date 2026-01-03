@echo off
setlocal EnableDelayedExpansion

cd /d "%~dp0"

set packdir=build\package
set packfile=%~dp0build\NSxfer.7z

rmdir /S /Q %packdir% > nul 2> nul
mkdir %packdir%\amd64-unicode
mkdir %packdir%\x86-unicode
mkdir %packdir%\x86-ansi

echo on
mklink /h %packdir%\amd64-unicode\NSxfer.dll        build\Release-mingw-amd64-unicode\NSxfer.dll || exit /b !errorlevel!
mklink /h %packdir%\x86-unicode\NSxfer.dll          build\Release-mingw-x86-unicode\NSxfer.dll || exit /b !errorlevel!
mklink /h %packdir%\x86-ansi\NSxfer.dll             build\Release-mingw-x86-ansi\NSxfer.dll || exit /b !errorlevel!
mklink /h %packdir%\NSxfer.Readme.txt               NSxfer.Readme.txt || exit /b !errorlevel!
mklink /h %packdir%\README.md                       README.md || exit /b !errorlevel!
mklink /h %packdir%\LICENSE.md                      LICENSE.md || exit /b !errorlevel!
@echo off

set PATH=%PATH%;%PROGRAMFILES%\7-Zip
pushd "%packdir%"
7z a "%packfile%" * -r
popd

rem pause
