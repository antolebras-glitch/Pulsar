// PULSAR - identité visuelle : noir et or, thème spatial (la même famille que Quasar).
// Un seul élément spectaculaire (le portail central), tout le reste reste discret.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "Params.h"

namespace ui
{
const juce::Colour space { 0xff040405 };       // noir du fond
const juce::Colour gold { 0xffe6b450 };        // or principal
const juce::Colour goldHot { 0xfffff1c9 };     // or presque blanc (lumière)
const juce::Colour goldDeep { 0xff7a5a1e };    // or sombre
const juce::Colour text { 0xffd8d1bf };        // texte courant
const juce::Colour textDim { 0xff8a8473 };     // texte secondaire
const juce::Colour axisX { 0xfffff1c9 };       // ce que pilote l'axe X du portail (or pâle)
const juce::Colour axisY { 0xffe8792b };       // ce que pilote l'axe Y du portail (ambre)

inline juce::Font font (float height, bool bold = false)
{
    // Bahnschrift est livrée avec Windows 10/11 ; ailleurs JUCE prend la police sans-serif par défaut.
    return juce::Font (juce::FontOptions ("Bahnschrift", height, bold ? juce::Font::bold : juce::Font::plain));
}

//==============================================================================
class Look final : public juce::LookAndFeel_V4
{
public:
    Look()
    {
        setColour (juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::ComboBox::textColourId, text);
        setColour (juce::ComboBox::outlineColourId, gold.withAlpha (0.35f));
        setColour (juce::ComboBox::arrowColourId, gold);
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff0c0c0d));
        setColour (juce::PopupMenu::textColourId, text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, gold.withAlpha (0.22f));
        setColour (juce::PopupMenu::highlightedTextColourId, goldHot);
        setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xff0c0c0d));
        setColour (juce::TooltipWindow::textColourId, text);
        setColour (juce::TooltipWindow::outlineColourId, gold.withAlpha (0.4f));
        setColour (juce::ToggleButton::textColourId, text);
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return font (13.5f); }
    juce::Font getPopupMenuFont() override { return font (14.0f); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return font (15.0f); }

    void drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box) override
    {
        const auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (bounds, 4.0f);
        g.setColour (gold.withAlpha (box.isMouseOver (true) ? 0.7f : 0.35f));
        g.drawRoundedRectangle (bounds, 4.0f, 1.0f);

        const float cx = (float) width - 10.5f, cy = (float) height * 0.5f;
        juce::Path chevron;
        chevron.startNewSubPath (cx - 3.5f, cy - 1.8f);
        chevron.lineTo (cx, cy + 2.2f);
        chevron.lineTo (cx + 3.5f, cy - 1.8f);
        g.setColour (gold);
        g.strokePath (chevron, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (7, 0, box.getWidth() - 23, box.getHeight());
        label.setFont (getComboBoxFont (box));
    }

    // Interrupteur : une petite pastille qui s'allume en or.
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool highlighted, bool) override
    {
        const bool on = button.getToggleState();
        const auto area = button.getLocalBounds().toFloat();
        const auto pill = juce::Rectangle<float> (0.0f, area.getCentreY() - 8.0f, 28.0f, 16.0f);

        g.setColour (on ? gold.withAlpha (0.25f) : juce::Colours::black.withAlpha (0.5f));
        g.fillRoundedRectangle (pill, 8.0f);
        g.setColour (gold.withAlpha (on ? 0.9f : (highlighted ? 0.6f : 0.35f)));
        g.drawRoundedRectangle (pill.reduced (0.5f), 7.5f, 1.0f);

        const float dotX = on ? pill.getRight() - 8.0f : pill.getX() + 8.0f;
        g.setColour (on ? goldHot : textDim);
        g.fillEllipse (dotX - 4.5f, pill.getCentreY() - 4.5f, 9.0f, 9.0f);

        g.setFont (font (13.0f));
        g.setColour (on ? text : textDim);
        g.drawText (button.getButtonText(), area.withTrimmedLeft (36.0f), juce::Justification::centredLeft, true);
    }

    // Onglets de mode : texte seul, souligné d'or quand il est actif.
    void drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&, bool highlighted, bool) override
    {
        const auto area = button.getLocalBounds().toFloat();

        if (button.getToggleState())
        {
            g.setColour (gold);
            g.fillRoundedRectangle (area.getCentreX() - 22.0f, area.getBottom() - 3.0f, 44.0f, 2.0f, 1.0f);
        }
        else if (highlighted)
        {
            g.setColour (gold.withAlpha (0.3f));
            g.fillRoundedRectangle (area.getCentreX() - 22.0f, area.getBottom() - 3.0f, 44.0f, 2.0f, 1.0f);
        }
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& button, bool highlighted, bool) override
    {
        g.setFont (font (16.0f, button.getToggleState()));
        g.setColour (button.getToggleState() ? goldHot : (highlighted ? text : textDim));
        g.drawText (button.getButtonText(), button.getLocalBounds().withTrimmedBottom (4), juce::Justification::centred, false);
    }
};

