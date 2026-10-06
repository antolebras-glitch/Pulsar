// PULSAR - les paramètres vus par l'hôte (FL Studio), construits à partir de la table du moteur.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "dsp/Engine.h"

namespace pulsar
{
// Texte accentué : les fichiers sont en UTF-8.
inline juce::String U (const char* utf8) { return juce::String::fromUTF8 (utf8); }

inline juce::StringArray choicesFor (int param)
{
    switch (param)
    {
        case pz::P::motionShape: return { U ("À la main"), U ("Cercle"), U ("Huit"), U ("Balayage X"), U ("Balayage Y"), U ("Dérive"), U ("Sauts") };
        case pz::P::motionRate:  return { U ("8 mesures"), U ("4 mesures"), U ("2 mesures"), U ("1 mesure"), U ("1/2"), U ("1/4"), U ("1/8"), U ("1/16") };
        case pz::P::timeMode:    return { U ("Demi-vitesse"), U ("Inversé"), U ("Répétition") };
        case pz::P::timeLen:     return { U ("1/16"), U ("1/8"), U ("1/4"), U ("1/2"), U ("1 mesure"), U ("2 mesures") };
        case pz::P::noiseType:   return { U ("Vinyle"), U ("Bande"), U ("Secteur") };
        case pz::P::driveType:   return { U ("Lampe"), U ("Bande"), U ("Clip"), U ("Radio") };
        case pz::P::filterType:  return { U ("Coupé"), U ("Passe-bas"), U ("Passe-bande"), U ("Passe-haut") };
        case pz::P::modType:     return { U ("Chorus"), U ("Flanger"), U ("Phaser") };
        case pz::P::dlyTime:     return { U ("Slap 95 ms"), U ("1/16"), U ("1/8 triolet"), U ("1/8"), U ("1/8 pointée"),
                                          U ("1/4 triolet"), U ("1/4"), U ("1/4 pointée"), U ("1/2") };
        default:                 return {};
    }
}

inline juce::String formatValue (const pz::Spec& spec, float v)
{
    using Unit = pz::Unit;

    switch (spec.unit)
    {
        case Unit::percent:   return juce::String (juce::roundToInt (v)) + " %";
        case Unit::decibels:  return (v > 0.04f ? "+" : "") + juce::String (std::abs (v) < 0.05f ? 0.0f : v, 1) + " dB";
        case Unit::hertz:     return v < 1000.0f ? juce::String (juce::roundToInt (v)) + " Hz" : juce::String (v / 1000.0f, 1) + " kHz";
        case Unit::rate:      return juce::String (v, v < 1.0f ? 2 : 1) + " Hz";
        case Unit::millis:    return juce::String (juce::roundToInt (v)) + " ms";
        case Unit::semitones: return (v > 0.04f ? "+" : "") + juce::String (juce::roundToInt (v)) + " st";
        case Unit::density:   return "x" + juce::String (v, 1);
        case Unit::choice:    break;
    }

    return juce::String (juce::roundToInt (v));
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const auto& table = pz::specs();

    for (int p = 0; p < pz::numParams; ++p)
    {
        const pz::Spec spec = table[(size_t) p];

        if (spec.unit == pz::Unit::choice)
        {
            layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { spec.id, 1 }, U (spec.name), choicesFor (p),
                                                                      juce::roundToInt (spec.def)));
            continue;
        }

        // Les conversions passent par la table du moteur : l'hôte, l'interface et le son parlent des mêmes valeurs.
        juce::NormalisableRange<float> range (spec.lo, spec.hi,
                                              [spec] (float, float, float proportion) { return spec.fromNorm (proportion); },
                                              [spec] (float, float, float value) { return spec.toNorm (value); },
                                              [spec] (float, float, float value) { return spec.fromNorm (spec.toNorm (value)); });

        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { spec.id, 1 }, U (spec.name), range, spec.def,
                                                                 juce::AudioParameterFloatAttributes().withStringFromValueFunction (
                                                                     [spec] (float v, int) { return formatValue (spec, v); })));
    }

    return layout;
}

} // namespace pulsar
