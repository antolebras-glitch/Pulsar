// PULSAR - le nuage de grains : le son entrant est découpé en petits morceaux ("grains")
// qui sont rejoués en se chevauchant, plus haut, plus bas, à l'envers ou éparpillés.
#pragma once

#include "Common.h"

namespace pz
{
class GrainCloud
{
public:
    static constexpr int maxGrains = 48;

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        ring.prepare ((int) (4.3 * sr));
        mixSmooth.setTime (0.02f, sr);
        reset();
    }

    void reset() noexcept
    {
        ring.clear();
        mixSmooth.reset (0.0f);

        for (auto& g : grains)
            g.on = false;

        untilNext = 0.0;
        active = 0;
    }

    int activeGrains() const noexcept { return active; }

    // size en ms, density = nombre de grains qui se chevauchent, pitch en demi-tons,
    // spray / reverse / shards entre 0 et 1.
    void process (float* l, float* r, int n, float mixTarget, float sizeMs, float density,
                  float pitchSemis, float spray, float reverse, float shards) noexcept
    {
        const bool running = mixTarget > 0.0005f || mixSmooth.y > 0.0005f;

        if (! running)
        {
            for (int i = 0; i < n; ++i)
                ring.write (l[i], r[i]);

            if (active > 0)
            {
                for (auto& g : grains)
                    g.on = false;

                active = 0;
            }

            untilNext = 0.0;
            return;
        }

        const int length = std::max (64, (int) (sizeMs * 0.001f * (float) sr));
        const double interval = (double) length / (double) std::max (0.25f, density);

        for (int i = 0; i < n; ++i)
        {
            ring.write (l[i], r[i]);
            const float mix = mixSmooth.next (mixTarget);

            untilNext -= 1.0;

            if (untilNext <= 0.0)
            {
                spawn (length, density, pitchSemis, spray, reverse, shards);
                untilNext += interval * (1.0 + 0.5 * (double) (spray * rng.bipolar()));
            }

            float wl = 0.0f, wr = 0.0f;

            for (auto& g : grains)
            {
                if (! g.on)
                    continue;

                float sl, sr2;
                ring.readCubic (g.delay, sl, sr2);

                // fenêtre de Hann calculée par récurrence (pas de cosinus par échantillon)
                const float window = 0.5f - 0.5f * (float) g.c0;
                const double c = g.k * g.c0 - g.c1;
                g.c1 = g.c0;
                g.c0 = c;

                wl += sl * window * g.gainL;
                wr += sr2 * window * g.gainR;

                g.delay += g.step;

                if (++g.pos >= g.len)
                {
                    g.on = false;
                    --active;
                }
            }

            l[i] += mix * (wl - l[i]);
            r[i] += mix * (wr - r[i]);
        }
    }

private:
    struct Grain
    {
        bool on = false;
        double delay = 0.0, step = 0.0;
        int pos = 0, len = 0;
        float gainL = 1.0f, gainR = 1.0f;
        double c0 = 1.0, c1 = 1.0, k = 2.0; // en double : en float la récurrence se fige sur les grains longs
    };

    void spawn (int length, float density, float pitchSemis, float spray, float reverse, float shards) noexcept
    {
        Grain* slot = nullptr;

        for (auto& g : grains)
        {
            if (! g.on)
            {
                slot = &g;
                break;
            }
        }

        if (slot == nullptr)
            return;

        // Hauteur du grain, avec parfois un "éclat" : une octave ou une quinte tirée au hasard.
        float semis = pitchSemis;

        if (shards > 0.0f && rng.unipolar() < shards)
        {
            const float pick = rng.unipolar();
            semis += pick < 0.5f ? 12.0f : (pick < 0.7f ? 7.0f : (pick < 0.9f ? -12.0f : 24.0f));
        }

        semis = clampf (semis, -36.0f, 36.0f);
        const double rate = std::pow (2.0, (double) semis / 12.0);
        const bool backwards = reverse > 0.0f && rng.unipolar() < reverse;

        // "delay" = distance derrière la tête d'écriture. Elle bouge de "step" à chaque échantillon.
        const double step = backwards ? 1.0 + rate : 1.0 - rate;
        const double travel = step * (double) length;
        const double scatter = (double) (spray * spray * rng.unipolar()) * 1.0 * sr;
        const double limit = (double) (ring.size() - 16);

        double start = 4.0 + scatter + (travel < 0.0 ? -travel : 0.0);
        const double furthest = start + std::max (0.0, travel);

        if (furthest > limit)
            start -= furthest - limit;

        if (start < 4.0)
            return; // grain impossible à loger (très long et très aigu) : on le saute

        slot->on = true;
        slot->delay = start;
        slot->step = step;
        slot->pos = 0;
        slot->len = length;

        // Hann : cos(2*pi*n/len) par récurrence, en partant de n = 0.
        const double w = 2.0 * kPi / (double) length;
        slot->k = 2.0 * std::cos (w);
        slot->c0 = 1.0;
        slot->c1 = std::cos (w);

        // Plus les grains se chevauchent, plus on baisse chacun pour garder le même volume.
        // Des grains identiques et alignés s'additionnent en amplitude ; des grains dispersés, transposés
        // ou inversés s'additionnent en énergie, donc moins vite.
        const bool aligned = std::abs (semis) < 0.01f && ! backwards && scatter < 8.0;
        const float level = aligned ? std::min (1.0f, 2.0f / density)
                                    : std::min (1.0f, 1.0f / std::sqrt (0.375f * density));
        const float pan = rng.bipolar() * (0.2f + 0.8f * spray);
        slot->gainL = level * std::min (1.0f, 1.0f - pan);
        slot->gainR = level * std::min (1.0f, 1.0f + pan);
        ++active;
    }

    double sr = 44100.0;
    StereoRing ring;
    Smooth mixSmooth;
    std::array<Grain, maxGrains> grains;
    Rng rng;
    double untilNext = 0.0;
    int active = 0;
};

} // namespace pz
