// Banc de test du moteur de Pulsar, sans JUCE ni carte son.
// Compilation : g++ -O2 -std=c++17 -I Source tests/engine_test.cpp -o engine_test
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>

#if defined (__SSE__) || defined (_M_X64) || defined (__x86_64__)
 #include <pmmintrin.h>
 #include <xmmintrin.h>
 #define PULSAR_TEST_SSE 1
#endif

#include "dsp/Engine.h"

using namespace pz;

static int failures = 0;

static void check (bool ok, const std::string& what)
{
    std::printf ("  [%s] %s\n", ok ? "ok" : "ECHEC", what.c_str());

    if (! ok)
        ++failures;
}

struct Stereo
{
    std::vector<float> l, r;
};

// Une petite mélodie stéréo : accords de dents de scie légèrement désaccordées, une note toutes les demi-secondes.
static Stereo melody (double sr, double seconds, float level = 0.12f)
{
    Stereo s;
    const int n = (int) (sr * seconds);
    s.l.resize ((size_t) n);
    s.r.resize ((size_t) n);
    static const double notes[] = { 220.0, 261.63, 329.63, 392.0, 293.66, 349.23 };
    double phase[6] = {};

    for (int i = 0; i < n; ++i)
    {
        const double t = (double) i / sr;
        const int step = (int) (t * 2.0);
        const double env = std::exp (-3.0 * (t * 2.0 - (double) step));
        const double f = notes[step % 6];
        const double freqs[6] = { f, f * 1.003, f * 1.5, f * 1.5 * 0.997, f * 2.0, f * 0.5 };
        double left = 0.0, right = 0.0;

        for (int k = 0; k < 6; ++k)
        {
            phase[k] += freqs[k] / sr;
            phase[k] -= std::floor (phase[k]);
            const double saw = 2.0 * phase[k] - 1.0;
            (k % 2 == 0 ? left : right) += saw;
            (k % 2 == 0 ? right : left) += 0.4 * saw;
        }

        s.l[(size_t) i] = (float) (level * env * left);
        s.r[(size_t) i] = (float) (level * env * right);
    }

    return s;
}

static Stereo sine (double sr, double seconds, double freq, float level = 0.25f)
{
    Stereo s;
    const int n = (int) (sr * seconds);
    s.l.resize ((size_t) n);
    s.r.resize ((size_t) n);

    for (int i = 0; i < n; ++i)
        s.l[(size_t) i] = s.r[(size_t) i] = level * (float) std::sin (2.0 * kPi * freq * (double) i / sr);

    return s;
}

static Controls defaults()
{
    Controls c;

    for (int p = 0; p < numParams; ++p)
        c.norm[(size_t) p] = specs()[(size_t) p].toNorm (specs()[(size_t) p].def);

    return c;
}

static void set (Controls& c, int p, float value) { c.norm[(size_t) p] = specs()[(size_t) p].toNorm (value); }

static Stereo run (Engine& e, const Stereo& in, const Controls& c, double sr, double bpm = 140.0, int block = 256)
{
    Stereo out = in;
    const int n = (int) in.l.size();
    double ppq = 0.0;

    for (int pos = 0; pos < n; pos += block)
    {
        const int len = std::min (block, n - pos);
        e.process (out.l.data() + pos, out.r.data() + pos, len, c, bpm, ppq, true);
        ppq += (double) len * bpm / (60.0 * sr);
    }

    return out;
}

static double rmsDb (const Stereo& s, size_t from = 0)
{
    double acc = 0.0;
    size_t count = 0;

    for (size_t i = from; i < s.l.size(); ++i)
    {
        acc += (double) s.l[i] * s.l[i] + (double) s.r[i] * s.r[i];
        count += 2;
    }

    return 10.0 * std::log10 (acc / (double) std::max<size_t> (1, count) + 1.0e-30);
}

static float peak (const Stereo& s)
{
    float p = 0.0f;

    for (size_t i = 0; i < s.l.size(); ++i)
        p = std::max (p, std::max (std::abs (s.l[i]), std::abs (s.r[i])));

    return p;
}

