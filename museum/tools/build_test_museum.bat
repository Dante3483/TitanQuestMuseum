@echo off
setlocal
set "ROOT=%~dp0.."
call "%~dp0find_vcvars.bat"
if errorlevel 1 exit /b 1
call "%VCVARS%" >nul
if not exist "%ROOT%\build\test" mkdir "%ROOT%\build\test"
cl /utf-8 /nologo /W4 /WX /EHsc /std:c++17 /MT /D_CRT_SECURE_NO_WARNINGS /I"%ROOT%\src" /I"%ROOT%\..\core\src" /I"%ROOT%\..\core\include" ^
 /Fo"%ROOT%\build\test\\" /Fe"%ROOT%\build\test\museum_test.exe" ^
 "%ROOT%\tests\museum_test.cpp" "%ROOT%\src\backend\collection_service.cpp" ^
 "%ROOT%\src\backend\model\layout.cpp" "%ROOT%\src\ui\row_layout.cpp" ^
 "%ROOT%\src\ui\text_renderer.cpp" "%ROOT%\src\ui\button.cpp" ^
 "%ROOT%\src\ui\statistics_view.cpp" "%ROOT%\src\ui\museum_panel.cpp"
if errorlevel 1 exit /b 1
pushd "%ROOT%"
"%ROOT%\build\test\museum_test.exe"
set "RESULT=%ERRORLEVEL%"
popd
exit /b %RESULT%
