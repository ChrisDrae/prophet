#pragma once

#include <cmath>
#include <complex>
#include <numbers>
#include <vector>

// Measuring tools for the tests: pitch, level and spectrum of a rendered signal.
namespace test
{
/// The frequency of a periodic signal, from the time between its first and last upward zero
/// crossing (each located between two samples). Accurate to a small fraction of a cent.
inline double measureFrequency(const std::vector<float>& x, double sampleRate)
{
    double first = -1.0, last = -1.0;
    int crossings = 0;
    for (size_t i = 1; i < x.size(); ++i)
        if (x[i - 1] < 0.0f && x[i] >= 0.0f)
        {
            const double t = static_cast<double>(i - 1) + static_cast<double>(-x[i - 1]) / static_cast<double>(x[i] - x[i - 1]);
            if (first < 0.0)
                first = t;
            last = t;
            ++crossings;
        }
    return crossings < 2 ? 0.0 : (crossings - 1) * sampleRate / (last - first);
}

/// The distance between two frequencies in cents (100 cents = one semitone).
inline double cents(double hz, double reference)
{
    return 1200.0 * std::log2(hz / reference);
}

inline double rms(const std::vector<float>& x, size_t from = 0)
{
    double sum = 0.0;
    for (size_t i = from; i < x.size(); ++i)
        sum += static_cast<double>(x[i]) * x[i];
    return std::sqrt(sum / static_cast<double>(x.size() - from));
}

inline double peak(const std::vector<float>& x)
{
    double p = 0.0;
    for (float v : x)
        p = std::max(p, static_cast<double>(std::abs(v)));
    return p;
}

inline double toDb(double ratio)
{
    return 20.0 * std::log10(std::max(ratio, 1.0e-12));
}

/// Magnitudes of the FFT of `x` (its length must be a power of two), bins 0 .. N/2.
inline std::vector<double> spectrum(const std::vector<float>& x)
{
    const size_t n = x.size();
    std::vector<std::complex<double>> a(n);
    for (size_t i = 0; i < n; ++i)
        a[i] = x[i];
    for (size_t i = 1, j = 0; i < n; ++i) // bit reversal
    {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
            std::swap(a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double angle = -2.0 * std::numbers::pi / static_cast<double>(len);
        const std::complex<double> wl(std::cos(angle), std::sin(angle));
        for (size_t i = 0; i < n; i += len)
        {
            std::complex<double> w(1.0);
            for (size_t k = 0; k < len / 2; ++k)
            {
                const auto u = a[i + k], v = a[i + k + len / 2] * w;
                a[i + k] = u + v;
                a[i + k + len / 2] = u - v;
                w *= wl;
            }
        }
    }
    std::vector<double> magnitude(n / 2 + 1);
    for (size_t i = 0; i <= n / 2; ++i)
        magnitude[i] = std::abs(a[i]) / static_cast<double>(n / 2);
    return magnitude;
}
} // namespace test
