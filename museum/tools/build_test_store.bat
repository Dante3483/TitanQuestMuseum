@echo off
rem build_test_store.bat - the private table (src\game\storage_adapter.cpp) and backend/deposit_rules.h, offline. NO GAME NEEDED.
rem TQ port of GD's tools\build_test_store.bat: x86, and the whole run stays inside
rem build\test\store-out (the test sets %TITANQUESTMUSEUM_OUT% to it before any path call, and %UNIQUETAB_SAVEDATA% to a fake SaveData folder under it).
setlocal
set "ROOT=%~dp0.."
set "OUT=%ROOT%\build\test"
set "JOUT=%OUT%\store-out-%RANDOM%"
if not exist "%OUT%" mkdir "%OUT%"
mkdir "%JOUT%"

if "%VSCMD_ARG_TGT_ARCH%"=="x86" goto :have_env
call "%ROOT%\tools\find_vcvars.bat"
if errorlevel 1 (echo [test] no x86 C++ toolchain - see the message above & exit /b 1)
call "%VCVARS%" >nul
:have_env

cl /utf-8 /nologo /W4 /WX /EHsc /std:c++17 /GR- /MT /DNDEBUG /D_CRT_SECURE_NO_WARNINGS ^
   /DWIN32_LEAN_AND_MEAN /DNOMINMAX /I"%ROOT%\src" /I"%ROOT%\..\core\src" /I"%ROOT%\..\core\include" ^
   /Fo"%OUT%\\" /Fe"%OUT%\test_store.exe" ^
   "%ROOT%\tools\test_store.cpp" "%ROOT%\src\game\storage_adapter.cpp" "%ROOT%\src\backend\journal.cpp" "%ROOT%\src\core\configuration.cpp" ^
   "%ROOT%\src\core\logging.cpp" "%ROOT%\src\core\paths.cpp"
if errorlevel 1 (echo [test] BUILD FAILED & exit /b 1)

"%OUT%\test_store.exe" "%JOUT%" > "%OUT%\test_store.out.txt" 2>&1
set "RC=%ERRORLEVEL%"
type "%OUT%\test_store.out.txt"
if not "%RC%"=="0" echo [test] the transcript is in "%OUT%\test_store.out.txt"
exit /b %RC%
