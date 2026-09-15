#!/bin/bash
# Double-click this file on macOS to remove every previous PuzzlEQ install.
cd "$(dirname "$0")"
echo "Removing every previous PuzzlEQ install..."
echo
if [[ -f "./install-macos.sh" ]]; then
    exec bash "./install-macos.sh" --uninstall
fi
if [[ -f "./scripts/install-macos.sh" ]]; then
    exec bash "./scripts/install-macos.sh" --uninstall
fi
echo "install-macos.sh not found next to this file."
read -r _
exit 1
