#!/usr/bin/env bash
# Pack Windows Release artefacts into a double-click installer folder/zip.
# Run this on Windows (Git Bash) after a Windows build. Linux .so files will not work.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VERSION="$(tr -d '[:space:]' < "${ROOT}/VERSION")"
ART="${PUZZLEQ_ARTEFACTS:-${ROOT}/build/PuzzlEQ_artefacts/Release}"
OUT="${1:-${ROOT}/dist}"

VST3="${ART}/VST3/PuzzlEQ.vst3"
if [[ ! -e "$VST3" ]]; then
    echo "Missing $VST3 - build PuzzlEQ_VST3 on Windows first." >&2
    exit 1
fi

# Refuse to pack a Linux bundle into a Windows installer.
if [[ -d "${VST3}/Contents/x86_64-linux" ]]; then
    echo "This is a Linux VST3. Build on Windows (or CI) before packaging the Windows installer." >&2
    exit 1
fi

STAGE="${OUT}/PuzzlEQ-Install-Windows"
rm -rf "$STAGE"
mkdir -p "$STAGE"
cp -a "$VST3" "$STAGE/"
[[ -e "${ART}/CLAP/PuzzlEQ.clap" ]] && cp -a "${ART}/CLAP/PuzzlEQ.clap" "$STAGE/"
if [[ -f "${ART}/Standalone/PuzzlEQ.exe" ]]; then
    cp -a "${ART}/Standalone/PuzzlEQ.exe" "$STAGE/"
fi

cp -a "${ROOT}/scripts/install-windows.ps1" "$STAGE/"
cp -a "${ROOT}/scripts/uninstall-windows.ps1" "$STAGE/"
cp -a "${ROOT}/scripts/Install-PuzzlEQ.bat" "$STAGE/"
cp -a "${ROOT}/scripts/Uninstall-PuzzlEQ.bat" "$STAGE/"
cp -a "${ROOT}/LICENSE" "$STAGE/"
cp -a "${ROOT}/VERSION" "$STAGE/"

cat > "$STAGE/README.txt" <<EOF
PuzzlEQ v${VERSION} - Windows installer
=======================================

Install:      double-click  Install-PuzzlEQ.bat
Uninstall:    double-click  Uninstall-PuzzlEQ.bat

The uninstaller removes every previous PuzzlEQ copy it can find:
  C:\\Program Files\\Common Files\\VST3\\PuzzlEQ.vst3
  C:\\Program Files\\Common Files\\CLAP\\PuzzlEQ.clap
  C:\\Program Files\\PuzzlEQ
  per-user copies under %LOCALAPPDATA%
  leftover files in Image-Line / FL Studio plugin folders

Then fully quit the DAW and rescan plugins.

Per-user install (no admin):  powershell -ExecutionPolicy Bypass -File install-windows.ps1 -User
EOF

mkdir -p "$OUT"
ZIP="${OUT}/PuzzlEQ-Install-Windows.zip"
rm -f "$ZIP"
if command -v zip >/dev/null 2>&1; then
    ( cd "$OUT" && zip -r -y "$(basename "$ZIP")" "$(basename "$STAGE")" )
elif command -v powershell.exe >/dev/null 2>&1; then
    STAGE_WIN="$(cygpath -w "$STAGE" 2>/dev/null || echo "$STAGE")"
    ZIP_WIN="$(cygpath -w "$ZIP" 2>/dev/null || echo "$ZIP")"
    powershell.exe -NoProfile -Command "Compress-Archive -Path '${STAGE_WIN}' -DestinationPath '${ZIP_WIN}' -Force"
else
    python -c "import shutil, os; out=os.path.abspath(os.environ.get('PUZZLEQ_OUT','.')); os.chdir(out); shutil.make_archive('PuzzlEQ-Install-Windows', 'zip', '.', 'PuzzlEQ-Install-Windows')"
fi
echo "Wrote $ZIP"
