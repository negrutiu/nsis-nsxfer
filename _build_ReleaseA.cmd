@echo off
SetLocal

set OUTDIR=ReleaseA
set OUTNAME=NSdown

set BUILD_SUCCESSFUL=0

if defined PROGRAMFILES(X86) set PF=%PROGRAMFILES(X86)%
if not defined PROGRAMFILES(X86) set PF=%PROGRAMFILES%

set VCVARSALL=%PF%\Microsoft Visual Studio 12.0\VC\VcVarsAll.bat
if exist "%VCVARSALL%" goto :BUILD

set VCVARSALL=%PF%\Microsoft Visual Studio 11.0\VC\VcVarsAll.bat
if exist "%VCVARSALL%" goto :BUILD

set VCVARSALL=%PF%\Microsoft Visual Studio 10.0\VC\VcVarsAll.bat
if exist "%VCVARSALL%" goto :BUILD

echo ERROR: Can't find Visual Studio 2010/2012/2013
pause
goto :EOF

:BUILD
call "%VCVARSALL%" x86

if not exist "%~dp0\%OUTDIR%" mkdir "%~dp0\%OUTDIR%"
if not exist "%~dp0\%OUTDIR%\temp" mkdir "%~dp0\%OUTDIR%\temp"

set CL=/nologo /O1 /Ob2 /Os /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_USRDLL" /D "_WINDLL" /D "_MBCS" /GF /FD /MT /LD /GS- /Fo".\%OUTDIR%\temp\\" /Fd".\%OUTDIR%\temp\\" /Fe".\%OUTDIR%\%OUTNAME%" /W3
rc.exe /Fo ".\%OUTDIR%\temp\resource.res" "resource.rc"
set LINK=/INCREMENTAL:NO /MANIFEST:NO /MACHINE:X86 /NODEFAULTLIB kernel32.lib user32.lib wininet.lib shlwapi.lib ".\%OUTDIR%\temp\resource.res"
cl.exe "main.c" "utils.c" "queue.c" "thread.c" "nsiswapi\pluginapi.c" && set BUILD_SUCCESSFUL=1

if %BUILD_SUCCESSFUL%==1 (
	echo Success!
	::pause
) else (
	pause
)