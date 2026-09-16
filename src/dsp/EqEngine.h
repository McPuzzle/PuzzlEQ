#pragma once

#include "state/BandState.h"
#include "dsp/FilterDesign.h"
#include "dsp/DynamicEngine.h"
#include "dsp/LinearPhaseEq.h"
#include "dsp/SpectralDynamics.h"
#include "dsp/SpectrumAnalyzer.h"
#include "dsp/EqMatch.h"
#include "dsp/Character.h"
#include "dsp/TptSvf.h"
#include "dsp/Oversampler2x.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

namespace puzzleq {

class EqEngine
{
public:
    void prepare (float sampleRate, int maxBlock);
    void reset();

    void setBands (const std::array<BandState, kMaxBands>& bands);
    void setGlobal (const GlobalState& g);
    void setSidechainListen (bool on, int band) noexcept
    {
        scListen = on;
        listenBand = band;
    }

    void process (float* left, float* right, const float* sideL, const float* sideR, int numSamples);
    int maxBlockSize() const noexcept { return maxBlock; }

    SpectrumAnalyzer& analyzer() noexcept { return spectrum; }
    EqMatch& matcher() noexcept { return match; }
    const EqMatch& matcher() const noexcept { return match; }
    LinearPhaseEq& linearPhase() noexcept { return linear; }

    int latencySamples() const noexcept;
    float lastAutoGainDb() const noexcept { return autoGainDb; }
    float dynamicGainDb (int band) const noexcept
    {
        if (band < 0 || band >= kMaxBands)
            return 0.0f;
        return dynGainDb[static_cast<size_t> (band)];
    }

    float compositeMagnitudeDb (float hz) const;
    float bandMagnitudeDb (int band, float hz) const;

    const std::array<BandState, kMaxBands>& bands() const noexcept { return currentBands; }
    const GlobalState& global() const noexcept { return currentGlobal; }

private:
    void refreshCascades (int bandIndex, float extraGainDb);
    void processIir (float* left, float* right, const float* sideL, const float* sideR, int numSamples);
    bool usesTpt (const BandState& b) const noexcept;
    uint64_t bandHash() const noexcept;
    void rebuildActiveList() noexcept;
    bool hasIirWork() const noexcept { return numIir > 0; }

    float sr = 48000.0f;
    int maxBlock = 512;
    std::array<BandState, kMaxBands> currentBands {};
    GlobalState currentGlobal {};

    std::array<Cascade, kMaxBands> casL {}, casR {};
    std::array<Cascade, kMaxBands> scBandL {}, scBandR {};
    std::array<Cascade, 2> soloIso {};
    std::array<DynamicBand, kMaxBands> dyn {};
    std::array<TptSvf, kMaxBands> tptL {}, tptR {}, tptTiltL {}, tptTiltR {};
    std::array<SmoothParam, kMaxBands> smFreq {}, smGain {}, smQ {};
    std::array<float, kMaxBands> lastDesignedGain {};
    std::array<BandState, kMaxBands> lastDesign {};
    std::array<float, kMaxBands> dynGainDb {};

    LinearPhaseEq linear;
    SpectralDynamics spectral;
    SpectrumAnalyzer spectrum;
    EqMatch match;
    Oversampler2x oversampler;
    std::vector<float> scUpL, scUpR;
    std::array<float, kMaxBands> tptFreq {}, tptQ {};
    std::array<int, kMaxBands> activeIir {};
    int numIir = 0;
    bool anySpectral = false;
    uint64_t lastLpHash = 0;

    float autoGainDb = 0.0f;
    float autoGainSmooth = 1.0f;
    int lastSolo = -99;
    std::atomic<bool> scListen { false };
    std::atomic<int> listenBand { -1 };
};

} // namespace puzzleq
