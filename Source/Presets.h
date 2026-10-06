// PULSAR - les univers et leurs styles.
//
// Pour ajouter un style : copie un bloc { "Nom", "Phrase", "ce que fait X", "ce que fait Y", { ... } },
// change le nom et les valeurs. Chaque ligne de réglage s'écrit :
//     { P::reglage, valeur }                 le potard est posé à cette valeur
//     { P::reglage, valeur, x, y }           en plus, le portail le déplace : x et y sont en % de la course
//                                            du potard (négatif = le portail le baisse)
// Tout ce qui n'est pas cité reste au neutre. Le portail en bas à gauche = les valeurs écrites ici.
#pragma once

#include <vector>

#include "dsp/Table.h"

namespace pulsar
{
struct Setting
{
    int param;
    float value;
    float x = 0.0f, y = 0.0f;
};

struct Preset
{
    const char* name;
    const char* hint;
    const char* xLabel;
    const char* yLabel;
    std::vector<Setting> settings;
};

struct Bank
{
    const char* name;
    const char* hint;
    std::vector<Preset> presets;
};

inline const std::vector<Bank>& getBanks()
{
    namespace P = pz::P;

    // valeurs lisibles pour les menus
    enum { handMade = 0, circle, eight, sweepX, sweepY, drift, jumps };                       // P::motionShape
    enum { bars8 = 0, bars4, bars2, bar1, half, quarter, eighth, sixteenth };                 // P::motionRate
    enum { halfSpeed = 0, reversed, repeat };                                                  // P::timeMode
    enum { len16 = 0, len8, len4, len2, lenBar, len2Bars };                                    // P::timeLen
    enum { vinyl = 0, tapeHiss, mains };                                                       // P::noiseType
    enum { tube = 0, tape, clip, radio };                                                      // P::driveType
    enum { filterOff = 0, lowpass, bandpass, highpass };                                       // P::filterType
    enum { chorus = 0, flanger, phaser };                                                      // P::modType
    enum { slap = 0, d16, d8t, d8, d8dot, d4t, d4, d4dot, d2 };                                // P::dlyTime

    static const std::vector<Bank> banks = {
        //======================================================================================
        { "Nuage", "Découpe le son en grains et le transforme en nappe mouvante.",
          {
              { "Poussière d'étoiles", "Ta mélodie éclate en petits grains qui scintillent à l'octave.", "Éclats", "Espace",
                { { P::grainMix, 70 }, { P::grainSize, 90 }, { P::grainDensity, 3 }, { P::grainSpray, 35, 40, 0 }, { P::grainShards, 40, 55, 0 },
                  { P::dlyMix, 12 }, { P::dlyTime, d8dot }, { P::dlyFeedback, 45 }, { P::revMix, 30, 0, 45 }, { P::revSize, 68, 0, 25 },
                  { P::air, 2 }, { P::padX, 40 }, { P::padY, 40 }, { P::motionShape, drift }, { P::motionRate, bars2 }, { P::motionSize, 45 } } },

              { "Nébuleuse", "Une nappe dense et floue : on ne reconnaît plus les notes, seulement la couleur.", "Brouillard", "Lumière",
                { { P::grainMix, 100 }, { P::grainSize, 300, 30, 0 }, { P::grainDensity, 6 }, { P::grainSpray, 65, 30, 0 }, { P::grainReverse, 25, 55, 0 },
                  { P::grainShards, 0, 0, 55 }, { P::filterType, lowpass }, { P::filterCutoff, 5000, 0, 20 }, { P::revMix, 50 }, { P::revSize, 84 },
                  { P::glue, 30 }, { P::width, 150 }, { P::padX, 45 }, { P::padY, 45 }, { P::motionShape, circle }, { P::motionRate, bars4 }, { P::motionSize, 60 } } },

              { "Octave fantôme", "Une copie de ta mélodie une octave au-dessus, qui flotte derrière l'original.", "Dosage", "Scintille",
                { { P::grainMix, 30, 55, 0 }, { P::grainSize, 140 }, { P::grainDensity, 3 }, { P::grainPitch, 12 }, { P::grainSpray, 10 },
                  { P::grainShards, 0, 0, 60 }, { P::dlyMix, 0, 0, 30 }, { P::dlyTime, d8dot }, { P::revMix, 28 }, { P::revSize, 62 },
                  { P::padX, 50 }, { P::padY, 20 } } },

              { "Marée inversée", "Chaque grain est joué à l'envers : le son reflue vers toi par vagues.", "Vagues", "Espace",
                { { P::grainMix, 100 }, { P::grainSize, 380, -55, 0 }, { P::grainDensity, 2.5f }, { P::grainSpray, 15 }, { P::grainReverse, 100 },
                  { P::revMix, 22, 0, 45 }, { P::revSize, 65, 0, 25 }, { P::padX, 20 }, { P::padY, 30 } } },

              { "Essaim", "Des grains minuscules qui sautent de hauteur en hauteur, calés sur le tempo.", "Agitation", "Hauteur",
                { { P::grainMix, 100 }, { P::grainSize, 70, -35, 0 }, { P::grainDensity, 4 }, { P::grainSpray, 40, 45, 0 }, { P::grainPitch, 0, 0, 50 },
                  { P::grainShards, 15 }, { P::modType, flanger }, { P::modAmount, 22 }, { P::modRate, 0.2f }, { P::revMix, 15 },
                  { P::padX, 50 }, { P::padY, 50 }, { P::motionShape, jumps }, { P::motionRate, eighth }, { P::motionSize, 85 } } },

              { "Verre brisé", "Des éclats aigus et clairsemés, renvoyés par un écho.", "Densité", "Écho",
                { { P::grainMix, 85 }, { P::grainSize, 45 }, { P::grainDensity, 1.2f, 60, 0 }, { P::grainPitch, 12 }, { P::grainSpray, 80 },
                  { P::grainReverse, 40 }, { P::grainShards, 70 }, { P::dlyMix, 18, 0, 40 }, { P::dlyTime, d8dot }, { P::dlyFeedback, 50, 0, 25 },
                  { P::revMix, 28 }, { P::revSize, 60 }, { P::air, 4 }, { P::padX, 30 }, { P::padY, 40 } } },

              { "Souffle grave", "Une octave plus bas, lent et large : une nappe sombre sous ta mélodie.", "Ouverture", "Profondeur",
                { { P::grainMix, 100 }, { P::grainSize, 260, 0, 20 }, { P::grainDensity, 4 }, { P::grainPitch, -12 }, { P::grainSpray, 30 },
                  { P::filterType, lowpass }, { P::filterCutoff, 2200, 35, 0 }, { P::filterRes, 20 }, { P::revMix, 35, 0, 40 }, { P::revSize, 76 },
                  { P::weight, 3 }, { P::padX, 35 }, { P::padY, 35 } } },
          } },

        //======================================================================================
        { "Lo-fi", "Vieillit le son : bande qui pleure, vinyle, cassette, radio.",
          {
              { "Cassette", "Une bonne vieille cassette : ça pleure un peu, ça souffle, c'est chaud.", "Usure", "Étouffé",
                { { P::wobble, 30, 45, 0 }, { P::wobbleRate, 1.0f }, { P::dropouts, 10, 40, 0 }, { P::noise, 22, 30, 0 }, { P::noiseType, tapeHiss },
                  { P::drive, 30 }, { P::driveType, tape }, { P::filterType, lowpass }, { P::filterCutoff, 10000, 0, -32 }, { P::glue, 20 },
                  { P::padX, 30 }, { P::padY, 25 } } },

              { "Vinyle poussiéreux", "Craquements, graves ronds, aigus rabotés : le sample sorti d'un vieux disque.", "Poussière", "Vieillesse",
                { { P::noise, 38, 45, 0 }, { P::noiseType, vinyl }, { P::wobble, 14 }, { P::wobbleRate, 0.55f }, { P::crush, 0, 18, 0 },
                  { P::drive, 15 }, { P::driveType, tube }, { P::filterType, lowpass }, { P::filterCutoff, 9000, 0, -34 }, { P::filterRes, 10 },
                  { P::weight, 2 }, { P::air, -2 }, { P::width, 85, 0, -22 }, { P::padX, 35 }, { P::padY, 30 } } },

              { "VHS", "L'image qui saute, en version son : hauteur instable et chorus baveux.", "Dérèglement", "Flou",
                { { P::wobble, 45, 40, 0 }, { P::wobbleRate, 3.2f, 25, 0 }, { P::dropouts, 18, 35, 0 }, { P::noise, 18 }, { P::noiseType, tapeHiss },
                  { P::crush, 10 }, { P::modType, chorus }, { P::modAmount, 25, 0, 50 }, { P::modRate, 0.4f }, { P::filterType, lowpass },
                  { P::filterCutoff, 7000 }, { P::revMix, 0, 0, 28 }, { P::revSize, 45 }, { P::padX, 30 }, { P::padY, 30 } } },

              { "Radio lointaine", "Une station mal captée, avec sa ronflette et ses pertes de signal.", "Réception", "Distance",
                { { P::drive, 55 }, { P::driveType, radio }, { P::filterType, bandpass }, { P::filterCutoff, 1500, 14, 0 }, { P::filterRes, 22 },
                  { P::noise, 30, 40, 0 }, { P::noiseType, mains }, { P::dropouts, 15, 45, 0 }, { P::revMix, 8, 0, 50 }, { P::revSize, 35, 0, 30 },
                  { P::padX, 25 }, { P::padY, 20 } } },

              { "Sampler 12 bits", "Le grain des vieux samplers : un peu crade, très punchy.", "Crush", "Punch",
                { { P::crush, 28, 50, 0 }, { P::drive, 18, 0, 35 }, { P::driveType, clip }, { P::filterType, lowpass }, { P::filterCutoff, 11000, -18, 0 },
                  { P::glue, 30, 0, 50 }, { P::weight, 3 }, { P::padX, 30 }, { P::padY, 30 } } },

              { "Bande ralentie", "Demi-vitesse sur une bande fatiguée : le classique pour retourner une mélodie.", "Usure", "Espace",
                { { P::timeMode, halfSpeed }, { P::timeLen, lenBar }, { P::timeMix, 100 }, { P::wobble, 22, 40, 0 }, { P::noise, 15, 30, 0 },
                  { P::noiseType, tapeHiss }, { P::dropouts, 0, 35, 0 }, { P::drive, 32 }, { P::driveType, tape }, { P::filterType, lowpass },
                  { P::filterCutoff, 9000 }, { P::revMix, 10, 0, 45 }, { P::revSize, 60 }, { P::padX, 30 }, { P::padY, 30 } } },
          } },

        //======================================================================================
        { "Rythme", "Joue avec le temps, toujours calé sur le tempo du projet.",
          {
              { "Demi-vitesse", "Tout est rejoué deux fois plus lentement, une octave plus bas.", "Sombre", "Espace",
                { { P::timeMode, halfSpeed }, { P::timeLen, lenBar }, { P::timeMix, 100 }, { P::filterType, lowpass }, { P::filterCutoff, 18000, -45, 0 },
                  { P::revMix, 0, 0, 45 }, { P::revSize, 60 }, { P::dlyMix, 0, 0, 22 }, { P::padX, 15 }, { P::padY, 20 } } },

              { "Demi-vitesse large", "La demi-vitesse sur deux mesures, élargie et collée.", "Largeur", "Espace",
                { { P::timeMode, halfSpeed }, { P::timeLen, len2Bars }, { P::timeMix, 100 }, { P::modType, chorus }, { P::modAmount, 15, 40, 0 },
                  { P::modRate, 0.3f }, { P::width, 120, 35, 0 }, { P::glue, 30 }, { P::revMix, 12, 0, 45 }, { P::revSize, 66 },
                  { P::padX, 40 }, { P::padY, 30 } } },

              { "Rewind", "Chaque demi-mesure repart à l'envers. X dose l'effet : à gauche, ton son d'origine.", "Inversion", "Espace",
                { { P::timeMode, reversed }, { P::timeLen, len2 }, { P::timeMix, 0, 100, 0 }, { P::revMix, 0, 0, 50 }, { P::revSize, 62 },
                  { P::padX, 100 }, { P::padY, 25 } } },

              { "Bégaiement", "Le début de chaque temps est répété quatre fois.", "Bégaiement", "Filtre",
                { { P::timeMode, repeat }, { P::timeLen, len4 }, { P::timeMix, 0, 100, 0 }, { P::filterType, highpass }, { P::filterCutoff, 30, 0, 62 },
                  { P::filterRes, 30 }, { P::padX, 100 }, { P::padY, 0 } } },

              { "Hachoir", "Le portail saute tout seul à chaque temps : par moments ça hache, par moments non.", "Hachage", "Écrasement",
                { { P::timeMode, repeat }, { P::timeLen, len8 }, { P::timeMix, 0, 100, 0 }, { P::pump, 45 }, { P::crush, 0, 0, 45 },
                  { P::drive, 15, 0, 35 }, { P::driveType, clip }, { P::padX, 50 }, { P::padY, 30 },
                  { P::motionShape, jumps }, { P::motionRate, quarter }, { P::motionSize, 100 } } },

              { "Pompe", "Le volume plonge à chaque temps, comme une sidechain sur un kick.", "Pompe", "Espace",
                { { P::pump, 55, 45, 0 }, { P::glue, 35 }, { P::width, 125 }, { P::revMix, 10, 0, 45 }, { P::revSize, 62 },
                  { P::padX, 40 }, { P::padY, 30 } } },

              { "Fantôme à l'envers", "L'envers de ta mélodie, noyé dans la réverbe, glissé sous l'original.", "Dosage", "Brume",
                { { P::timeMode, reversed }, { P::timeLen, lenBar }, { P::timeMix, 25, 60, 0 }, { P::revMix, 30, 0, 40 }, { P::revSize, 80 },
                  { P::dlyMix, 15 }, { P::dlyTime, d4 }, { P::dlyFeedback, 45 }, { P::padX, 50 }, { P::padY, 40 } } },
          } },

        //======================================================================================
        { "Cosmos", "Grands espaces et mouvements lents : le son devient un décor.",
          {
              { "Supermassif", "Une réverbe immense et un écho qui n'en finit pas.", "Sombre", "Immensité",
                { { P::revMix, 45, 0, 40 }, { P::revSize, 90, 0, 10 }, { P::dlyMix, 18, 0, 30 }, { P::dlyTime, d4dot }, { P::dlyFeedback, 62 },
                  { P::modType, chorus }, { P::modAmount, 28 }, { P::modRate, 0.25f }, { P::filterType, lowpass }, { P::filterCutoff, 12000, -42, 0 },
                  { P::width, 150 }, { P::padX, 30 }, { P::padY, 40 }, { P::motionShape, drift }, { P::motionRate, bars4 }, { P::motionSize, 40 } } },

              { "Trou de ver", "Phaser lent, écho qui tourne et octave fantôme : le son se tord sur lui-même.", "Torsion", "Vortex",
                { { P::modType, phaser }, { P::modAmount, 45, 40, 0 }, { P::modRate, 0.15f, 30, 0 }, { P::wobble, 12, 40, 0 }, { P::wobbleRate, 0.5f },
                  { P::grainMix, 15, 0, 50 }, { P::grainPitch, 12 }, { P::grainSize, 200 }, { P::grainDensity, 3 }, { P::grainSpray, 20 },
                  { P::dlyMix, 30 }, { P::dlyTime, d8dot }, { P::dlyFeedback, 62, 0, 22 }, { P::revMix, 28, 0, 30 }, { P::revSize, 72 },
                  { P::padX, 45 }, { P::padY, 45 }, { P::motionShape, eight }, { P::motionRate, bars2 }, { P::motionSize, 70 } } },

              { "Horizon", "Le filtre s'ouvre et se referme tout seul toutes les deux mesures.", "Ouverture", "Résonance",
                { { P::filterType, lowpass }, { P::filterCutoff, 350, 58, 0 }, { P::filterRes, 30, 0, 45 }, { P::revMix, 30, 0, 25 }, { P::revSize, 78 },
                  { P::glue, 30 }, { P::padX, 50 }, { P::padY, 30 }, { P::motionShape, sweepX }, { P::motionRate, bars2 }, { P::motionSize, 90 } } },

              { "Cathédrale", "Ta mélodie jouée au fond d'une nef en pierre.", "Taille", "Distance",
                { { P::revMix, 35, 0, 50 }, { P::revSize, 62, 34, 0 }, { P::air, 3 }, { P::weight, -2 }, { P::width, 140 },
                  { P::padX, 50 }, { P::padY, 35 } } },

              { "Aurore", "Un voile brillant et très large qui scintille au-dessus des notes.", "Scintillement", "Halo",
                { { P::grainMix, 20, 45, 0 }, { P::grainPitch, 12 }, { P::grainSize, 180 }, { P::grainDensity, 4 }, { P::grainSpray, 25 },
                  { P::grainShards, 35, 35, 0 }, { P::modType, chorus }, { P::modAmount, 30, 0, 35 }, { P::modRate, 0.25f }, { P::revMix, 35, 0, 40 },
                  { P::revSize, 86 }, { P::air, 4 }, { P::width, 165 }, { P::glue, 25 },
                  { P::padX, 45 }, { P::padY, 45 }, { P::motionShape, circle }, { P::motionRate, bars8 }, { P::motionSize, 50 } } },

              { "Abysse", "Tout descend d'une octave et s'enfonce sous l'eau.", "Pression", "Profondeur",
                { { P::grainMix, 45, 0, 40 }, { P::grainPitch, -12 }, { P::grainSize, 300 }, { P::grainDensity, 4 }, { P::grainSpray, 25 },
                  { P::filterType, lowpass }, { P::filterCutoff, 1800, -22, 0 }, { P::filterRes, 28 }, { P::drive, 0, 50, 0 }, { P::driveType, tube },
                  { P::wobble, 22 }, { P::wobbleRate, 0.3f }, { P::revMix, 40, 0, 35 }, { P::revSize, 90 }, { P::weight, 4 },
                  { P::padX, 30 }, { P::padY, 40 }, { P::motionShape, drift }, { P::motionRate, bars8 }, { P::motionSize, 45 } } },
          } },

        //======================================================================================
        { "Finition", "Donne un son fini : dense, brillant, large. À poser en dernier.",
          {
              { "Particule", "Le coup de polish : plus dense, plus brillant, un peu plus large.", "Densité", "Brillance",
                { { P::glue, 35, 50, 0 }, { P::drive, 10, 20, 0 }, { P::driveType, tape }, { P::weight, 1.5f }, { P::air, 2, 0, 28 },
                  { P::width, 115, 0, 22 }, { P::padX, 40 }, { P::padY, 40 } } },

              { "Écrasé", "Compression extrême sur trois bandes : chaque détail remonte à la surface.", "Dosage", "Air",
                { { P::glue, 45, 55, 0 }, { P::air, 0, 0, 30 }, { P::padX, 70 }, { P::padY, 30 } } },

              { "Large et brillant", "Écarte la stéréo et ouvre le haut du spectre.", "Largeur", "Air",
                { { P::width, 120, 38, 0 }, { P::air, 1, 0, 34 }, { P::modType, chorus }, { P::modAmount, 12, 25, 0 }, { P::modRate, 0.3f },
                  { P::glue, 25 }, { P::padX, 50 }, { P::padY, 50 } } },

              { "Chaleur analogique", "Lampes et bande : des graves ronds et une saturation douce.", "Chaleur", "Âge",
                { { P::drive, 25, 50, 0 }, { P::driveType, tube }, { P::weight, 2, 14, 0 }, { P::air, -1 }, { P::glue, 22 },
                  { P::wobble, 0, 0, 30 }, { P::noise, 0, 0, 28 }, { P::noiseType, tapeHiss }, { P::filterType, lowpass },
                  { P::filterCutoff, 18000, 0, -28 }, { P::padX, 35 }, { P::padY, 15 } } },

              { "Centre solide", "Resserre la stéréo et densifie : pour une mélodie qui doit rester devant.", "Largeur", "Punch",
                { { P::width, 55, 45, 0 }, { P::weight, 2 }, { P::glue, 30, 0, 45 }, { P::drive, 8, 0, 30 }, { P::driveType, clip },
                  { P::padX, 20 }, { P::padY, 35 } } },

              { "Page blanche", "Tout est à zéro et le portail ne pilote rien : à toi de construire.", "libre", "libre",
                { { P::padX, 50 }, { P::padY, 50 } } },
          } },
    };

    return banks;
}

} // namespace pulsar
