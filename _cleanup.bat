REM :: Marius Negrutiu (marius.negrutiu@protonmail.com)

@echo off
echo.

cd /d "%~dp0"

call "%cd%\Test\cleanup.bat"
call :CLEANUP
call :CLEANUP
call :CLEANUP
goto :EOF


:CLEANUP
rd /S /Q .vs
rd /S /Q ipch

rd /S /Q build
rd /S /Q nsis

del *.aps
del *.bak
::del *.user
del *.ncb
del /AH *.suo
del *.sdf
del *.VC.db
