// PULSAR - le processeur : fait le lien entre l'hôte (FL Studio) et le moteur audio.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Params.h"
#include "Presets.h"
#include "dsp/Engine.h"

class PulsarProcessor final : public juce::AudioProcessor
{
public:
    PulsarProcessor();
    ~PulsarProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Pulsar"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 20.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Charge un style : remet tout au neutre (sauf le gain d'entrée, qui dépend de ton sample),
    // puis applique ses réglages et ce que le portail pilote.
    void loadPreset (int bankIndex, int presetIndex);
    int getBank() const noexcept { return bank.load(); }
    int getPreset() const noexcept { return preset.load(); }

    // De combien le portail déplace un potard (axis 0 = X, 1 = Y), entre -1 et +1.
    float getDepth (int axis, int param) const noexcept { return (axis == 0 ? depthX : depthY)[(size_t) param].load (std::memory_order_relaxed); }
    void setDepth (int axis, int param, float depth);

    // Le moteur tourne-t-il en ce moment ? (FL Studio met les plugins en veille quand rien ne joue.)
    uint32_t heartbeat() const noexcept { return beats.load (std::memory_order_relaxed); }

    juce::RangedAudioParameter* parameter (int param) const noexcept { return params[(size_t) param]; }
    float baseNorm (int param) const noexcept;

    juce::AudioProcessorValueTreeState apvts;
    pz::Engine engine;

private:
    std::array<juce::RangedAudioParameter*, pz::numParams> params {};
    std::array<std::atomic<float>*, pz::numParams> raw {};
    std::array<std::atomic<float>, pz::numParams> depthX, depthY;
    std::atomic<int> bank { 0 }, preset { 0 };
    std::atomic<uint32_t> beats { 0 };
    juce::AudioBuffer<float> scratch;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PulsarProcessor)
};
