@echo off
setlocal
set "ROOT=%~dp0.."
call "%~dp0find_vcvars.bat"
if errorlevel 1 exit /b 1
if not defined VSCMD_ARG_TGT_ARCH for %%V in ("%VCVARS%") do call "%%~dpVvcvarsall.bat" amd64_x86 >nul
if not exist "%ROOT%\build\test" mkdir "%ROOT%\build\test"
cl /utf-8 /nologo /W4 /WX /EHsc /std:c++17 /MT /DNOMINMAX /D_CRT_SECURE_NO_WARNINGS /I"%ROOT%\src" /I"%ROOT%\..\core\src" /I"%ROOT%\..\core\include" ^
 /Fo"%ROOT%\build\test\\" /Fe"%ROOT%\build\test\farm_source_test.exe" ^
 "%ROOT%\..\core\tests\farm_source_test.cpp" "%ROOT%\..\core\src\game\archive\loot_sources.cpp" ^
 "%ROOT%\..\core\src\game\archive\arz_reader.cpp" "%ROOT%\..\core\src\game\archive\inflate.cpp"
if errorlevel 1 exit /b 1
pushd "%ROOT%\build\test"
farm_source_test.exe %*
set "RESULT=%ERRORLEVEL%"
popd
exit /b %RESULT%
