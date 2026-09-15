#!/usr/bin/env bash
# Pack macOS Release artefacts into a double-click installer folder/zip.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VERSION="$(tr -d '[:space:]' < "${ROOT}/VERSION")"
ART="${PUZZLEQ_ARTEFACTS:-${ROOT}/build/PuzzlEQ_artefacts/Release}"
OUT="${1:-${ROOT}/dist}"

if [[ ! -d "${ART}/VST3/PuzzlEQ.vst3" ]]; then
    echo "Missing ${ART}/VST3/PuzzlEQ.vst3 — build on macOS first." >&2
    exit 1
fi

STAGE="${OUT}/PuzzlEQ-Install-macOS"
rm -rf "$STAGE"
mkdir -p "$STAGE"

cp -a "${ART}/VST3/PuzzlEQ.vst3" "$STAGE/"
[[ -d "${ART}/AU/PuzzlEQ.component" ]] && cp -a "${ART}/AU/PuzzlEQ.component" "$STAGE/"
[[ -d "${ART}/CLAP/PuzzlEQ.clap" || -f "${ART}/CLAP/PuzzlEQ.clap" ]] && cp -a "${ART}/CLAP/PuzzlEQ.clap" "$STAGE/"
[[ -d "${ART}/Standalone/PuzzlEQ.app" ]] && cp -a "${ART}/Standalone/PuzzlEQ.app" "$STAGE/"

cp -a "${ROOT}/scripts/install-macos.sh" "$STAGE/"
cp -a "${ROOT}/scripts/Install-PuzzlEQ.command" "$STAGE/"
cp -a "${ROOT}/scripts/Uninstall-PuzzlEQ.command" "$STAGE/"
cp -a "${ROOT}/LICENSE" "$STAGE/"
chmod 755 "$STAGE/install-macos.sh" "$STAGE/Install-PuzzlEQ.command" "$STAGE/Uninstall-PuzzlEQ.command"

# Finder often needs this so .command is executable after unzip.
if command -v xattr >/dev/null 2>&1; then
    xattr -cr "$STAGE" || true
fi

cat > "$STAGE/README.txt" <<EOF
PuzzlEQ v${VERSION} — macOS installer
=====================================

Double-click  Install-PuzzlEQ.command

That copies the plugin into:
  ~/Library/Audio/Plug-Ins/VST3/PuzzlEQ.vst3
  ~/Library/Audio/Plug-Ins/Components/PuzzlEQ.component
  ~/Library/Audio/Plug-Ins/CLAP/PuzzlEQ.clap

Then rescan plugins (Logic: restart Logic). No dragging into a VST folder.

If macOS blocks the .command: right-click → Open.
Uninstall:  double-click Uninstall-PuzzlEQ.command
            (removes user and system copies from previous versions)
EOF

mkdir -p "$OUT"
ZIP="${OUT}/PuzzlEQ-Install-macOS.zip"
rm -f "$ZIP"
( cd "$OUT" && zip -r -y "$(basename "$ZIP")" "$(basename "$STAGE")" )
echo "Wrote $ZIP"

# Real .pkg when pkgbuild exists (macOS).
if command -v pkgbuild >/dev/null 2>&1; then
    PKGROOT="$(mktemp -d)"
    trap 'rm -rf "$PKGROOT"' RETURN
    mkdir -p "$PKGROOT/Library/Audio/Plug-Ins/VST3" \
             "$PKGROOT/Library/Audio/Plug-Ins/Components" \
             "$PKGROOT/Library/Audio/Plug-Ins/CLAP"
    cp -a "${ART}/VST3/PuzzlEQ.vst3" "$PKGROOT/Library/Audio/Plug-Ins/VST3/"
    [[ -d "${ART}/AU/PuzzlEQ.component" ]] && cp -a "${ART}/AU/PuzzlEQ.component" "$PKGROOT/Library/Audio/Plug-Ins/Components/"
    [[ -e "${ART}/CLAP/PuzzlEQ.clap" ]] && cp -a "${ART}/CLAP/PuzzlEQ.clap" "$PKGROOT/Library/Audio/Plug-Ins/CLAP/"
    pkgbuild --root "$PKGROOT" \
             --identifier com.puzzl.puzzleq \
             --version "$VERSION" \
             --install-location / \
             "${OUT}/PuzzlEQ-${VERSION}-macOS.pkg"
    echo "Wrote ${OUT}/PuzzlEQ-${VERSION}-macOS.pkg"
fi
