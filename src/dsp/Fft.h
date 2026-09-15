#pragma once

#include <vector>
#include <cstddef>

namespace puzzleq {

class Fft
{
public:
    void setup (int size);

    int size() const noexcept { return n; }

    // In-place complex FFT. re/im length = n.
    void forward (float* re, float* im) const;
    void inverse (float* re, float* im) const;

    // Real forward: time[n] -> re[n/2+1], im[n/2+1] packed into mag/phase helpers.
    void forwardReal (const float* time, float* re, float* im) const;
    void inverseReal (const float* re, const float* im, float* time) const;

private:
    int n = 0;
    std::vector<int> bitrev;
    std::vector<float> wr, wi;

    void transform (float* re, float* im, bool inverse) const;
};

void applyHann (float* dest, const float* src, int n);
void applyHannInPlace (float* x, int n);

} // namespace puzzleq
