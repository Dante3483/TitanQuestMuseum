@echo off
setlocal
if not exist "%~dp0museum\build\test-temp" mkdir "%~dp0museum\build\test-temp"
set "TEMP=%~dp0museum\build\test-temp"
set "TMP=%TEMP%"
call "%~dp0core\tools\test.bat"
if errorlevel 1 exit /b 1
call "%~dp0museum\tools\test.bat"
if errorlevel 1 exit /b 1
call "%~dp0farm-radar\tools\test.bat"
exit /b %ERRORLEVEL%
