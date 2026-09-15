#!/bin/bash
# Double-click this file on macOS. It installs PuzzlEQ into your plugin folders.
cd "$(dirname "$0")"
export PUZZLEQ_INSTALL_ASSUME_YES=1
if [[ -f "./install-macos.sh" ]]; then
    exec bash "./install-macos.sh" "$@"
fi
if [[ -f "./scripts/install-macos.sh" ]]; then
    exec bash "./scripts/install-macos.sh" "$@"
fi
echo "install-macos.sh not found next to this file."
read -r _
exit 1
