# PuzzlEQ

A premium parametric equalizer plugin. Original DSP and UI. Not a clone of any commercial EQ.

**Formats:** VST3, AU (macOS), CLAP, Standalone  
**Platforms:** macOS, Windows, Linux  
**License:** AGPLv3 (JUCE commercial license required for closed-source distribution)

## Features (v0.2)

- 24-band parametric EQ: Bell, Notch, High/Low Shelf, High/Low Cut, Band Pass, Tilt Shelf, Flat Tilt, All Pass
- Slopes up to 96 dB/oct plus brickwall HP/LP
- Per-band Stereo / Left / Right / Mid / Side
- Zero Latency, Natural Phase, and Linear Phase processing
- Dynamic EQ with attack, release, threshold, and sidechain trigger
- Spectral dynamics (per-bin treatment inside a band)
- Spectrum analyzer (pre/post), Spectrum Grab, piano roll, intelligent solo
- Auto Gain, Gain Scale, Gentle / Warm character
- EQ Match (capture source / reference and fit bands)
- Undo / redo, A/B, MIDI Learn, instance clipboard
- Interactive curve editor with multi-select, text entry, Hz/note readout, fullscreen
- Factory presets (vocal, mix bus, master, de-mud, air, telephone, kick, snare, guitar)
- Instance list with spectrum collision overlay
- Intelligent solo, sidechain listen, stereo I/O meters, latency readout

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
