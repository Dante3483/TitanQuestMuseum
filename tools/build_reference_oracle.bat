@echo off
setlocal
set "ROOT=%~dp0.."
set "REF=%ROOT%\.."
call "%~dp0find_vcvars.bat"
if errorlevel 1 exit /b 1
call "%VCVARS%" >nul
if not exist "%ROOT%\build\reference" mkdir "%ROOT%\build\reference"
if not exist "%ROOT%\data\oracle" mkdir "%ROOT%\data\oracle"
rem Compile unmodified v4.1 generator sources read-only, with all output inside Museum.
cl /utf-8 /nologo /W4 /WX /EHsc /O2 /std:c++17 /MT /D_CRT_SECURE_NO_WARNINGS ^
 /I"%REF%\src" /Fo"%ROOT%\build\reference\\" /Fe"%ROOT%\build\reference\catalogue.exe" ^
 "%ROOT%\tests\reference_catalogue.cpp" "%REF%\src\gen\inflate.cpp" ^
 "%REF%\src\gen\arz_reader.cpp" "%REF%\src\gen\arc_reader.cpp" ^
 "%REF%\src\gen\catalogue_gen.cpp" "%REF%\src\gen\generate.cpp" ^
 "%REF%\src\model\catalogue.cpp" psapi.lib advapi32.lib
if errorlevel 1 exit /b 1
"%ROOT%\build\reference\catalogue.exe" --peak "%ROOT%\data\oracle"
exit /b %ERRORLEVEL%
