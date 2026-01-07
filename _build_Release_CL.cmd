REM :: Marius Negrutiu (marius.negrutiu@protonmail.com)

@echo off
setlocal EnableDelayedExpansion
echo.

:: This script builds the project by directly calling cl.exe
:: The sln/vcxproj files are ignored

cd /d "%~dp0"

if "%config%" equ "" set config=%~1
if "%config%" equ "" set config=Release

for /f "delims=*" %%i in ('dir /b /od *.sln 2^> nul') do set solution=%%~fi

rem | ------------------------------------------------------------

if not exist "%PF%" set PF=%PROGRAMFILES(X86)%
if not exist "%PF%" set PF=%PROGRAMFILES%
set VSWHERE=%PF%\Microsoft Visual Studio\Installer\vswhere.exe
if not exist "%vcvarsall%" for /f "tokens=1* delims=: " %%i in ('"%VSWHERE%" -version 17 -requires Microsoft.Component.MSBuild 2^> NUL') do if /i "%%i"=="installationPath" set vcvarsall=%%j\VC\Auxiliary\Build\VCVarsAll.bat&& set toolset=v143
if not exist "%vcvarsall%" for /f "tokens=1* delims=: " %%i in ('"%VSWHERE%" -version 16 -requires Microsoft.Component.MSBuild 2^> NUL') do if /i "%%i"=="installationPath" set vcvarsall=%%j\VC\Auxiliary\Build\VCVarsAll.bat&& set toolset=v142
if not exist "%vcvarsall%" for /f "tokens=1* delims=: " %%i in ('"%VSWHERE%" -version 15 -requires Microsoft.Component.MSBuild 2^> NUL') do if /i "%%i"=="installationPath" set vcvarsall=%%j\VC\Auxiliary\Build\VCVarsAll.bat&& set toolset=v141
if not exist "%vcvarsall%" set vcvarsall=%PF%\Microsoft Visual Studio 14.0\VC\VcVarsAll.bat&& set toolset=v140
if not exist "%vcvarsall%" set vcvarsall=%PF%\Microsoft Visual Studio 12.0\VC\VcVarsAll.bat&& set toolset=v120
if not exist "%vcvarsall%" set vcvarsall=%PF%\Microsoft Visual Studio 11.0\VC\VcVarsAll.bat&& set toolset=v110
if not exist "%vcvarsall%" set vcvarsall=%PF%\Microsoft Visual Studio 10.0\VC\VcVarsAll.bat&& set toolset=v100
if not exist "%vcvarsall%" echo ERROR: Can't find Visual Studio 2010-2022 && pause && exit /b 2

rem | ------------------------------------------------------------

:pluginapi
call py -3 _get_nsis_sdk.py || exit /b !errorlevel!

:environment
set OUTNAME=NSxfer
set RCNAME=resource

:BUILD
pushd "%cd%"
call "%vcvarsall%" x86
popd

echo -----------------------------------
set OUTDIR=build\Release-cl-x86-ansi
echo %OUTDIR%
echo -----------------------------------
set BUILD_MACHINE=X86
call :BUILD_PARAMS
set CL=/D "_MBCS" /arch:SSE %CL%
set LINK=/MACHINE:X86 /SAFESEH %LINK%
call :BUILD_CL
if %errorlevel% neq 0 pause && exit /b %errorlevel%

echo -----------------------------------
set OUTDIR=build\Release-cl-x86-unicode
echo %OUTDIR%
echo -----------------------------------
call :BUILD_PARAMS
set CL=/D "_UNICODE" /D "UNICODE" /arch:SSE %CL%
set LINK=/MACHINE:X86 /SAFESEH %LINK%
call :BUILD_CL
if %errorlevel% neq 0 pause && exit /b %errorlevel%

:BUILD64
pushd "%cd%"
call "%vcvarsall%" amd64
popd

echo -----------------------------------
set OUTDIR=build\Release-cl-amd64-unicode
echo %OUTDIR%
echo -----------------------------------
call :BUILD_PARAMS
set CL=/D "_UNICODE" /D "UNICODE" %CL%
set LINK=/MACHINE:AMD64 %LINK%
call :BUILD_CL
if %errorlevel% neq 0 pause && exit /b %errorlevel%

:: Finish
exit /b 0


:BUILD_PARAMS
set CL=^
	/nologo ^
	/Zi ^
	/W3 /WX- ^
	/O2 /Os /Oy- ^
	/D WIN32 /D NDEBUG /D _WINDOWS /D _USRDLL /D _WINDLL ^
	/Gm- /EHsc /MT /GS- /Gd /TC /GF /FD /LD ^
	/Fo".\%OUTDIR%\temp\\" ^
	/Fd".\%OUTDIR%\temp\\" ^
	/Fe".\%OUTDIR%\%OUTNAME%" ^
	/I.

set LINK=^
	/NOLOGO ^
	/NODEFAULTLIB ^
	/DYNAMICBASE /NXCOMPAT ^
	/DEBUG ^
	/OPT:REF ^
	/OPT:ICF ^
	/INCREMENTAL:NO ^
	/MANIFEST:NO ^
	/ENTRY:"DllMain" ^
	kernel32.lib advapi32.lib ole32.lib uuid.lib user32.lib shlwapi.lib version.lib wininet.lib ^
	".\%OUTDIR%\temp\%RCNAME%.res"

set FILES=^
	"main.c" ^
	"gui.c" ^
	"queue.c" ^
	"thread.c" ^
	"utils.c" ^
	"nsis\pluginapi.c"

exit /b 0


:BUILD_CL
title %OUTDIR%
echo.
mkdir "%~dp0\%OUTDIR%" 2> nul
mkdir "%~dp0\%OUTDIR%\temp" 2> nul

echo %RCNAME%.rc
rc.exe /l"0x0409" /Fo".\%OUTDIR%\temp\%RCNAME%.res" "%RCNAME%.rc"
if %errorlevel% neq 0 exit /b %errorlevel%

cl.exe %FILES%
if %errorlevel% neq 0 exit /b %errorlevel%

exit /b 0