//==============================================================================
// Ce dont un potard a besoin pour dialoguer avec le portail.
struct PortalLink
{
    virtual ~PortalLink() = default;
    virtual int assignAxis() const = 0;                          // -1 = réglage normal, 0 = on assigne X, 1 = on assigne Y
    virtual float getDepth (int axis, int param) const = 0;      // -1..+1
    virtual void setDepth (int axis, int param, float depth) = 0;
    virtual float getLive (int param) const = 0;                 // position réelle (0..1), ou < 0 si le moteur ne tourne pas
};

//==============================================================================
// Potard "orbite" : une piste fine, un arc d'or pour la valeur posée, et un satellite qui montre
// la valeur réelle (il bouge quand le portail pilote le potard).
// Deux petits arcs intérieurs montrent jusqu'où X (or pâle) et Y (ambre) peuvent l'emmener.
// En mode "assigner", tourner le potard règle cette course au lieu de la valeur.
class Knob final : public juce::Slider
{
public:
    Knob (const juce::String& labelText, int paramIndex, bool canBeDriven, PortalLink& portalLink)
        : label (labelText), param (paramIndex), drivable (canBeDriven), link (portalLink)
    {
        setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        setMouseDragSensitivity (180);
    }

    // Appelé par l'éditeur ~30 fois par seconde : ne redessine que si quelque chose a bougé.
    void refresh()
    {
        const int axis = link.assignAxis();
        const float dx = drivable ? link.getDepth (0, param) : 0.0f;
        const float dy = drivable ? link.getDepth (1, param) : 0.0f;
        const float now = drivable ? link.getLive (param) : -1.0f;

        if (axis != shownAxis || std::abs (dx - shownX) > 0.0005f || std::abs (dy - shownY) > 0.0005f || std::abs (now - shownLive) > 0.002f)
        {
            shownAxis = axis;
            shownX = dx;
            shownY = dy;
            shownLive = now;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();
        const auto textArea = area.removeFromBottom (17.0f);
        const float radius = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f - 6.0f;
        const auto centre = area.getCentre().translated (0.0f, 1.0f);
        const auto rp = getRotaryParameters();
        const float span = rp.endAngleRadians - rp.startAngleRadians;
        const float base = (float) valueToProportionOfLength (getValue());
        const float live = shownLive >= 0.0f ? shownLive : juce::jlimit (0.0f, 1.0f, base);
        const auto angleOf = [&rp, span] (float proportion) { return rp.startAngleRadians + juce::jlimit (0.0f, 1.0f, proportion) * span; };
        const bool hover = isMouseOverOrDragging();
        const bool assigning = shownAxis >= 0;
        const float dim = assigning && ! drivable ? 0.3f : 1.0f;

        // disque
        juce::ColourGradient body (juce::Colour (0xff1d1b17).withAlpha (dim), centre.x, centre.y - radius * 0.5f,
                                   juce::Colour (0xff070707).withAlpha (dim), centre.x, centre.y + radius * 0.6f, false);
        g.setGradientFill (body);
        g.fillEllipse (juce::Rectangle<float> (radius * 1.04f, radius * 1.04f).withCentre (centre));

        // piste
        juce::Path track;
        track.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, rp.startAngleRadians, rp.endAngleRadians, true);
        g.setColour (gold.withAlpha (0.16f * dim));
        g.strokePath (track, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // arc de valeur (depuis le milieu pour les réglages +/-)
        const bool bipolar = getMinimum() < 0.0 && getMaximum() > 0.0;
        const float from = bipolar ? angleOf ((float) valueToProportionOfLength (0.0)) : rp.startAngleRadians;
        const float baseAngle = angleOf (base);

        if (std::abs (baseAngle - from) > 0.01f)
        {
            juce::Path arc;
            arc.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, juce::jmin (from, baseAngle), juce::jmax (from, baseAngle), true);
            g.setColour (gold.withAlpha ((hover ? 1.0f : 0.85f) * dim * (assigning ? 0.45f : 1.0f)));
            g.strokePath (arc, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // course donnée par le portail : X à l'intérieur, Y encore plus à l'intérieur
        const auto drawRange = [&] (float depth, float r, juce::Colour colour, bool emphasised)
        {
            if (std::abs (depth) < 0.004f)
                return;

            const float to = angleOf (base + depth);
            juce::Path arc;
            arc.addCentredArc (centre.x, centre.y, r, r, 0.0f, juce::jmin (baseAngle, to), juce::jmax (baseAngle, to), true);
            g.setColour (colour.withAlpha (emphasised ? 1.0f : (assigning ? 0.3f : 0.8f)));
            g.strokePath (arc, juce::PathStrokeType (emphasised ? 2.4f : 1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        };

        drawRange (shownX, radius - 4.6f, axisX, shownAxis == 0);
        drawRange (shownY, radius - 8.4f, axisY, shownAxis == 1);

        // repère de la valeur posée, quand le satellite s'en est éloigné
        const float liveAngle = angleOf (live);

        if (std::abs (liveAngle - baseAngle) > 0.03f)
        {
            const juce::Point<float> p0 (centre.x + (radius - 3.0f) * std::sin (baseAngle), centre.y - (radius - 3.0f) * std::cos (baseAngle));
            const juce::Point<float> p1 (centre.x + (radius + 3.0f) * std::sin (baseAngle), centre.y - (radius + 3.0f) * std::cos (baseAngle));
            g.setColour (gold.withAlpha (0.8f));
            g.drawLine ({ p0, p1 }, 1.4f);
        }

        // satellite = valeur réelle
        const juce::Point<float> sat (centre.x + radius * std::sin (liveAngle), centre.y - radius * std::cos (liveAngle));
        g.setColour (gold.withAlpha (0.22f * dim));
        g.fillEllipse (juce::Rectangle<float> (11.0f, 11.0f).withCentre (sat));
        g.setColour ((hover ? goldHot : gold).withAlpha (dim));
        g.fillEllipse (juce::Rectangle<float> (5.6f, 5.6f).withCentre (sat));

        // texte : nom, valeur au survol, ou course en mode "assigner"
        g.setFont (font (12.5f));
        juce::String shown = label;
        juce::Colour colour = textDim;

        if (assigning && drivable)
        {
            const float depth = shownAxis == 0 ? shownX : shownY;

            if (std::abs (depth) >= 0.004f || hover)
            {
                shown = juce::String (shownAxis == 0 ? "X " : "Y ") + (depth > 0.0f ? "+" : "") + juce::String (juce::roundToInt (depth * 100.0f)) + " %";
                colour = shownAxis == 0 ? axisX : axisY;
            }
        }
        else if (hover && ! assigning)
        {
            shown = getTextFromValue (getValue());
            colour = goldHot;
        }

        g.setColour (colour.withAlpha (dim));
        g.drawFittedText (shown, textArea.toNearestInt(), juce::Justification::centred, 1, 0.75f);
    }

    //---------------------------------------------------------------- souris
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (link.assignAxis() < 0)
        {
            juce::Slider::mouseDown (e);
            return;
        }

        dragStartDepth = drivable ? link.getDepth (link.assignAxis(), param) : 0.0f;
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        const int axis = link.assignAxis();

        if (axis < 0)
        {
            juce::Slider::mouseDrag (e);
            return;
        }

        if (drivable)
        {
            const float moved = (float) (e.getDistanceFromDragStartX() - e.getDistanceFromDragStartY()) / 160.0f;
            float depth = juce::jlimit (-1.0f, 1.0f, dragStartDepth + moved);

            if (std::abs (depth) < 0.02f)
                depth = 0.0f; // petit cran à zéro, pour retirer facilement une assignation

            link.setDepth (axis, param, depth);
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (link.assignAxis() < 0)
            juce::Slider::mouseUp (e);
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const int axis = link.assignAxis();

        if (axis < 0)
            juce::Slider::mouseDoubleClick (e);
        else if (drivable)
            link.setDepth (axis, param, 0.0f);
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
    {
        if (link.assignAxis() < 0)
            juce::Slider::mouseWheelMove (e, wheel);
    }

    void mouseEnter (const juce::MouseEvent& e) override
    {
        juce::Slider::mouseEnter (e);
        repaint();
    }

    void mouseExit (const juce::MouseEvent& e) override
    {
        juce::Slider::mouseExit (e);
        repaint();
    }

private:
    juce::String label;
    int param;
    bool drivable;
    PortalLink& link;
    int shownAxis = -1;
    float shownX = 0.0f, shownY = 0.0f, shownLive = -1.0f, dragStartDepth = 0.0f;
};

} // namespace ui
