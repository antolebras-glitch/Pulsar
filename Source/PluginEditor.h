// PULSAR - l'interface.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "Look.h"
#include "PluginProcessor.h"

//==============================================================================
// Le portail : un pad X/Y. Le point (un petit trou noir) déforme la grille autour de lui.
// On le déplace à la souris ; si une orbite est choisie, il tourne tout seul autour de l'endroit où on l'a posé.
class Portal final : public juce::Component,
                     public juce::SettableTooltipClient
{
public:
    explicit Portal (PulsarProcessor&);

    void setAxisLabels (const juce::String& x, const juce::String& y);
    void setAssignAxis (int axis);
    void animate (float level, bool engineRunning); // ~30 fois par seconde

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> field() const;
    juce::Point<float> toScreen (float x, float y) const;
    void moveTo (juce::Point<float> position);

    PulsarProcessor& processor;
    juce::String xLabel, yLabel;
    int assignAxis = -1;
    float puckX = 0.0f, puckY = 0.0f, baseX = 0.0f, baseY = 0.0f, level = 0.0f, spin = 0.0f;
    int shape = 0;
    float orbitSize = 0.0f;
    std::array<juce::Point<float>, 18> trail {};
    int trailCount = 0;
    bool dragging = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Portal)
};

//==============================================================================
// Tout le contenu est dessiné sur une surface de 1060 x 640, que l'éditeur agrandit ou réduit d'un bloc.
class PulsarPanel final : public juce::Component,
                          public ui::PortalLink,
                          private juce::Timer
{
public:
    static constexpr int baseWidth = 1060, baseHeight = 640;

    explicit PulsarPanel (PulsarProcessor&);
    ~PulsarPanel() override;

    void paint (juce::Graphics&) override;

    // ui::PortalLink
    int assignAxis() const override { return assigning; }
    float getDepth (int axis, int param) const override { return processor.getDepth (axis, param); }
    void setDepth (int axis, int param, float depth) override { processor.setDepth (axis, param, depth); }
    float getLive (int param) const override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    struct Module
    {
        juce::String title;
        juce::Rectangle<int> bounds;
    };

    struct Caption
    {
        juce::String text;
        juce::Rectangle<int> bounds;
    };

    void timerCallback() override;

    juce::Rectangle<int> cellBounds (int column, int row, int cell, int span = 1) const;
    void addModule (const char* title, int column, int row, int firstCell, int numCells);
    void addKnob (int param, const char* label, const char* tip, int column, int row, int cell);
    void addCombo (juce::ComboBox& box, int param, const juce::StringArray& items, const char* tip, juce::Rectangle<int> bounds);
    void addComboCell (juce::ComboBox& box, int param, const juce::StringArray& items, const char* caption, const char* tip,
                       int column, int row, int cell, int span, int width);

    void buildBackground();
    void selectBank (int bankIndex, bool loadFirstStyle);
    void stepStyle (int delta);
    void setAssigning (int axis);
    void refreshLabels();
    void paintMeters (juce::Graphics&);
    void paintInfo (juce::Graphics&);

    PulsarProcessor& processor;
    juce::Image background;

    std::vector<Module> modules;
    std::vector<Caption> captions;
    juce::OwnedArray<ui::Knob> knobs;
    juce::OwnedArray<SliderAttachment> sliderAttachments;
    juce::OwnedArray<ComboAttachment> comboAttachments;

    Portal portal;
    juce::ToggleButton assignX, assignY;
    juce::ComboBox timeModeBox, timeLenBox, noiseBox, driveBox, filterBox, modBox, delayBox, shapeBox, rateBox, styleBox;
    juce::OwnedArray<juce::TextButton> bankTabs;
    juce::ArrowButton previousStyle { "previous", 0.5f, ui::gold.withAlpha (0.75f) }, nextStyle { "next", 0.0f, ui::gold.withAlpha (0.75f) };

    juce::Rectangle<int> metersArea, glueArea, infoArea;
    int shownBank = -1, shownPreset = -1, assigning = -1;
    float levelIn = 0.0f, levelOut = 0.0f, reduction = 0.0f;
    uint32_t lastHeartbeat = 0;
    int silentTicks = 100;
    bool engineRunning = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PulsarPanel)
};

//==============================================================================
class PulsarEditor final : public juce::AudioProcessorEditor
{
public:
    explicit PulsarEditor (PulsarProcessor&);
    ~PulsarEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    ui::Look look;
    PulsarPanel panel;
    juce::TooltipWindow tooltips { this, 600 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PulsarEditor)
};
