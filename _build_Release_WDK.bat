@echo off

:: This script builds the project using WDK6 + W2K
:: The sln/vcxproj files are ignored


cd /d "%~dp0"
set WDK_CONFIG=fre

set WDK6=C:\WDK6
set WDK7=C:\WDK7
if not exist "%WDK6%" ( echo ERROR: Can't find WDK6. && pause && goto :EOF )
::if not exist "%WDK7%" ( echo ERROR: Can't find WDK7. && pause && goto :EOF )

:: Create temp.c (to include .c files from subdirectories)
echo #include "nsiswapi\pluginapi.c" > "%~dp0\temp.c"


echo ---------------------------------
echo ANSI
echo ---------------------------------
%COMSPEC% /C "call %WDK6%\bin\setenv.bat %WDK6%\ %WDK_CONFIG% x86 W2K && cd /d """%cd%""" && set C_DEFINES=/D_MBCS && build -gcewZ && echo."
::%COMSPEC% /C "call %WDK7%\bin\setenv.bat %WDK7%\ %WDK_CONFIG% x86 WXP no_oacr && cd /d """%cd%""" && set C_DEFINES=/D_MBCS && build -gcewZ && echo."
if %ERRORLEVEL% neq 0 ( echo ERRORLEVEL = %err% && pause && goto :EOF )

set OUTDIR=%cd%\ReleaseA-wdk
if not exist "%OUTDIR%" mkdir "%OUTDIR%"
copy /Y "%~dp0\obj%WDK_CONFIG%_w2k_x86\i386\NSxfer.dll" "%OUTDIR%\"
copy /Y "%~dp0\obj%WDK_CONFIG%_w2k_x86\i386\NSxfer.pdb" "%OUTDIR%\"

for /D %%a in (obj%WDK_CONFIG%_*) do rd /S /Q "%%a"
del build*.log


echo ---------------------------------
echo Unicode
echo ---------------------------------
%COMSPEC% /C "call %WDK6%\bin\setenv.bat %WDK6%\ %WDK_CONFIG% x86 W2K && cd /d """%cd%""" && set C_DEFINES=/D_UNICODE /DUNICODE && build -gcewZ && echo."
::%COMSPEC% /C "call %WDK7%\bin\setenv.bat %WDK7%\ %WDK_CONFIG% x86 WXP no_oacr && cd /d """%cd%""" && set C_DEFINES=/D_MBCS && build -gcewZ && echo."
if %ERRORLEVEL% neq 0 ( echo ERRORLEVEL = %err% && pause && goto :EOF )

set OUTDIR=%cd%\ReleaseW-wdk
if not exist "%OUTDIR%" mkdir "%OUTDIR%"
copy /Y "%~dp0\obj%WDK_CONFIG%_w2k_x86\i386\NSxfer.dll" "%OUTDIR%\"
copy /Y "%~dp0\obj%WDK_CONFIG%_w2k_x86\i386\NSxfer.pdb" "%OUTDIR%\"

for /D %%a in (obj%WDK_CONFIG%_*) do rd /S /Q "%%a"
del build*.log

:: Cleanup
del temp.c

::pause
