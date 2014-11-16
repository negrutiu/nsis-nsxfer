@echo off

%~d0
cd "\%~p0"

rem  >>> Delete everything twice! <<<
call :CLEANUP
call :CLEANUP
del "%~dp0\Test\*.exe"
goto :EOF


:CLEANUP
for /D %%a in (Debug*) do rd /S /Q "%%a"
for /D %%a in (Release*) do rd /S /Q "%%a"
rd /S /Q ipch

attrib -H *.suo /S /D
del *.suo
attrib -R *.ncb /S /D
del *.ncb
del *.user
del *.bak
del *.sdf
del *.aps
