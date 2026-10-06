// PULSAR - le moteur : lit les potards, applique le portail (pad X/Y) et son orbite,
// puis fait passer le son dans toute la chaîne d'effets. Aucune dépendance à JUCE.
//
// Chaîne : entrée -> temps -> pompe -> grain -> wobble -> pertes -> bruit -> drive -> crush
//          -> filtre -> modulation -> écho -> espace -> colle -> ton -> largeur -> mix -> volume -> limiteur
#pragma once

#include <atomic>

#include "Color.h"
#include "Common.h"
#include "Finish.h"
#include "Grain.h"
#include "Space.h"
#include "Table.h"
#include "Time.h"
#include "Wear.h"

namespace pz
{
class Engine
{
public:
    static constexpr int chunk = 32; // les réglages sont recalculés tous les 32 échantillons

    Engine()
    {
        for (auto& v : live)
            v.store (0.0f);
    }

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        timeWarp.prepare (sr);
        pump.prepare (sr);
        grain.prepare (sr);
        wobble.prepare (sr);
        dropouts.prepare (sr);
        noise.prepare (sr);

        for (int ch = 0; ch < 2; ++ch)
        {
            drive[ch].prepare (sr);
            crusher[ch].reset();
            dryDelay[ch].prepare (Drive::getLatency() + 8);
        }

        filter.prepare (sr);
        modulator.prepare (sr);
        echo.prepare (sr);
        reverb.prepare (sr);
        glue.prepare (sr);
        tone.prepare (sr);
        width.prepare (sr);
        limiter.prepare (sr);

        inSmooth.setTime (0.02f, sr);
        outSmooth.setTime (0.02f, sr);
        mixSmooth.setTime (0.02f, sr);
        echoSmooth.setTime (0.03f, sr);
        reverbSmooth.setTime (0.03f, sr);
        inSmooth.reset (1.0f);
        outSmooth.reset (1.0f);
        mixSmooth.reset (1.0f);
        echoSmooth.reset (0.0f);
        reverbSmooth.reset (0.0f);

        puckCoef = coefFromTime (0.018f, sr / (double) chunk);
        presenceCoef = coefFromTime (0.35f, sr / (double) chunk);
        puckStarted = false;
        presence = 0.0f;
        presenceHold = 0;
        freeBeat = 0.0;
        echoIdle = reverbIdle = true;
    }

    static int getLatency() noexcept { return Drive::getLatency(); }

    // Ce que l'interface vient lire (écrit par le thread audio).
    std::array<std::atomic<float>, numParams> live;          // position réelle de chaque potard, portail compris
    std::atomic<float> puckX { 0.0f }, puckY { 0.0f };       // position réelle du point du portail
    std::atomic<float> meterIn { 0.0f }, meterOut { 0.0f }, meterGlue { 0.0f };
    std::atomic<int> grainCount { 0 };

