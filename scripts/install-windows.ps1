# Install PuzzlEQ on Windows (VST3 / CLAP / standalone).
# Keep this file ASCII-only. Windows PowerShell 5.1 misreads UTF-8 dashes as quotes.
# Double-click Install-PuzzlEQ.bat  or  Uninstall-PuzzlEQ.bat

[CmdletBinding()]
param(
    [switch]$User,
    [switch]$System,
    [switch]$Uninstall,
    [switch]$Help
)

$ErrorActionPreference = "Stop"

function Show-Help {
    Write-Host "PuzzlEQ Windows installer"
    Write-Host ""
    Write-Host "  Install-PuzzlEQ.bat              System install (UAC, recommended)"
    Write-Host "  Install-PuzzlEQ.bat -User        Current-user folders (no admin)"
    Write-Host "  Uninstall-PuzzlEQ.bat            Remove every previous PuzzlEQ copy"
}

if ($Help) { Show-Help; exit 0 }

$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$uninst = Join-Path $here "uninstall-windows.ps1"
if (-not (Test-Path $uninst)) {
    $uninst = Join-Path $here "scripts\uninstall-windows.ps1"
}

if ($Uninstall) {
    if (-not (Test-Path $uninst)) {
        Write-Error "Missing uninstall-windows.ps1"
    }
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $uninst
    exit $LASTEXITCODE
}

$useSystem = -not $User
if ($System) { $useSystem = $true }

function Test-IsAdmin {
    $id = [Security.Principal.WindowsIdentity]::GetCurrent()
    $p = New-Object Security.Principal.WindowsPrincipal($id)
    return $p.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

if ($useSystem -and -not (Test-IsAdmin)) {
    $argList = @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $PSCommandPath, "-System")
    $proc = Start-Process -FilePath "powershell.exe" -Verb RunAs -ArgumentList $argList -Wait -PassThru
    if ($null -eq $proc) { exit 1 }
    exit $proc.ExitCode
}

if ($useSystem) {
    $vst3Dir = Join-Path $env:COMMONPROGRAMFILES "VST3"
    $clapDir = Join-Path $env:COMMONPROGRAMFILES "CLAP"
    $binDir  = Join-Path $env:PROGRAMFILES "PuzzlEQ"
} else {
    $vst3Dir = Join-Path $env:LOCALAPPDATA "Programs\Common\VST3"
    $clapDir = Join-Path $env:LOCALAPPDATA "Programs\Common\CLAP"
    $binDir  = Join-Path $env:LOCALAPPDATA "Programs\PuzzlEQ"
}

# Wipe leftover copies so FL Studio cannot keep loading an old binary.
if (Test-Path $uninst) {
    Write-Host "Clearing previous PuzzlEQ copies first..."
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $uninst -SkipPause
}

$candidates = @(
    $here,
    (Join-Path $here "windows"),
    (Join-Path $here "..\windows"),
    (Join-Path $here "..\build\PuzzlEQ_artefacts\Release"),
    (Join-Path $here "..\..\build\PuzzlEQ_artefacts\Release")
)

$payload = $null
foreach ($dir in $candidates) {
    if (-not (Test-Path $dir)) { continue }
    $a = Join-Path $dir "PuzzlEQ.vst3"
    $b = Join-Path $dir "VST3\PuzzlEQ.vst3"
    if ((Test-Path $a) -or (Test-Path $b)) {
        $payload = $dir
        break
    }
}

if (-not $payload) {
    Write-Error "Could not find PuzzlEQ.vst3 next to the installer. Use the Windows zip, not the Linux installer."
}

function Pick-Existing([string]$a, [string]$b) {
    if (Test-Path $a) { return $a }
    if (Test-Path $b) { return $b }
    return $null
}

