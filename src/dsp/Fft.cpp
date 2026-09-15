#include "dsp/Fft.h"
#include <cmath>
#include <algorithm>

namespace puzzleq {

namespace {
constexpr float kPi = 3.14159265358979323846f;
}

void Fft::setup (int size)
{
    n = 1;
    while (n < size)
        n <<= 1;

    bitrev.resize (static_cast<size_t> (n));
    wr.resize (static_cast<size_t> (n / 2));
    wi.resize (static_cast<size_t> (n / 2));

    int bits = 0;
    for (int t = n; t > 1; t >>= 1)
        ++bits;

    for (int i = 0; i < n; ++i)
    {
        int r = 0;
        int x = i;
        for (int b = 0; b < bits; ++b)
        {
            r = (r << 1) | (x & 1);
            x >>= 1;
        }
        bitrev[static_cast<size_t> (i)] = r;
    }

    for (int i = 0; i < n / 2; ++i)
    {
        const float a = -2.0f * kPi * static_cast<float> (i) / static_cast<float> (n);
        wr[static_cast<size_t> (i)] = std::cos (a);
        wi[static_cast<size_t> (i)] = std::sin (a);
    }
}

void Fft::transform (float* re, float* im, bool inverse) const
{
    for (int i = 0; i < n; ++i)
    {
        const int j = bitrev[static_cast<size_t> (i)];
        if (j > i)
        {
            std::swap (re[i], re[j]);
            std::swap (im[i], im[j]);
        }
    }

    for (int len = 2; len <= n; len <<= 1)
    {
        const int half = len / 2;
        const int step = n / len;
        for (int i = 0; i < n; i += len)
        {
            int tw = 0;
            for (int j = 0; j < half; ++j, tw += step)
            {
                float wrr = wr[static_cast<size_t> (tw)];
                float wii = wi[static_cast<size_t> (tw)];
                if (inverse)
                    wii = -wii;

                const int even = i + j;
                const int odd = even + half;
                const float tr = wrr * re[odd] - wii * im[odd];
                const float ti = wrr * im[odd] + wii * re[odd];
                re[odd] = re[even] - tr;
                im[odd] = im[even] - ti;
                re[even] += tr;
                im[even] += ti;
            }
        }
    }

    if (inverse)
    {
        const float s = 1.0f / static_cast<float> (n);
        for (int i = 0; i < n; ++i)
        {
            re[i] *= s;
            im[i] *= s;
        }
    }
}

void Fft::forward (float* re, float* im) const { transform (re, im, false); }
void Fft::inverse (float* re, float* im) const { transform (re, im, true); }

void Fft::forwardReal (const float* time, float* re, float* im) const
{
    for (int i = 0; i < n; ++i)
    {
        re[i] = time[i];
        im[i] = 0.0f;
    }
    forward (re, im);
}

void Fft::inverseReal (const float* re, const float* im, float* time) const
{
    std::vector<float> rr (static_cast<size_t> (n)), ii (static_cast<size_t> (n));
    for (int i = 0; i < n; ++i)
    {
        rr[static_cast<size_t> (i)] = re[i];
        ii[static_cast<size_t> (i)] = im[i];
    }
    inverse (rr.data(), ii.data());
    for (int i = 0; i < n; ++i)
        time[i] = rr[static_cast<size_t> (i)];
}

void applyHann (float* dest, const float* src, int n)
{
    if (n <= 1)
        return;
    for (int i = 0; i < n; ++i)
    {
        const float w = 0.5f * (1.0f - std::cos (2.0f * kPi * static_cast<float> (i) / static_cast<float> (n - 1)));
        dest[i] = src[i] * w;
    }
}

void applyHannInPlace (float* x, int n)
{
    applyHann (x, x, n);
}

} // namespace puzzleq
