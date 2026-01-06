REM :: Marius Negrutiu (marius.negrutiu@protonmail.com)

@echo off
setlocal EnableDelayedExpansion

if "%config%" equ "" set config=%~1
if "%config%" equ "" set config=Release

for %%d in ("%MINGW32_INSTDIR%" %SYSTEMDRIVE%\mingw32 %SYSTEMDRIVE%\msys64\mingw32 "") do (
  if not exist "!MINGW32!\bin\gcc.exe" set MINGW32=%%~d
)
for %%d in ("%MINGW64_INSTDIR%" %SYSTEMDRIVE%\mingw64 %SYSTEMDRIVE%\msys64\mingw64 "") do (
  if not exist "!MINGW64!\bin\gcc.exe" set MINGW64=%%~d
)
for %%d in (%SYSTEMDRIVE%\msys64\usr\bin %SYSTEMDRIVE%\cygwin64\bin "%PROGRAMFILES%\Git\usr\bin" "") do (
  if not exist "!posix_shell!\grep.exe" set posix_shell=%%~d
)

set ORIGINAL_PATH=%PATH%

cd /d "%~dp0"

:x86
set PATH=%MINGW32%\bin;%posix_shell%;%ORIGINAL_PATH%

echo.
echo -------------------------------------------------------------------
set OUTDIR=build\%config%-mingw-x86-ansi
echo %OUTDIR%
title %OUTDIR%
echo -------------------------------------------------------------------
mingw32-make.exe ARCH=X86 CHAR=ANSI CONFIG=%config% OUTDIR=%OUTDIR% -fMakefile.mingw clean all || pause && exit /b !errorlevel!

echo.
echo -------------------------------------------------------------------
set OUTDIR=build\%config%-mingw-x86-unicode
echo %OUTDIR%
title %OUTDIR%
echo -------------------------------------------------------------------
mingw32-make.exe ARCH=X86 CHAR=Unicode CONFIG=%config% OUTDIR=%OUTDIR% -fMakefile.mingw clean all || pause && exit /b !errorlevel!


:amd64
set PATH=%MINGW64%\bin;%posix_shell%;%ORIGINAL_PATH%

echo.
echo -------------------------------------------------------------------
set OUTDIR=build\%config%-mingw-amd64-unicode
echo %OUTDIR%
title %OUTDIR%
echo -------------------------------------------------------------------
mingw32-make.exe ARCH=X64 CHAR=Unicode CONFIG=%config% OUTDIR=%OUTDIR% -fMakefile.mingw clean all || pause && exit /b !errorlevel!

echo.
REM pause
