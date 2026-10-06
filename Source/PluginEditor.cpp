#include "PluginEditor.h"

using pulsar::U;
namespace P = pz::P;

namespace
{
    constexpr int columnX[3] = { 24, 368, 716 };
    constexpr int columnW[3] = { 320, 324, 320 };
    constexpr int rowY[4] = { 86, 226, 366, 506 };
    constexpr int moduleHeight = 106, titleHeight = 26, knobHeight = 80;

    float levelToProportion (float gain)
    {
        return juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (gain, -60.0f) + 54.0f) / 54.0f);
    }

    void extend (juce::Path& path, juce::Point<float> point, bool first)
    {
        if (first)
            path.startNewSubPath (point);
        else
            path.lineTo (point);
    }
}

//==============================================================================
Portal::Portal (PulsarProcessor& p) : processor (p)
{
    setTooltip (U ("Le portail. Déplace le point pour transformer le son : en bas à gauche, les potards restent où tu les as posés ; "
                   "plus tu t'éloignes, plus X et Y les emmènent loin."));
    setMouseCursor (juce::MouseCursor::CrosshairCursor);
}

void Portal::setAxisLabels (const juce::String& x, const juce::String& y)
{
    if (x != xLabel || y != yLabel)
    {
        xLabel = x;
        yLabel = y;
        repaint();
    }
}

void Portal::setAssignAxis (int axis)
{
    assignAxis = axis;
    repaint();
}

juce::Rectangle<float> Portal::field() const
{
    return getLocalBounds().toFloat().reduced (20.0f, 20.0f);
}

juce::Point<float> Portal::toScreen (float x, float y) const
{
    const auto f = field();
    return { f.getX() + juce::jlimit (0.0f, 1.0f, x) * f.getWidth(), f.getBottom() - juce::jlimit (0.0f, 1.0f, y) * f.getHeight() };
}

void Portal::animate (float newLevel, bool engineRunning)
{
    level += (newLevel - level) * (newLevel > level ? 0.5f : 0.12f);
    spin += 0.03f + 0.12f * level;

    if (spin > juce::MathConstants<float>::twoPi * 64.0f)
        spin -= juce::MathConstants<float>::twoPi * 64.0f;

    baseX = processor.baseNorm (P::padX);
    baseY = processor.baseNorm (P::padY);
    shape = juce::roundToInt (processor.baseNorm (P::motionShape) * 6.0f);
    orbitSize = processor.baseNorm (P::motionSize);

    // Si le moteur est en veille (rien ne joue), le point reste là où on l'a posé.
    const float x = engineRunning ? processor.engine.puckX.load() : baseX;
    const float y = engineRunning ? processor.engine.puckY.load() : baseY;

    for (int i = (int) trail.size() - 1; i > 0; --i)
        trail[(size_t) i] = trail[(size_t) (i - 1)];

    trail[0] = { puckX, puckY };
    trailCount = juce::jmin ((int) trail.size(), trailCount + 1);
    puckX = x;
    puckY = y;
    repaint();
}

