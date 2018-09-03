@echo off

REM :: Note: mingw binaries are the most backward compatible (NT4+)
set DST=%~dp0\..\NSbuild\msys2\tests
set SRCA=%~dp0\ReleaseA-mingw
set SRCW=%~dp0\ReleaseW-mingw
set SRC64=%~dp0\ReleaseW-mingw-amd64


set DSTA=%DST%\Plugins\x86-ansi
mkdir "%DSTA%" 2> NUL
echo on
	copy "%SRCA%\NSxfer.dll" "%DSTA%"
	copy "%SRCA%\NSxfer.pdb" "%DSTA%"
	copy "%~dp0\NSxfer.Readme.txt" "%DSTA%"
@echo off

set DSTW=%DST%\Plugins\x86-unicode
mkdir "%DSTW%" 2> NUL
echo on
	copy "%SRCW%\NSxfer.dll" "%DSTW%"
	copy "%SRCW%\NSxfer.pdb" "%DSTW%"
	copy "%~dp0\NSxfer.Readme.txt" "%DSTW%"
@echo off

set DST64=%DST%\Plugins\amd64-unicode
mkdir "%DST64%" 2> NUL
echo on
	copy "%SRC64%\NSxfer.dll" "%DST64%"
	copy "%SRC64%\NSxfer.pdb" "%DST64%"
	copy "%~dp0\NSxfer.Readme.txt" "%DST64%"
@echo off

pause