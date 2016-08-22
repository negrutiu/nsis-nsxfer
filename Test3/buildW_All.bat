@echo off
set NSIS_PATH=%~dp0\..\..\NSISbin

"%NSIS_PATH%\makensis.exe" /V4 /DUSE_NSXFER "%~dp0\HttpFlood.nsi"
if %ERRORLEVEL% neq 0 pause && goto :EOF
move /Y "HttpFlood.exe" "HttpFlood-NSxfer.exe"

"%NSIS_PATH%\makensis.exe" /V4 /DUSE_NSISDL "%~dp0\HttpFlood.nsi"
if %ERRORLEVEL% neq 0 pause && goto :EOF
move /Y "HttpFlood.exe" "HttpFlood-NSISdl.exe"

"%NSIS_PATH%\makensis.exe" /V4 /DUSE_NSinetc "%~dp0\HttpFlood.nsi"
if %ERRORLEVEL% neq 0 pause && goto :EOF
move /Y "HttpFlood.exe" "HttpFlood-NSinetc.exe"

:pause