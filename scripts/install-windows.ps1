# Install PuzzlEQ into Windows VST3 / CLAP folders. No dragging into a VST directory.
# Run via Install-PuzzlEQ.bat (bypasses execution policy) or:
#   powershell -ExecutionPolicy Bypass -File install-windows.ps1

[CmdletBinding()]
param(
    [switch]$User,
    [switch]$System,
    [switch]$Uninstall,
    [switch]$Help
)

$ErrorActionPreference = "Stop"

function Show-Help {
    @"
PuzzlEQ Windows installer

  Install-PuzzlEQ.bat                 System install (UAC, recommended)
  Install-PuzzlEQ.bat -User           Current-user folders (no admin)
  Install-PuzzlEQ.bat -Uninstall
  Install-PuzzlEQ.bat -Uninstall -User

System destinations:
  VST3   $env:COMMONPROGRAMFILES\VST3\PuzzlEQ.vst3
  CLAP   $env:COMMONPROGRAMFILES\CLAP\PuzzlEQ.clap
  App    $env:PROGRAMFILES\PuzzlEQ\PuzzlEQ.exe

User destinations:
  VST3   $env:LOCALAPPDATA\Programs\Common\VST3\PuzzlEQ.vst3
  CLAP   $env:LOCALAPPDATA\Programs\Common\CLAP\PuzzlEQ.clap
"@
}

if ($Help) { Show-Help; exit 0 }

$useSystem = -not $User
if ($System) { $useSystem = $true }

function Test-IsAdmin {
    $id = [Security.Principal.WindowsIdentity]::GetCurrent()
    $p = New-Object Security.Principal.WindowsPrincipal($id)
    return $p.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

if ($useSystem -and -not (Test-IsAdmin)) {
    $argList = @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $PSCommandPath)
    if ($Uninstall) { $argList += "-Uninstall" }
    if ($System) { $argList += "-System" }
    Start-Process -FilePath "powershell.exe" -Verb RunAs -ArgumentList $argList -Wait
    exit $LASTEXITCODE
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

function Remove-One([string]$path) {
    if (Test-Path $path) {
        Remove-Item -LiteralPath $path -Recurse -Force
        Write-Host "  removed $path"
    }
}

if ($Uninstall) {
    Write-Host "Uninstalling PuzzlEQ..."
    Remove-One (Join-Path $vst3Dir "PuzzlEQ.vst3")
    Remove-One (Join-Path $clapDir "PuzzlEQ.clap")
    Remove-One $binDir
    Write-Host "Done. Rescan plugins in your DAW."
    exit 0
}

$here = Split-Path -Parent $MyInvocation.MyCommand.Path
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
    if ((Test-Path (Join-Path $dir "PuzzlEQ.vst3")) -or (Test-Path (Join-Path $dir "VST3\PuzzlEQ.vst3"))) {
        $payload = $dir
        break
    }
}

if (-not $payload) {
    Write-Error @"
Could not find PuzzlEQ.vst3 next to the installer.
Build on this Windows PC first, or run scripts\build-and-install-windows.ps1 from the source tree.
The Linux installer will not load in Windows DAWs.
"@
}

function Pick-Existing([string]$a, [string]$b) {
    if (Test-Path $a) { return $a }
    if (Test-Path $b) { return $b }
    return $null
}

$srcVst3 = Pick-Existing (Join-Path $payload "PuzzlEQ.vst3") (Join-Path $payload "VST3\PuzzlEQ.vst3")
$srcClap = Pick-Existing (Join-Path $payload "PuzzlEQ.clap") (Join-Path $payload "CLAP\PuzzlEQ.clap")
$srcBin  = $null
foreach ($name in @("PuzzlEQ.exe", "Standalone\PuzzlEQ.exe", "Standalone\PuzzlEQ.app")) {
    $p = Join-Path $payload $name
    if (Test-Path $p) { $srcBin = $p; break }
}

New-Item -ItemType Directory -Force -Path $vst3Dir, $clapDir | Out-Null

Write-Host "Installing PuzzlEQ..."
$destVst3 = Join-Path $vst3Dir "PuzzlEQ.vst3"
Remove-One $destVst3
Copy-Item -LiteralPath $srcVst3 -Destination $destVst3 -Recurse -Force
Write-Host "  VST3  $destVst3"

if ($srcClap) {
    New-Item -ItemType Directory -Force -Path $clapDir | Out-Null
    $destClap = Join-Path $clapDir "PuzzlEQ.clap"
    Copy-Item -LiteralPath $srcClap -Destination $destClap -Force
    Write-Host "  CLAP  $destClap"
}

if ($srcBin -and (Test-Path $srcBin) -and $srcBin.ToLower().EndsWith(".exe")) {
    New-Item -ItemType Directory -Force -Path $binDir | Out-Null
    Copy-Item -LiteralPath $srcBin -Destination (Join-Path $binDir "PuzzlEQ.exe") -Force
    Write-Host "  App   $(Join-Path $binDir 'PuzzlEQ.exe')"
}

if (-not $useSystem) {
    Write-Host ""
    Write-Host "User install: if the DAW does not see PuzzlEQ, add this VST3 folder in its plugin settings:"
    Write-Host "  $vst3Dir"
}

Write-Host ""
Write-Host "PuzzlEQ is installed. Rescan plugins in Ableton, FL, Reaper, or Cubase."
if ($Host.Name -eq "ConsoleHost") {
    Write-Host "Press Enter to close."
    [void][Console]::ReadLine()
}
