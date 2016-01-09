@echo off

cd /d "%~dp0"

del "%CD%\Test\*.exe"
del "%CD%\Test2\*.exe"
call :CLEANUP
call :CLEANUP
call :CLEANUP
goto :EOF


:CLEANUP
rd /S /Q .vs
rd /S /Q ipch

for /D %%a in (Debug*) do rd /S /Q "%%a"
for /D %%a in (Release*) do rd /S /Q "%%a"
for /D %%a in (objchk*) do rd /S /Q "%%a"
for /D %%a in (objfre*) do rd /S /Q "%%a"

del build*.err
del build*.log
del build*.wrn
del temp.c

del *.aps
del *.bak
::del *.user
del *.ncb
del /AH *.suo
del *.sdf
del *.VC.db
