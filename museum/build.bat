@echo off
rem ============================================================================================
rem  build.bat - the mod's hook DLL
rem
rem  Produces bin\TitanQuestMuseum.asi : x86 Release, /MT. Titan Quest AE is a 32-bit game built with
rem  VS2012 (MSVCR110); this DLL keeps its OWN static CRT and never shares a heap with the engine:
rem  every string the engine fills is freed through MSVCR110's operator delete, resolved by name.
rem  Toolchain: MSVC 2022 Build Tools, x86 target (vcvars32, found by tools\find_vcvars.bat).
rem
rem  The .asi extension is what Ultimate ASI Loader looks for: it LoadLibrary's every *.asi
rem  beside the exe and in scripts\ / plugins\ under it. The file is an ordinary DLL - it
rem  exposes TQM_GetAddonApi for optional dependent plugins; DllMain starts the host.
rem
rem  Layers:
rem    third_party\minhook\src\*.c        MinHook (MIT), compiled as C, hde32
rem    src\*.cpp                          the mod itself
rem    src\model\*.cpp                    the catalogue / collection / layout model: LINKED since
rem                                       (the footprint packer and the group table parser)
rem ============================================================================================
setlocal enabledelayedexpansion

set "ROOT=%~dp0"

call "%ROOT%tools\find_vcvars.bat"
if errorlevel 1 (
    echo [build] ERROR: no x86 C++ toolchain - see the message above
    exit /b 1
)
if not defined VSCMD_ARG_TGT_ARCH for %%V in ("%VCVARS%") do call "%%~dpVvcvarsall.bat" amd64_x86 >nul
if errorlevel 1 (
    echo [build] ERROR: vcvars32.bat failed
    exit /b 1
)
if not "%VSCMD_ARG_TGT_ARCH%"=="x86" (
    echo [build] ERROR: this shell targets %VSCMD_ARG_TGT_ARCH%, not x86 - run it from a plain prompt
    exit /b 1
)

set "OBJ=%ROOT%build\obj"
set "MOBJ=%ROOT%build\obj\model"
set "BIN=%ROOT%build\staging"
if not exist "%OBJ%" mkdir "%OBJ%"
if not exist "%MOBJ%" mkdir "%MOBJ%"
if not exist "%BIN%" mkdir "%BIN%"
del /q "%OBJ%\*.obj" 2>nul
del /q "%MOBJ%\*.obj" 2>nul

set "MINHOOK=%ROOT%..\core\third_party\minhook"
if not exist "%MINHOOK%\include\MinHook.h" (
    echo [build] ERROR: MinHook missing. Run:
    echo         git clone https://github.com/TsudaKageyu/minhook "%MINHOOK%"
    exit /b 1
)

rem ---- 1. MinHook (C, third-party: no /WX) ---------------------------------------------------
echo [build] MinHook...
cl /utf-8 /nologo /c /O2 /MT /W3 /GS- /Zi /DNDEBUG /D_CRT_SECURE_NO_WARNINGS ^
   /I"%MINHOOK%\include" /Fo"%OBJ%\\" /Fd"%OBJ%\minhook.pdb" ^
   "%MINHOOK%\src\buffer.c" "%MINHOOK%\src\hook.c" "%MINHOOK%\src\trampoline.c" ^
   "%MINHOOK%\src\hde\hde32.c"
if errorlevel 1 goto :fail

rem ---- 2. the mod --------------------------------------------------------------------------
echo [build] mod sources...
rem /Oy-: frame pointers kept. /O2 implies /Oy on x86, and the
rem watchdog's "is this DLL on the faulting stack" test (RtlCaptureStackBackTrace) walks EBP on
rem x86 - GD's x64 walk used unwind data instead - so without it the mod's frames are invisible.
cl /utf-8 /nologo /c /O2 /Oy- /MT /Zi /W4 /WX /EHsc /std:c++17 /GR- /DNDEBUG ^
   /DWIN32_LEAN_AND_MEAN /DNOMINMAX /D_CRT_SECURE_NO_WARNINGS ^
   /I"%MINHOOK%\include" /I"%ROOT%src" /I"%ROOT%..\core\src" /I"%ROOT%..\core\include" /Fo"%OBJ%\\" /Fd"%OBJ%\TitanQuestMuseum_cl.pdb" ^
   "%ROOT%src\game\bootstrap.cpp" "%ROOT%src\core\logging.cpp" "%ROOT%src\core\paths.cpp" ^
   "%ROOT%src\core\configuration.cpp" "%ROOT%src\game\catalogue_generation.cpp" ^
   "%ROOT%src\game\game_api.cpp" "%ROOT%src\game\hooks.cpp" "%ROOT%src\game\bindings.cpp" ^
   "%ROOT%src\game\view_adapter.cpp" "%ROOT%src\game\item_adapter.cpp" "%ROOT%src\backend\collection_runtime.cpp" ^
   "%ROOT%src\game\caravan.cpp" "%ROOT%src\game\native_surface.cpp" "%ROOT%src\game\ownership_adapter.cpp" ^
   "%ROOT%src\backend\journal.cpp" "%ROOT%src\game\storage_adapter.cpp" "%ROOT%src\game\tooltip_adapter.cpp" ^
   "%ROOT%src\game\reconciliation.cpp" "%ROOT%src\game\search_adapter.cpp" ^
   "%ROOT%src\backend\collection_service.cpp" "%ROOT%src\game\collection_bridge.cpp" ^
   "%ROOT%src\features\museum_controller.cpp" ^
   "%ROOT%src\ui\row_layout.cpp" "%ROOT%src\ui\text_renderer.cpp" "%ROOT%src\ui\button.cpp" ^
   "%ROOT%src\ui\statistics_view.cpp" "%ROOT%src\ui\museum_panel.cpp" "%ROOT%src\game\native_renderer.cpp" "%ROOT%src\game\journal_import.cpp"
