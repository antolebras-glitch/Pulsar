// PULSAR - briques DSP de base (aucune dépendance à JUCE, pour pouvoir tester le moteur seul)
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace pz
{
constexpr double kPi = 3.14159265358979323846;
constexpr float kPiF = 3.14159265358979323846f;
constexpr float kTwoPiF = 6.28318530717958647692f;

inline float dbToGain (float db) noexcept { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g) noexcept { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }
inline float clampf (float x, float lo, float hi) noexcept { return x < lo ? lo : (x > hi ? hi : x); }
inline double clampd (double x, double lo, double hi) noexcept { return x < lo ? lo : (x > hi ? hi : x); }
inline float lerpf (float a, float b, float t) noexcept { return a + (b - a) * t; }

// Coefficient d'un filtre un pôle pour une constante de temps donnée (en secondes).
inline float coefFromTime (float seconds, double sampleRate) noexcept
{
    return seconds <= 0.0f ? 0.0f : (float) std::exp (-1.0 / ((double) seconds * sampleRate));
}

inline int nextPow2 (int v) noexcept
{
    int p = 1;
    while (p < v)
        p <<= 1;
    return p;
}

// Générateur pseudo-aléatoire léger (xorshift32).
struct Rng
{
    uint32_t state = 0x1234abcdu;

    inline uint32_t nextU() noexcept
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }

    inline float bipolar() noexcept { return (float) (int32_t) nextU() * (1.0f / 2147483648.0f); }
    inline float unipolar() noexcept { return (float) (nextU() >> 8) * (1.0f / 16777216.0f); }
};

// Hash déterministe (pour que les glitchs tombent au même endroit à chaque lecture).
inline uint32_t hash32 (uint32_t x) noexcept
{
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

//==============================================================================
// Filtre biquad (forme transposée directe II), coefficients RBJ.
struct Biquad
{
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    float z1 = 0.0f, z2 = 0.0f;

    void reset() noexcept { z1 = z2 = 0.0f; }

    inline float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void setRaw (double B0, double B1, double B2, double A0, double A1, double A2) noexcept
    {
        const double inv = 1.0 / A0;
        b0 = (float) (B0 * inv);
        b1 = (float) (B1 * inv);
        b2 = (float) (B2 * inv);
        a1 = (float) (A1 * inv);
        a2 = (float) (A2 * inv);
    }

    static double safeFreq (double sr, double f) noexcept { return clampd (f, 10.0, sr * 0.47); }

    void setLowpass (double sr, double f, double q) noexcept
    {
        const double w = 2.0 * kPi * safeFreq (sr, f) / sr, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        setRaw ((1.0 - c) * 0.5, 1.0 - c, (1.0 - c) * 0.5, 1.0 + al, -2.0 * c, 1.0 - al);
    }

    void setHighpass (double sr, double f, double q) noexcept
    {
        const double w = 2.0 * kPi * safeFreq (sr, f) / sr, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        setRaw ((1.0 + c) * 0.5, -(1.0 + c), (1.0 + c) * 0.5, 1.0 + al, -2.0 * c, 1.0 - al);
    }

    // Passe-bande à gain crête unitaire.
    void setBandpass (double sr, double f, double q) noexcept
    {
        const double w = 2.0 * kPi * safeFreq (sr, f) / sr, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        setRaw (al, 0.0, -al, 1.0 + al, -2.0 * c, 1.0 - al);
    }

    void setPeak (double sr, double f, double q, double gainDb) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = 2.0 * kPi * safeFreq (sr, f) / sr, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        setRaw (1.0 + al * A, -2.0 * c, 1.0 - al * A, 1.0 + al / A, -2.0 * c, 1.0 - al / A);
    }

    void setLowShelf (double sr, double f, double q, double gainDb) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = 2.0 * kPi * safeFreq (sr, f) / sr, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        const double s = 2.0 * std::sqrt (A) * al;
        setRaw (A * ((A + 1.0) - (A - 1.0) * c + s), 2.0 * A * ((A - 1.0) - (A + 1.0) * c), A * ((A + 1.0) - (A - 1.0) * c - s),
                (A + 1.0) + (A - 1.0) * c + s, -2.0 * ((A - 1.0) + (A + 1.0) * c), (A + 1.0) + (A - 1.0) * c - s);
    }

    void setHighShelf (double sr, double f, double q, double gainDb) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = 2.0 * kPi * safeFreq (sr, f) / sr, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        const double s = 2.0 * std::sqrt (A) * al;
        setRaw (A * ((A + 1.0) + (A - 1.0) * c + s), -2.0 * A * ((A - 1.0) + (A + 1.0) * c), A * ((A + 1.0) + (A - 1.0) * c - s),
                (A + 1.0) - (A - 1.0) * c + s, 2.0 * ((A - 1.0) - (A + 1.0) * c), (A + 1.0) - (A - 1.0) * c - s);
    }
};