void Portal::paint (juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat().reduced (0.5f);
    const auto p = toScreen (puckX, puckY);
    const float energy = juce::jlimit (0.0f, 1.0f, level);

    juce::Path outline;
    outline.addRoundedRectangle (area, 9.0f);

    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillPath (outline);

    {
        juce::Graphics::ScopedSaveState saved (g);
        g.reduceClipRegion (outline);

        // lueur autour du point
        juce::ColourGradient glow (ui::gold.withAlpha (0.13f + 0.22f * energy), p.x, p.y, ui::gold.withAlpha (0.0f), p.x, p.y + 150.0f, true);
        g.setGradientFill (glow);
        g.fillRect (area);

        // grille d'espace-temps, tirée vers le point
        const float strength = 0.5f + 0.3f * energy, reach = 44.0f * 44.0f;
        const auto warp = [&p, strength, reach] (juce::Point<float> q)
        {
            const auto d = q - p;
            const float k = strength * reach / (d.x * d.x + d.y * d.y + reach);
            return q - d * k;
        };

        const int columns = 13, rows = 10, steps = 36;
        juce::Path grid;

        for (int i = 0; i <= columns; ++i)
        {
            const float x = area.getX() + area.getWidth() * (float) i / (float) columns;

            for (int s = 0; s <= steps; ++s)
                extend (grid, warp ({ x, area.getY() + area.getHeight() * (float) s / (float) steps }), s == 0);
        }

        for (int j = 0; j <= rows; ++j)
        {
            const float y = area.getY() + area.getHeight() * (float) j / (float) rows;

            for (int s = 0; s <= steps; ++s)
                extend (grid, warp ({ area.getX() + area.getWidth() * (float) s / (float) steps, y }), s == 0);
        }

        g.setColour (ui::gold.withAlpha (0.15f + 0.07f * energy));
        g.strokePath (grid, juce::PathStrokeType (0.9f));

        // trajet de l'orbite
        if (shape != pz::Orbit::manual && orbitSize > 0.01f)
        {
            const float radius = 0.5f * orbitSize;
            juce::Path path;

            if (shape == pz::Orbit::drift || shape == pz::Orbit::jumps)
            {
                // mouvement tiré au hasard : on montre seulement la zone où le point peut aller
                const auto a = toScreen (baseX - radius, baseY + radius), b = toScreen (baseX + radius, baseY - radius);
                path.addRoundedRectangle (juce::Rectangle<float> (a, b), 6.0f);
            }
            else
            {
                for (int s = 0; s <= 96; ++s)
                {
                    float ox, oy;
                    pz::Orbit::offset (shape, (double) s / 96.0, ox, oy);
                    extend (path, toScreen (baseX + radius * ox, baseY + radius * oy), s == 0);
                }
            }

            const float dashes[] = { 4.0f, 5.0f };
            juce::Path dashed;
            juce::PathStrokeType (1.1f).createDashedStroke (dashed, path, dashes, 2);
            g.setColour (ui::gold.withAlpha (0.5f));
            g.fillPath (dashed);

            // l'endroit où le point a été posé à la main
            const auto home = toScreen (baseX, baseY);
            g.setColour (ui::gold.withAlpha (0.75f));
            g.drawEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre (home), 1.2f);
        }

        // traînée
        for (int i = trailCount - 1; i >= 0; --i)
        {
            const float fade = 1.0f - (float) (i + 1) / (float) (trail.size() + 1);
            const auto q = toScreen (trail[(size_t) i].x, trail[(size_t) i].y);
            g.setColour (ui::gold.withAlpha (0.20f * fade * fade));
            g.fillEllipse (juce::Rectangle<float> (4.0f + 6.0f * fade, 4.0f + 6.0f * fade).withCentre (q));
        }

        // le point : un petit trou noir avec son anneau de lumière
        {
            const float pi = juce::MathConstants<float>::pi;
            juce::ColourGradient halo (ui::gold.withAlpha (0.45f + 0.3f * energy), p.x, p.y, ui::gold.withAlpha (0.0f), p.x, p.y - 30.0f - 12.0f * energy, true);
            g.setGradientFill (halo);
            g.fillEllipse (juce::Rectangle<float> (84.0f, 84.0f).withCentre (p));

            for (int i = 0; i < 3; ++i)
            {
                const float rr = 17.0f + 4.5f * (float) i;
                const float a0 = spin / (1.0f + 0.5f * (float) i) + (float) i * 2.1f;
                juce::Path arc;
                arc.addCentredArc (p.x, p.y, rr, rr * 0.92f, 0.0f, a0, a0 + pi * (1.1f - 0.2f * (float) i), true);
                g.setColour ((i == 0 ? ui::goldHot : ui::gold).withAlpha ((0.55f - 0.14f * (float) i) * (0.45f + 0.55f * energy)));
                g.strokePath (arc, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }

            g.setColour (juce::Colours::black);
            g.fillEllipse (juce::Rectangle<float> (25.0f, 25.0f).withCentre (p));
            g.setColour (dragging ? ui::goldHot : ui::gold);
            g.drawEllipse (juce::Rectangle<float> (25.0f, 25.0f).withCentre (p), 1.8f);
            juce::Path flare;
            flare.addCentredArc (p.x, p.y, 12.5f, 12.5f, 0.0f, -2.4f, -0.7f, true);
            g.setColour (ui::goldHot.withAlpha (0.9f));
            g.strokePath (flare, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    g.setColour (ui::gold.withAlpha (assignAxis >= 0 ? 0.8f : 0.35f));
    g.strokePath (outline, juce::PathStrokeType (1.0f));

    // ce que pilotent les deux axes
    const auto inner = getLocalBounds().reduced (10, 7);
    g.setFont (ui::font (12.5f));
    g.setColour (ui::axisY.withAlpha (assignAxis == 0 ? 0.35f : 1.0f));
    g.drawText ("Y   " + yLabel, inner, juce::Justification::topLeft, false);
    g.setColour (ui::axisX.withAlpha (assignAxis == 1 ? 0.35f : 1.0f));
    g.drawText (xLabel + "   X", inner, juce::Justification::bottomRight, false);

    if (assignAxis >= 0)
    {
        g.setColour (assignAxis == 0 ? ui::axisX : ui::axisY);
        g.drawText (U (assignAxis == 0 ? "Tourne les potards : tu règles ce que X pilote"
                                       : "Tourne les potards : tu règles ce que Y pilote"),
                    inner.withTrimmedTop (18), juce::Justification::centredTop, false);
    }
}

void Portal::moveTo (juce::Point<float> position)
{
    const auto f = field();
    const float x = juce::jlimit (0.0f, 1.0f, (position.x - f.getX()) / f.getWidth());
    const float y = juce::jlimit (0.0f, 1.0f, (f.getBottom() - position.y) / f.getHeight());

    if (auto* px = processor.parameter (P::padX))
        px->setValueNotifyingHost (x);

    if (auto* py = processor.parameter (P::padY))
        py->setValueNotifyingHost (y);
}

void Portal::mouseDown (const juce::MouseEvent& e)
{
    dragging = true;

    if (auto* px = processor.parameter (P::padX)) px->beginChangeGesture();
    if (auto* py = processor.parameter (P::padY)) py->beginChangeGesture();

    moveTo (e.position);
}

void Portal::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging)
        moveTo (e.position);
}

void Portal::mouseUp (const juce::MouseEvent&)
{
    if (dragging)
    {
        if (auto* px = processor.parameter (P::padX)) px->endChangeGesture();
        if (auto* py = processor.parameter (P::padY)) py->endChangeGesture();
        dragging = false;
    }
}

//==============================================================================
PulsarPanel::PulsarPanel (PulsarProcessor& p) : processor (p), portal (p)
{
    setSize (baseWidth, baseHeight);
    setOpaque (true);
    buildBackground();

    const juce::StringArray shortLengths { "1/16", "1/8", "1/4", "1/2", "1 mes.", "2 mes." };
    const juce::StringArray shortFilters { U ("Coupé"), U ("Bas"), U ("Bande"), U ("Haut") };
    const juce::StringArray shortDelays { "Slap", "1/16", "1/8 T", "1/8", "1/8 P", "1/4 T", "1/4", "1/4 P", "1/2" };

    //---------------------------------------------------------------- colonne gauche : la matière
    addModule ("Temps", 0, 0, 0, 5);
    addKnob (P::timeMix, "Dosage", "Mélange entre le son normal et le son passé dans l'effet de temps.", 0, 0, 0);
    addKnob (P::pump, "Pompe", "Le volume plonge à chaque temps puis remonte, comme une sidechain sur un kick.", 0, 0, 1);
    addComboCell (timeModeBox, P::timeMode, pulsar::choicesFor (P::timeMode), "Mode",
                  "Demi-vitesse = deux fois plus lent, une octave plus bas. Inversé = joué à l'envers. Répétition = le début du segment bégaie quatre fois.",
                  0, 0, 2, 2, 114);
    addComboCell (timeLenBox, P::timeLen, shortLengths, "Durée", "Longueur du segment traité, calée sur le tempo du projet.", 0, 0, 4, 1, 64);

    addModule ("Grain", 0, 1, 0, 5);
    addKnob (P::grainMix, "Dosage", "Mélange entre le son normal et le nuage de grains.", 0, 1, 0);
    addKnob (P::grainSize, "Taille", "Longueur de chaque grain. Court = bourdonnant et haché, long = nappe fluide.", 0, 1, 1);
    addKnob (P::grainDensity, "Densité", "Nombre de grains qui se chevauchent. Sous 1, des trous apparaissent entre les grains.", 0, 1, 2);
    addKnob (P::grainPitch, "Pitch", "Hauteur des grains en demi-tons. La durée ne change pas : c'est l'intérêt du granulaire.", 0, 1, 3);
    addKnob (P::grainSpray, "Dispersion", "Va chercher les grains de plus en plus loin dans le passé et les éparpille en stéréo.", 0, 1, 4);

    addModule ("Grain, hasard", 0, 2, 0, 2);
    addKnob (P::grainReverse, "Inversé", "Part des grains joués à l'envers.", 0, 2, 0);
    addKnob (P::grainShards, "Éclats", "Part des grains qui sautent à l'octave ou à la quinte : ça scintille.", 0, 2, 1);

    addModule ("Modulation", 0, 2, 2, 3);
    addKnob (P::modAmount, "Dosage", "Quantité de modulation.", 0, 2, 2);
    addKnob (P::modRate, "Vitesse", "Vitesse du balayage.", 0, 2, 3);
    addComboCell (modBox, P::modType, pulsar::choicesFor (P::modType), "Type",
                  "Chorus = épaissit et élargit. Flanger = balayage métallique. Phaser = creux qui se déplacent.", 0, 2, 4, 1, 64);

    addModule ("Usure", 0, 3, 0, 5);
    addKnob (P::wobble, "Wobble", "La hauteur ondule comme sur une bande ou un vinyle fatigué.", 0, 3, 0);
    addKnob (P::wobbleRate, "Vitesse", "Vitesse de l'ondulation du Wobble.", 0, 3, 1);
    addKnob (P::dropouts, "Pertes", "De temps en temps le son s'affaisse et perd ses aigus, comme une bande abîmée.", 0, 3, 2);
    addKnob (P::noise, "Bruit", "Bruit de fond. Il s'éteint tout seul quand plus rien ne joue.", 0, 3, 3);
    addComboCell (noiseBox, P::noiseType, pulsar::choicesFor (P::noiseType), "Type",
                  "Vinyle = craquements. Bande = souffle. Secteur = ronflette électrique.", 0, 3, 4, 1, 64);

    //---------------------------------------------------------------- centre : le portail
    addAndMakeVisible (portal);
    portal.setBounds (columnX[1], 78, columnW[1], 252);

    const auto setUpAssign = [this] (juce::ToggleButton& button, const char* text, const char* tip, int x, int axis)
    {
        button.setButtonText (U (text));
        button.setTooltip (U (tip));
        button.setBounds (x, 336, 124, 22);
        button.onClick = [this, &button, axis] { setAssigning (button.getToggleState() ? axis : -1); };
        addAndMakeVisible (button);
    };

    setUpAssign (assignX, "Assigner X", "Choisis ce que l'axe X du portail pilote : active, puis tourne les potards voulus. "
                                        "Double-clic sur un potard = le retirer. Désactive quand tu as fini.", columnX[1] + 4, 0);
    setUpAssign (assignY, "Assigner Y", "Choisis ce que l'axe Y du portail pilote : active, puis tourne les potards voulus. "
                                        "Double-clic sur un potard = le retirer. Désactive quand tu as fini.", columnX[1] + columnW[1] - 120, 1);

    addModule ("Orbite", 1, 2, 0, 5);
    addKnob (P::motionSize, "Taille", "Ampleur du mouvement automatique du point autour de l'endroit où tu l'as posé.", 1, 2, 0);
    addComboCell (shapeBox, P::motionShape, pulsar::choicesFor (P::motionShape), "Forme",
                  "Le point du portail bouge tout seul, calé sur le tempo. Dérive et Sauts tirent des positions au hasard, "
                  "mais toujours les mêmes à chaque lecture du morceau.", 1, 2, 1, 2, 118);
    addComboCell (rateBox, P::motionRate, pulsar::choicesFor (P::motionRate), "Durée",
                  "Temps d'un tour complet (ou temps entre deux positions pour Dérive et Sauts).", 1, 2, 3, 2, 118);

    addModule ("Chaleur", 1, 3, 0, 3);
    addKnob (P::drive, "Drive", "Saturation : ajoute du grain et des harmoniques.", 1, 3, 0);
    addKnob (P::crush, "Crush", "Écrase la qualité numérique : son de vieux sampler.", 1, 3, 1);
    addComboCell (driveBox, P::driveType, pulsar::choicesFor (P::driveType), "Type",
                  "Lampe = chaud. Bande = rond. Clip = agressif. Radio = étroit et sale.", 1, 3, 2, 1, 64);

    addModule ("Filtre", 1, 3, 3, 2);
    {
        const auto cell = cellBounds (1, 3, 3, 2);
        addCombo (filterBox, P::filterType, shortFilters, "Coupé = pas de filtre. Bas = garde les graves. Bande = garde le milieu. Haut = garde les aigus.",
                  { cell.getRight() - 78, rowY[3], 78, 22 });
    }
    addKnob (P::filterCutoff, "Coupure", "Fréquence du filtre. C'est le réglage le plus parlant à piloter avec le portail.", 1, 3, 3);
    addKnob (P::filterRes, "Résonance", "Accentue la fréquence de coupure : le filtre siffle et chante.", 1, 3, 4);

    //---------------------------------------------------------------- colonne droite : l'espace et la sortie
    addModule ("Style", 2, 0, 0, 5);
    infoArea = juce::Rectangle<int> (columnX[2], rowY[0], columnW[2], moduleHeight).withTrimmedTop (titleHeight + 4);

    addModule ("Écho", 2, 1, 0, 3);
    addKnob (P::dlyMix, "Dosage", "Niveau des échos (ping-pong gauche-droite).", 2, 1, 0);
    addKnob (P::dlyFeedback, "Feedback", "Nombre de répétitions.", 2, 1, 1);
    addComboCell (delayBox, P::dlyTime, shortDelays, "Temps", "Temps de l'écho, calé sur le tempo. T = triolet, P = pointée.", 2, 1, 2, 1, 64);

    addModule ("Espace", 2, 1, 3, 2);
    addKnob (P::revMix, "Dosage", "Niveau de la réverbe.", 2, 1, 3);
    addKnob (P::revSize, "Taille", "De la petite pièce (0,4 s) à l'espace qui ne retombe presque plus (18 s).", 2, 1, 4);

    addModule ("Finition", 2, 2, 0, 5);
    addKnob (P::glue, "Colle", "Compression sur trois bandes, dans les deux sens : tasse ce qui dépasse, remonte ce qui est faible. Dense et détaillé.", 2, 2, 0);
    addKnob (P::weight, "Poids", "Pousse ou allège les graves.", 2, 2, 1);
    addKnob (P::air, "Air", "Ouvre ou ferme le haut du spectre.", 2, 2, 2);
    addKnob (P::width, "Largeur", "0 % = mono, 100 % = inchangé, 200 % = très large. Les graves restent au centre.", 2, 2, 3);
    {
        auto cell = cellBounds (2, 2, 4);
        captions.push_back ({ U ("Réduction"), cell.removeFromBottom (17) });
        glueArea = cell.reduced (6, 0);
    }

    addModule ("Sortie", 2, 3, 0, 5);
    addKnob (P::inGain, "Entrée", "Gain d'entrée. Il n'est pas touché quand tu changes de style.", 2, 3, 0);
    addKnob (P::mix, "Mix", "Mélange entre le son d'origine et le son traité.", 2, 3, 1);
    addKnob (P::outGain, "Volume", "Volume de sortie. Un limiteur empêche de dépasser 0 dB.", 2, 3, 2);
    metersArea = cellBounds (2, 3, 3, 2).reduced (8, 14);

    //---------------------------------------------------------------- en-tête : univers et styles
    const auto& banks = pulsar::getBanks();

    for (int i = 0; i < (int) banks.size(); ++i)
    {
        auto* tab = bankTabs.add (new juce::TextButton (U (banks[(size_t) i].name)));
        tab->setClickingTogglesState (true);
        tab->setRadioGroupId (7101);
        tab->setTooltip (U (banks[(size_t) i].hint));
        const int w = (columnW[1] + 40) / (int) banks.size();
        tab->setBounds (columnX[1] - 20 + i * w, 14, w, 40);
        tab->onClick = [this, i]
        {
            if (i != shownBank)
                selectBank (i, true);
        };
        addAndMakeVisible (tab);
    }

    addAndMakeVisible (styleBox);
    styleBox.setBounds (columnX[2] + 34, 19, columnW[2] - 60, 30);
    styleBox.setTooltip (U ("Le style : une chaîne complète, avec ce que le portail pilote. Tu peux tout retoucher ensuite."));
    styleBox.onChange = [this]
    {
        const int index = styleBox.getSelectedItemIndex();

        if (index >= 0 && index != shownPreset)
        {
            processor.loadPreset (shownBank, index);
            shownPreset = index;
            refreshLabels();
            repaint (infoArea);
        }
    };

    addAndMakeVisible (previousStyle);
    addAndMakeVisible (nextStyle);
    previousStyle.setBounds (columnX[2] + 14, 28, 12, 12);
    nextStyle.setBounds (columnX[2] + columnW[2] - 14, 28, 12, 12);
    previousStyle.onClick = [this] { stepStyle (-1); };
    nextStyle.onClick = [this] { stepStyle (1); };

    selectBank (processor.getBank(), false);
    startTimerHz (30);
}

PulsarPanel::~PulsarPanel()
{
    stopTimer();
}

//==============================================================================
juce::Rectangle<int> PulsarPanel::cellBounds (int column, int row, int cell, int span) const
{
    const int w = columnW[column] / 5;
    return { columnX[column] + cell * w, rowY[row] + titleHeight, w * span, knobHeight };
}

void PulsarPanel::addModule (const char* title, int column, int row, int firstCell, int numCells)
{
    const int w = columnW[column] / 5;
    const bool last = firstCell + numCells == 5;
    modules.push_back ({ U (title), { columnX[column] + firstCell * w, rowY[row], last ? columnW[column] - firstCell * w : numCells * w - 12, moduleHeight } });
}

void PulsarPanel::addKnob (int param, const char* label, const char* tip, int column, int row, int cell)
{
    auto* knob = knobs.add (new ui::Knob (U (label), param, pz::specs()[(size_t) param].mod, *this));
    knob->setBounds (cellBounds (column, row, cell));
    knob->setTooltip (U (tip));
    addAndMakeVisible (knob);
    sliderAttachments.add (new SliderAttachment (processor.apvts, pz::specs()[(size_t) param].id, *knob));
}

void PulsarPanel::addCombo (juce::ComboBox& box, int param, const juce::StringArray& items, const char* tip, juce::Rectangle<int> bounds)
{
    box.addItemList (items, 1);
    box.setBounds (bounds);
    box.setTooltip (U (tip));
    addAndMakeVisible (box);
    comboAttachments.add (new ComboAttachment (processor.apvts, pz::specs()[(size_t) param].id, box));
}

void PulsarPanel::addComboCell (juce::ComboBox& box, int param, const juce::StringArray& items, const char* caption, const char* tip,
                                int column, int row, int cell, int span, int width)
{
    auto area = cellBounds (column, row, cell, span);
    captions.push_back ({ U (caption), area.removeFromBottom (17) });
    addCombo (box, param, items, tip, area.withSizeKeepingCentre (width, 26));
}

//==============================================================================
float PulsarPanel::getLive (int param) const
{
    return engineRunning ? processor.engine.live[(size_t) param].load (std::memory_order_relaxed) : -1.0f;
}

void PulsarPanel::setAssigning (int axis)
{
    assigning = axis;
    assignX.setToggleState (axis == 0, juce::dontSendNotification);
    assignY.setToggleState (axis == 1, juce::dontSendNotification);
    portal.setAssignAxis (axis);

    for (auto* knob : knobs)
        knob->refresh();
}

void PulsarPanel::refreshLabels()
{
    const auto& banks = pulsar::getBanks();

    if (shownBank >= 0 && shownBank < (int) banks.size() && shownPreset >= 0 && shownPreset < (int) banks[(size_t) shownBank].presets.size())
    {
        const auto& preset = banks[(size_t) shownBank].presets[(size_t) shownPreset];
        portal.setAxisLabels (U (preset.xLabel), U (preset.yLabel));
    }
}

void PulsarPanel::selectBank (int bankIndex, bool loadFirstStyle)
{
    const auto& banks = pulsar::getBanks();
    bankIndex = juce::jlimit (0, (int) banks.size() - 1, bankIndex);

    if (loadFirstStyle)
        processor.loadPreset (bankIndex, 0);

    shownBank = bankIndex;
    shownPreset = juce::jlimit (0, (int) banks[(size_t) bankIndex].presets.size() - 1, processor.getPreset());

    for (int i = 0; i < bankTabs.size(); ++i)
        bankTabs[i]->setToggleState (i == bankIndex, juce::dontSendNotification);

    styleBox.clear (juce::dontSendNotification);
    int itemId = 1;

    for (const auto& preset : banks[(size_t) bankIndex].presets)
        styleBox.addItem (U (preset.name), itemId++);

    styleBox.setSelectedItemIndex (shownPreset, juce::dontSendNotification);
    refreshLabels();
    repaint();
}

void PulsarPanel::stepStyle (int delta)
{
    const int count = styleBox.getNumItems();

    if (count > 0)
        styleBox.setSelectedItemIndex ((styleBox.getSelectedItemIndex() + delta + count) % count, juce::sendNotificationSync);
}

void PulsarPanel::timerCallback()
{
    // Le moteur tourne-t-il ? (sinon on affiche les potards et le point là où ils sont posés)
    const uint32_t beat = processor.heartbeat();
    silentTicks = beat != lastHeartbeat ? 0 : juce::jmin (1000, silentTicks + 1);
    lastHeartbeat = beat;
    engineRunning = silentTicks < 8;

    auto& engine = processor.engine;
    levelIn = juce::jmax (engine.meterIn.exchange (0.0f), levelIn * 0.84f);
    levelOut = juce::jmax (engine.meterOut.exchange (0.0f), levelOut * 0.84f);
    reduction = juce::jmax (engine.meterGlue.exchange (0.0f), reduction * 0.84f);

    portal.animate (levelToProportion (levelOut), engineRunning);
    repaint (metersArea);
    repaint (glueArea);

    for (auto* knob : knobs)
        knob->refresh();

    // L'hôte a rechargé un projet ou un état : on resynchronise les menus.
    if (processor.getBank() != shownBank || processor.getPreset() != shownPreset)
        selectBank (processor.getBank(), false);
}

//==============================================================================
void PulsarPanel::buildBackground()
{
    const int scale = 2;
    background = juce::Image (juce::Image::RGB, baseWidth * scale, baseHeight * scale, true);
    juce::Graphics g (background);
    g.addTransform (juce::AffineTransform::scale ((float) scale));
    g.fillAll (ui::space);

    // lueur chaude derrière le portail
    const juce::Point<float> centre ((float) columnX[1] + (float) columnW[1] * 0.5f, 204.0f);
    juce::ColourGradient glow (ui::gold.withAlpha (0.075f), centre.x, centre.y, ui::gold.withAlpha (0.0f), centre.x, centre.y + 440.0f, true);
    g.setGradientFill (glow);
    g.fillRect (0, 0, baseWidth, baseHeight);

    juce::ColourGradient dust (juce::Colour (0xff8a4a12).withAlpha (0.05f), 940.0f, 640.0f, juce::Colours::transparentBlack, 560.0f, 300.0f, true);
    g.setGradientFill (dust);
    g.fillRect (0, 0, baseWidth, baseHeight);

    // étoiles
    juce::Random random (20261007);

    for (int i = 0; i < 520; ++i)
    {
        const float x = random.nextFloat() * (float) baseWidth;
        const float y = random.nextFloat() * (float) baseHeight;
        const float size = 0.5f + 1.1f * std::pow (random.nextFloat(), 3.0f);
        const float alpha = 0.10f + 0.55f * std::pow (random.nextFloat(), 2.2f);
        g.setColour ((random.nextInt (5) == 0 ? ui::gold : ui::goldHot).withAlpha (alpha));
        g.fillEllipse (x, y, size, size);
    }
}

void PulsarPanel::paint (juce::Graphics& g)
{
    g.drawImage (background, getLocalBounds().toFloat());

    //---------------------------------------------------------------- en-tête
    {
        // logo : une étoile qui tourne et son faisceau
        const juce::Point<float> c (41.0f, 34.0f);
        juce::ColourGradient halo (ui::gold.withAlpha (0.35f), c.x, c.y, ui::gold.withAlpha (0.0f), c.x, c.y - 19.0f, true);
        g.setGradientFill (halo);
        g.fillEllipse (juce::Rectangle<float> (38.0f, 38.0f).withCentre (c));
        g.setColour (ui::goldHot.withAlpha (0.75f));
        g.drawLine (c.x - 17.0f, c.y + 9.0f, c.x + 17.0f, c.y - 9.0f, 1.3f);
        g.setColour (juce::Colours::black);
        g.fillEllipse (juce::Rectangle<float> (18.0f, 18.0f).withCentre (c));
        g.setColour (ui::gold);
        g.drawEllipse (juce::Rectangle<float> (18.0f, 18.0f).withCentre (c), 1.6f);
        g.setColour (ui::goldHot);
        g.fillEllipse (juce::Rectangle<float> (4.0f, 4.0f).withCentre (c));

        g.setFont (ui::font (24.0f, true).withExtraKerningFactor (0.26f));
        g.drawText ("PULSAR", 70, 14, 200, 40, juce::Justification::centredLeft, false);

        g.setColour (ui::gold.withAlpha (0.14f));
        g.fillRect (24, 66, baseWidth - 48, 1);
    }

    //---------------------------------------------------------------- modules
    for (const auto& m : modules)
    {
        g.setColour (ui::gold);
        g.setFont (ui::font (14.5f, true));
        g.drawText (m.title, m.bounds.getX(), m.bounds.getY(), m.bounds.getWidth(), 22, juce::Justification::centredLeft, false);
        g.setColour (ui::gold.withAlpha (0.14f));
        g.fillRect (m.bounds.getX(), m.bounds.getY() + 24, m.bounds.getWidth(), 1);
    }

    g.setFont (ui::font (12.5f));
    g.setColour (ui::textDim);

    for (const auto& c : captions)
        g.drawText (c.text, c.bounds, juce::Justification::centred, false);

    paintInfo (g);
    paintMeters (g);
}

void PulsarPanel::paintInfo (juce::Graphics& g)
{
    const auto& banks = pulsar::getBanks();

    if (shownBank < 0 || shownBank >= (int) banks.size())
        return;

    const auto& bank = banks[(size_t) shownBank];
    auto area = infoArea;

    if (shownPreset >= 0 && shownPreset < (int) bank.presets.size())
    {
        const auto& preset = bank.presets[(size_t) shownPreset];
        g.setColour (ui::text);
        g.setFont (ui::font (14.0f));
        g.drawFittedText (U (preset.hint), area.removeFromTop (36), juce::Justification::topLeft, 2, 1.0f);

        auto line = area.removeFromTop (20);
        g.setFont (ui::font (13.0f));
        g.setColour (ui::axisX);
        g.drawText (U ("X : ") + U (preset.xLabel), line.removeFromLeft (line.getWidth() / 2), juce::Justification::centredLeft, false);
        g.setColour (ui::axisY);
        g.drawText (U ("Y : ") + U (preset.yLabel), line, juce::Justification::centredLeft, false);
    }

    g.setColour (ui::textDim);
    g.setFont (ui::font (12.5f));
    g.drawFittedText (U (bank.hint), area, juce::Justification::topLeft, 1, 0.8f);
}

void PulsarPanel::paintMeters (juce::Graphics& g)
{
    const auto bar = [&g] (juce::Rectangle<float> track, float proportion, juce::Colour colour)
    {
        g.setColour (ui::gold.withAlpha (0.14f));
        g.fillRoundedRectangle (track, 2.0f);

        if (proportion > 0.005f)
        {
            g.setColour (colour);
            g.fillRoundedRectangle (track.withWidth (juce::jmax (4.0f, track.getWidth() * proportion)), 2.0f);
        }
    };

    auto area = metersArea;
    const int rowHeight = area.getHeight() / 2;
    g.setFont (ui::font (12.5f));

    const std::pair<const char*, float> rows[] = { { "Entrée", levelToProportion (levelIn) }, { "Sortie", levelToProportion (levelOut) } };

    for (int i = 0; i < 2; ++i)
    {
        auto line = area.removeFromTop (rowHeight);
        g.setColour (ui::textDim);
        g.drawText (U (rows[i].first), line.removeFromLeft (46), juce::Justification::centredLeft, false);
        bar (line.toFloat().withSizeKeepingCentre ((float) line.getWidth(), 4.0f), rows[i].second, i == 0 ? ui::gold.withAlpha (0.75f) : ui::goldHot);
    }

    bar (glueArea.toFloat().withSizeKeepingCentre ((float) glueArea.getWidth(), 4.0f), juce::jlimit (0.0f, 1.0f, reduction / 18.0f), ui::axisY);
}

//==============================================================================
PulsarEditor::PulsarEditor (PulsarProcessor& p) : juce::AudioProcessorEditor (&p), panel (p)
{
    setLookAndFeel (&look);
    addAndMakeVisible (panel);
    setResizable (true, true);
    setResizeLimits (795, 480, 1590, 960);
    getConstrainer()->setFixedAspectRatio ((double) PulsarPanel::baseWidth / (double) PulsarPanel::baseHeight);
    setSize (PulsarPanel::baseWidth, PulsarPanel::baseHeight);
}

PulsarEditor::~PulsarEditor()
{
    setLookAndFeel (nullptr);
}

void PulsarEditor::paint (juce::Graphics& g)
{
    g.fillAll (ui::space);
}

void PulsarEditor::resized()
{
    const float scale = (float) getWidth() / (float) PulsarPanel::baseWidth;
    panel.setTransform (juce::AffineTransform::scale (scale));
    panel.setBounds (0, 0, PulsarPanel::baseWidth, PulsarPanel::baseHeight);
}
