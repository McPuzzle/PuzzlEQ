#!/usr/bin/env bash
# Pack Release artefacts into a self-extracting installer and an optional .deb.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VERSION="$(tr -d '[:space:]' < "${ROOT}/VERSION")"
ART="${PUZZLEQ_ARTEFACTS:-${ROOT}/build/PuzzlEQ_artefacts/Release}"
OUT="${1:-${ROOT}/dist}"

VST3="${ART}/VST3/PuzzlEQ.vst3"
CLAP="${ART}/CLAP/PuzzlEQ.clap"
BIN="${ART}/Standalone/PuzzlEQ"

if [[ ! -d "$VST3" ]]; then
    echo "Missing $VST3 — build PuzzlEQ_VST3 first." >&2
    exit 1
fi

mkdir -p "$OUT"
STAGE="$(mktemp -d)"
trap 'rm -rf "$STAGE"' EXIT

PAYLOAD="${STAGE}/payload"
mkdir -p "${PAYLOAD}/linux-x86_64"
cp -a "$VST3" "${PAYLOAD}/linux-x86_64/PuzzlEQ.vst3"
[[ -f "$CLAP" ]] && cp -a "$CLAP" "${PAYLOAD}/linux-x86_64/PuzzlEQ.clap"
[[ -f "$BIN" ]] && cp -a "$BIN" "${PAYLOAD}/linux-x86_64/PuzzlEQ"
cp -a "${ROOT}/scripts/install-linux.sh" "${PAYLOAD}/install-linux.sh"
cp -a "${ROOT}/LICENSE" "${PAYLOAD}/LICENSE"
chmod 755 "${PAYLOAD}/install-linux.sh" "${PAYLOAD}/linux-x86_64/PuzzlEQ" "${PAYLOAD}/linux-x86_64/PuzzlEQ.clap" 2>/dev/null || true

cat > "${PAYLOAD}/README.txt" <<EOF
PuzzlEQ v${VERSION} — Linux installer
====================================

Run:

  chmod +x PuzzlEQ-Install-linux-x86_64.run
  ./PuzzlEQ-Install-linux-x86_64.run

That copies the plugin into:

  ~/.vst3/PuzzlEQ.vst3
  ~/.clap/PuzzlEQ.clap
  ~/.local/bin/PuzzlEQ

No manual copy into a VST folder. Then rescan plugins in your DAW.

Options:
  ./PuzzlEQ-Install-linux-x86_64.run --system     (system-wide, uses sudo)
  ./PuzzlEQ-Install-linux-x86_64.run --uninstall

Debian/Ubuntu alternative:
  sudo dpkg -i puzzleq_${VERSION}_amd64.deb
EOF

# --- self-extracting .run ---
ARCHIVE="${STAGE}/payload.tar.gz"
tar -C "$PAYLOAD" -czf "$ARCHIVE" .
STUB="${STAGE}/stub.sh"
cat > "$STUB" <<'STUB'
#!/usr/bin/env bash
set -euo pipefail
INSTALLER="$0"
ARGS=("$@")
ARCHIVE_LINE=0
ARCHIVE_LINE="$(awk '/^__PUZZLEQ_ARCHIVE__$/{print NR+1; exit}' "$INSTALLER")"
if [[ -z "$ARCHIVE_LINE" || "$ARCHIVE_LINE" -lt 2 ]]; then
    echo "Corrupt installer (missing archive marker)." >&2
    exit 1
fi
WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT
tail -n +"$ARCHIVE_LINE" "$INSTALLER" | tar -xz -C "$WORKDIR"
exec bash "$WORKDIR/install-linux.sh" "${ARGS[@]}"
STUB
RUN="${OUT}/PuzzlEQ-Install-linux-x86_64.run"
cat "$STUB" > "$RUN"
printf '\n__PUZZLEQ_ARCHIVE__\n' >> "$RUN"
cat "$ARCHIVE" >> "$RUN"
chmod 755 "$RUN"

# --- .deb ---
DEB_OK=0
if command -v dpkg-deb >/dev/null 2>&1; then
    DEBROOT="${STAGE}/deb"
    mkdir -p "${DEBROOT}/DEBIAN" \
             "${DEBROOT}/usr/lib/vst3" \
             "${DEBROOT}/usr/lib/clap" \
             "${DEBROOT}/usr/bin" \
             "${DEBROOT}/usr/share/applications" \
             "${DEBROOT}/usr/share/doc/puzzleq"
    cp -a "$VST3" "${DEBROOT}/usr/lib/vst3/PuzzlEQ.vst3"
    [[ -f "$CLAP" ]] && cp -a "$CLAP" "${DEBROOT}/usr/lib/clap/PuzzlEQ.clap"
    if [[ -f "$BIN" ]]; then
        cp -a "$BIN" "${DEBROOT}/usr/bin/PuzzlEQ"
        chmod 755 "${DEBROOT}/usr/bin/PuzzlEQ"
    fi
    cat > "${DEBROOT}/usr/share/applications/puzzleq.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=PuzzlEQ
Comment=PuzzlEQ parametric equalizer
Exec=/usr/bin/PuzzlEQ
Icon=audio-x-generic
Terminal=false
Categories=AudioVideo;Audio;
EOF
    cp -a "${ROOT}/LICENSE" "${DEBROOT}/usr/share/doc/puzzleq/copyright"
    INSTALLED_SIZE="$(du -sk "${DEBROOT}/usr" | awk '{print $1}')"
    cat > "${DEBROOT}/DEBIAN/control" <<EOF
Package: puzzleq
Version: ${VERSION}
Section: sound
Priority: optional
Architecture: amd64
Installed-Size: ${INSTALLED_SIZE}
Maintainer: Puzzl <puzzl@localhost>
Homepage: https://puzzl.dev
Description: PuzzlEQ parametric equalizer (VST3, CLAP, standalone)
 Premium 24-band EQ plugin. Installs into the system VST3 and CLAP folders
 so DAWs find it after a plugin rescan.
EOF
    DEB="${OUT}/puzzleq_${VERSION}_amd64.deb"
    dpkg-deb --build --root-owner-group "$DEBROOT" "$DEB" >/dev/null
    DEB_OK=1
fi

echo "Wrote $RUN"
[[ "$DEB_OK" -eq 1 ]] && echo "Wrote ${OUT}/puzzleq_${VERSION}_amd64.deb"
ls -lh "$RUN" ${DEB_OK:+${OUT}/puzzleq_${VERSION}_amd64.deb}
