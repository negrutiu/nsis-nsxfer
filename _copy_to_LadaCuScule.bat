@echo off

REM :: Note: mingw binaries are the most backward compatible (NT4+)
set DST=%~dp0\..\LadaCuScule
set SRCA=%~dp0\ReleaseA-mingw
set SRCW=%~dp0\ReleaseW-mingw
set SRC64=%~dp0\ReleaseW-mingw-amd64


set DSTA=%DST%\NSIS.extra\NSxfer
mkdir "%DSTA%" 2> NUL
echo on
	copy "%SRCA%\NSxfer.dll" "%DSTA%"
	copy "%SRCA%\NSxfer.pdb" "%DSTA%"
	copy "%~dp0\NSxfer.Readme.txt" "%DSTA%"
@echo off

set DSTW=%DST%\NSISw.extra\NSxfer
mkdir "%DSTW%" 2> NUL
echo on
	copy "%SRCW%\NSxfer.dll" "%DSTW%"
	copy "%SRCW%\NSxfer.pdb" "%DSTW%"
	copy "%~dp0\NSxfer.Readme.txt" "%DSTW%"
@echo off

set DST64=%DST%\NSISamd64.extra\NSxfer
mkdir "%DST64%" 2> NUL
echo on
	copy "%SRC64%\NSxfer.dll" "%DST64%"
	copy "%SRC64%\NSxfer.pdb" "%DST64%"
	copy "%~dp0\NSxfer.Readme.txt" "%DST64%"
@echo off

pause