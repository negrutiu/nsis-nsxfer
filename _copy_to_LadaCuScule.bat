@set SRC=%~dp0\ReleaseA-nocrt
@set DST=%~dp0\..\LadaCuScule\NSIS.extra\NSxfer
@if not exist "%DST%" mkdir "%DST%"
copy "%SRC%\NSxfer.dll" "%DST%"
copy "%SRC%\NSxfer.pdb" "%DST%"
copy "%~dp0\NSxfer.Readme.txt" "%DST%"

@set SRC=%~dp0\ReleaseW-nocrt
@set DST=%~dp0\..\LadaCuScule\NSISw.extra\NSxfer
@if not exist "%DST%" mkdir "%DST%"
copy "%SRC%\NSxfer.dll" "%DST%"
copy "%SRC%\NSxfer.pdb" "%DST%"
copy "%~dp0\NSxfer.Readme.txt" "%DST%"

pause