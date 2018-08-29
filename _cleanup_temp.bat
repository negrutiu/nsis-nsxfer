REM :: Marius Negrutiu (marius.negrutiu@protonmail.com)

@echo off
echo.

cd /d "%~dp0"

for /D %%a in (Debug*)   do call :CLEANUP_TEMP "%%a"
for /D %%a in (Release*) do call :CLEANUP_TEMP "%%a"

echo.
pause
exit /B 0

:CLEANUP_TEMP
echo %~1

REM :: mingw specific
del "%~1\*.o" 2> NUL
del "%~1\*.a" 2> NUL
del "%~1\*.def" 2> NUL

REM :: MSVC specific
rmdir /S /Q "%~1\temp" 2> NUL
del "%~1\*.res" 2> NUL
del "%~1\*.ilk" 2> NUL
del "%~1\*.exp" 2> NUL

exit /B 0