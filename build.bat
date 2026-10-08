@echo off
setlocal
call "%~dp0core\build.bat"
if errorlevel 1 exit /b 1
call "%~dp0museum\build.bat"
if errorlevel 1 exit /b 1
call "%~dp0mob-radar\build.bat"
if errorlevel 1 exit /b 1
call "%~dp0museum-radar\build.bat"
if errorlevel 1 exit /b 1
if not exist "%~dp0dist" mkdir "%~dp0dist"
if errorlevel 1 exit /b 1
for %%C in (core museum mob-radar museum-radar) do (
    copy /b /y "%~dp0%%C\dist\*.asi" "%~dp0dist\" >nul
    if errorlevel 1 exit /b 1
)
xcopy "%~dp0museum\dist\localization\*" "%~dp0dist\localization\" /E /I /Y >nul
if errorlevel 1 exit /b 1
echo Published all ASI modules and Museum localization to %~dp0dist
exit /b 0
