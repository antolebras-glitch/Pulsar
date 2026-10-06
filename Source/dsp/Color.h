// PULSAR - la couleur : saturation, écrasement numérique, filtre et modulation.
#pragma once

#include "Common.h"

namespace pz
{
//==============================================================================
// Saturation, calculée à deux fois la fréquence d'échantillonnage pour éviter le repliement.
// 0 lampe, 1 bande, 2 clip, 3 radio (bande étroite et sale).
class Drive
{
public:
    static constexpr int taps = 63;

    void prepare (double sampleRate)
    {
        sr = sampleRate;

        // Filtre demi-bande (sinc fenêtré Blackman-Harris).
        const int centre = (taps - 1) / 2;
        double sum = 0.0;

        for (int i = 0; i < taps; ++i)
        {
            const int k = i - centre;
            const double sinc = k == 0 ? 0.5 : std::sin (0.5 * kPi * (double) k) / (kPi * (double) k);
            const double ph = 2.0 * kPi * (double) i / (double) (taps - 1);
            const double win = 0.35875 - 0.48829 * std::cos (ph) + 0.14128 * std::cos (2.0 * ph) - 0.01168 * std::cos (3.0 * ph);
            h[(size_t) i] = sinc * win;
            sum += h[(size_t) i];
        }

        numActive = 0;

        for (int i = 0; i < taps; ++i)
        {
            const float c = (float) (h[(size_t) i] / sum);

            if (std::abs (c) > 1.0e-9f) // un filtre demi-bande a un coefficient sur deux à zéro : on les saute
            {
                activeCoef[(size_t) numActive] = c;
                activeIndex[(size_t) numActive] = i + 1;
                ++numActive;
            }
        }

        megaHp.setHighpass (sr * 2.0, 480.0, 0.707);
        megaLp.setLowpass (sr * 2.0, 3600.0, 0.707);
        megaPost.setLowpass (sr * 2.0, 4800.0, 0.707);
        dcBlock.setHighpass (sr, 18.0, 0.707);
        amountSmooth.setTime (0.03f, sr);
        reset();
    }

    void reset() noexcept
    {
        upBuf.fill (0.0f);
        downBuf.fill (0.0f);
        upPos = downPos = 0;
        megaHp.reset();
        megaLp.reset();
        megaPost.reset();
        dcBlock.reset();
        tapeState = 0.0f;
        amountSmooth.reset (0.0f);
        idleDelay.fill (0.0f);
        idlePos = 0;
        fade = 0.0f;
    }

    static int getLatency() noexcept { return (taps - 1) / 2; }

    void process (float* x, int n, int type, float amountTarget) noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            float amount = amountSmooth.next (amountTarget);

            if (amount < 1.0e-4f)
                amount = 0.0f;

            // Au repos : simple retard (même latence), sans calculer les filtres.
            const float straight = idleDelay[(size_t) ((idlePos - getLatency()) & 63)];
            idleDelay[(size_t) idlePos] = x[i];
            idlePos = (idlePos + 1) & 63;

            if (amount <= 0.0f && fade <= 0.0f)
            {
                x[i] = straight;
                continue;
            }

            const float blend = std::min (1.0f, amount * 4.0f);
            updateShaper (type, amount);

            // Montée x2 : on insère un zéro entre chaque échantillon puis on filtre.
            pushUp (2.0f * x[i]);
            const float u0 = shape (firUp(), type, amount, blend);
            pushDown (u0);
            const float out = dcBlock.process (firDown());

            pushUp (0.0f);
            const float u1 = shape (firUp(), type, amount, blend);
            pushDown (u1);

            // Fondu entre le chemin "repos" et le chemin suréchantillonné (évite tout clic à l'activation).
            fade = clampf (fade + (amount > 0.0f ? 1.0f : -1.0f) / 256.0f, 0.0f, 1.0f);
            x[i] = straight + fade * (out - straight);
        }
    }