$srcVst3 = Pick-Existing (Join-Path $payload "PuzzlEQ.vst3") (Join-Path $payload "VST3\PuzzlEQ.vst3")
$srcClap = Pick-Existing (Join-Path $payload "PuzzlEQ.clap") (Join-Path $payload "CLAP\PuzzlEQ.clap")
$srcBin  = $null
foreach ($name in @("PuzzlEQ.exe", "Standalone\PuzzlEQ.exe")) {
    $p = Join-Path $payload $name
    if (Test-Path $p) { $srcBin = $p; break }
}

New-Item -ItemType Directory -Force -Path $vst3Dir, $clapDir | Out-Null

Write-Host ""
Write-Host "Installing PuzzlEQ..."
$destVst3 = Join-Path $vst3Dir "PuzzlEQ.vst3"
Copy-Item -LiteralPath $srcVst3 -Destination $destVst3 -Recurse -Force
Write-Host "  VST3  $destVst3"

if ($srcClap) {
    New-Item -ItemType Directory -Force -Path $clapDir | Out-Null
    $destClap = Join-Path $clapDir "PuzzlEQ.clap"
    Copy-Item -LiteralPath $srcClap -Destination $destClap -Force
    Write-Host "  CLAP  $destClap"
}

New-Item -ItemType Directory -Force -Path $binDir | Out-Null
if ($srcBin -and (Test-Path $srcBin) -and $srcBin.ToLower().EndsWith(".exe")) {
    Copy-Item -LiteralPath $srcBin -Destination (Join-Path $binDir "PuzzlEQ.exe") -Force
    Write-Host ("  App   " + (Join-Path $binDir "PuzzlEQ.exe"))
}

foreach ($name in @("install-windows.ps1", "uninstall-windows.ps1", "Uninstall-PuzzlEQ.bat", "Install-PuzzlEQ.bat")) {
    $src = Join-Path $here $name
    if (Test-Path $src) {
        Copy-Item -LiteralPath $src -Destination (Join-Path $binDir $name) -Force
    }
}

$version = "0.0.0"
$verFile = Join-Path $payload "VERSION"
if (-not (Test-Path $verFile)) { $verFile = Join-Path $here "VERSION" }
if (-not (Test-Path $verFile)) { $verFile = Join-Path $here "..\VERSION" }
if (Test-Path $verFile) { $version = (Get-Content -LiteralPath $verFile -Raw).Trim() }

$uninstallBat = Join-Path $binDir "Uninstall-PuzzlEQ.bat"
$key = if ($useSystem) {
    "HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall\PuzzlEQ"
} else {
    "HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\PuzzlEQ"
}
New-Item -Path $key -Force | Out-Null
New-ItemProperty -Path $key -Name "DisplayName" -Value "PuzzlEQ" -PropertyType String -Force | Out-Null
New-ItemProperty -Path $key -Name "Publisher" -Value "Puzzl" -PropertyType String -Force | Out-Null
New-ItemProperty -Path $key -Name "DisplayVersion" -Value $version -PropertyType String -Force | Out-Null
New-ItemProperty -Path $key -Name "UninstallString" -Value ('"' + $uninstallBat + '"') -PropertyType String -Force | Out-Null
New-ItemProperty -Path $key -Name "QuietUninstallString" -Value ('"' + $uninstallBat + '"') -PropertyType String -Force | Out-Null
New-ItemProperty -Path $key -Name "NoModify" -Value 1 -PropertyType DWord -Force | Out-Null
New-ItemProperty -Path $key -Name "NoRepair" -Value 1 -PropertyType DWord -Force | Out-Null
Write-Host ("  Uninstall  " + $uninstallBat)

if (-not $useSystem) {
    Write-Host ""
    Write-Host "User install: if the DAW does not see PuzzlEQ, add this VST3 folder:"
    Write-Host ("  " + $vst3Dir)
}

Write-Host ""
Write-Host "PuzzlEQ is installed. Fully quit the DAW and rescan plugins."
if ($Host.Name -eq "ConsoleHost") {
    Write-Host "Press Enter to close."
    [void][Console]::ReadLine()
}
