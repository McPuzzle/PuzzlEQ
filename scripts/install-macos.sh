#!/usr/bin/env bash
# Install PuzzlEQ into macOS plugin folders (no manual copy into a VST folder).
set -euo pipefail

usage() {
    cat <<'EOF'
PuzzlEQ macOS installer

Usage:
  ./install-macos.sh              Install for this user (no password)
  ./install-macos.sh --system     Install for all users (needs sudo)
  ./install-macos.sh --uninstall
  ./install-macos.sh --help

User destinations:
  VST3   ~/Library/Audio/Plug-Ins/VST3/PuzzlEQ.vst3
  AU     ~/Library/Audio/Plug-Ins/Components/PuzzlEQ.component
  CLAP   ~/Library/Audio/Plug-Ins/CLAP/PuzzlEQ.clap
  App    /Applications/PuzzlEQ.app   (system) or ~/Applications/PuzzlEQ.app
EOF
}

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MODE="user"
DO_UNINSTALL=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --system) MODE="system"; shift ;;
        --user) MODE="user"; shift ;;
        --uninstall) DO_UNINSTALL=1; shift ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Unknown option: $1" >&2; usage; exit 1 ;;
    esac
done

if [[ "$MODE" == "system" ]]; then
    PLUG_ROOT="/Library/Audio/Plug-Ins"
    APP_DIR="/Applications"
    if [[ "$(id -u)" -ne 0 ]]; then
        echo "System install needs admin. Re-running with sudo..."
        exec sudo -- "$0" --system ${DO_UNINSTALL:+--uninstall}
    fi
else
    PLUG_ROOT="${HOME}/Library/Audio/Plug-Ins"
    APP_DIR="${HOME}/Applications"
fi

VST3_DIR="${PLUG_ROOT}/VST3"
AU_DIR="${PLUG_ROOT}/Components"
CLAP_DIR="${PLUG_ROOT}/CLAP"

remove_one() {
    local path="$1"
    if [[ -e "$path" || -L "$path" ]]; then
        if rm -rf "$path" 2>/dev/null; then
            echo "  removed $path"
        else
            echo "  needs admin to remove $path"
        fi
    fi
}

if [[ "$DO_UNINSTALL" -eq 1 ]]; then
    echo "Removing previous PuzzlEQ versions from user and system plugin folders..."
    remove_one "${HOME}/Library/Audio/Plug-Ins/VST3/PuzzlEQ.vst3"
    remove_one "${HOME}/Library/Audio/Plug-Ins/Components/PuzzlEQ.component"
    remove_one "${HOME}/Library/Audio/Plug-Ins/CLAP/PuzzlEQ.clap"
    remove_one "${HOME}/Applications/PuzzlEQ.app"
    remove_one "/Library/Audio/Plug-Ins/VST3/PuzzlEQ.vst3"
    remove_one "/Library/Audio/Plug-Ins/Components/PuzzlEQ.component"
    remove_one "/Library/Audio/Plug-Ins/CLAP/PuzzlEQ.clap"
    remove_one "/Applications/PuzzlEQ.app"
    if [[ "$(id -u)" -ne 0 ]]; then
        if [[ -e "/Library/Audio/Plug-Ins/VST3/PuzzlEQ.vst3" \
           || -e "/Library/Audio/Plug-Ins/Components/PuzzlEQ.component" \
           || -e "/Library/Audio/Plug-Ins/CLAP/PuzzlEQ.clap" \
           || -e "/Applications/PuzzlEQ.app" ]]; then
            echo "System copies need admin. Re-running with sudo..."
            exec sudo -- "$0" --uninstall
        fi
    fi
    if command -v killall >/dev/null 2>&1; then
        killall -9 AudioComponentRegistrar >/dev/null 2>&1 || true
    fi
    echo "Done. Fully quit your DAW and rescan plugins."
    exit 0
fi

find_payload() {
    local dir
    for dir in \
        "$SCRIPT_DIR" \
        "${SCRIPT_DIR}/macos" \
        "${SCRIPT_DIR}/../macos" \
        "${SCRIPT_DIR}/../build/PuzzlEQ_artefacts/Release" \
        "${SCRIPT_DIR}/../../build/PuzzlEQ_artefacts/Release"
    do
        [[ -d "$dir" ]] || continue
        if [[ -d "${dir}/PuzzlEQ.vst3" || -d "${dir}/VST3/PuzzlEQ.vst3" ]]; then
            echo "$dir"
            return 0
        fi
    done
    return 1
}

