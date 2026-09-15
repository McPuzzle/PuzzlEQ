#pragma once

#include "state/Types.h"

namespace puzzleq {

struct BandState
{
    bool active = false;
    bool enabled = true;
    FilterShape shape = FilterShape::Bell;
    float frequencyHz = 1000.0f;
    float gainDb = 0.0f;
    float q = 1.0f;
    float slopeDbOct = 12.0f;
    bool brickwall = false;
    StereoPlacement placement = StereoPlacement::Stereo;

    float dynRangeDb = 0.0f;
    float thresholdDb = -24.0f;
    float attackMs = 12.0f;
    float releaseMs = 80.0f;
    bool spectral = false;
    DynamicTrigger trigger = DynamicTrigger::Band;
    float scLowHz = 20.0f;
    float scHighHz = 20000.0f;

    bool isUsed() const noexcept { return active; }

    bool isProcessing() const noexcept { return active && enabled; }

    float effectiveGain (float gainScale) const noexcept
    {
        return gainDb * gainScale;
    }
};

struct GlobalState
{
    ProcessingMode mode = ProcessingMode::ZeroLatency;
    LinearResolution lpResolution = LinearResolution::Medium;
    float outputGainDb = 0.0f;
    bool autoGain = false;
    float gainScale = 1.0f;
    CharacterMode character = CharacterMode::Off;
    bool phaseInvert = false;
    bool pianoRoll = false;
    float analyzerTilt = 4.5f;
    float analyzerRangeDb = 12.0f;
    float analyzerSpeed = 0.55f;
    bool analyzerFreeze = false;
    bool analyzerPre = true;
    int soloBand = -1;
    int displayRangeDb = 12;
};

} // namespace puzzleq
