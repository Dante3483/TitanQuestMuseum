@echo off
setlocal
set "ROOT=%~dp0.."
call "%~dp0find_vcvars.bat"
if errorlevel 1 exit /b 1
if not defined VSCMD_ARG_TGT_ARCH for %%V in ("%VCVARS%") do call "%%~dpVvcvarsall.bat" amd64_x86 >nul
if not exist "%ROOT%\build\test" mkdir "%ROOT%\build\test"
cl /nologo /utf-8 /O2 /W4 /WX /EHsc /std:c++17 /MT /DWIN32_LEAN_AND_MEAN /DNOMINMAX /D_CRT_SECURE_NO_WARNINGS /DTQT_CORE_TEST ^
 /I"%ROOT%\src" /I"%ROOT%\include" /I"%ROOT%\third_party\minhook\include" /Fo"%ROOT%\build\test\\" /Fe"%ROOT%\build\test\source_service_test.exe" ^
 "%ROOT%\tests\source_service_test.cpp" "%ROOT%\src\runtime.cpp" "%ROOT%\src\game\addon_host.cpp" ^
 "%ROOT%\dist\TitanQuestCore.lib" "%ROOT%\build\obj\buffer.obj" "%ROOT%\build\obj\hook.obj" "%ROOT%\build\obj\trampoline.obj" "%ROOT%\build\obj\hde32.obj" ^
 /link kernel32.lib user32.lib psapi.lib
if errorlevel 1 exit /b 1
"%ROOT%\build\test\source_service_test.exe" %*
exit /b %ERRORLEVEL%
