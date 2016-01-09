set SRC=%~dp0\ReleaseA-wdk
set DST=%~dp0\..\LadaCuScule\NSIS.extra\NSxfer
mkdir "%DST%"
copy "%SRC%\NSxfer.dll" "%DST%"
copy "%SRC%\NSxfer.pdb" "%DST%"
copy "%~dp0\NSxfer.Readme.txt" "%DST%"

set SRC=%~dp0\ReleaseW-wdk
set DST=%~dp0\..\LadaCuScule\NSISw.extra\NSxfer
mkdir "%DST%"
copy "%SRC%\NSxfer.dll" "%DST%"
copy "%SRC%\NSxfer.pdb" "%DST%"
copy "%~dp0\NSxfer.Readme.txt" "%DST%"

pause