private:
    inline void pushUp (float v) noexcept
    {
        upBuf[(size_t) upPos] = v;
        upPos = (upPos + 1) & 63;
    }

    inline void pushDown (float v) noexcept
    {
        downBuf[(size_t) downPos] = v;
        downPos = (downPos + 1) & 63;
    }

    inline float firUp() const noexcept
    {
        float acc = 0.0f;

        for (int k = 0; k < numActive; ++k)
            acc += activeCoef[(size_t) k] * upBuf[(size_t) ((upPos - activeIndex[(size_t) k]) & 63)];

        return acc;
    }

    inline float firDown() const noexcept
    {
        float acc = 0.0f;

        for (int k = 0; k < numActive; ++k)
            acc += activeCoef[(size_t) k] * downBuf[(size_t) ((downPos - activeIndex[(size_t) k]) & 63)];

        return acc;
    }

    void updateShaper (int type, float amount) noexcept
    {
        if (type == shaperType && std::abs (amount - shaperAmount) < 1.0e-4f)
            return;

        shaperType = type;
        shaperAmount = amount;

        switch (type)
        {
            case 1:  shaperK = 1.0f + 5.0f * amount;  shaperNorm = 1.0f / std::pow (shaperK, 0.75f); break;
            case 2:  shaperK = 1.0f + 11.0f * amount; shaperNorm = 1.0f / std::pow (shaperK, 0.9f);  break;
            case 3:  shaperK = 4.0f + 26.0f * amount; shaperNorm = 0.35f / std::pow (shaperK * 0.25f, 0.6f); break;
            default: shaperK = 1.0f + 7.0f * amount;  shaperNorm = 1.0f / std::pow (shaperK, 0.6f);
                     shaperOffset = std::tanh (shaperK * 0.12f);                                    break;
        }
    }

    inline float shape (float x, int type, float amount, float blend) noexcept
    {
        if (blend <= 0.0f)
            return x;

        const float k = shaperK;
        float y;

        switch (type)
        {
            case 1: // bande : compression douce + aigus arrondis
            {
                const float s = 0.6366198f * std::atan (1.5707963f * k * x) * shaperNorm;
                tapeState += lerpf (0.75f, 0.22f, amount) * (s - tapeState);
                y = tapeState;
                break;
            }

            case 2: // clip : agressif, façon rage / hyperpop
            {
                const float z = clampf (k * x, -1.5f, 1.5f);
                y = (z - 0.1481481f * z * z * z) * shaperNorm;
                break;
            }

            case 3: // mégaphone : bande étroite + grosse distorsion asymétrique
            {
                const float z = k * megaLp.process (megaHp.process (x));
                const float s = z > 0.0f ? std::tanh (z) : 0.7f * std::tanh (z * 1.4f);
                y = megaPost.process (s) * shaperNorm;
                break;
            }

            default: // lampe : chaleur, harmoniques paires
            {
                y = (std::tanh (k * (x + 0.12f)) - shaperOffset) * shaperNorm;
                break;
            }
        }

        return x + blend * (y - x);
    }

    double sr = 44100.0;
    std::array<double, taps> h {};
    std::array<float, taps> activeCoef {};
    std::array<int, taps> activeIndex {};
    int numActive = 0, shaperType = -1;
    float shaperAmount = -1.0f, shaperK = 1.0f, shaperNorm = 1.0f, shaperOffset = 0.0f;
    std::array<float, 64> upBuf {}, downBuf {}, idleDelay {};
    int upPos = 0, downPos = 0, idlePos = 0;
    float fade = 0.0f;
    Biquad megaHp, megaLp, megaPost, dcBlock;
    float tapeState = 0.0f;
    Smooth amountSmooth;
};

//==============================================================================
// Bitcrush : réduit la fréquence d'échantillonnage et la résolution (son "console 8 bits").
class Crusher
{
public:
    void reset() noexcept
    {
        phase = 0.0f;
        held = 0.0f;
    }

