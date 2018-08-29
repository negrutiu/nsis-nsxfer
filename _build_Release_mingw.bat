REM :: Marius Negrutiu (marius.negrutiu@protonmail.com)

@echo off
echo.

if not exist "%MSYS2%" set MSYS2=C:\msys2
if not exist "%MSYS2%" set MSYS2=C:\msys64
set MINGW32=%MSYS2%\mingw32
set MINGW64=%MSYS2%\mingw64
set ORIGINAL_PATH=%PATH%

cd /d "%~dp0"

:x86
if not exist "%MINGW32%" echo ERROR: Missing "%MINGW32%" && pause && exit /B 2
set PATH=%MINGW32%\bin;%ORIGINAL_PATH%

echo.
echo -------------------------------------------------------------------
echo ReleaseA (x86)
echo -------------------------------------------------------------------
mingw32-make.exe ARCH=X86 CHAR=ANSI OUTDIR=ReleaseA-mingw -fMakefile.mingw clean all
if %ERRORLEVEL% neq 0 echo ERRORLEVEL == %ERRORLEVEL% && pause && goto :EOF

echo.
echo -------------------------------------------------------------------
echo ReleaseW (x86)
echo -------------------------------------------------------------------
mingw32-make.exe ARCH=X86 CHAR=Unicode OUTDIR=ReleaseW-mingw -fMakefile.mingw clean all
if %ERRORLEVEL% neq 0 echo ERRORLEVEL == %ERRORLEVEL% && pause && goto :EOF


:amd64
if not exist "%MINGW64%" echo ERROR: Missing "%MINGW64%" && pause && exit /B 2
set PATH=%MINGW64%\bin;%ORIGINAL_PATH%

echo.
echo -------------------------------------------------------------------
echo ReleaseW (amd64)
echo -------------------------------------------------------------------
mingw32-make.exe ARCH=X64 CHAR=Unicode OUTDIR=ReleaseW-mingw-amd64 -fMakefile.mingw clean all
if %ERRORLEVEL% neq 0 echo ERRORLEVEL == %ERRORLEVEL% && pause && goto :EOF

echo.
REM pause
