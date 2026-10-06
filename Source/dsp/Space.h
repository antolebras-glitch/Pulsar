// PULSAR - l'espace : écho ping-pong calé sur le tempo et grande réverbe.
#pragma once

#include "Common.h"

namespace pz
{
//==============================================================================
// Écho ping-pong : les répétitions rebondissent de gauche à droite et s'assombrissent à chaque tour.
class PingPongDelay
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        left.prepare ((int) (2.1 * sr));
        right.prepare ((int) (2.1 * sr));
        hpL.setHighpass (sr, 180.0, 0.707);
        hpR.setHighpass (sr, 180.0, 0.707);
        lpL.setLowpass (sr, 5600.0, 0.707);
        lpR.setLowpass (sr, 5600.0, 0.707);
        timeSmooth.setTime (0.08f, sr);
        timeSmooth.reset ((float) (0.25 * sr));
    }

    void reset() noexcept
    {
        left.clear();
        right.clear();
        hpL.reset();
        hpR.reset();
        lpL.reset();
        lpR.reset();
    }

    // index : 0 = slap 95 ms, puis valeurs de notes.
    static double beatsForIndex (int index) noexcept
    {
        static const double table[] = { 0.0, 0.25, 1.0 / 3.0, 0.5, 0.75, 2.0 / 3.0, 1.0, 1.5, 2.0 };
        return table[std::max (0, std::min (index, 8))];
    }

    void setTime (int index, double bpm) noexcept
    {
        const double beats = beatsForIndex (index);
        const double seconds = beats <= 0.0 ? 0.095 : beats * 60.0 / std::max (40.0, bpm);
        targetSamples = (float) clampd (seconds * sr, 32.0, 2.0 * sr);
    }

    inline void process (float x, float feedback, float& outL, float& outR) noexcept
    {
        const float d = timeSmooth.next (targetSamples);
        const float yl = left.readFrac (d);
        const float yr = right.readFrac (d);
        left.write (x + feedback * lpR.process (hpR.process (yr)));
        right.write (feedback * lpL.process (hpL.process (yl)) + 0.0f);
        outL = yl;
        outR = yr;
    }

private:
    double sr = 44100.0;
    DelayLine left, right;
    Biquad hpL, hpR, lpL, lpR;
    Smooth timeSmooth;
    float targetSamples = 11025.0f;
};

//==============================================================================

//==============================================================================
// Réverbe : 4 diffuseurs passe-tout + réseau de 8 lignes à retard bouclées (FDN, matrice de Householder).
// La taille va d'une petite pièce (0,4 s) à un espace qui ne retombe presque plus (18 s).
class Reverb
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        const double scale = sr / 44100.0;
        static const int apLen[4] = { 211, 158, 561, 410 };
        static const int lineLen[8] = { 1309, 1637, 1811, 1931, 2351, 2647, 2983, 3229 };

        for (int i = 0; i < 4; ++i)
        {
            apLength[(size_t) i] = std::max (2, (int) std::lround (apLen[i] * scale));
            ap[(size_t) i].prepare (apLength[(size_t) i] + 4);
        }

        for (int i = 0; i < 8; ++i)
        {
            length[(size_t) i] = std::max (8, (int) std::lround (lineLen[i] * scale));
            lines[(size_t) i].prepare (length[(size_t) i] + 64);
            damp[(size_t) i] = 0.0f;
        }

        preDelaySamples = (int) (0.022 * sr);
        preDelay.prepare (preDelaySamples + 4);
        inputHp.setHighpass (sr, 130.0, 0.707);
        inputLp.setLowpass (sr, 9500.0, 0.707);
        currentSize = -1.0f;
        setSize (0.4f);
        lfoPhase = 0.0f;
    }

    void reset() noexcept
    {
        for (auto& a : ap)
            a.clear();

        for (auto& l : lines)
            l.clear();

        preDelay.clear();
        inputHp.reset();
        inputLp.reset();
        damp.fill (0.0f);
    }

    void setSize (float size) noexcept
    {
        if (std::abs (size - currentSize) < 0.001f)
            return;

        currentSize = size;
        const double rt60 = 0.4 * std::pow (45.0, (double) size); // 0,4 s -> 18 s

        for (int i = 0; i < 8; ++i)
            feedback[(size_t) i] = (float) std::pow (10.0, -3.0 * (double) length[(size_t) i] / (rt60 * sr));

        const double cutoff = 9000.0 - 4500.0 * (double) size;
        dampCoef = (float) std::exp (-2.0 * kPi * cutoff / sr);
        outputGain = 0.3f * (float) std::pow (1.8 / rt60, 0.3); // une grande réverbe accumule plus d'énergie : on compense
    }

    inline void process (float x, float& outL, float& outR) noexcept
    {
        float in = preDelay.readInt (preDelaySamples);
        preDelay.write (x);
        in = inputLp.process (inputHp.process (in));

        static const float apGain[4] = { 0.72f, 0.72f, 0.62f, 0.62f };

        for (int i = 0; i < 4; ++i)
        {
            const float delayed = ap[(size_t) i].readInt (apLength[(size_t) i]);
            const float v = in + apGain[i] * delayed;
            ap[(size_t) i].write (v);
            in = delayed - apGain[i] * v;
        }

        lfoPhase += kTwoPiF * 0.43f / (float) sr;

        if (lfoPhase > kTwoPiF)
            lfoPhase -= kTwoPiF;

        const float mod = 5.0f * std::sin (lfoPhase);
        float d[8];
        float sum = 0.0f;

        for (int i = 0; i < 8; ++i)
        {
            float tap;

            if (i == 0)
                tap = lines[0].readFrac ((float) length[0] + mod);
            else if (i == 5)
                tap = lines[5].readFrac ((float) length[5] - mod);
            else
                tap = lines[(size_t) i].readInt (length[(size_t) i]);

            d[i] = tap;
            damp[(size_t) i] = tap + dampCoef * (damp[(size_t) i] - tap);
            const float s = damp[(size_t) i] * feedback[(size_t) i];
            fb[(size_t) i] = s;
            sum += s;
        }

        sum *= 0.25f;

        for (int i = 0; i < 8; ++i)
            lines[(size_t) i].write (((i & 1) ? -in : in) + fb[(size_t) i] - sum + 1.0e-20f);

        outL = 1.08f * outputGain * (d[0] - d[2] + d[4] - d[6]);
        outR = 0.93f * outputGain * (d[1] - d[3] + d[5] - d[7]);
    }

private:
    double sr = 44100.0;
    std::array<DelayLine, 4> ap;
    std::array<DelayLine, 8> lines;
    std::array<int, 4> apLength {};
    std::array<int, 8> length {};
    std::array<float, 8> feedback {}, damp {}, fb {};
    DelayLine preDelay;
    Biquad inputHp, inputLp;
    int preDelaySamples = 970;
    float dampCoef = 0.5f, currentSize = -1.0f, lfoPhase = 0.0f, outputGain = 0.3f;
};

} // namespace pz
