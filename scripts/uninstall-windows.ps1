# Remove every previous PuzzlEQ copy on this PC.
# Keep this file ASCII-only. Windows PowerShell 5.1 misreads UTF-8 dashes as quotes.
# Called by Uninstall-PuzzlEQ.bat (elevates via UAC).

[CmdletBinding()]
param(
    [switch]$SkipPause
)

$ErrorActionPreference = "Stop"

function Test-IsAdmin {
    $id = [Security.Principal.WindowsIdentity]::GetCurrent()
    $p = New-Object Security.Principal.WindowsPrincipal($id)
    return $p.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

if (-not (Test-IsAdmin)) {
    $argList = @(
        "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $PSCommandPath
    )
    if ($SkipPause) { $argList += "-SkipPause" }
    $proc = Start-Process -FilePath "powershell.exe" -Verb RunAs -ArgumentList $argList -Wait -PassThru
    if ($null -eq $proc) { exit 1 }
    exit $proc.ExitCode
}

function Remove-One([string]$path) {
    if (-not $path) { return $false }
    if (-not (Test-Path -LiteralPath $path)) { return $false }
    try {
        Remove-Item -LiteralPath $path -Recurse -Force -ErrorAction Stop
        Write-Host "  removed $path"
        return $true
    } catch {
        Write-Host ("  could not remove " + $path + " - " + $_.Exception.Message)
        return $false
    }
}

function Get-ProgramFilesX86 {
    return [Environment]::GetEnvironmentVariable("ProgramFiles(x86)")
}

function Get-SweepTargets {
    $targets = New-Object System.Collections.Generic.List[string]
    $pfx86 = Get-ProgramFilesX86
    $commonX86 = [Environment]::GetEnvironmentVariable("CommonProgramFiles(x86)")

    $roots = @($env:COMMONPROGRAMFILES, $commonX86, $env:ProgramFiles, $pfx86) |
        Where-Object { $_ }

    foreach ($root in $roots) {
        $targets.Add((Join-Path $root "VST3\PuzzlEQ.vst3")) | Out-Null
        $targets.Add((Join-Path $root "CLAP\PuzzlEQ.clap")) | Out-Null
        $targets.Add((Join-Path $root "VST3\PuzzlEQ")) | Out-Null
        $targets.Add((Join-Path $root "VST2\PuzzlEQ.dll")) | Out-Null
        $targets.Add((Join-Path $root "Steinberg\VST3\PuzzlEQ.vst3")) | Out-Null
    }
    foreach ($pf in @($env:ProgramFiles, $pfx86)) {
        if ($pf) { $targets.Add((Join-Path $pf "PuzzlEQ")) | Out-Null }
    }

    $usersRoot = Split-Path $env:USERPROFILE -Parent
    if (Test-Path $usersRoot) {
        Get-ChildItem -LiteralPath $usersRoot -Directory -ErrorAction SilentlyContinue | ForEach-Object {
            $home = $_.FullName
            $la = Join-Path $home "AppData\Local"
            $targets.Add((Join-Path $la "Programs\Common\VST3\PuzzlEQ.vst3")) | Out-Null
            $targets.Add((Join-Path $la "Programs\Common\CLAP\PuzzlEQ.clap")) | Out-Null
            $targets.Add((Join-Path $la "Programs\PuzzlEQ")) | Out-Null
            $targets.Add((Join-Path $home "Documents\VST3\PuzzlEQ.vst3")) | Out-Null
            $targets.Add((Join-Path $home "Documents\VST\PuzzlEQ.vst3")) | Out-Null
            $targets.Add((Join-Path $home "Documents\VST\PuzzlEQ.dll")) | Out-Null
        }
    }

    $ilRoots = New-Object System.Collections.Generic.List[string]
    if ($env:ProgramFiles) {
        $ilRoots.Add((Join-Path $env:ProgramFiles "Image-Line")) | Out-Null
    }
    if ($pfx86) {
        $ilRoots.Add((Join-Path $pfx86 "Image-Line")) | Out-Null
    }
    foreach ($ilRoot in $ilRoots) {
        if (-not (Test-Path $ilRoot)) { continue }
        Get-ChildItem -LiteralPath $ilRoot -Directory -ErrorAction SilentlyContinue | ForEach-Object {
            foreach ($sub in @("Plugins\VST", "Plugins\VST3", "Plugins\Fruity\VST", "Data\Plugins\VST", "Data\Plugins\VST3")) {
                $d = Join-Path $_.FullName $sub
                if (-not (Test-Path $d)) { continue }
                Get-ChildItem -LiteralPath $d -Filter "PuzzlEQ*" -ErrorAction SilentlyContinue | ForEach-Object {
                    $targets.Add($_.FullName) | Out-Null
                }
            }
        }
    }

    return $targets
}

Write-Host "Removing previous PuzzlEQ versions from every known folder..."
$n = 0
foreach ($path in Get-SweepTargets) {
    if (Remove-One $path) { $n++ }
}

foreach ($key in @(
        "HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall\PuzzlEQ",
        "HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\PuzzlEQ"
    )) {
    if (Test-Path $key) {
        Remove-Item $key -Recurse -Force
        Write-Host "  removed $key"
        $n++
    }
}

if ($n -eq 0) {
    Write-Host "  no PuzzlEQ plugin files were found."
} else {
    Write-Host ("  " + $n + " location(s) cleaned.")
}
Write-Host "Done. Fully quit your DAW and rescan plugins so leftover copies cannot reload."

if (-not $SkipPause -and $Host.Name -eq "ConsoleHost") {
    Write-Host "Press Enter to close."
    [void][Console]::ReadLine()
}
