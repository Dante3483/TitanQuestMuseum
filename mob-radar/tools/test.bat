@echo off
setlocal
set "ROOT=%~dp0.."
call "%~dp0find_vcvars.bat"
if errorlevel 1 exit /b 1
if not defined VSCMD_ARG_TGT_ARCH for %%V in ("%VCVARS%") do call "%%~dpVvcvarsall.bat" amd64_x86 >nul
if not exist "%ROOT%\build\test" mkdir "%ROOT%\build\test"
cl /utf-8 /nologo /W4 /WX /EHsc /std:c++17 /MT /DNOMINMAX /I"%ROOT%\src" ^
 /Fo"%ROOT%\build\test\\" /Fe"%ROOT%\build\test\farm_radar_test.exe" ^
 "%ROOT%\tests\farm_radar_test.cpp" "%ROOT%\src\ui\farm_radar_panel.cpp" "%ROOT%\src\ui\text_renderer.cpp"
if errorlevel 1 exit /b 1
"%ROOT%\build\test\farm_radar_test.exe"
if errorlevel 1 exit /b 1
exit /b %ERRORLEVEL%
