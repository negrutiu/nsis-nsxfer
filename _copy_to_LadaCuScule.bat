mkdir "%~dp0\..\LadaCuScule\NSIS.extra\NSxfer"
copy "%~dp0\ReleaseA\NSxfer.dll" "%~dp0\..\LadaCuScule\NSIS.extra\NSxfer"
copy "%~dp0\ReleaseA\NSxfer.pdb" "%~dp0\..\LadaCuScule\NSIS.extra\NSxfer"
copy "%~dp0\NSxfer.Readme.txt"   "%~dp0\..\LadaCuScule\NSIS.extra\NSxfer"

mkdir "%~dp0\..\LadaCuScule\NSISw.extra\NSxfer"
copy "%~dp0\ReleaseW\NSxfer.dll" "%~dp0\..\LadaCuScule\NSISw.extra\NSxfer"
copy "%~dp0\ReleaseW\NSxfer.pdb" "%~dp0\..\LadaCuScule\NSISw.extra\NSxfer"
copy "%~dp0\NSxfer.Readme.txt"   "%~dp0\..\LadaCuScule\NSISw.extra\NSxfer"

pause