#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$root/modules"
if [ ! -f "$root/modules/JUCE/CMakeLists.txt" ]; then
  git clone --depth 1 --branch 8.0.10 https://github.com/juce-framework/JUCE.git "$root/modules/JUCE"
fi
if [ ! -f "$root/modules/clap-juce-extensions/CMakeLists.txt" ]; then
  git clone --depth 1 --recurse-submodules https://github.com/free-audio/clap-juce-extensions.git "$root/modules/clap-juce-extensions"
fi
echo "Dependencies ready."
