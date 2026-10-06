// PULSAR - la table des réglages. Un seul endroit décrit chaque potard : son nom, ses bornes,
// sa valeur par défaut, et s'il peut être piloté par le portail (le pad X/Y).
#pragma once

#include <array>
#include <cmath>

namespace pz
{
// Les réglages sont rangés dans leur propre espace de noms : on écrit P::drive, P::mix, etc.
namespace P
{
enum Id : int
{
    inGain, padX, padY, motionShape, motionRate, motionSize,
    timeMode, timeLen, timeMix, pump,
    grainMix, grainSize, grainDensity, grainPitch, grainSpray, grainReverse, grainShards,
    wobble, wobbleRate, dropouts, noise, noiseType,
    drive, driveType, crush,
    filterType, filterCutoff, filterRes,
    modType, modAmount, modRate,
    dlyMix, dlyTime, dlyFeedback,
    revMix, revSize,
    glue, weight, air, width,
    mix, outGain,
    count
};
}

constexpr int numParams = P::count;

enum class Unit { percent, decibels, hertz, rate, millis, semitones, density, choice };

struct Spec
{
    const char* id;
    const char* name;
    float lo, hi, def;
    float curve;     // 0 = linéaire, > 0 = puissance (valeur = lo + étendue * p^curve), < 0 = logarithmique
    float step;      // 0 = continu
    Unit unit;
    bool mod;        // pilotable par le portail

    float fromNorm (float p) const noexcept
    {
        p = p < 0.0f ? 0.0f : (p > 1.0f ? 1.0f : p);
        float v;

        if (curve < 0.0f)
            v = lo * std::pow (hi / lo, p);
        else if (curve > 0.0f)
            v = lo + (hi - lo) * std::pow (p, curve);
        else
            v = lo + (hi - lo) * p;

        if (step > 0.0f)
            v = lo + step * std::round ((v - lo) / step);

        return v < lo ? lo : (v > hi ? hi : v);
    }

