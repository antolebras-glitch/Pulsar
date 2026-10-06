// PULSAR - la finition : ce qui rend le résultat "fini" (dense, brillant, large) avant de le ressortir.
#pragma once

#include "Common.h"

namespace pz
{
//==============================================================================
// Colle : compression sur trois bandes (graves / médiums / aigus) qui agit dans les deux sens.
// Ce qui dépasse est tassé, ce qui est faible est remonté : le son devient dense et détaillé.
class Glue
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;

        for (auto& c : chan)
        {
            c.lowLp.set (sr, 130.0, false);
            c.lowHp.set (sr, 130.0, true);
            c.highLp.set (sr, 2600.0, false);
            c.highHp.set (sr, 2600.0, true);
            c.alignLp.set (sr, 2600.0, false);
            c.alignHp.set (sr, 2600.0, true);
        }

        static const float attack[3] = { 0.005f, 0.001f, 0.0003f };
        static const float release[3] = { 0.220f, 0.130f, 0.080f };

        for (int b = 0; b < 3; ++b)
        {
            att[b] = coefFromTime (attack[b], sr);
            rel[b] = coefFromTime (release[b], sr);
        }

        amountSmooth.setTime (0.04f, sr);
        reset();
    }

    void reset() noexcept
    {
        for (auto& c : chan)
            c.reset();

        env[0] = env[1] = env[2] = 0.0f;
        gainDb[0] = gainDb[1] = gainDb[2] = 0.0f;
        amountSmooth.reset (0.0f);
        idle = true;
        reduction = 0.0f;
    }

    float reduction = 0.0f; // plus forte réduction de gain du dernier bloc, en dB (pour l'affichage)

    void process (float* l, float* r, int n, float amountTarget) noexcept
    {
        if (amountTarget <= 0.0005f && amountSmooth.y <= 0.0005f)
        {
            if (! idle)
            {
                for (auto& c : chan)
                    c.reset();

                env[0] = env[1] = env[2] = 0.0f;
                idle = true;
            }

            reduction = 0.0f;
            return;
        }

        idle = false;
        float worst = 0.0f;

        static const float threshold[3] = { -20.0f, -24.0f, -32.0f };
        static const float maxUp[3] = { 5.0f, 10.0f, 13.0f };

        for (int i = 0; i < n; ++i)
        {
            const float amount = amountSmooth.next (amountTarget);
            float bl[3], br[3];
            chan[0].split (l[i], bl);
            chan[1].split (r[i], br);

            float outL = 0.0f, outR = 0.0f;

            for (int b = 0; b < 3; ++b)
            {
                const float peak = std::max (std::abs (bl[b]), std::abs (br[b]));
                env[b] = peak + (peak > env[b] ? att[b] : rel[b]) * (env[b] - peak);

                // le calcul en dB coûte cher : on le refait un échantillon sur quatre
                if ((i & 3) == 0)
                {
                    const float level = gainToDb (env[b]);
                    float gdb;

                    if (level > threshold[b])
                    {
                        gdb = -0.6f * (level - threshold[b]);
                    }
                    else
                    {
                        gdb = std::min (maxUp[b], 0.5f * (threshold[b] - level));

                        if (level < -52.0f) // ne pas remonter le silence ni le souffle
                            gdb *= clampf ((level + 66.0f) / 14.0f, 0.0f, 1.0f);
                    }

                    gainDb[b] = gdb;
                    worst = std::min (worst, gdb * amount);
                }

                const float g = dbToGainFast (gainDb[b] * amount);
                outL += bl[b] * g;
                outR += br[b] * g;
            }

            const float makeup = 1.0f + 1.0f * amount;
            l[i] = outL * makeup;
            r[i] = outR * makeup;
        }

        reduction = -worst;
    }