//==============================================================================
// Lissage exponentiel d'une valeur de paramètre.
struct Smooth
{
    float y = 0.0f, a = 0.0f;

    void setTime (float seconds, double sr) noexcept { a = coefFromTime (seconds, sr); }
    void reset (float v) noexcept { y = v; }
    inline float next (float target) noexcept
    {
        y = target + a * (y - target);
        return y;
    }
};

//==============================================================================
// Ligne à retard circulaire (taille puissance de 2).
// Convention : y = read(D); write(x);  ->  retard exact de D échantillons (D >= 1).
struct DelayLine
{
    std::vector<float> buf;
    int mask = 0;
    int w = 0;

    void prepare (int minSize)
    {
        const int size = nextPow2 (std::max (minSize + 8, 16));
        buf.assign ((size_t) size, 0.0f);
        mask = size - 1;
        w = 0;
    }

    void clear() noexcept { std::fill (buf.begin(), buf.end(), 0.0f); }

    inline void write (float x) noexcept
    {
        buf[(size_t) w] = x;
        w = (w + 1) & mask;
    }

    inline float readInt (int delay) const noexcept { return buf[(size_t) ((w - delay) & mask)]; }

    inline float readFrac (float delay) const noexcept
    {
        const int d = (int) delay;
        const float fr = delay - (float) d;
        const float s0 = buf[(size_t) ((w - d) & mask)];
        const float s1 = buf[(size_t) ((w - d - 1) & mask)];
        return s0 + (s1 - s0) * fr;
    }
};

//==============================================================================
// Mémoire circulaire stéréo (taille puissance de 2) : on écrit d'abord, puis on lit
// "delay" échantillons en arrière. delay = 0 rend l'échantillon qu'on vient d'écrire.
struct StereoRing
{
    std::vector<float> l, r;
    int mask = 0;
    int w = 0;

    void prepare (int minSize)
    {
        const int size = nextPow2 (std::max (minSize + 8, 16));
        l.assign ((size_t) size, 0.0f);
        r.assign ((size_t) size, 0.0f);
        mask = size - 1;
        w = 0;
    }

    int size() const noexcept { return mask + 1; }

    void clear() noexcept
    {
        std::fill (l.begin(), l.end(), 0.0f);
        std::fill (r.begin(), r.end(), 0.0f);
    }

    inline void write (float xl, float xr) noexcept
    {
        w = (w + 1) & mask;
        l[(size_t) w] = xl;
        r[(size_t) w] = xr;
    }

    // Lecture linéaire.
    inline void read (float delay, float& outL, float& outR) const noexcept
    {
        const int d = (int) delay;
        const float fr = delay - (float) d;
        const size_t i0 = (size_t) ((w - d) & mask), i1 = (size_t) ((w - d - 1) & mask);
        outL = l[i0] + (l[i1] - l[i0]) * fr;
        outR = r[i0] + (r[i1] - r[i0]) * fr;
    }

    // Lecture cubique (Hermite) : plus propre quand on lit à une autre vitesse que l'écriture.
    inline void readCubic (double delay, float& outL, float& outR) const noexcept
    {
        const int d = (int) delay;
        const float t = 1.0f - (float) (delay - (double) d); // position entre l'échantillon d+1 (ancien) et d (récent)
        const size_t im1 = (size_t) ((w - d - 2) & mask), i0 = (size_t) ((w - d - 1) & mask),
                     i1 = (size_t) ((w - d) & mask), i2 = (size_t) ((w - d + 1) & mask);
        outL = hermite (l[im1], l[i0], l[i1], l[i2], t);
        outR = hermite (r[im1], r[i0], r[i1], r[i2], t);
    }

    static inline float hermite (float ym1, float y0, float y1, float y2, float t) noexcept
    {
        const float c1 = 0.5f * (y1 - ym1);
        const float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
        const float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
        return ((c3 * t + c2) * t + c1) * t + y0;
    }
};

} // namespace pz
