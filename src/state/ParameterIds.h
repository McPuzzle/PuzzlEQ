#pragma once

#include "state/Types.h"
#include <juce_core/juce_core.h>

namespace puzzleq {

inline juce::String bandId (int band, const char* suffix)
{
    return juce::String::formatted ("b%02d_%s", band, suffix);
}

namespace pid {
inline constexpr const char* bypass       = "bypass";
inline constexpr const char* outputGain   = "outputGain";
inline constexpr const char* processingMode = "processingMode";
inline constexpr const char* lpResolution = "lpResolution";
inline constexpr const char* autoGain     = "autoGain";
inline constexpr const char* gainScale    = "gainScale";
inline constexpr const char* character    = "character";
inline constexpr const char* phaseInvert  = "phaseInvert";
inline constexpr const char* pianoRoll    = "pianoRoll";
inline constexpr const char* analyzerTilt = "analyzerTilt";
inline constexpr const char* analyzerRange = "analyzerRange";
inline constexpr const char* analyzerSpeed = "analyzerSpeed";
inline constexpr const char* analyzerFreeze = "analyzerFreeze";
inline constexpr const char* analyzerPre  = "analyzerPre";
inline constexpr const char* soloBand     = "soloBand";
inline constexpr const char* displayRange = "displayRange";
} // namespace pid

} // namespace puzzleq