static bool finite (const Stereo& s)
{
    for (size_t i = 0; i < s.l.size(); ++i)
        if (! std::isfinite (s.l[i]) || ! std::isfinite (s.r[i]))
            return false;

    return true;
}

// Énergie autour d'une fréquence (Goertzel).
static double power (const std::vector<float>& x, double sr, double freq, size_t from)
{
    const double w = 2.0 * kPi * freq / sr, coef = 2.0 * std::cos (w);
    double s1 = 0.0, s2 = 0.0;

    for (size_t i = from; i < x.size(); ++i)
    {
        const double s0 = (double) x[i] + coef * s1 - s2;
        s2 = s1;
        s1 = s0;
    }

    return (s1 * s1 + s2 * s2 - coef * s1 * s2) / (double) ((x.size() - from) * (x.size() - from));
}

static double cpuPercent (double sr, const Controls& c, double seconds = 6.0)
{
    auto engine = std::make_unique<Engine>();
    engine->prepare (sr);
    const Stereo in = melody (sr, seconds);
    Stereo out = in;
    const int n = (int) in.l.size(), block = 256;
    double ppq = 0.0;
    const auto t0 = std::chrono::steady_clock::now();

    for (int pos = 0; pos < n; pos += block)
    {
        const int len = std::min (block, n - pos);
        engine->process (out.l.data() + pos, out.r.data() + pos, len, c, 140.0, ppq, true);
        ppq += (double) len * 140.0 / (60.0 * sr);
    }

    const double elapsed = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    return 100.0 * elapsed / seconds;
}