if errorlevel 1 goto :fail

rem Shared data/model code is built once as TitanQuestCore.lib.
if not exist "%ROOT%..\core\dist\TitanQuestCore.lib" (
 call "%ROOT%..\core\build.bat"
 if errorlevel 1 goto :fail
)

rem ---- 3. the model: LINKED (ut_live lays the groups out with gdut::packSlots) ----
echo [build] model sources...
cl /utf-8 /nologo /c /O2 /Oy- /MT /Zi /W4 /WX /EHsc /std:c++17 /GR- /DNDEBUG ^
   /DWIN32_LEAN_AND_MEAN /DNOMINMAX /D_CRT_SECURE_NO_WARNINGS ^
   /I"%ROOT%src" /I"%ROOT%..\core\src" /I"%ROOT%..\core\include" /Fo"%OBJ%\\" /Fd"%OBJ%\TitanQuestMuseum_model.pdb" ^
   "%ROOT%src\backend\model\layout.cpp"
if errorlevel 1 goto :fail

rem ---- 3c. the hover proof reads the page mouse handler's EBP through the frame
rem      of hk_GetItemUnderPoint, so that function MUST open with
rem      push ebp / mov ebp,esp (55 8B EC). A compiler or flag change that drops it fails here.
echo [build] checking hk_GetItemUnderPoint's frame...
dumpbin /nologo /disasm "%OBJ%\hooks.obj" > "%ROOT%build\hooks.dis"
if errorlevel 1 goto :fail
powershell -NoProfile -ExecutionPolicy Bypass -Command "$l = @(Get-Content -LiteralPath '%ROOT%build\hooks.dis'); $i = -1; for ($k = 0; $k -lt $l.Count; $k++) { if ($l[$k] -match '^\?hk_GetItemUnderPoint@.*:\s*$') { $i = $k; break } }; if ($i -lt 0 -or $l[$i+1] -notmatch '^\s+00000000: 55\s+push\s+ebp\s*$' -or $l[$i+2] -notmatch '^\s+00000001: 8B EC\s+mov\s+ebp,esp\s*$') { exit 1 }"
if errorlevel 1 (
    echo [build] ERROR: hk_GetItemUnderPoint does not start with push ebp / mov ebp,esp - the hover
    echo         proof would read the wrong EBP ^(see build\hooks.dis^)
    goto :fail
)

rem ---- 4. link -------------------------------------------------------------------------------
echo [build] link...
rem EVERY build carries symbols. Without them a crash dump has to be read structurally, frame by
rem frame, and by then a deploy has usually overwritten the binary that crashed. /MAP gives the
rem rva -> function table, /DEBUG puts TitanQuestMuseum.pdb next to the .asi, and deploy.bat archives the
rem file it replaces, so a dump can always be matched to the binary that ran.
rem /PDBALTPATH writes only the bare file name "TitanQuestMuseum.pdb" into the binary's debug record, so
rem the folder this was built in never travels with the .asi; the local .pdb still resolves.
rem /SAFESEH: every object here is compiled by cl, so the image carries its safe-handler table and
rem the __try/__except blocks of the detours are registered handlers.
rem user32.lib: the game window's procedure for the pad, the wheel and the hotkey.
rem /Brepro: the PE and debug-directory stamps are a hash, not the link time, so the binary
rem carries no build clock (nor the time zone it was built in).
link /nologo /DLL /MACHINE:X86 /SAFESEH /DYNAMICBASE /NXCOMPAT /OPT:REF /OPT:ICF /Brepro ^
     /INCREMENTAL:NO /DEBUG /PDBALTPATH:%%_PDB%% ^
     /MAP:"%ROOT%build\TitanQuestMuseum.map" /PDB:"%BIN%\TitanQuestMuseum.pdb" ^
     /OUT:"%BIN%\TitanQuestMuseum.asi" ^
     "%OBJ%\*.obj" "%ROOT%..\core\dist\TitanQuestCore.lib" kernel32.lib user32.lib psapi.lib
if errorlevel 1 goto :fail

echo.
powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%tools\publish.ps1" -ProjectRoot "%ROOT%."
if errorlevel 1 goto :fail
echo [build] OK -^> %BIN%\TitanQuestMuseum.asi
for %%F in ("%BIN%\TitanQuestMuseum.asi") do echo [build] size %%~zF bytes, %%~tF
exit /b 0

:fail
echo.
echo [build] FAILED
exit /b 1
