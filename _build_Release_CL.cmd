@echo off
SetLocal

:: This script builds the project by directly calling cl.exe
:: The sln/vcxproj files are ignored
:: Suitable for Visual Studio 2008+

cd /d "%~dp0"

set OUTNAME=NSxfer
set RCNAME=resource

if defined PROGRAMFILES(X86) set PF=%PROGRAMFILES(X86)%
if not defined PROGRAMFILES(X86) set PF=%PROGRAMFILES%

set VCVARSALL=%PF%\Microsoft Visual Studio 14.0\VC\VcVarsAll.bat
if exist "%VCVARSALL%" goto :BUILD

set VCVARSALL=%PF%\Microsoft Visual Studio 12.0\VC\VcVarsAll.bat
if exist "%VCVARSALL%" goto :BUILD

set VCVARSALL=%PF%\Microsoft Visual Studio 11.0\VC\VcVarsAll.bat
if exist "%VCVARSALL%" goto :BUILD

set VCVARSALL=%PF%\Microsoft Visual Studio 10.0\VC\VcVarsAll.bat
if exist "%VCVARSALL%" goto :BUILD

set VCVARSALL=%PF%\Microsoft Visual Studio 9.0\VC\VcVarsAll.bat
if exist "%VCVARSALL%" goto :BUILD

echo ERROR: Can't find Visual Studio 2008/2010/2012/2013/2015
pause
goto :EOF

:BUILD
call "%VCVARSALL%" x86

echo -----------------------------------
echo ANSI
echo -----------------------------------
set OUTDIR=ReleaseA-nocrt-cl
call :BUILD_PARAMS
set CL=/D "_MBCS" %CL%
call :BUILD_CL
if %ERRORLEVEL% neq 0 pause && goto :EOF

echo -----------------------------------
echo Unicode
echo -----------------------------------
set OUTDIR=ReleaseW-nocrt-cl
call :BUILD_PARAMS
set CL=/D "_UNICODE" /D "UNICODE" %CL%
call :BUILD_CL
if %ERRORLEVEL% neq 0 pause && goto :EOF

goto :EOF


:BUILD_PARAMS
set CL=^
	/nologo ^
	/Zi ^
	/W3 /WX- ^
	/O2 /Os /Oy- ^
	/D WIN32 /D NDEBUG /D _WINDOWS /D _USRDLL /D _WINDLL ^
	/Gm- /EHsc /MT /GS- /arch:SSE /Gd /TC /GF /FD /LD ^
	/Fo".\%OUTDIR%\temp\\" ^
	/Fd".\%OUTDIR%\temp\\" ^
	/Fe".\%OUTDIR%\%OUTNAME%"

set LINK=^
	/NOLOGO ^
	/NODEFAULTLIB ^
	/DYNAMICBASE /NXCOMPAT /SAFESEH ^
	/DEBUG ^
	/OPT:REF ^
	/OPT:ICF ^
	/INCREMENTAL:NO ^
	/MANIFEST:NO ^
	/MACHINE:X86 ^
	/ENTRY:"DllMain" ^
	kernel32.lib advapi32.lib ole32.lib uuid.lib user32.lib shlwapi.lib version.lib wininet.lib ^
	".\%OUTDIR%\temp\%RCNAME%.res"

set FILES=^
	"main.c" ^
	"gui.c" ^
	"queue.c" ^
	"thread.c" ^
	"utils.c" ^
	"nsiswapi\pluginapi.c"

goto :EOF


:BUILD_CL
if not exist "%~dp0\%OUTDIR%"      mkdir "%~dp0\%OUTDIR%"
if not exist "%~dp0\%OUTDIR%\temp" mkdir "%~dp0\%OUTDIR%\temp"

echo %RCNAME%.rc
rc.exe ^
	/l"0x0409" ^
	/Fo".\%OUTDIR%\temp\%RCNAME%.res" ^
	"%RCNAME%.rc"
echo ERRORLEVEL == %ERRORLEVEL%

cl.exe %FILES%
echo ERRORLEVEL == %ERRORLEVEL%

goto :EOF
