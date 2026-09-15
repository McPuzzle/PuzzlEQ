#pragma once

#include <array>
#include <cstdint>

namespace puzzleq {

inline constexpr int kMaxBands = 24;
inline constexpr int kMaxSections = 16;
inline constexpr float kMinHz = 10.0f;
inline constexpr float kMaxHz = 30000.0f;
inline constexpr float kMinGainDb = -30.0f;
inline constexpr float kMaxGainDb = 30.0f;
inline constexpr float kMinQ = 0.1f;
inline constexpr float kMaxQ = 40.0f;
inline constexpr float kMinSlope = 6.0f;
inline constexpr float kMaxSlope = 96.0f;

enum class FilterShape : int
{
    Bell = 0,
    Notch,
    HighShelf,
    LowShelf,
    HighCut,
    LowCut,
    BandPass,
    TiltShelf,
    FlatTilt,
    AllPass,
    NumShapes
};

enum class StereoPlacement : int
{
    Stereo = 0,
    Left,
    Right,
    Mid,
    Side,
    NumPlacements
};

enum class ProcessingMode : int
{
    ZeroLatency = 0,
    NaturalPhase,
    LinearPhase,
    NumModes
};

enum class CharacterMode : int
{
    Off = 0,
    Gentle,
    Warm,
    NumModes
};

enum class DynamicTrigger : int
{
    Band = 0,
    Free,
    External
};

enum class LinearResolution : int
{
    Low = 0,
    Medium,
    High,
    VeryHigh,
    Maximum,
    NumResolutions
};

inline constexpr const char* kShapeNames[] = {
    "Bell", "Notch", "High Shelf", "Low Shelf", "High Cut",
    "Low Cut", "Band Pass", "Tilt Shelf", "Flat Tilt", "All Pass"
};

inline constexpr const char* kPlacementNames[] = {
    "Stereo", "Left", "Right", "Mid", "Side"
};

inline constexpr const char* kModeNames[] = {
    "Zero Latency", "Natural Phase", "Linear Phase"
};

inline bool shapeUsesGain (FilterShape s) noexcept
{
    return s != FilterShape::HighCut
        && s != FilterShape::LowCut
        && s != FilterShape::Notch
        && s != FilterShape::BandPass
        && s != FilterShape::AllPass;
}

inline bool shapeUsesQ (FilterShape s) noexcept
{
    return s == FilterShape::Bell
        || s == FilterShape::Notch
        || s == FilterShape::BandPass
        || s == FilterShape::AllPass;
}

inline bool shapeUsesSlope (FilterShape s) noexcept
{
    return s == FilterShape::HighCut
        || s == FilterShape::LowCut
        || s == FilterShape::HighShelf
        || s == FilterShape::LowShelf
        || s == FilterShape::TiltShelf
        || s == FilterShape::BandPass;
}

inline bool shapeCanBeDynamic (FilterShape s) noexcept
{
    return s == FilterShape::Bell
        || s == FilterShape::HighShelf
        || s == FilterShape::LowShelf
        || s == FilterShape::TiltShelf
        || s == FilterShape::FlatTilt;
}

} // namespace puzzleq
