#!/usr/bin/env bash
# Build PuzzlEQ on this Mac and install it into VST3 / AU / CLAP folders.
set -euo pipefail

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "This script must be run on macOS." >&2
    exit 1
fi

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

if ! command -v cmake >/dev/null 2>&1; then
    echo "Install CMake first:  brew install cmake ninja" >&2
    exit 1
fi
if ! xcode-select -p >/dev/null 2>&1; then
    echo "Installing Xcode command-line tools (a GUI dialog may appear)..."
    xcode-select --install || true
    echo "Re-run this script after the tools finish installing." >&2
    exit 1
fi

./scripts/fetch-deps.sh

GEN=()
if command -v ninja >/dev/null 2>&1; then
    GEN=(-G Ninja)
fi

cmake -S . -B build "${GEN[@]}" -DCMAKE_BUILD_TYPE=Release
cmake --build build --target PuzzlEQ_Standalone PuzzlEQ_VST3 PuzzlEQ_AU PuzzlEQ_CLAP --parallel

PUZZLEQ_INSTALL_ASSUME_YES=1 ./scripts/install-macos.sh "$@"
echo
echo "Build + install finished."
