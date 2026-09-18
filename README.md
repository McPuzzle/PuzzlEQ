# PuzzlEQ

A premium parametric equalizer plugin. Original DSP and UI. Not a clone of any commercial EQ.

**Formats:** VST3, AU (macOS), CLAP, Standalone  
**Platforms:** macOS, Windows, Linux  
**License:** AGPLv3 (JUCE commercial license required for closed-source distribution)

## Features (v0.3.10)

- 24-band parametric EQ: Bell, Notch, High/Low Shelf, High/Low Cut, Band Pass, Tilt Shelf, Flat Tilt, All Pass
- Analog-matched Zero Latency / Natural Phase via 2× oversampling, Q compensation, and fractional slopes (6–96 dB/oct)
- Per-band Stereo / Left / Right / Mid / Side
- Zero Latency, Natural Phase, and Linear Phase processing
- Dynamic EQ with attack, release, threshold, and sidechain trigger
- Spectral dynamics (per-bin treatment inside a band)
- Spectrum analyzer (pre/post), parabolic Spectrum Grab, piano roll, intelligent solo
- Auto Gain, Gain Scale, Gentle / Warm character
- Residual EQ Match (Cap Src / Cap Ref / Match) plus Match Overlay from another instance
- Interactive curve editor: multi-select drag, EQ Sketch, text entry, Hz/note readout, fullscreen
- Host-aware Bypass, undo / redo, A/B, MIDI Learn, instance clipboard
- Remote instance edit (Edit Inst writes into another PuzzlEQ on the same host)
- Factory presets (vocal, mix bus, master, de-mud, air, telephone, kick, snare, guitar)
- Instance list with spectrum collision overlay
- Intelligent solo, sidechain listen, stereo I/O meters, latency readout
- **EQ Chat (Hebrew + English):** type mix notes and PuzzlEQ writes bands. It always runs a free local FFT analyze first (smoothed mag + peak-hold, 1/3-octave prominence, parabolic peaks) and only then applies the move or asks a free model. Examples: `roll off the low end`, `make the band narrower`, `תוריד ב 500 הרץ 3 דיבי`, `תעשה את הברנד צר יותר`, `תוסיף קצת גבוהים מאיזור ה 8 אלף`, `תוסיף בל דינאמי צר באיזור הכי Harsh הווקאל הזה`. Optional free models: local [Ollama](https://ollama.com), [Groq](https://console.groq.com) Llama 3.1 8B, or [Gemini](https://aistudio.google.com/apikey) 2.0 Flash (keys stay in `PuzzlEQ/assistant.json`). Open with **Chat** or `/`.

## Install

The Linux `.run` / `.deb` **will not load on Windows or macOS**. Each OS needs its own build and installer.

### macOS (VST3 + AU + CLAP)

```bash
chmod +x scripts/build-and-install-macos.sh
./scripts/build-and-install-macos.sh
```

That compiles on this Mac and copies into `~/Library/Audio/Plug-Ins/VST3`, `.../Components`, and `.../CLAP`. Then rescan (restart Logic). Double-click installer after a build: `./scripts/package-macos.sh` → `dist/PuzzlEQ-Install-macOS.zip` → **Install-PuzzlEQ.command**.

### Windows (VST3 + CLAP)

In PowerShell from the source tree:

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\scripts\build-and-install-windows.ps1
```

That compiles with Visual Studio and copies into `C:\Program Files\Common Files\VST3` (UAC prompt). Then rescan the DAW. After a Windows build, `scripts\package-windows.sh` makes `dist\PuzzlEQ-Install-Windows.zip`. Unzip it, fully quit the DAW, double-click **Uninstall-PuzzlEQ.bat**, then **Install-PuzzlEQ.bat**, then rescan. Optional GUI setup: compile `installer/windows/PuzzlEQ.iss` with Inno Setup.

### Linux

```bash
./scripts/package-linux-installer.sh
./dist/PuzzlEQ-Install-linux-x86_64.run
```

### CI installers

Push to GitHub and run the **build-installers** workflow. It uploads `PuzzlEQ-Install-Windows.zip` and `PuzzlEQ-Install-macOS.zip` (plus a `.pkg` on Mac).

## Build

```bash
# First time: fetch JUCE and CLAP helpers
./scripts/fetch-deps.sh

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --target PuzzlEQ_Standalone PuzzlEQ_VST3 PuzzlEQ_CLAP PuzzlEQTests
./build/PuzzlEQTests
```

Linux needs the usual JUCE packages (`libasound2-dev`, `libfreetype6-dev`, X11 cursor/randr/inerama/composite, GL).

AU is produced only when configuring on macOS.

## Performance

PuzzlEQ is built for real-time hosts: no heap allocations on the audio thread, denormals flushed, and idle instances skip the oversampler.

**Formats:** VST3, AU (macOS), CLAP, Standalone. AAX / Pro Tools is not shipped. Match the installer to the OS (Windows zip on Windows, macOS zip/pkg on Mac).

**Buffer size:** In the DAW, start at **256–512 samples** at 44.1/48 kHz. Raise the buffer if you hear clicks or dropouts; lower it only if you need less monitoring latency. Linear Phase and Spectral bands cost more CPU than Zero Latency.

**Hardware:** A recent dual-core CPU and 8 GB RAM is enough for a few instances. Many instances, Maximum linear-phase resolution, or lots of dynamic/spectral bands need more headroom.

**Updates:** Use the latest GitHub Release so the DAW loads a binary that matches current Windows/macOS. After installing, fully quit the DAW and rescan plugins.
