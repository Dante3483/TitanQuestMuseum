@echo off
setlocal
set "ROOT=%~dp0.."
rem Suites share object names/output paths: run sequentially.
for %%S in (bindings config journal proto store tooltip search viewgate catalogue museum import farm_sources addon_host) do (
    call "%~dp0build_test_%%S.bat"
    if errorlevel 1 exit /b 1
)
echo All Museum suites passed.
exit /b 0
