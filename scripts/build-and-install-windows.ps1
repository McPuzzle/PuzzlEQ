# Build PuzzlEQ on this Windows PC and install it into the VST3 / CLAP folders.
#   powershell -ExecutionPolicy Bypass -File scripts\build-and-install-windows.ps1

[CmdletBinding()]
param(
    [switch]$User
)

$ErrorActionPreference = "Stop"
if ($env:OS -notmatch "Windows") {
    Write-Error "This script must be run on Windows."
}

$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Set-Location $Root

function Need-Cmd($name) {
    if (-not (Get-Command $name -ErrorAction SilentlyContinue)) {
        Write-Error "Missing '$name'. Install Git for Windows and CMake, and Visual Studio 2022 with C++."
    }
}

Need-Cmd git
Need-Cmd cmake

$bash = Get-Command bash -ErrorAction SilentlyContinue
if ($bash) {
    & bash "$Root\scripts\fetch-deps.sh"
} else {
    New-Item -ItemType Directory -Force -Path "$Root\modules" | Out-Null
    if (-not (Test-Path "$Root\modules\JUCE\CMakeLists.txt")) {
        git clone --depth 1 --branch 8.0.10 https://github.com/juce-framework/JUCE.git "$Root\modules\JUCE"
    }
    if (-not (Test-Path "$Root\modules\clap-juce-extensions\CMakeLists.txt")) {
        git clone --depth 1 --recurse-submodules https://github.com/free-audio/clap-juce-extensions.git "$Root\modules\clap-juce-extensions"
    }
}

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$targets = @("PuzzlEQ_Standalone", "PuzzlEQ_VST3")
cmake --build build --config Release --target @targets --parallel
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

# CLAP target exists only when clap-juce-extensions was found.
cmake --build build --config Release --target PuzzlEQ_CLAP --parallel 2>$null

$inst = Join-Path $Root "scripts\install-windows.ps1"
if ($User) {
    & $inst -User
} else {
    & $inst -System
}

Write-Host "Build + install finished."
