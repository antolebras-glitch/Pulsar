// PULSAR - l'usure : tout ce qui fait "vieux support" (bande qui pleure, pertes de signal, bruit de fond).
#pragma once

#include "Common.h"

namespace pz
{
//==============================================================================
// Wobble : la hauteur ondule comme sur une bande ou un vinyle fatigué.
// Un pleurage lent, un scintillement rapide et une dérive aléatoire par-dessus.
class Wobble
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        ring.prepare ((int) (0.16 * sr));
        depthSmooth.setTime (0.06f, sr);
        driftSmooth.setTime (0.35f, sr);
        reset();
    }

    void reset() noexcept
    {
        ring.clear();
        depthSmooth.reset (0.0f);
        driftSmooth.reset (0.0f);
        wow = flutter = 0.0f;
        driftTarget = 0.0f;
        driftCount = 0;
    }

    void process (float* l, float* r, int n, float amount, float rateHz) noexcept
    {
        // Écart de hauteur visé : jusqu'à environ +/- 60 cents. L'amplitude du retard qui donne
        // cet écart dépend de la vitesse (plus c'est lent, plus le retard doit bouger loin).
        const float deviation = 0.035f * amount * (0.4f + 0.6f * amount);
        const float flutterHz = 5.5f + 0.9f * rateHz;
        const float depthTarget = std::min (0.045f, deviation / (kTwoPiF * rateHz)) * (float) sr;
        const float flutterDepth = 0.22f * deviation / (kTwoPiF * flutterHz) * (float) sr;
        const float wowStep = kTwoPiF * rateHz / (float) sr;
        const float flutterStep = kTwoPiF * flutterHz / (float) sr;

        for (int i = 0; i < n; ++i)
        {
            ring.write (l[i], r[i]);
            const float depth = depthSmooth.next (depthTarget);

            if (depth < 0.02f)
                continue;

            if (--driftCount <= 0)
            {
                driftCount = (int) (sr / std::max (0.4f, rateHz * 1.7f));
                driftTarget = rng.bipolar();
            }

            const float drift = driftSmooth.next (driftTarget);
            const float scale = depth / std::max (1.0f, depthTarget);
            const float base = 3.0f + depth * 1.45f + flutterDepth * scale;

            wow += wowStep;
            flutter += flutterStep;

            if (wow > kTwoPiF) wow -= kTwoPiF;
            if (flutter > kTwoPiF) flutter -= kTwoPiF;

            const float common = depth * 0.45f * drift + flutterDepth * scale * std::sin (flutter);
            float a, b, unused;
            ring.readCubic ((double) (base + depth * std::sin (wow) + common), a, unused);
            ring.readCubic ((double) (base + depth * std::sin (wow + 0.35f) + common), unused, b);
            l[i] = a;
            r[i] = b;
        }
    }

private:
    double sr = 44100.0;
    StereoRing ring;
    Smooth depthSmooth, driftSmooth;
    Rng rng;
    float wow = 0.0f, flutter = 0.0f, driftTarget = 0.0f;
    int driftCount = 0;
};

//==============================================================================
// Pertes : de temps en temps le son s'affaisse et perd ses aigus, comme une bande abîmée.
class Dropouts
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        down = coefFromTime (0.012f, sr);
        up = coefFromTime (0.05f, sr);
        reset();
    }

    void reset() noexcept
    {
        gain = 1.0f;
        target = 1.0f;
        countdown = 0;
        holding = 0;
        lpL = lpR = 0.0f;
    }

    void process (float* l, float* r, int n, float amount) noexcept
    {
        if (amount <= 0.0005f && gain > 0.9995f)
        {
            target = 1.0f;
            holding = 0;
            return;
        }

        const float lpCoef = (float) std::exp (-2.0 * kPi * 1400.0 / sr);

        for (int i = 0; i < n; ++i)
        {
            if (holding > 0)
            {
                if (--holding == 0)
                    target = 1.0f;
            }
            else if (--countdown <= 0)
            {
                const float perSecond = 0.5f + 5.5f * amount;
                countdown = (int) (-std::log (std::max (1.0e-4f, rng.unipolar())) * (float) sr / perSecond);
                holding = (int) ((0.03f + 0.2f * rng.unipolar() * rng.unipolar()) * (float) sr);
                target = 1.0f - amount * (0.3f + 0.7f * rng.unipolar());
            }

            gain = target + (target < gain ? down : up) * (gain - target);

            // les aigus tombent plus vite que le reste
            lpL = l[i] + lpCoef * (lpL - l[i]);
            lpR = r[i] + lpCoef * (lpR - r[i]);
            l[i] = gain * (lpL + gain * (l[i] - lpL));
            r[i] = gain * (lpR + gain * (r[i] - lpR));
        }
    }