static void testRate (double sr)
{
    std::printf ("\n===== %.0f Hz =====\n", sr);
    const Stereo in = melody (sr, 6.0);
    const double inDb = rmsDb (in);
    std::printf ("  niveau du signal de test : %.1f dB RMS, crete %.2f\n", inDb, peak (in));

    //---------------------------------------------------------------- A. neutre
    {
        auto e = std::make_unique<Engine>();
        e->prepare (sr);
        const Stereo out = run (*e, in, defaults(), sr);
        const int lat = Engine::getLatency();
        double err = 0.0, sig = 0.0;

        for (size_t i = (size_t) lat + 2000; i < in.l.size(); ++i)
        {
            const double d = (double) out.l[i] - (double) in.l[i - (size_t) lat];
            err += d * d;
            sig += (double) in.l[i - (size_t) lat] * in.l[i - (size_t) lat];
        }

        const double snr = 10.0 * std::log10 (sig / (err + 1.0e-30));
        std::printf ("  A. neutre : SNR %.1f dB (latence %d)\n", snr, lat);
        check (snr > 90.0, "tout a zero = le son ressort intact");
    }

    //---------------------------------------------------------------- B. chaque module seul
    {
        struct Case
        {
            const char* name;
            std::vector<std::pair<int, float>> values;
            double lo, hi; // écart de niveau toléré par rapport à l'entrée (dB)
        };

        const std::vector<Case> cases = {
            { "temps demi-vitesse",  { { P::timeMode, 0 }, { P::timeMix, 100 } }, -6, 3 },
            { "temps inverse",       { { P::timeMode, 1 }, { P::timeMix, 100 } }, -6, 3 },
            { "temps repetition",    { { P::timeMode, 2 }, { P::timeMix, 100 }, { P::timeLen, 2 } }, -8, 4 },
            { "pompe",               { { P::pump, 100 } }, -9, 0 },
            { "grain defaut",        { { P::grainMix, 100 } }, -5, 3 },
            { "grain dense",         { { P::grainMix, 100 }, { P::grainDensity, 8 }, { P::grainSpray, 60 } }, -6, 3 },
            { "grain +12 eclats",    { { P::grainMix, 100 }, { P::grainPitch, 12 }, { P::grainShards, 50 }, { P::grainDensity, 4 } }, -6, 3 },
            { "grain -12 inverse",   { { P::grainMix, 100 }, { P::grainPitch, -12 }, { P::grainReverse, 100 }, { P::grainSize, 300 } }, -6, 3 },
            { "grain clairseme",     { { P::grainMix, 100 }, { P::grainDensity, 0.5f }, { P::grainSize, 60 } }, -12, 0 },
            { "wobble",              { { P::wobble, 100 } }, -2, 2 },
            { "pertes",              { { P::dropouts, 100 } }, -9, 0 },
            { "bruit vinyle",        { { P::noise, 100 }, { P::noiseType, 0 } }, -1, 4 },
            { "bruit bande",         { { P::noise, 100 }, { P::noiseType, 1 } }, -1, 4 },
            { "bruit secteur",       { { P::noise, 100 }, { P::noiseType, 2 } }, -1, 4 },
            { "drive lampe",         { { P::drive, 100 }, { P::driveType, 0 } }, -3, 5 },
            { "drive bande",         { { P::drive, 100 }, { P::driveType, 1 } }, -3, 5 },
            { "drive clip",          { { P::drive, 100 }, { P::driveType, 2 } }, -3, 5 },
            { "drive radio",         { { P::drive, 100 }, { P::driveType, 3 } }, -6, 5 },
            { "crush",               { { P::crush, 100 } }, -3, 3 },
            { "filtre bas 800",      { { P::filterType, 1 }, { P::filterCutoff, 800 }, { P::filterRes, 50 } }, -12, 2 },
            { "filtre bande 1k",     { { P::filterType, 2 }, { P::filterCutoff, 1000 }, { P::filterRes, 50 } }, -18, 0 },
            { "filtre haut 1k",      { { P::filterType, 3 }, { P::filterCutoff, 1000 }, { P::filterRes, 80 } }, -16, 2 },
            { "chorus",              { { P::modType, 0 }, { P::modAmount, 100 } }, -3, 3 },
            { "flanger",             { { P::modType, 1 }, { P::modAmount, 100 } }, -3, 4 },
            { "phaser",              { { P::modType, 2 }, { P::modAmount, 100 } }, -4, 3 },
            { "echo",                { { P::dlyMix, 100 }, { P::dlyFeedback, 80 } }, 0, 6 },
            { "espace petit",        { { P::revMix, 100 }, { P::revSize, 10 } }, -5, 4 },
            { "espace moyen",        { { P::revMix, 100 }, { P::revSize, 50 } }, -5, 4 },
            { "espace immense",      { { P::revMix, 100 }, { P::revSize, 100 } }, -5, 5 },
            { "colle",               { { P::glue, 100 } }, -2, 9 },
            { "poids + air",         { { P::weight, 9 }, { P::air, 9 } }, 0, 8 },
            { "largeur 200",         { { P::width, 200 } }, -1, 4 },
            { "largeur 0",           { { P::width, 0 } }, -4, 1 },
        };

        std::printf ("  B. chaque effet seul, a fond (ecart de niveau par rapport a l'entree) :\n");
        bool allFinite = true, allBounded = true, allInRange = true;

        for (const auto& k : cases)
        {
            auto e = std::make_unique<Engine>();
            e->prepare (sr);
            Controls c = defaults();

            for (const auto& v : k.values)
                set (c, v.first, v.second);

            const Stereo out = run (*e, in, c, sr);
            const double delta = rmsDb (out) - inDb;
            const bool inRange = delta >= k.lo && delta <= k.hi;
            std::printf ("       %-20s %+6.1f dB  crete %.2f%s\n", k.name, delta, peak (out), inRange ? "" : "   <-- hors plage");
            allFinite = allFinite && finite (out);
            allBounded = allBounded && peak (out) <= 1.0f;
            allInRange = allInRange && inRange;
        }

        check (allFinite, "aucune valeur invalide");
        check (allBounded, "la sortie ne depasse jamais 0 dB");
        check (allInRange, "aucun effet ne fait exploser ni disparaitre le niveau");
    }

    //---------------------------------------------------------------- C. hauteurs
    {
        const Stereo tone = sine (sr, 4.0, 440.0);
        const size_t from = (size_t) (sr * 1.0);

        auto e = std::make_unique<Engine>();
        e->prepare (sr);
        Controls c = defaults();
        set (c, P::timeMode, 0);
        set (c, P::timeMix, 100);
        Stereo out = run (*e, tone, c, sr);
        const double half = power (out.l, sr, 220.0, from), same = power (out.l, sr, 440.0, from);
        std::printf ("  C. demi-vitesse sur 440 Hz : 220 Hz %.1f dB, 440 Hz %.1f dB\n", 10 * std::log10 (half + 1e-30), 10 * std::log10 (same + 1e-30));
        check (half > 30.0 * same, "la demi-vitesse descend bien d'une octave");

        e = std::make_unique<Engine>();
        e->prepare (sr);
        c = defaults();
        set (c, P::grainMix, 100);
        set (c, P::grainPitch, 12);
        out = run (*e, tone, c, sr);
        const double up = power (out.l, sr, 880.0, from), base = power (out.l, sr, 440.0, from);
        std::printf ("     grain +12 sur 440 Hz : 880 Hz %.1f dB, 440 Hz %.1f dB\n", 10 * std::log10 (up + 1e-30), 10 * std::log10 (base + 1e-30));
        check (up > 30.0 * base, "le grain +12 monte bien d'une octave");

        e = std::make_unique<Engine>();
        e->prepare (sr);
        c = defaults();
        set (c, P::grainMix, 100);
        set (c, P::grainPitch, -12);
        out = run (*e, tone, c, sr);
        const double down = power (out.l, sr, 220.0, from), base2 = power (out.l, sr, 440.0, from);
        check (down > 30.0 * base2, "le grain -12 descend bien d'une octave");
    }

    //---------------------------------------------------------------- D. le portail pilote les réglages
    {
        auto e = std::make_unique<Engine>();
        e->prepare (sr);
        Controls c = defaults();
        set (c, P::filterType, 1);
        c.depthX[P::filterCutoff] = -0.6f;
        c.depthY[P::revMix] = 0.5f;
        c.norm[P::padX] = 1.0f;
        c.norm[P::padY] = 0.0f;
        const Stereo closed = run (*e, in, c, sr);
        const float liveCutoff = e->live[P::filterCutoff].load();
        const float liveReverb = e->live[P::revMix].load();
        std::printf ("  D. portail a droite : coupure a %.0f Hz, niveau %+.1f dB\n",
                     specs()[P::filterCutoff].fromNorm (liveCutoff), rmsDb (closed) - inDb);
        check (std::abs (liveCutoff - 0.4f) < 0.01f && liveReverb < 0.01f, "X deplace le filtre, Y ne touche a rien tant qu'il est en bas");
        check (rmsDb (closed) < inDb - 1.0, "le filtre pilote par X assombrit bien le son");

        // orbite en cercle : le point doit tourner
        e = std::make_unique<Engine>();
        e->prepare (sr);
        c = defaults();
        set (c, P::motionShape, 1);
        set (c, P::motionRate, 5);
        set (c, P::motionSize, 100);
        c.norm[P::padX] = 0.5f;
        c.norm[P::padY] = 0.5f;
        float minX = 1.0f, maxX = 0.0f, minY = 1.0f, maxY = 0.0f;
        Stereo buf = in;

        for (int pos = 0; pos + 256 <= (int) buf.l.size(); pos += 256)
        {
            e->process (buf.l.data() + pos, buf.r.data() + pos, 256, c, 140.0, -1.0e9, false);
            minX = std::min (minX, e->puckX.load());
            maxX = std::max (maxX, e->puckX.load());
            minY = std::min (minY, e->puckY.load());
            maxY = std::max (maxY, e->puckY.load());
        }

        std::printf ("     orbite en cercle : X de %.2f a %.2f, Y de %.2f a %.2f\n", minX, maxX, minY, maxY);
        check (minX < 0.1f && maxX > 0.9f && minY < 0.1f && maxY > 0.9f, "l'orbite fait bien le tour du portail");
    }

    //---------------------------------------------------------------- E. stress
    {
        auto e = std::make_unique<Engine>();
        e->prepare (sr);
        Rng rng;
        rng.state = 0xc0ffee11u;
        Stereo buf = melody (sr, 20.0, 0.5f);
        Controls c = defaults();
        bool ok = true;
        float worst = 0.0f;
        double ppq = 0.0;
        int pos = 0;

        while (pos < (int) buf.l.size())
        {
            if ((rng.nextU() & 15u) == 0)
            {
                for (int p = 0; p < numParams; ++p)
                {
                    if (p == P::inGain || p == P::outGain)
                        c.norm[(size_t) p] = 0.5f + 0.5f * rng.unipolar();
                    else
                        c.norm[(size_t) p] = rng.unipolar();

                    c.depthX[(size_t) p] = rng.bipolar();
                    c.depthY[(size_t) p] = rng.bipolar();
                }

                if ((rng.nextU() & 7u) == 0)
                    ppq = 64.0 * rng.unipolar(); // saut dans le morceau
            }

            const int len = std::min ((int) buf.l.size() - pos, 1 + (int) (rng.nextU() % 700u));
            const double bpm = 60.0 + 140.0 * rng.unipolar();
            e->process (buf.l.data() + pos, buf.r.data() + pos, len, c, bpm, ppq, (rng.nextU() & 3u) != 0);
            ppq += (double) len * bpm / (60.0 * sr);

            for (int i = pos; i < pos + len; ++i)
            {
                ok = ok && std::isfinite (buf.l[(size_t) i]) && std::isfinite (buf.r[(size_t) i]);
                worst = std::max (worst, std::max (std::abs (buf.l[(size_t) i]), std::abs (buf.r[(size_t) i])));
            }

            pos += len;
        }

        std::printf ("  E. stress (reglages, tempo et position tires au hasard) : crete max %.3f\n", worst);
        check (ok, "aucune valeur invalide");
        check (worst <= 1.0f, "la sortie reste sous 0 dB");
    }

    //---------------------------------------------------------------- F. CPU
    {
        Controls typical = defaults();
        set (typical, P::grainMix, 60);
        set (typical, P::grainDensity, 4);
        set (typical, P::wobble, 40);
        set (typical, P::noise, 30);
        set (typical, P::drive, 40);
        set (typical, P::filterType, 1);
        set (typical, P::filterCutoff, 4000);
        set (typical, P::dlyMix, 25);
        set (typical, P::revMix, 30);
        set (typical, P::glue, 40);
        set (typical, P::motionShape, 1);

        Controls everything = typical;
        set (everything, P::timeMix, 100);
        set (everything, P::pump, 50);
        set (everything, P::grainMix, 100);
        set (everything, P::grainDensity, 8);
        set (everything, P::grainSpray, 80);
        set (everything, P::grainShards, 50);
        set (everything, P::dropouts, 50);
        set (everything, P::crush, 40);
        set (everything, P::modAmount, 60);
        set (everything, P::weight, 3);
        set (everything, P::air, 4);
        set (everything, P::width, 150);

        const double idle = cpuPercent (sr, defaults());
        const double typ = cpuPercent (sr, typical);
        const double all = cpuPercent (sr, everything);
        std::printf ("  F. CPU d'un coeur : repos %.2f %%, reglage courant %.2f %%, tout allume %.2f %%\n", idle, typ, all);
        check (all < 25.0, "charge raisonnable meme avec tout allume");
    }
}

int main()
{
   #ifdef PULSAR_TEST_SSE
    // Comme dans le plugin (ScopedNoDenormals) : les nombres minuscules sont arrondis à zéro.
    _MM_SET_FLUSH_ZERO_MODE (_MM_FLUSH_ZERO_ON);
    _MM_SET_DENORMALS_ZERO_MODE (_MM_DENORMALS_ZERO_ON);
   #endif

    for (const double sr : { 44100.0, 48000.0, 96000.0 })
        testRate (sr);

    std::printf ("\n%s\n", failures == 0 ? "TOUS LES TESTS PASSENT" : "DES TESTS ECHOUENT");
    return failures == 0 ? 0 : 1;
}
