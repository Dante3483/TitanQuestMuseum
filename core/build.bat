@echo off
setlocal
set "ROOT=%~dp0"
call "%ROOT%tools\find_vcvars.bat"
if errorlevel 1 exit /b 1
if not defined VSCMD_ARG_TGT_ARCH for %%V in ("%VCVARS%") do call "%%~dpVvcvarsall.bat" amd64_x86 >nul
if not "%VSCMD_ARG_TGT_ARCH%"=="x86" exit /b 1
if not exist "%ROOT%build\obj" mkdir "%ROOT%build\obj"
if not exist "%ROOT%dist" mkdir "%ROOT%dist"
cl /utf-8 /nologo /c /O2 /Oy- /MT /Zi /W4 /WX /EHsc /std:c++17 /GR- /DNDEBUG /DWIN32_LEAN_AND_MEAN /DNOMINMAX /D_CRT_SECURE_NO_WARNINGS ^
 /I"%ROOT%src" /Fo"%ROOT%build\obj\\" /Fd"%ROOT%build\obj\TitanQuestCore.pdb" ^
 "%ROOT%src\game\archive\inflate.cpp" "%ROOT%src\game\archive\arz_reader.cpp" "%ROOT%src\game\archive\arc_reader.cpp" ^
 "%ROOT%src\game\archive\loot_sources.cpp" "%ROOT%src\game\archive\catalogue_gen.cpp" "%ROOT%src\game\archive\generate.cpp" ^
 "%ROOT%src\backend\model\catalogue.cpp"
if errorlevel 1 exit /b 1
lib /nologo /OUT:"%ROOT%dist\TitanQuestCore.lib" "%ROOT%build\obj\inflate.obj" "%ROOT%build\obj\arz_reader.obj" "%ROOT%build\obj\arc_reader.obj" "%ROOT%build\obj\loot_sources.obj" "%ROOT%build\obj\catalogue_gen.obj" "%ROOT%build\obj\generate.obj" "%ROOT%build\obj\catalogue.obj"
if errorlevel 1 exit /b 1
set "MINHOOK=%ROOT%third_party\minhook"
cl /nologo /c /O2 /MT /W3 /Zi /I"%MINHOOK%\include" /Fo"%ROOT%build\obj\\" /Fd"%ROOT%build\obj\minhook.pdb" ^
 "%MINHOOK%\src\buffer.c" "%MINHOOK%\src\hook.c" "%MINHOOK%\src\trampoline.c" "%MINHOOK%\src\hde\hde32.c"
if errorlevel 1 exit /b 1
cl /nologo /utf-8 /c /O2 /Oy- /MT /Zi /W4 /WX /EHsc /std:c++17 /DNOMINMAX /DWIN32_LEAN_AND_MEAN /D_CRT_SECURE_NO_WARNINGS ^
 /I"%ROOT%src" /I"%ROOT%include" /I"%MINHOOK%\include" /Fo"%ROOT%build\obj\\" /Fd"%ROOT%build\obj\runtime.pdb" ^
 "%ROOT%src\runtime.cpp" "%ROOT%src\game\addon_host.cpp"
if errorlevel 1 exit /b 1
if not exist "%ROOT%build\staging" mkdir "%ROOT%build\staging"
link /nologo /DLL /MACHINE:X86 /SAFESEH /DYNAMICBASE /NXCOMPAT /OPT:REF /OPT:ICF /Brepro /INCREMENTAL:NO /DEBUG /PDBALTPATH:%%_PDB%% ^
 /MAP:"%ROOT%build\TitanQuestCore.map" /PDB:"%ROOT%build\staging\TitanQuestCore.pdb" /OUT:"%ROOT%build\staging\TitanQuestCore.asi" ^
 "%ROOT%build\obj\*.obj" kernel32.lib user32.lib psapi.lib
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%tools\publish.ps1" -ProjectRoot "%ROOT%."
exit /b %ERRORLEVEL%
