#pragma once

#include "state/Types.h"
#include <cmath>
#include <algorithm>

namespace puzzleq {

inline float applyCharacter (float x, CharacterMode mode) noexcept
{
    switch (mode)
    {
        case CharacterMode::Off:
        case CharacterMode::NumModes:
            return x;
        case CharacterMode::Gentle:
        {
            // Soft tanh, light drive
            const float d = 1.35f;
            return std::tanh (x * d) / std::tanh (d);
        }
        case CharacterMode::Warm:
        {
            // Even + odd harmonics, still bounded
            const float d = 1.8f;
            const float y = std::tanh (x * d);
            const float even = x * x * (x >= 0.0f ? 1.0f : -1.0f) * 0.12f;
            return std::clamp (y / std::tanh (d) + even, -1.2f, 1.2f);
        }
        default:
            return x;
    }
}

inline float dbToGain (float db) noexcept
{
    return std::pow (10.0f, db / 20.0f);
}

} // namespace puzzleq