    void process (float* x, int n, float amount, double sampleRate) noexcept
    {
        if (amount <= 0.001f)
            return;

        const float blend = std::min (1.0f, amount * 3.0f);
        const float targetRate = lerpf (16000.0f, 2200.0f, amount * amount);
        const float step = std::min (1.0f, targetRate / (float) sampleRate);
        const float levels = std::pow (2.0f, lerpf (12.0f, 4.5f, amount));

        for (int i = 0; i < n; ++i)
        {
            phase += step;

            if (phase >= 1.0f)
            {
                phase -= 1.0f;
                held = std::round (x[i] * levels) / levels;
            }

            x[i] += blend * (held - x[i]);
        }
    }

private:
    float phase = 0.0f, held = 0.0f;
};


//==============================================================================
// Filtre 24 dB par octave (deux étages à variable d'état). 0 coupé, 1 passe-bas, 2 passe-bande, 3 passe-haut.
// La coupure glisse en douceur : c'est le réglage le plus agréable à piloter avec le portail.
class MorphFilter
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        reset();
    }

    void reset() noexcept
    {
        for (auto& s : state)
            s = {};

        g = k1 = k2 = -1.0f;
        lastType = 0;
    }

    void process (float* l, float* r, int n, int type, float cutoffHz, float resonance) noexcept
    {
        if (type <= 0)
        {
            lastType = 0;
            return;
        }

        if (lastType != type)
        {
            for (auto& s : state)
                s = {};

            g = -1.0f;
            lastType = type;
        }

        const float q = 0.6f * std::pow (20.0f, clampf (resonance, 0.0f, 1.0f));
        const float gTarget = (float) std::tan (kPi * clampd ((double) cutoffHz, 20.0, sr * 0.45) / sr);
        const float k1Target = type == 2 ? 1.0f / std::max (0.5f, q * 0.7f) : 1.4142f;
        const float k2Target = type == 2 ? k1Target : 1.0f / q;
        const float trim = 1.0f / (1.0f + 0.07f * std::max (0.0f, q - 0.7f));

        if (g < 0.0f)
        {
            g = gTarget;
            k1 = k1Target;
            k2 = k2Target;
        }

        const float inv = 1.0f / (float) n;
        const float gStep = (gTarget - g) * inv, k1Step = (k1Target - k1) * inv, k2Step = (k2Target - k2) * inv;

        for (int i = 0; i < n; ++i)
        {
            g += gStep;
            k1 += k1Step;
            k2 += k2Step;

            const float a1 = 1.0f / (1.0f + g * (g + k1)), b1 = 1.0f / (1.0f + g * (g + k2));
            l[i] = trim * stage (state[1], stage (state[0], l[i], type, g, k1, a1), type, g, k2, b1);
            r[i] = trim * stage (state[3], stage (state[2], r[i], type, g, k1, a1), type, g, k2, b1);
        }
    }

private:
    struct State
    {
        float ic1 = 0.0f, ic2 = 0.0f;
    };

    static inline float stage (State& s, float x, int type, float gg, float k, float a1) noexcept
    {
        const float a2 = gg * a1, a3 = gg * a2;
        const float v3 = x - s.ic2;
        const float v1 = a1 * s.ic1 + a2 * v3;
        const float v2 = s.ic2 + a2 * s.ic1 + a3 * v3;
        s.ic1 = 2.0f * v1 - s.ic1;
        s.ic2 = 2.0f * v2 - s.ic2;

        if (type == 1) return v2;
        if (type == 2) return k * v1;
        return x - k * v1 - v2;
    }

    double sr = 44100.0;
    std::array<State, 4> state;
    float g = -1.0f, k1 = 1.4142f, k2 = 1.0f;
    int lastType = 0;
};

