@echo off
setlocal
set "ROOT=%~dp0"
call "%ROOT%tools\find_vcvars.bat"
if errorlevel 1 exit /b 1
rem x64-hosted x86 tools avoid the installed x86-host PDB service failure.
if not defined VSCMD_ARG_TGT_ARCH for %%V in ("%VCVARS%") do call "%%~dpVvcvarsall.bat" amd64_x86 >nul
if errorlevel 1 exit /b 1
if not "%VSCMD_ARG_TGT_ARCH%"=="x86" exit /b 1
if not exist "%ROOT%build\obj" mkdir "%ROOT%build\obj"
if not exist "%ROOT%build\staging" mkdir "%ROOT%build\staging"
if not exist "%ROOT%dist" mkdir "%ROOT%dist"
cl /utf-8 /nologo /c /O2 /Oy- /MT /Zi /W4 /WX /EHsc /std:c++17 /GR- /DNDEBUG ^
 /DWIN32_LEAN_AND_MEAN /DNOMINMAX /D_CRT_SECURE_NO_WARNINGS /I"%ROOT%src" /I"%ROOT%..\core\include" ^
 /Fo"%ROOT%build\obj\\" /Fd"%ROOT%build\obj\TitanQuestMuseumRadar_cl.pdb" ^
 "%ROOT%src\plugin.cpp" "%ROOT%src\collection_reader.cpp"
if errorlevel 1 exit /b 1
link /nologo /DLL /MACHINE:X86 /SAFESEH /DYNAMICBASE /NXCOMPAT /OPT:REF /OPT:ICF /Brepro ^
 /INCREMENTAL:NO /DEBUG /PDBALTPATH:%%_PDB%% ^
 /MAP:"%ROOT%build\TitanQuestMuseumRadar.map" /PDB:"%ROOT%build\staging\TitanQuestMuseumRadar.pdb" ^
 /OUT:"%ROOT%build\staging\TitanQuestMuseumRadar.asi" ^
 "%ROOT%build\obj\plugin.obj" "%ROOT%build\obj\collection_reader.obj" kernel32.lib user32.lib
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%tools\publish.ps1" -ProjectRoot "%ROOT%."
exit /b %ERRORLEVEL%
