REM :: Marius Negrutiu (marius.negrutiu@protonmail.com)

@echo off
setlocal
echo.

:CHDIR
cd /d "%~dp0"

:DEFINITIONS
for /f "delims=*" %%i in ('dir /B /OD *.sln 2^> nul') do set BUILD_SOLUTION=%%~fi
set BUILD_CONFIG=Debug
set BUILD_VERBOSITY=normal

:COMPILER
if not exist "%PF%" set PF=%PROGRAMFILES(X86)%
if not exist "%PF%" set PF=%PROGRAMFILES%
set VSWHERE=%PF%\Microsoft Visual Studio\Installer\vswhere.exe
if not exist "%VCVARSALL%" for /f "tokens=1* delims=: " %%i in ('"%VSWHERE%" -version 17 -requires Microsoft.Component.MSBuild 2^> NUL') do if /i "%%i"=="installationPath" set VCVARSALL=%%j\VC\Auxiliary\Build\VCVarsAll.bat&& set BUILD_PLATFORMTOOLSET=v143
if not exist "%VCVARSALL%" for /f "tokens=1* delims=: " %%i in ('"%VSWHERE%" -version 16 -requires Microsoft.Component.MSBuild 2^> NUL') do if /i "%%i"=="installationPath" set VCVARSALL=%%j\VC\Auxiliary\Build\VCVarsAll.bat&& set BUILD_PLATFORMTOOLSET=v142
if not exist "%VCVARSALL%" for /f "tokens=1* delims=: " %%i in ('"%VSWHERE%" -version 15 -requires Microsoft.Component.MSBuild 2^> NUL') do if /i "%%i"=="installationPath" set VCVARSALL=%%j\VC\Auxiliary\Build\VCVarsAll.bat&& set BUILD_PLATFORMTOOLSET=v141
if not exist "%VCVARSALL%" set VCVARSALL=%PF%\Microsoft Visual Studio 14.0\VC\VcVarsAll.bat&& set BUILD_PLATFORMTOOLSET=v140
if not exist "%VCVARSALL%" set VCVARSALL=%PF%\Microsoft Visual Studio 12.0\VC\VcVarsAll.bat&& set BUILD_PLATFORMTOOLSET=v120
if not exist "%VCVARSALL%" set VCVARSALL=%PF%\Microsoft Visual Studio 11.0\VC\VcVarsAll.bat&& set BUILD_PLATFORMTOOLSET=v110
if not exist "%VCVARSALL%" set VCVARSALL=%PF%\Microsoft Visual Studio 10.0\VC\VcVarsAll.bat&& set BUILD_PLATFORMTOOLSET=v100
if not exist "%VCVARSALL%" echo ERROR: Can't find Visual Studio 2010-2022 && pause && exit /b 2

:BUILD
pushd "%CD%"
call "%VCVARSALL%" x86
popd

title %BUILD_CONFIG%-x86-ansi&&    msbuild /m /t:build "%BUILD_SOLUTION%" /p:Configuration=%BUILD_CONFIG%-x86-ansi    /p:Platform=Win32 /p:PlatformToolset=%BUILD_PLATFORMTOOLSET% /p:WindowsTargetPlatformVersion=%WindowsSDKVersion% /nologo /verbosity:%BUILD_VERBOSITY% || pause && exit /b !errorlevel!
title %BUILD_CONFIG%-x86-unicode&& msbuild /m /t:build "%BUILD_SOLUTION%" /p:Configuration=%BUILD_CONFIG%-x86-unicode /p:Platform=Win32 /p:PlatformToolset=%BUILD_PLATFORMTOOLSET% /p:WindowsTargetPlatformVersion=%WindowsSDKVersion% /nologo /verbosity:%BUILD_VERBOSITY% || pause && exit /b !errorlevel!
