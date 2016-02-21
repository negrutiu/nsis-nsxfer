@echo off
set SRCA=%~dp0\ReleaseA-mingw
set SRCW=%~dp0\ReleaseW-mingw
set DST=%~dp0\..\LadaCuScule

set DSTA=%DST%\NSIS.extra\NSxfer
if not exist "%DSTA%" mkdir "%DSTA%"
echo on
copy "%SRCA%\NSxfer.dll" "%DSTA%"
copy "%SRCA%\NSxfer.pdb" "%DSTA%"
copy "%~dp0\NSxfer.Readme.txt" "%DSTA%"
@echo off

set DSTW=%DST%\NSISw.extra\NSxfer
if not exist "%DSTW%" mkdir "%DSTW%"
echo on
copy "%SRCW%\NSxfer.dll" "%DSTW%"
copy "%SRCW%\NSxfer.pdb" "%DSTW%"
copy "%~dp0\NSxfer.Readme.txt" "%DSTW%"
@echo off

pause