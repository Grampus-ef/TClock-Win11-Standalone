@echo off
setlocal EnableExtensions
cd /d "%~dp0" || exit /b 1
where cl.exe >nul 2>&1
if errorlevel 1 (
  echo ERROR: MSVC cl.exe not found. Use the x64 VS 2022 developer environment.
  exit /b 1
)
if exist "TClock11Standalone.exe" del /q "TClock11Standalone.exe"
cl /nologo /std:c++17 /EHsc /W4 /DUNICODE /D_UNICODE /O2 /MT /utf-8 TClock11Standalone.cpp /link /SUBSYSTEM:WINDOWS /MACHINE:X64 /OUT:TClock11Standalone.exe user32.lib gdi32.lib shell32.lib iphlpapi.lib
if errorlevel 1 (
  echo ERROR: MSVC compilation or link failed.
  exit /b 1
)
if not exist "TClock11Standalone.exe" (
  echo ERROR: compiler returned success but EXE is missing.
  exit /b 1
)
echo Built TClock11Standalone.exe
exit /b 0
