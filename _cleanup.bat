@echo off

rem  >>> Delete everything twice! <<<
call :CLEANUP
call :CLEANUP
del "%~dp0\Test\*.exe"
goto :EOF


:CLEANUP
rd /S /Q DebugA
rd /S /Q ReleaseA
rd /S /Q DebugW
rd /S /Q ReleaseW
rd /S /Q ipch

attrib -H *.suo /S /D
del *.suo
attrib -R *.ncb /S /D
del *.ncb
del *.user
del *.bak
del *.sdf
del *.aps
