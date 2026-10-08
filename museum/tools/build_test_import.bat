@echo off
setlocal
set "ROOT=%~dp0.."
call "%~dp0find_vcvars.bat"
if errorlevel 1 exit /b 1
call "%VCVARS%" >nul
if not exist "%ROOT%\build\test" mkdir "%ROOT%\build\test"
set "SCRATCH=%ROOT%\build\test\import-%RANDOM%-%RANDOM%"
mkdir "%SCRATCH%"
cl /utf-8 /nologo /W4 /WX /EHsc /std:c++17 /MT /D_CRT_SECURE_NO_WARNINGS ^
 /DWIN32_LEAN_AND_MEAN /DNOMINMAX /I"%ROOT%\src" /I"%ROOT%\..\core\src" /I"%ROOT%\..\core\include" /Fo"%ROOT%\build\test\\" ^
 /Fe"%ROOT%\build\test\import_test.exe" "%ROOT%\tests\journal_import_test.cpp" ^
 "%ROOT%\src\game\journal_import.cpp" "%ROOT%\src\core\paths.cpp" "%ROOT%\src\core\logging.cpp"
if errorlevel 1 exit /b 1
"%ROOT%\build\test\import_test.exe" "%SCRATCH%"
exit /b %ERRORLEVEL%
