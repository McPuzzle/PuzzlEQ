#!/usr/bin/env bash
# Install PuzzlEQ into the standard Linux plugin folders (no manual copy).
set -euo pipefail

usage() {
    cat <<'EOF'
PuzzlEQ Linux installer

Usage:
  ./install-linux.sh           Install for this user (no sudo)
  ./install-linux.sh --system  Install system-wide (needs sudo)
  ./install-linux.sh --uninstall
  ./install-linux.sh --help

Default destinations (user):
  VST3        ~/.vst3/PuzzlEQ.vst3
  CLAP        ~/.clap/PuzzlEQ.clap
  Standalone  ~/.local/bin/PuzzlEQ
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
    VST3_DIR="/usr/lib/vst3"
    CLAP_DIR="/usr/lib/clap"
    BIN_DIR="/usr/local/bin"
    APP_DIR="/usr/share/applications"
    NEED_ROOT=1
else
    VST3_DIR="${HOME}/.vst3"
    CLAP_DIR="${HOME}/.clap"
    BIN_DIR="${HOME}/.local/bin"
    APP_DIR="${HOME}/.local/share/applications"
    NEED_ROOT=0
fi

if [[ "$NEED_ROOT" -eq 1 && "$(id -u)" -ne 0 ]]; then
    echo "System install needs root. Re-running with sudo..."
    exec sudo -- "$0" --system ${DO_UNINSTALL:+--uninstall}
fi

remove_one() {
    local path="$1"
    if [[ -e "$path" || -L "$path" ]]; then
        if rm -rf "$path" 2>/dev/null; then
            echo "  removed $path"
        else
            echo "  needs root to remove $path"
        fi
    fi
}

if [[ "$DO_UNINSTALL" -eq 1 ]]; then
    echo "Removing previous PuzzlEQ versions..."
    remove_one "${HOME}/.vst3/PuzzlEQ.vst3"
    remove_one "${HOME}/.clap/PuzzlEQ.clap"
    remove_one "${HOME}/.local/bin/PuzzlEQ"
    remove_one "${HOME}/.local/share/applications/puzzleq.desktop"
    remove_one "/usr/lib/vst3/PuzzlEQ.vst3"
    remove_one "/usr/lib/clap/PuzzlEQ.clap"
    remove_one "/usr/local/bin/PuzzlEQ"
    remove_one "/usr/share/applications/puzzleq.desktop"
    if [[ "$(id -u)" -ne 0 ]]; then
        if [[ -e "/usr/lib/vst3/PuzzlEQ.vst3" || -e "/usr/lib/clap/PuzzlEQ.clap" || -e "/usr/local/bin/PuzzlEQ" ]]; then
            echo "System copies need root. Re-running with sudo..."
            exec sudo -- "$0" --uninstall
        fi
    fi
    echo "Done."
    exit 0
fi

find_payload() {
    local candidates=(
        "${SCRIPT_DIR}/linux-x86_64"
        "${SCRIPT_DIR}/../linux-x86_64"
        "${SCRIPT_DIR}/../build/PuzzlEQ_artefacts/Release"
        "${SCRIPT_DIR}/../../build/PuzzlEQ_artefacts/Release"
    )
    local dir
    for dir in "${candidates[@]}"; do
        [[ -d "$dir" ]] || continue
        if [[ -d "${dir}/PuzzlEQ.vst3" ]]; then
            echo "$dir"
            return 0
        fi
        if [[ -d "${dir}/VST3/PuzzlEQ.vst3" ]]; then
            echo "$dir"
            return 0
        fi
    done
    return 1
}

PAYLOAD="$(find_payload)" || {
    echo "Could not find PuzzlEQ.vst3 next to the installer or in build/PuzzlEQ_artefacts/Release." >&2
    exit 1
}

if [[ -d "${PAYLOAD}/VST3/PuzzlEQ.vst3" ]]; then
    SRC_VST3="${PAYLOAD}/VST3/PuzzlEQ.vst3"
    SRC_CLAP="${PAYLOAD}/CLAP/PuzzlEQ.clap"
    SRC_BIN="${PAYLOAD}/Standalone/PuzzlEQ"
else
    SRC_VST3="${PAYLOAD}/PuzzlEQ.vst3"
    SRC_CLAP="${PAYLOAD}/PuzzlEQ.clap"
    SRC_BIN="${PAYLOAD}/PuzzlEQ"
fi

if [[ ! -d "$SRC_VST3" ]]; then
    echo "Missing VST3 bundle: $SRC_VST3" >&2
    exit 1
fi

ask_gui() {
    local text="$1"
    if [[ -n "${DISPLAY:-}${WAYLAND_DISPLAY:-}" ]] && command -v zenity >/dev/null 2>&1; then
        zenity --question --title="PuzzlEQ" --ok-label="Install" --cancel-label="Cancel" --no-wrap --text="$text"
        return $?
    fi
    if [[ -n "${DISPLAY:-}${WAYLAND_DISPLAY:-}" ]] && command -v kdialog >/dev/null 2>&1; then
        kdialog --yesno "$text" --title "PuzzlEQ"
        return $?
    fi
    return 0
}

notify_gui() {
    local text="$1"
    if command -v zenity >/dev/null 2>&1 && [[ -n "${DISPLAY:-}${WAYLAND_DISPLAY:-}" ]]; then
        zenity --info --title="PuzzlEQ" --no-wrap --text="$text" || true
    elif command -v notify-send >/dev/null 2>&1; then
        notify-send "PuzzlEQ" "$text" || true
    fi
}

if [[ -t 0 || -n "${DISPLAY:-}${WAYLAND_DISPLAY:-}" ]]; then
    if ! ask_gui "Install PuzzlEQ into your plugin folders?

VST3 → ${VST3_DIR}/PuzzlEQ.vst3
CLAP → ${CLAP_DIR}/PuzzlEQ.clap
App  → ${BIN_DIR}/PuzzlEQ

Then rescan plugins in your DAW."; then
        echo "Cancelled."
        exit 1
    fi
fi

mkdir -p "$VST3_DIR" "$CLAP_DIR" "$BIN_DIR" "$APP_DIR"

echo "Installing PuzzlEQ ($MODE)..."
rm -rf "${VST3_DIR}/PuzzlEQ.vst3"
cp -a "$SRC_VST3" "${VST3_DIR}/PuzzlEQ.vst3"
echo "  VST3  ${VST3_DIR}/PuzzlEQ.vst3"

if [[ -f "$SRC_CLAP" ]]; then
    cp -a "$SRC_CLAP" "${CLAP_DIR}/PuzzlEQ.clap"
    chmod 755 "${CLAP_DIR}/PuzzlEQ.clap"
    echo "  CLAP  ${CLAP_DIR}/PuzzlEQ.clap"
fi

if [[ -f "$SRC_BIN" ]]; then
    cp -a "$SRC_BIN" "${BIN_DIR}/PuzzlEQ"
    chmod 755 "${BIN_DIR}/PuzzlEQ"
    echo "  App   ${BIN_DIR}/PuzzlEQ"
    cat > "${APP_DIR}/puzzleq.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=PuzzlEQ
Comment=PuzzlEQ parametric equalizer
Exec=${BIN_DIR}/PuzzlEQ
Icon=audio-x-generic
Terminal=false
Categories=AudioVideo;Audio;
StartupNotify=true
EOF
    chmod 644 "${APP_DIR}/puzzleq.desktop"
    command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database "$APP_DIR" >/dev/null 2>&1 || true
fi

if [[ "$MODE" == "user" ]]; then
    case ":${PATH}:" in
        *":${BIN_DIR}:"*) ;;
        *) echo "  Note: add ${BIN_DIR} to PATH if the standalone app is not found." ;;
    esac
fi

MSG="PuzzlEQ is installed.

VST3: ${VST3_DIR}/PuzzlEQ.vst3
CLAP: ${CLAP_DIR}/PuzzlEQ.clap

Rescan plugins in your DAW (Ableton, Reaper, Bitwig, Ardour…)."
echo
echo "$MSG"
notify_gui "$MSG"
