#pragma once

#include <string>
#include <vector>

namespace puzzleq {

struct SpectrumPeak
{
    float hz = 0.0f;
    float magDb = -120.0f;
    float prominenceDb = 0.0f;
};

struct SpectrumReport
{
    bool ok = false;
    int fftSize = 0;
    float sampleRate = 48000.0f;
    float harshHz = 3200.0f;
    float harshMagDb = -60.0f;
    float harshProminenceDb = 0.0f;
    float sibilanceHz = 7500.0f;
    float mudHz = 280.0f;
    float presenceHz = 3500.0f;
    float rumbleHz = 40.0f;
    float peakHz = 1000.0f;
    std::vector<SpectrumPeak> topPeaks;
    std::string summary;
};

// Highest-quality free analysis: plugin FFT + peak-hold + 1/3-octave prominence
// + parabolic interpolation. No cloud model required.
SpectrumReport analyzeSpectrum (const std::vector<float>& magDb,
                                const std::vector<float>& peakDb,
                                float sampleRate,
                                int fftSize);

} // namespace puzzleq