    float toNorm (float v) const noexcept
    {
        v = v < lo ? lo : (v > hi ? hi : v);
        float p;

        if (curve < 0.0f)
            p = std::log (v / lo) / std::log (hi / lo);
        else if (curve > 0.0f)
            p = std::pow ((v - lo) / (hi - lo), 1.0f / curve);
        else
            p = (v - lo) / (hi - lo);

        return p < 0.0f ? 0.0f : (p > 1.0f ? 1.0f : p);
    }
};

// L'ordre doit suivre exactement l'énumération P::Id.
inline const std::array<Spec, numParams>& specs() noexcept
{
    using U = Unit;
    static const std::array<Spec, numParams> table { {
        { "inGain",       "Entrée",            -24.0f,    24.0f,     0.0f,  0.0f, 0.1f, U::decibels,  false },
        { "padX",         "Portail X",           0.0f,   100.0f,     0.0f,  0.0f, 0.0f, U::percent,   false },
        { "padY",         "Portail Y",           0.0f,   100.0f,     0.0f,  0.0f, 0.0f, U::percent,   false },
        { "motionShape",  "Orbite forme",        0.0f,     6.0f,     0.0f,  0.0f, 1.0f, U::choice,    false },
        { "motionRate",   "Orbite durée",        0.0f,     7.0f,     3.0f,  0.0f, 1.0f, U::choice,    false },
        { "motionSize",   "Orbite taille",       0.0f,   100.0f,    50.0f,  0.0f, 0.1f, U::percent,   false },

        { "timeMode",     "Temps mode",          0.0f,     2.0f,     0.0f,  0.0f, 1.0f, U::choice,    false },
        { "timeLen",      "Temps durée",         0.0f,     5.0f,     4.0f,  0.0f, 1.0f, U::choice,    false },
        { "timeMix",      "Temps",               0.0f,   100.0f,     0.0f,  0.0f, 0.1f, U::percent,   true  },
        { "pump",         "Pompe",               0.0f,   100.0f,     0.0f,  0.0f, 0.1f, U::percent,   true  },

        { "grainMix",     "Grain",               0.0f,   100.0f,     0.0f,  0.0f, 0.1f, U::percent,   true  },
        { "grainSize",    "Grain taille",       15.0f,   500.0f,   120.0f, -1.0f, 0.0f, U::millis,    true  },
        { "grainDensity", "Grain densité",       0.5f,     8.0f,     2.0f,  0.0f, 0.0f, U::density,   true  },
        { "grainPitch",   "Grain pitch",       -24.0f,    24.0f,     0.0f,  0.0f, 1.0f, U::semitones, true  },
        { "grainSpray",   "Grain dispersion",    0.0f,   100.0f,     0.0f,  0.0f, 0.1f, U::percent,   true  },
        { "grainReverse", "Grain inversé",       0.0f,   100.0f,     0.0f,  0.0f, 0.1f, U::percent,   true  },
        { "grainShards",  "Grain éclats",        0.0f,   100.0f,     0.0f,  0.0f, 0.1f, U::percent,   true  },

        { "wobble",       "Wobble",              0.0f,   100.0f,     0.0f,  0.0f, 0.1f, U::percent,   true  },
        { "wobbleRate",   "Wobble vitesse",      0.2f,     8.0f,     1.2f, -1.0f, 0.0f, U::rate,      true  },
        { "dropouts",     "Pertes",              0.0f,   100.0f,     0.0f,  0.0f, 0.1f, U::percent,   true  },
        { "noise",        "Bruit",               0.0f,   100.0f,     0.0f,  0.0f, 0.1f, U::percent,   true  },
        { "noiseType",    "Bruit type",          0.0f,     2.0f,     0.0f,  0.0f, 1.0f, U::choice,    false },

        { "drive",        "Drive",               0.0f,   100.0f,     0.0f,  0.0f, 0.1f, U::percent,   true  },
        { "driveType",    "Drive type",          0.0f,     3.0f,     1.0f,  0.0f, 1.0f, U::choice,    false },
        { "crush",        "Crush",               0.0f,   100.0f,     0.0f,  0.0f, 0.1f, U::percent,   true  },

        { "filterType",   "Filtre type",         0.0f,     3.0f,     0.0f,  0.0f, 1.0f, U::choice,    false },
        { "filterCutoff", "Filtre coupure",     30.0f, 18000.0f, 18000.0f, -1.0f, 0.0f, U::hertz,     true  },
        { "filterRes",    "Filtre résonance",    0.0f,   100.0f,    15.0f,  0.0f, 0.1f, U::percent,   true  },

        { "modType",      "Modulation type",     0.0f,     2.0f,     0.0f,  0.0f, 1.0f, U::choice,    false },
        { "modAmount",    "Modulation",          0.0f,   100.0f,     0.0f,  0.0f, 0.1f, U::percent,   true  },
        { "modRate",      "Modulation vitesse",  0.05f,    8.0f,     0.6f, -1.0f, 0.0f, U::rate,      true  },

        { "dlyMix",       "Écho",                0.0f,   100.0f,     0.0f,  0.0f, 0.1f, U::percent,   true  },
        { "dlyTime",      "Écho temps",          0.0f,     8.0f,     4.0f,  0.0f, 1.0f, U::choice,    false },
        { "dlyFeedback",  "Écho feedback",       0.0f,    95.0f,    40.0f,  0.0f, 0.1f, U::percent,   true  },

        { "revMix",       "Espace",              0.0f,   100.0f,     0.0f,  0.0f, 0.1f, U::percent,   true  },
        { "revSize",      "Espace taille",       0.0f,   100.0f,    50.0f,  0.0f, 0.1f, U::percent,   true  },

        { "glue",         "Colle",               0.0f,   100.0f,     0.0f,  0.0f, 0.1f, U::percent,   true  },
        { "weight",       "Poids",              -9.0f,     9.0f,     0.0f,  0.0f, 0.1f, U::decibels,  true  },
        { "air",          "Air",                -9.0f,     9.0f,     0.0f,  0.0f, 0.1f, U::decibels,  true  },
        { "width",        "Largeur",             0.0f,   200.0f,   100.0f,  0.0f, 0.1f, U::percent,   true  },

        { "mix",          "Mix",                 0.0f,   100.0f,   100.0f,  0.0f, 0.1f, U::percent,   true  },
        { "outGain",      "Volume",            -24.0f,    24.0f,     0.0f,  0.0f, 0.1f, U::decibels,  false },
    } };

    return table;
}

// Ce que l'interface et l'hôte envoient au moteur : tout est normalisé entre 0 et 1.
struct Controls
{
    std::array<float, numParams> norm {};   // position de chaque potard
    std::array<float, numParams> depthX {}; // de combien le portail le déplace quand X va de 0 à 1 (-1..+1)
    std::array<float, numParams> depthY {}; // idem pour Y
};

} // namespace pz
