#pragma once

#include "state/BandState.h"
#include "dsp/FilterDesign.h"
#include "dsp/DynamicEngine.h"
#include "dsp/LinearPhaseEq.h"
#include "dsp/SpectralDynamics.h"
#include "dsp/SpectrumAnalyzer.h"
#include "dsp/EqMatch.h"
#include "dsp/Character.h"
#include <array>

namespace puzzleq {

class EqEngine
{
public:
    void prepare (float sampleRate, int maxBlock);
    void reset();

    void setBands (const std::array<BandState, kMaxBands>& bands);
    void setGlobal (const GlobalState& g);

    void process (float* left, float* right, const float* sideL, const float* sideR, int numSamples);

    SpectrumAnalyzer& analyzer() noexcept { return spectrum; }
    EqMatch& matcher() noexcept { return match; }
    const EqMatch& matcher() const noexcept { return match; }

    int latencySamples() const noexcept;
    float lastAutoGainDb() const noexcept { return autoGainDb; }

    // Composite magnitude of the static (non-spectral) curve, including gain scale.
    float compositeMagnitudeDb (float hz) const;

    const std::array<BandState, kMaxBands>& bands() const noexcept { return currentBands; }
    const GlobalState& global() const noexcept { return currentGlobal; }

private:
    void refreshCascades (int bandIndex, float extraGainDb);
    void processIir (float* left, float* right, const float* sideL, const float* sideR, int numSamples);

    float sr = 48000.0f;
    std::array<BandState, kMaxBands> currentBands {};
    GlobalState currentGlobal {};

    std::array<Cascade, kMaxBands> casL {}, casR {};
    std::array<Cascade, kMaxBands> scBandL {}, scBandR {};
    std::array<DynamicBand, kMaxBands> dyn {};
    std::array<float, kMaxBands> lastDesignedGain {};
    std::array<BandState, kMaxBands> lastDesign {};

    LinearPhaseEq linear;
    SpectralDynamics spectral;
    SpectrumAnalyzer spectrum;
    EqMatch match;

    float autoGainDb = 0.0f;
    float autoGainSmooth = 1.0f;
    int samplesUntilLinearRebuild = 0;
};

} // namespace puzzleq