    void process (float* l, float* r, int numSamples, const Controls& c, double bpm, double ppq, bool playing) noexcept
    {
        using namespace P;
        const auto& table = specs();
        bpm = clampd (bpm, 30.0, 400.0);
        const double beatsPerSample = bpm / (60.0 * sr);

        // Position dans le morceau : celle de l'hôte quand il joue, sinon un compteur interne qui continue de tourner.
        if (playing && ppq > -1.0e8)
            freeBeat = ppq;

        float peakIn = 0.0f, peakOut = 0.0f, worstGlue = 0.0f;

        for (int start = 0; start < numSamples; start += chunk)
        {
            const int n = std::min (chunk, numSamples - start);
            float* cl = l + start;
            float* cr = r + start;
            const double beat = freeBeat;
            freeBeat += (double) n * beatsPerSample;

            //------------------------------------------------------------ portail + orbite
            {
                const int shape = (int) std::lround (table[motionShape].fromNorm (c.norm[motionShape]));
                const int rate = (int) std::lround (table[motionRate].fromNorm (c.norm[motionRate]));
                float ox = 0.0f, oy = 0.0f;
                Orbit::offset (shape, beat / Orbit::beatsForIndex (rate), ox, oy);
                const float radius = 0.5f * c.norm[motionSize];
                const float tx = clampf (c.norm[padX] + radius * ox, 0.0f, 1.0f);
                const float ty = clampf (c.norm[padY] + radius * oy, 0.0f, 1.0f);

                if (! puckStarted)
                {
                    px = tx;
                    py = ty;
                    puckStarted = true;
                }

                px = tx + puckCoef * (px - tx);
                py = ty + puckCoef * (py - ty);
            }

            //------------------------------------------------------------ valeur réelle de chaque réglage
            float v[numParams];

            for (int p = 0; p < numParams; ++p)
            {
                float position = c.norm[(size_t) p];

                if (table[(size_t) p].mod)
                    position = clampf (position + c.depthX[(size_t) p] * px + c.depthY[(size_t) p] * py, 0.0f, 1.0f);

                v[p] = table[(size_t) p].fromNorm (position);
                live[(size_t) p].store (position, std::memory_order_relaxed);
            }

            const auto pc = [&v] (int p) { return v[p] * 0.01f; };
            const auto pick = [&v] (int p) { return (int) std::lround (v[p]); };

            //------------------------------------------------------------ entrée
            const float inTarget = dbToGain (v[inGain]);

            for (int i = 0; i < n; ++i)
            {
                const float g = inSmooth.next (inTarget);
                cl[i] *= g;
                cr[i] *= g;
                peakIn = std::max (peakIn, std::max (std::abs (cl[i]), std::abs (cr[i])));
                dry[0][i] = dryDelay[0].readInt (Drive::getLatency());
                dry[1][i] = dryDelay[1].readInt (Drive::getLatency());
                dryDelay[0].write (cl[i]);
                dryDelay[1].write (cr[i]);
            }

            // "présence" : y a-t-il eu du son récemment ? (sert à éteindre le bruit de fond dans les silences)
            {
                float chunkPeak = 0.0f;

                for (int i = 0; i < n; ++i)
                    chunkPeak = std::max (chunkPeak, std::max (std::abs (cl[i]), std::abs (cr[i])));

                if (chunkPeak > 1.0e-4f)
                    presenceHold = (int) (1.2 * sr);
                else
                    presenceHold = std::max (0, presenceHold - n);

                const float target = presenceHold > 0 ? 1.0f : 0.0f;
                presence = target + presenceCoef * (presence - target);
            }

            //------------------------------------------------------------ matière
            timeWarp.process (cl, cr, n, pick (timeMode), pick (timeLen), pc (timeMix), bpm, beat);
            pump.process (cl, cr, n, pc (P::pump), bpm, beat);
            grain.process (cl, cr, n, pc (grainMix), v[grainSize], v[grainDensity], v[grainPitch],
                           pc (grainSpray), pc (grainReverse), pc (grainShards));

            //------------------------------------------------------------ usure
            wobble.process (cl, cr, n, pc (P::wobble), v[wobbleRate]);
            dropouts.process (cl, cr, n, pc (P::dropouts));
            noise.process (cl, cr, n, pc (P::noise), pick (noiseType), presence);

            //------------------------------------------------------------ couleur
            drive[0].process (cl, n, pick (driveType), pc (P::drive));
            drive[1].process (cr, n, pick (driveType), pc (P::drive));
            crusher[0].process (cl, n, pc (crush), sr);
            crusher[1].process (cr, n, pc (crush), sr);
            filter.process (cl, cr, n, pick (filterType), v[filterCutoff], pc (filterRes));
            modulator.process (cl, cr, n, pick (modType), pc (modAmount), v[modRate]);

            //------------------------------------------------------------ écho
            {
                const float target = pc (dlyMix);

                if (target > 0.0005f || echoSmooth.y > 0.0005f)
                {
                    echoIdle = false;
                    echo.setTime (pick (dlyTime), bpm);
                    const float feedback = pc (dlyFeedback);

                    for (int i = 0; i < n; ++i)
                    {
                        const float amount = echoSmooth.next (target);
                        float el, er;
                        echo.process (0.5f * (cl[i] + cr[i]), feedback, el, er);
                        cl[i] += amount * el;
                        cr[i] += amount * er;
                    }
                }
                else if (! echoIdle)
                {
                    echo.reset();
                    echoIdle = true;
                }
            }

            //------------------------------------------------------------ espace
            {
                const float target = pc (revMix);

                if (target > 0.0005f || reverbSmooth.y > 0.0005f)
                {
                    reverbIdle = false;
                    reverb.setSize (pc (revSize));

                    for (int i = 0; i < n; ++i)
                    {
                        const float amount = reverbSmooth.next (target);
                        float wl, wr;
                        reverb.process (0.5f * (cl[i] + cr[i]), wl, wr);
                        const float keep = 1.0f - 0.65f * amount * amount;
                        cl[i] = cl[i] * keep + amount * 1.5f * wl;
                        cr[i] = cr[i] * keep + amount * 1.5f * wr;
                    }
                }
                else if (! reverbIdle)
                {
                    reverb.reset();
                    reverbIdle = true;
                }
            }

            //------------------------------------------------------------ finition
            glue.process (cl, cr, n, pc (P::glue));
            worstGlue = std::max (worstGlue, glue.reduction);
            tone.process (cl, cr, n, v[weight], v[air]);
            width.process (cl, cr, n, pc (P::width));

            //------------------------------------------------------------ mix et sortie
            const float mixTarget = pc (mix);
            const float outTarget = dbToGain (v[outGain]);

            for (int i = 0; i < n; ++i)
            {
                const float m = mixSmooth.next (mixTarget);
                const float g = outSmooth.next (outTarget);
                cl[i] = g * (dry[0][i] + m * (cl[i] - dry[0][i]));
                cr[i] = g * (dry[1][i] + m * (cr[i] - dry[1][i]));
            }

            limiter.process (cl, cr, n);

            for (int i = 0; i < n; ++i)
                peakOut = std::max (peakOut, std::max (std::abs (cl[i]), std::abs (cr[i])));
        }

        puckX.store (px, std::memory_order_relaxed);
        puckY.store (py, std::memory_order_relaxed);
        grainCount.store (grain.activeGrains(), std::memory_order_relaxed);
        storeMax (meterIn, peakIn);
        storeMax (meterOut, peakOut);
        storeMax (meterGlue, worstGlue);
    }

private:
    static void storeMax (std::atomic<float>& target, float value) noexcept
    {
        if (value > target.load (std::memory_order_relaxed))
            target.store (value, std::memory_order_relaxed);
    }

    double sr = 44100.0, freeBeat = 0.0;

    TimeWarp timeWarp;
    Pump pump;
    GrainCloud grain;
    Wobble wobble;
    Dropouts dropouts;
    Noise noise;
    Drive drive[2];
    Crusher crusher[2];
    MorphFilter filter;
    Modulator modulator;
    PingPongDelay echo;
    Reverb reverb;
    Glue glue;
    Tone tone;
    Width width;
    Limiter limiter;

    DelayLine dryDelay[2];
    float dry[2][chunk] {};
    Smooth inSmooth, outSmooth, mixSmooth, echoSmooth, reverbSmooth;
    float px = 0.0f, py = 0.0f, puckCoef = 0.0f, presence = 0.0f, presenceCoef = 0.0f;
    int presenceHold = 0;
    bool puckStarted = false, echoIdle = true, reverbIdle = true;
};

} // namespace pz