//==============================================================================
// Modulation : 0 chorus (épaissit), 1 flanger (balayage métallique), 2 phaser (creux qui se déplacent).
class Modulator
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        ring.prepare ((int) (0.05 * sr));
        amountSmooth.setTime (0.03f, sr);
        reset();
    }

    void reset() noexcept
    {
        ring.clear();
        amountSmooth.reset (0.0f);
        phase = 0.0f;
        fbL = fbR = 0.0f;

        for (auto& a : apL) a = 0.0f;
        for (auto& a : apR) a = 0.0f;
    }

    void process (float* l, float* r, int n, int type, float amountTarget, float rateHz) noexcept
    {
        if (amountTarget <= 0.0005f && amountSmooth.y <= 0.0005f)
        {
            if (type != 2)
                for (int i = 0; i < n; ++i)
                    ring.write (l[i], r[i]);

            fbL = fbR = 0.0f;
            return;
        }

        const float step = rateHz / (float) sr;
        const float ms = 0.001f * (float) sr;

        for (int i = 0; i < n; ++i)
        {
            const float amount = amountSmooth.next (amountTarget);
            phase += step;

            if (phase >= 1.0f)
                phase -= 1.0f;

            const float a = kTwoPiF * phase;
            const float lfoL = std::sin (a), lfoR = std::cos (a);

            if (type == 0)
            {
                ring.write (l[i], r[i]);
                const float lfo2L = std::sin (1.37f * a + 1.0f), lfo2R = std::cos (1.37f * a + 1.0f);
                float a1, a2, b1, b2, unused;
                ring.readCubic ((double) ((13.0f + 5.0f * lfoL) * ms), a1, unused);
                ring.readCubic ((double) ((19.0f + 6.0f * lfo2R) * ms), a2, unused);
                ring.readCubic ((double) ((13.0f + 5.0f * lfoR) * ms), unused, b1);
                ring.readCubic ((double) ((19.0f + 6.0f * lfo2L) * ms), unused, b2);
                l[i] = l[i] * (1.0f - 0.3f * amount) + amount * 0.6f * (a1 + a2);
                r[i] = r[i] * (1.0f - 0.3f * amount) + amount * 0.6f * (b1 + b2);
            }
            else if (type == 1)
            {
                ring.write (l[i] + 0.62f * fbL, r[i] + 0.62f * fbR);
                float yl, yr, unused;
                ring.readCubic ((double) ((0.5f + 2.3f * (1.0f + lfoL)) * ms) + 2.0, yl, unused);
                ring.readCubic ((double) ((0.5f + 2.3f * (1.0f + lfoR)) * ms) + 2.0, unused, yr);
                fbL = yl;
                fbR = yr;
                l[i] = l[i] * (1.0f - 0.25f * amount) + amount * 0.75f * yl;
                r[i] = r[i] * (1.0f - 0.25f * amount) + amount * 0.75f * yr;
            }
            else
            {
                const float wl = kPiF * 260.0f * std::pow (2.0f, 2.0f * (1.0f + lfoL)) / (float) sr;
                const float wr = kPiF * 260.0f * std::pow (2.0f, 2.0f * (1.0f + lfoR)) / (float) sr;
                const float tl = wl + wl * wl * wl * 0.3333f, tr = wr + wr * wr * wr * 0.3333f;
                const float cl = (tl - 1.0f) / (tl + 1.0f), cr = (tr - 1.0f) / (tr + 1.0f);

                float yl = l[i] + 0.55f * fbL, yr = r[i] + 0.55f * fbR;

                for (int s = 0; s < 6; ++s)
                {
                    const float ol = cl * yl + apL[(size_t) s];
                    apL[(size_t) s] = yl - cl * ol;
                    yl = ol;
                    const float orr = cr * yr + apR[(size_t) s];
                    apR[(size_t) s] = yr - cr * orr;
                    yr = orr;
                }

                fbL = yl;
                fbR = yr;
                l[i] += amount * 0.5f * (yl - l[i]);
                r[i] += amount * 0.5f * (yr - r[i]);
            }
        }
    }

private:
    double sr = 44100.0;
    StereoRing ring;
    Smooth amountSmooth;
    float phase = 0.0f, fbL = 0.0f, fbR = 0.0f;
    std::array<float, 6> apL {}, apR {};
};

} // namespace pz
