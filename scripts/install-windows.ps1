# Install or uninstall PuzzlEQ on Windows (VST3 / CLAP / standalone).
# Double-click Install-PuzzlEQ.bat or Uninstall-PuzzlEQ.bat.

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
  Uninstall-PuzzlEQ.bat               Remove every previous PuzzlEQ copy
  Install-PuzzlEQ.bat -Uninstall      Same as Uninstall-PuzzlEQ.bat

The uninstaller sweeps system folders, every Windows user profile, and
FL Studio plugin folders so leftover 0.3.x copies cannot stay loaded.
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

# Uninstall always elevates so Program Files copies from older installers can go.
$needsAdmin = $Uninstall -or $useSystem
if ($needsAdmin -and -not (Test-IsAdmin)) {
    $argList = @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $PSCommandPath)
    if ($Uninstall) { $argList += "-Uninstall" }
    elseif ($User) { $argList += "-User" }
    else { $argList += "-System" }
    Start-Process -FilePath "powershell.exe" -Verb RunAs -ArgumentList $argList -Wait
    exit $LASTEXITCODE
}

function Remove-One([string]$path) {
    if (-not $path) { return $false }
    if (-not (Test-Path -LiteralPath $path)) { return $false }
    try {
        Remove-Item -LiteralPath $path -Recurse -Force -ErrorAction Stop
        Write-Host "  removed $path"
        return $true
    } catch {
        Write-Host "  could not remove $path — $($_.Exception.Message)"
        return $false
    }
}

function Get-SweepTargets {
    $targets = New-Object System.Collections.Generic.List[string]
    $add = {
        param($p)
        if ($p -and -not $targets.Contains($p)) { [void]$targets.Add($p) }
    }

    $roots = @(
        $env:COMMONPROGRAMFILES,
        ${env:COMMONPROGRAMFILES(x86)},
        $env:ProgramFiles,
        ${env:ProgramFiles(x86)}
    ) | Where-Object { $_ }

    foreach ($root in $roots) {
        & $add (Join-Path $root "VST3\PuzzlEQ.vst3")
        & $add (Join-Path $root "CLAP\PuzzlEQ.clap")
        & $add (Join-Path $root "VST3\PuzzlEQ")
        & $add (Join-Path $root "VST2\PuzzlEQ.dll")
        & $add (Join-Path $root "Steinberg\VST3\PuzzlEQ.vst3")
    }
    foreach ($pf in @($env:ProgramFiles, ${env:ProgramFiles(x86)})) {
        if ($pf) { & $add (Join-Path $pf "PuzzlEQ") }
    }

    $usersRoot = Split-Path $env:USERPROFILE -Parent
    if (Test-Path $usersRoot) {
        Get-ChildItem -LiteralPath $usersRoot -Directory -ErrorAction SilentlyContinue | ForEach-Object {
            $home = $_.FullName
            $la = Join-Path $home "AppData\Local"
            & $add (Join-Path $la "Programs\Common\VST3\PuzzlEQ.vst3")
            & $add (Join-Path $la "Programs\Common\CLAP\PuzzlEQ.clap")
            & $add (Join-Path $la "Programs\PuzzlEQ")
            & $add (Join-Path $home "Documents\VST3\PuzzlEQ.vst3")
            & $add (Join-Path $home "Documents\VST\PuzzlEQ.vst3")
            & $add (Join-Path $home "Documents\VST\PuzzlEQ.dll")
        }
    }

    foreach ($ilRoot in @((Join-Path $env:ProgramFiles "Image-Line"), $(if (${env:ProgramFiles(x86)}) { Join-Path ${env:ProgramFiles(x86)} "Image-Line" }))) {
        if (-not $ilRoot -or -not (Test-Path $ilRoot)) { continue }
        Get-ChildItem -LiteralPath $ilRoot -Directory -ErrorAction SilentlyContinue | ForEach-Object {
            foreach ($sub in @("Plugins\VST", "Plugins\VST3", "Plugins\Fruity\VST", "Data\Plugins\VST", "Data\Plugins\VST3")) {
                $d = Join-Path $_.FullName $sub
                if (-not (Test-Path $d)) { continue }
                Get-ChildItem -LiteralPath $d -Filter "PuzzlEQ*" -ErrorAction SilentlyContinue | ForEach-Object {
                    & $add $_.FullName
                }
            }
        }
    }

    return $targets
}

function Remove-UninstallRegistry {
    foreach ($key in @(
            "HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall\PuzzlEQ",
            "HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\PuzzlEQ"
        )) {
        if (Test-Path $key) {
            Remove-Item $key -Recurse -Force
            Write-Host "  removed $key"
        }
    }
}

function Register-Uninstall([string]$binDir, [string]$version) {
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
    New-ItemProperty -Path $key -Name "UninstallString" -Value "`"$uninstallBat`"" -PropertyType String -Force | Out-Null
    New-ItemProperty -Path $key -Name "QuietUninstallString" -Value "`"$uninstallBat`"" -PropertyType String -Force | Out-Null
    New-ItemProperty -Path $key -Name "NoModify" -Value 1 -PropertyType DWord -Force | Out-Null
    New-ItemProperty -Path $key -Name "NoRepair" -Value 1 -PropertyType DWord -Force | Out-Null
}

function Uninstall-PreviousVersions {
    Write-Host "Removing previous PuzzlEQ versions from every known folder..."
    $n = 0
    foreach ($path in Get-SweepTargets) {
        if (Remove-One $path) { $n++ }
    }
    Remove-UninstallRegistry
    if ($n -eq 0) {
        Write-Host "  no PuzzlEQ plugin files were found."
    } else {
        Write-Host "  $n location(s) cleaned."
    }
}

if ($Uninstall) {
    Uninstall-PreviousVersions
    Write-Host "Done. Fully quit your DAW and rescan plugins so leftover copies cannot reload."
    if ($Host.Name -eq "ConsoleHost") {
        Write-Host "Press Enter to close."
        [void][Console]::ReadLine()
    }
    exit 0
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

# Wipe leftover 0.3.x copies so FL Studio cannot keep loading an old binary.
Uninstall-PreviousVersions

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
    Write-Host "  App   $(Join-Path $binDir 'PuzzlEQ.exe')"
}

foreach ($name in @("install-windows.ps1", "Uninstall-PuzzlEQ.bat", "Install-PuzzlEQ.bat")) {
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
Register-Uninstall $binDir $version
Write-Host "  Uninstall  $(Join-Path $binDir 'Uninstall-PuzzlEQ.bat')"

if (-not $useSystem) {
    Write-Host ""
    Write-Host "User install: if the DAW does not see PuzzlEQ, add this VST3 folder in its plugin settings:"
    Write-Host "  $vst3Dir"
}

Write-Host ""
Write-Host "PuzzlEQ is installed. Fully quit the DAW and rescan plugins."
if ($Host.Name -eq "ConsoleHost") {
    Write-Host "Press Enter to close."
    [void][Console]::ReadLine()
}