private:
    double sr = 44100.0;
    Rng rng { 0x51f15eedu };
    float gain = 1.0f, target = 1.0f, down = 0.0f, up = 0.0f, lpL = 0.0f, lpR = 0.0f;
    int countdown = 0, holding = 0;
};

//==============================================================================
// Bruit de fond : 0 vinyle (craquements + souffle), 1 bande (souffle), 2 secteur (ronflette électrique).
// "presence" (0..1) éteint le bruit quand plus rien n'entre dans le plugin.
class Noise
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        levelSmooth.setTime (0.05f, sr);
        levelSmooth.reset (0.0f);

        for (int ch = 0; ch < 2; ++ch)
        {
            click[ch].setBandpass (sr, ch == 0 ? 3300.0 : 3900.0, 1.1);
            hissHp[ch].setHighpass (sr, 500.0, 0.707);
            hissLp[ch].setLowpass (sr, 7000.0, 0.707);
            tapeHp[ch].setHighpass (sr, 1800.0, 0.6);
            tapeLp[ch].setLowpass (sr, 11000.0, 0.707);
            buzzHp[ch].setHighpass (sr, 900.0, 0.707);
        }

        rumbleLp.setLowpass (sr, 70.0, 0.707);
        reset();
    }

    void reset() noexcept
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            click[ch].reset();
            hissHp[ch].reset();
            hissLp[ch].reset();
            tapeHp[ch].reset();
            tapeLp[ch].reset();
            buzzHp[ch].reset();
        }

        rumbleLp.reset();
        humPhase = 0.0f;
    }

    void process (float* l, float* r, int n, float amount, int type, float presence) noexcept
    {
        const float target = 2.8f * amount * amount * presence;

        if (target <= 1.0e-5f && levelSmooth.y <= 1.0e-5f)
        {
            levelSmooth.y = 0.0f;
            return;
        }

        const float clicksPerSample = (6.0f + 34.0f * amount) / (float) sr;
        const float humStep = 50.0f / (float) sr;

        for (int i = 0; i < n; ++i)
        {
            const float level = levelSmooth.next (target);
            float nl = 0.0f, nr = 0.0f;

            if (type == 0)
            {
                // craquements : des impulsions rares et inégales, qui font sonner un petit filtre
                float il = 0.0f, ir = 0.0f;

                if (rng.unipolar() < clicksPerSample)
                {
                    const float u = rng.unipolar();
                    const float a = (rng.nextU() & 1u ? 1.0f : -1.0f) * (0.15f + 2.6f * u * u * u * u);
                    const float pan = rng.unipolar();
                    il = a * (1.0f - 0.7f * pan);
                    ir = a * (0.3f + 0.7f * pan);
                }

                const float rumble = rumbleLp.process (rng.bipolar()) * 0.35f;
                nl = click[0].process (il) * 0.3f + hissLp[0].process (hissHp[0].process (rng.bipolar())) * 0.014f + rumble;
                nr = click[1].process (ir) * 0.3f + hissLp[1].process (hissHp[1].process (rng.bipolar())) * 0.014f + rumble;
            }
            else if (type == 1)
            {
                nl = tapeLp[0].process (tapeHp[0].process (rng.bipolar())) * 0.04f;
                nr = tapeLp[1].process (tapeHp[1].process (rng.bipolar())) * 0.04f;
            }
            else
            {
                humPhase += humStep;

                if (humPhase >= 1.0f)
                    humPhase -= 1.0f;

                const float a = kTwoPiF * humPhase;
                const float hum = 0.018f * std::sin (a) + 0.010f * std::sin (2.0f * a) + 0.007f * std::sin (3.0f * a) + 0.003f * std::sin (5.0f * a);
                const float saw = 2.0f * humPhase - 1.0f;
                nl = hum + buzzHp[0].process (saw) * 0.005f + rng.bipolar() * 0.001f;
                nr = hum + buzzHp[1].process (-saw) * 0.005f + rng.bipolar() * 0.001f;
            }

            l[i] += level * nl;
            r[i] += level * nr;
        }
    }

private:
    double sr = 44100.0;
    Rng rng { 0x0badcafeu };
    Smooth levelSmooth;
    Biquad click[2], hissHp[2], hissLp[2], tapeHp[2], tapeLp[2], buzzHp[2], rumbleLp;
    float humPhase = 0.0f;
};

} // namespace pz
