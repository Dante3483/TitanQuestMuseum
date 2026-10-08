@echo off
setlocal
set "ROOT=%~dp0.."
call "%~dp0find_vcvars.bat"
if errorlevel 1 exit /b 1
if not defined VSCMD_ARG_TGT_ARCH for %%V in ("%VCVARS%") do call "%%~dpVvcvarsall.bat" amd64_x86 >nul
if not exist "%ROOT%\build\test" mkdir "%ROOT%\build\test"
cl /utf-8 /nologo /W4 /WX /EHsc /std:c++17 /MT /DWIN32_LEAN_AND_MEAN /DNOMINMAX /I"%ROOT%\src" /I"%ROOT%\..\core\src" /I"%ROOT%\..\core\include" ^
 /Fo"%ROOT%\build\test\\" /Fe"%ROOT%\build\test\addon_host_test.exe" ^
 "%ROOT%\..\core\tests\addon_host_test.cpp" "%ROOT%\..\core\src\game\addon_host.cpp"
if errorlevel 1 exit /b 1
"%ROOT%\build\test\addon_host_test.exe"
exit /b %ERRORLEVEL%
