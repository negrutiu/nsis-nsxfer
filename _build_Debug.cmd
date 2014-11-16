@echo off
SetLocal

set BUILD_SOLUTION=%~dp0\NSxfer.sln
set BUILD_CONFIG=Debug

if defined PROGRAMFILES(X86) set PF=%PROGRAMFILES(X86)%
if not defined PROGRAMFILES(X86) set PF=%PROGRAMFILES%

set VCVARSALL=%PF%\Microsoft Visual Studio 12.0\VC\VcVarsAll.bat
set BUILD_PLATFORMTOOLSET=v120_xp
if exist "%VCVARSALL%" goto :BUILD

set VCVARSALL=%PF%\Microsoft Visual Studio 11.0\VC\VcVarsAll.bat
set BUILD_PLATFORMTOOLSET=v110_xp
if exist "%VCVARSALL%" goto :BUILD

set VCVARSALL=%PF%\Microsoft Visual Studio 10.0\VC\VcVarsAll.bat
set BUILD_PLATFORMTOOLSET=v100
if exist "%VCVARSALL%" goto :BUILD

echo ERROR: Can't find Visual Studio 2010/2012/2013
pause
goto :EOF

:BUILD
call "%VCVARSALL%" x86

msbuild /m /t:build "%BUILD_SOLUTION%" /p:Configuration=%BUILD_CONFIG%A /p:Platform=Win32 /p:PlatformToolset=%BUILD_PLATFORMTOOLSET% /nologo /verbosity:normal
if %ERRORLEVEL% neq 0 ( pause && goto :EOF )

msbuild /m /t:build "%BUILD_SOLUTION%" /p:Configuration=%BUILD_CONFIG%W /p:Platform=Win32 /p:PlatformToolset=%BUILD_PLATFORMTOOLSET% /nologo /verbosity:normal
if %ERRORLEVEL% neq 0 ( pause && goto :EOF )

msbuild /m /t:build "%BUILD_SOLUTION%" /p:Configuration=%BUILD_CONFIG%A-CRT /p:Platform=Win32 /p:PlatformToolset=%BUILD_PLATFORMTOOLSET% /nologo /verbosity:normal
if %ERRORLEVEL% neq 0 ( pause && goto :EOF )

msbuild /m /t:build "%BUILD_SOLUTION%" /p:Configuration=%BUILD_CONFIG%W-CRT /p:Platform=Win32 /p:PlatformToolset=%BUILD_PLATFORMTOOLSET% /nologo /verbosity:normal
if %ERRORLEVEL% neq 0 ( pause && goto :EOF )
