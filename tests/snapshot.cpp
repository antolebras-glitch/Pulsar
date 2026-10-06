// Outil de développement : ouvre l'interface hors écran, fait passer du son dans le plugin
// et enregistre une image PNG. Usage : PulsarSnapshot sortie.png [univers] [style] [echelle] [assigner: 0 = X, 1 = Y]
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginEditor.h"
#include "PluginProcessor.h"

struct Runner final : private juce::Timer
{
    Runner (const juce::String& path, int bank, int style, float scale, int assign) : outPath (path)
    {
        processor.setPlayConfigDetails (2, 2, 48000.0, 512);
        processor.prepareToPlay (48000.0, 512);
        processor.loadPreset (bank, style);
        editor.reset (processor.createEditor());
        editor->setSize (juce::roundToInt (1060.0f * scale), juce::roundToInt (640.0f * scale));

        if (assign >= 0)
            clickToggle (*editor, assign == 0 ? "Assigner X" : "Assigner Y");

        startTimerHz (30);
    }

    static void clickToggle (juce::Component& parent, const juce::String& text)
    {
        for (auto* child : parent.getChildren())
        {
            if (auto* toggle = dynamic_cast<juce::ToggleButton*> (child))
                if (toggle->getButtonText() == text)
                    toggle->triggerClick();

            clickToggle (*child, text);
        }
    }

    void timerCallback() override
    {
        // un accord en dents de scie qui s'éteint, pour faire vivre le portail et les indicateurs
        juce::AudioBuffer<float> buffer (2, 512);
        juce::MidiBuffer midi;

        for (int block = 0; block < 3; ++block)
        {
            for (int i = 0; i < 512; ++i)
            {
                time += 1.0 / 48000.0;
                const double env = std::exp (-3.0 * std::fmod (time, 0.5));
                double s = 0.0;

                for (double f : { 220.0, 277.18, 329.63 })
                    s += 2.0 * std::fmod (time * f, 1.0) - 1.0;

                buffer.setSample (0, i, (float) (0.12 * env * s));
                buffer.setSample (1, i, (float) (0.12 * env * s));
            }

            processor.processBlock (buffer, midi);
        }

        if (++ticks < 60)
            return;

        stopTimer();
        const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
        juce::File file = juce::File::getCurrentWorkingDirectory().getChildFile (outPath);
        file.deleteFile();
        juce::FileOutputStream stream (file);
        juce::PNGImageFormat png;
        ok = stream.openedOk() && png.writeImageToStream (image, stream);
        std::printf ("%s -> %s (%d x %d), latence %d\n", ok ? "ok" : "ECHEC", file.getFullPathName().toRawUTF8(),
                     image.getWidth(), image.getHeight(), processor.getLatencySamples());
        juce::MessageManager::getInstance()->stopDispatchLoop();
    }

    PulsarProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor;
    juce::String outPath;
    double time = 0.0;
    int ticks = 0;
    bool ok = false;
};

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    bool ok = false;

    {
        Runner runner (argc > 1 ? argv[1] : "pulsar.png", argc > 2 ? std::atoi (argv[2]) : 0, argc > 3 ? std::atoi (argv[3]) : 0,
                       argc > 4 ? (float) std::atof (argv[4]) : 1.0f, argc > 5 ? std::atoi (argv[5]) : -1);
        juce::MessageManager::getInstance()->runDispatchLoop();
        ok = runner.ok;
    }

    return ok ? 0 : 1;
}
