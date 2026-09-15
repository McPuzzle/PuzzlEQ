@echo off
title PuzzlEQ uninstaller
cd /d "%~dp0"
echo Removing every previous PuzzlEQ install (VST3, CLAP, app, FL Studio copies)...
echo.
if exist "%~dp0install-windows.ps1" (
    powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0install-windows.ps1" -Uninstall
) else if exist "%~dp0scripts\install-windows.ps1" (
    powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\install-windows.ps1" -Uninstall
) else (
    echo Could not find install-windows.ps1 next to this uninstaller.
    pause
    exit /b 1
)
if errorlevel 1 pause