PAYLOAD="$(find_payload)" || {
    echo "Could not find PuzzlEQ.vst3 next to the installer." >&2
    echo "Build on this Mac first, or run scripts/build-and-install-macos.sh from the source tree." >&2
    exit 1
}

pick() {
    local a="$1" b="$2"
    if [[ -e "$a" ]]; then echo "$a"
    elif [[ -e "$b" ]]; then echo "$b"
    else echo ""
    fi
}

SRC_VST3="$(pick "${PAYLOAD}/PuzzlEQ.vst3" "${PAYLOAD}/VST3/PuzzlEQ.vst3")"
SRC_AU="$(pick "${PAYLOAD}/PuzzlEQ.component" "${PAYLOAD}/AU/PuzzlEQ.component")"
SRC_CLAP="$(pick "${PAYLOAD}/PuzzlEQ.clap" "${PAYLOAD}/CLAP/PuzzlEQ.clap")"
SRC_APP="$(pick "${PAYLOAD}/PuzzlEQ.app" "${PAYLOAD}/Standalone/PuzzlEQ.app")"

if [[ -z "$SRC_VST3" ]]; then
    echo "Missing VST3 bundle in $PAYLOAD" >&2
    exit 1
fi

if [[ -z "${PUZZLEQ_INSTALL_ASSUME_YES:-}" ]]; then
    echo "Install PuzzlEQ into:"
    echo "  VST3  ${VST3_DIR}/PuzzlEQ.vst3"
    [[ -n "$SRC_AU" ]] && echo "  AU    ${AU_DIR}/PuzzlEQ.component"
    [[ -n "$SRC_CLAP" ]] && echo "  CLAP  ${CLAP_DIR}/PuzzlEQ.clap"
    if [[ -t 0 ]]; then
        printf "Continue? [Y/n] "
        read -r ans || true
        case "${ans:-Y}" in
            n|N|no|NO) echo "Cancelled."; exit 1 ;;
        esac
    fi
fi

mkdir -p "$VST3_DIR" "$AU_DIR" "$CLAP_DIR" "$APP_DIR"

echo "Clearing leftover PuzzlEQ copies..."
remove_one "${HOME}/Library/Audio/Plug-Ins/VST3/PuzzlEQ.vst3"
remove_one "${HOME}/Library/Audio/Plug-Ins/Components/PuzzlEQ.component"
remove_one "${HOME}/Library/Audio/Plug-Ins/CLAP/PuzzlEQ.clap"
remove_one "${HOME}/Applications/PuzzlEQ.app"
if [[ "$MODE" == "system" || "$(id -u)" -eq 0 ]]; then
    remove_one "/Library/Audio/Plug-Ins/VST3/PuzzlEQ.vst3"
    remove_one "/Library/Audio/Plug-Ins/Components/PuzzlEQ.component"
    remove_one "/Library/Audio/Plug-Ins/CLAP/PuzzlEQ.clap"
    remove_one "/Applications/PuzzlEQ.app"
fi

echo "Installing PuzzlEQ ($MODE)..."
rm -rf "${VST3_DIR}/PuzzlEQ.vst3"
cp -a "$SRC_VST3" "${VST3_DIR}/PuzzlEQ.vst3"
echo "  VST3  ${VST3_DIR}/PuzzlEQ.vst3"

if [[ -n "$SRC_AU" ]]; then
    rm -rf "${AU_DIR}/PuzzlEQ.component"
    cp -a "$SRC_AU" "${AU_DIR}/PuzzlEQ.component"
    echo "  AU    ${AU_DIR}/PuzzlEQ.component"
fi

if [[ -n "$SRC_CLAP" ]]; then
    rm -rf "${CLAP_DIR}/PuzzlEQ.clap"
    cp -a "$SRC_CLAP" "${CLAP_DIR}/PuzzlEQ.clap"
    echo "  CLAP  ${CLAP_DIR}/PuzzlEQ.clap"
fi

if [[ -n "$SRC_APP" ]]; then
    rm -rf "${APP_DIR}/PuzzlEQ.app"
    cp -a "$SRC_APP" "${APP_DIR}/PuzzlEQ.app"
    echo "  App   ${APP_DIR}/PuzzlEQ.app"
fi

# Help Logic/Ableton/GarageBand notice the new Audio Unit.
if command -v killall >/dev/null 2>&1; then
    killall -9 AudioComponentRegistrar >/dev/null 2>&1 || true
fi

echo
echo "PuzzlEQ is installed. Rescan plugins in Logic, Ableton, Reaper, or Bitwig."
echo "Uninstall: double-click Uninstall-PuzzlEQ.command  (or $0 --uninstall)"
