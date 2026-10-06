// PULSAR - tout ce qui dépend du tempo : l'orbite du portail, les effets de temps et la pompe.
#pragma once

#include "Common.h"

namespace pz
{
//==============================================================================
// Orbite : fait bouger le point du portail tout seul, calé sur le tempo.
// Rend un décalage (x, y) entre -1 et +1 autour de la position posée à la main.
struct Orbit
{
    enum Shape { manual = 0, circle, eight, sweepX, sweepY, drift, jumps };

    static double beatsForIndex (int index) noexcept
    {
        static const double table[] = { 32.0, 16.0, 8.0, 4.0, 2.0, 1.0, 0.5, 0.25 };
        return table[std::max (0, std::min (index, 7))];
    }

    static float noise (int n, uint32_t seed) noexcept
    {
        return (float) (hash32 ((uint32_t) n * 2654435761u + seed) >> 8) * (2.0f / 16777216.0f) - 1.0f;
    }

    static void offset (int shape, double phase, float& x, float& y) noexcept
    {
        const double turn = phase - std::floor (phase);
        const float a = kTwoPiF * (float) turn;

        switch (shape)
        {
            case circle: x = std::cos (a);        y = std::sin (a);        break;
            case eight:  x = std::sin (a);        y = std::sin (2.0f * a); break;
            case sweepX: x = -std::cos (a);       y = 0.0f;                break;
            case sweepY: x = 0.0f;                y = -std::cos (a);       break;

            case drift: // glisse en douceur d'un point tiré au hasard au suivant
            {
                const int n = (int) std::floor (phase);
                const float t = (float) turn, s = t * t * (3.0f - 2.0f * t);
                x = lerpf (noise (n, 11u), noise (n + 1, 11u), s);
                y = lerpf (noise (n, 97u), noise (n + 1, 97u), s);
                break;
            }

            case jumps: // saute d'un point tiré au hasard au suivant
            {
                const int n = (int) std::floor (phase);
                x = noise (n, 11u);
                y = noise (n, 97u);
                break;
            }

            default: x = 0.0f; y = 0.0f; break;
        }
    }
};

//==============================================================================
// Effets de temps, calés sur la grille du morceau :
//   0 demi-vitesse : chaque segment est rejoué deux fois plus lentement (une octave plus bas)
//   1 inversé      : le segment précédent est rejoué à l'envers
//   2 répétition   : le premier quart du segment est répété quatre fois
class TimeWarp
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        ring.prepare ((int) (8.4 * sr));
        fadeLen = std::max (32, (int) (0.006 * sr));
        mixSmooth.setTime (0.02f, sr);
        reset();
    }

    void reset() noexcept
    {
        ring.clear();
        mixSmooth.reset (0.0f);
        t = 0;
        segment = 48000;
        headA = headB = {};
        fade = fadeLen;
        lastMode = -1;
        lastSlice = 0;
        wasActive = false;
    }

    static double beatsForIndex (int index) noexcept
    {
        static const double table[] = { 0.25, 0.5, 1.0, 2.0, 4.0, 8.0 };
        return table[std::max (0, std::min (index, 5))];
    }

    void process (float* l, float* r, int n, int mode, int lenIndex, float mixTarget, double bpm, double beat) noexcept
    {
        // Longueur du segment en échantillons (jamais plus de 4 s : au-delà on divise par deux).
        double beats = beatsForIndex (lenIndex);
        const double samplesPerBeat = sr * 60.0 / std::max (30.0, bpm);

        while (beats * samplesPerBeat > 4.0 * sr)
            beats *= 0.5;

        const int newSegment = std::max (64, (int) std::lround (beats * samplesPerBeat));
        const bool active = mixTarget > 0.0005f || mixSmooth.y > 0.0005f;

        if (active)
        {
            // Où devrait-on en être dans le segment d'après la position du morceau ?
            double inSegment = std::fmod (beat, beats);

            if (inSegment < 0.0)
                inSegment += beats;

            const int expected = std::min (newSegment - 1, (int) (inSegment * samplesPerBeat));
            int gap = std::abs (expected - t);
            gap = std::min (gap, newSegment - gap);

            if (! wasActive || mode != lastMode || newSegment != segment || gap > 192)
            {
                segment = newSegment;
                lastMode = mode;
                t = expected;
                startHead (mode, t);
            }
        }

        wasActive = active;

        for (int i = 0; i < n; ++i)
        {
            ring.write (l[i], r[i]);
            const float mix = mixSmooth.next (mixTarget);

            if (! active)
                continue;

            if (t >= segment)
            {
                t = 0;
                startHead (mode, 0);
            }
            else if (mode == 2)
            {
                const int slice = std::max (1, segment / 4);
                const int k = std::min (3, t / slice);

                if (k != lastSlice)
                    startHead (mode, t);
            }

            float wl, wr;
            ring.readCubic (headA.delay + 2.0, wl, wr);

            if (fade < fadeLen)
            {
                float ol, orr;
                ring.readCubic (headB.delay + 2.0, ol, orr);
                const float f = (float) fade / (float) fadeLen;
                const float g = f * f * (3.0f - 2.0f * f);
                wl = ol + g * (wl - ol);
                wr = orr + g * (wr - orr);
                headB.delay = std::min (headB.delay + headB.rate, maxDelay());
                ++fade;
            }

            headA.delay = std::min (headA.delay + headA.rate, maxDelay());
            ++t;

            l[i] += mix * (wl - l[i]);
            r[i] += mix * (wr - r[i]);
        }
    }

private:
    struct Head
    {
        double delay = 0.0, rate = 0.0;
    };

    double maxDelay() const noexcept { return (double) (ring.size() - 16); }

    // Place la tête de lecture comme si le segment avait commencé il y a "at" échantillons.
    void startHead (int mode, int at) noexcept
    {
        headB = headA;
        fade = 0;

        if (mode == 0)
        {
            headA.delay = 0.5 * (double) at;
            headA.rate = 0.5;
        }
        else if (mode == 1)
        {
            headA.delay = 2.0 * (double) at;
            headA.rate = 2.0;
        }
        else
        {
            const int slice = std::max (1, segment / 4);
            lastSlice = std::min (3, at / slice);
            headA.delay = (double) (lastSlice * slice);
            headA.rate = 0.0;
        }
    }

    double sr = 44100.0;
    StereoRing ring;
    Smooth mixSmooth;
    Head headA, headB;
    int t = 0, segment = 48000, fade = 0, fadeLen = 256, lastMode = -1, lastSlice = 0;
    bool wasActive = false;
};

//==============================================================================
// Pompe : le volume plonge à chaque temps puis remonte, comme une sidechain sur un kick.
class Pump
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        smooth.setTime (0.004f, sr);
        smooth.reset (1.0f);
    }

    void process (float* l, float* r, int n, float amount, double bpm, double beat) noexcept
    {
        if (amount <= 0.0005f && smooth.y > 0.9995f)
            return;

        const double beatsPerSample = std::max (30.0, bpm) / (60.0 * sr);

        for (int i = 0; i < n; ++i)
        {
            const double b = beat + (double) i * beatsPerSample;
            const float f = (float) (b - std::floor (b));
            const float rise = clampf (1.0f - f / 0.8f, 0.0f, 1.0f);
            const float g = smooth.next (1.0f - amount * rise * rise);
            l[i] *= g;
            r[i] *= g;
        }
    }

private:
    double sr = 44100.0;
    Smooth smooth;
};

} // namespace pz
