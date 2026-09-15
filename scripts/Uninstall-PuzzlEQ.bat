@echo off
title PuzzlEQ uninstaller
cd /d "%~dp0"
echo Removing every previous PuzzlEQ install (VST3, CLAP, app, FL Studio copies)...
echo.
set "PS1=%~dp0uninstall-windows.ps1"
if not exist "%PS1%" set "PS1=%~dp0scripts\uninstall-windows.ps1"
if not exist "%PS1%" (
    echo Could not find uninstall-windows.ps1 next to this uninstaller.
    pause
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -File "%PS1%"
if errorlevel 1 pause
