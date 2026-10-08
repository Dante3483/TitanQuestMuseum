@echo off
setlocal
call "%~dp0core\build.bat"
if errorlevel 1 exit /b 1
call "%~dp0museum\build.bat"
if errorlevel 1 exit /b 1
call "%~dp0farm-radar\build.bat"
exit /b %ERRORLEVEL%
