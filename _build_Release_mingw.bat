REM :: Marius Negrutiu (marius.negrutiu@protonmail.com)

@echo off
echo.

set MINGW=%SYSTEMDRIVE%\TDM-GCC-64
if not exist "%MINGW%\mingwvars.bat" echo ERROR: "%MINGW%\mingwvars.bat" not found && pause && goto :EOF
if not exist "%MINGW%\bin\mingw32-make.exe" echo ERROR: "%MINGW%\bin\mingw32-make.exe" not found && pause && goto :EOF
call "%MINGW%\mingwvars.bat"

cd /d "%~dp0"

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

::echo.
::echo -------------------------------------------------------------------
::echo ReleaseW (x64)
::echo -------------------------------------------------------------------
::mingw32-make.exe ARCH=X64 CHAR=Unicode OUTDIR=ReleaseW-mingw-x64 -fMakefile.mingw clean all
::if %ERRORLEVEL% neq 0 echo ERRORLEVEL == %ERRORLEVEL% && pause && goto :EOF

echo.
::pause