private:
    // Filtre de Linkwitz-Riley du 4e ordre = deux Butterworth du 2e ordre à la suite.
    struct LR4
    {
        Biquad a, b;

        void set (double sampleRate, double f, bool highpass) noexcept
        {
            if (highpass)
            {
                a.setHighpass (sampleRate, f, 0.7071);
                b.setHighpass (sampleRate, f, 0.7071);
            }
            else
            {
                a.setLowpass (sampleRate, f, 0.7071);
                b.setLowpass (sampleRate, f, 0.7071);
            }
        }

        void reset() noexcept
        {
            a.reset();
            b.reset();
        }

        inline float process (float x) noexcept { return b.process (a.process (x)); }
    };

    struct Channel
    {
        LR4 lowLp, lowHp, highLp, highHp, alignLp, alignHp;

        void reset() noexcept
        {
            lowLp.reset();
            lowHp.reset();
            highLp.reset();
            highHp.reset();
            alignLp.reset();
            alignHp.reset();
        }

        // bandes : 0 graves, 1 médiums, 2 aigus. Leur somme redonne le signal (à la phase près).
        inline void split (float x, float* bands) noexcept
        {
            const float low = lowLp.process (x);
            const float rest = lowHp.process (x);
            bands[0] = alignLp.process (low) + alignHp.process (low);
            bands[1] = highLp.process (rest);
            bands[2] = highHp.process (rest);
        }
    };

    // 10^(dB/20) sans appeler pow : exp(x) approché par (1 + x/64)^64, largement assez précis ici.
    static inline float dbToGainFast (float db) noexcept
    {
        float y = 1.0f + db * (0.11512925f / 64.0f);
        y *= y; y *= y; y *= y; y *= y; y *= y; y *= y;
        return y;
    }

    double sr = 44100.0;
    Channel chan[2];
    float env[3] {}, gainDb[3] {}, att[3] {}, rel[3] {};
    Smooth amountSmooth;
    bool idle = true;
};

//==============================================================================
// Ton : "Poids" pousse ou allège les graves, "Air" ouvre ou ferme le haut du spectre.
class Tone
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        lastWeight = lastAir = 1000.0f;

        for (int ch = 0; ch < 2; ++ch)
        {
            low[ch].reset();
            high[ch].reset();
        }
    }

    void process (float* l, float* r, int n, float weightDb, float airDb) noexcept
    {
        if (std::abs (weightDb - lastWeight) > 0.02f)
        {
            lastWeight = weightDb;
            low[0].setLowShelf (sr, 110.0, 0.707, (double) weightDb);
            low[1] = copyCoefs (low[1], low[0]);
        }

        if (std::abs (airDb - lastAir) > 0.02f)
        {
            lastAir = airDb;
            high[0].setHighShelf (sr, 8500.0, 0.707, (double) airDb);
            high[1] = copyCoefs (high[1], high[0]);
        }

        if (std::abs (weightDb) > 0.03f)
        {
            for (int i = 0; i < n; ++i)
            {
                l[i] = low[0].process (l[i]);
                r[i] = low[1].process (r[i]);
            }
        }

        if (std::abs (airDb) > 0.03f)
        {
            for (int i = 0; i < n; ++i)
            {
                l[i] = high[0].process (l[i]);
                r[i] = high[1].process (r[i]);
            }
        }
    }

private:
    static Biquad copyCoefs (Biquad target, const Biquad& source) noexcept
    {
        target.b0 = source.b0; target.b1 = source.b1; target.b2 = source.b2;
        target.a1 = source.a1; target.a2 = source.a2;
        return target;
    }

    double sr = 44100.0;
    Biquad low[2], high[2];
    float lastWeight = 1000.0f, lastAir = 1000.0f;
};

//==============================================================================
// Largeur : 0 % = mono, 100 % = inchangé, 200 % = très large. Les graves restent au centre.
class Width
{
public:
    void prepare (double sampleRate)
    {
        sideHp.setHighpass (sampleRate, 160.0, 0.707);
        sideHp.reset();
        smooth.setTime (0.03f, sampleRate);
        smooth.reset (1.0f);
    }

    void process (float* l, float* r, int n, float widthTarget) noexcept
    {
        if (std::abs (widthTarget - 1.0f) < 0.002f && std::abs (smooth.y - 1.0f) < 0.002f)
            return;

        for (int i = 0; i < n; ++i)
        {
            const float w = smooth.next (widthTarget);
            const float mid = 0.5f * (l[i] + r[i]);
            float side = 0.5f * (l[i] - r[i]);
            const float highSide = sideHp.process (side);
            side = w <= 1.0f ? side * w : side + (w - 1.0f) * highSide;
            l[i] = mid + side;
            r[i] = mid - side;
        }
    }

private:
    Biquad sideHp;
    Smooth smooth;
};

//==============================================================================
// Limiteur de sécurité : empêche la sortie de dépasser 0 dB, quoi qu'on fasse avec les potards.
class Limiter
{
public:
    void prepare (double sampleRate)
    {
        release = coefFromTime (0.08f, sampleRate);
        env = 0.0f;
    }

    void process (float* l, float* r, int n) noexcept
    {
        const float ceiling = 0.966f;

        for (int i = 0; i < n; ++i)
        {
            const float p = std::max (std::abs (l[i]), std::abs (r[i]));
            env = p > env ? p : env * release;

            if (env > ceiling)
            {
                const float g = ceiling / env;
                l[i] *= g;
                r[i] *= g;
            }
        }
    }

private:
    float env = 0.0f, release = 0.0f;
};

} // namespace pz
