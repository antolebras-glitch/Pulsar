#include "PluginProcessor.h"

#include "PluginEditor.h"

PulsarProcessor::PulsarProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PULSAR", pulsar::createLayout())
{
    const auto& table = pz::specs();

    for (int p = 0; p < pz::numParams; ++p)
    {
        params[(size_t) p] = apvts.getParameter (table[(size_t) p].id);
        raw[(size_t) p] = apvts.getRawParameterValue (table[(size_t) p].id);
        depthX[(size_t) p].store (0.0f);
        depthY[(size_t) p].store (0.0f);
    }

    // À la première ouverture : Nuage / Poussière d'étoiles, pour entendre et voir quelque chose tout de suite.
    loadPreset (0, 0);
}

void PulsarProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate);
    setLatencySamples (pz::Engine::getLatency());
    scratch.setSize (2, juce::jmax (16, samplesPerBlock), false, true, true);
}

bool PulsarProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo()
           && (in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono());
}

float PulsarProcessor::baseNorm (int param) const noexcept
{
    return pz::specs()[(size_t) param].toNorm (raw[(size_t) param]->load (std::memory_order_relaxed));
}

void PulsarProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (numSamples == 0 || numChannels == 0)
        return;

    double bpm = 120.0, ppq = -1.0e9;
    bool playing = false;

    if (auto* head = getPlayHead())
    {
        if (const auto position = head->getPosition())
        {
            if (const auto b = position->getBpm())
                bpm = *b;

            if (const auto p = position->getPpqPosition())
                ppq = *p;

            playing = position->getIsPlaying();
        }
    }

    pz::Controls controls;

    for (int p = 0; p < pz::numParams; ++p)
    {
        controls.norm[(size_t) p] = baseNorm (p);
        controls.depthX[(size_t) p] = depthX[(size_t) p].load (std::memory_order_relaxed);
        controls.depthY[(size_t) p] = depthY[(size_t) p].load (std::memory_order_relaxed);
    }

    if (numChannels >= 2)
    {
        float* left = buffer.getWritePointer (0);
        float* right = buffer.getWritePointer (1);

        if (getTotalNumInputChannels() < 2)
            juce::FloatVectorOperations::copy (right, left, numSamples);

        engine.process (left, right, numSamples, controls, bpm, ppq, playing);

        for (int ch = 2; ch < numChannels; ++ch)
            buffer.clear (ch, 0, numSamples);
    }
    else
    {
        // Cas rare d'un hôte qui ne donne qu'un canal : on traite en stéréo interne et on resomme.
        if (scratch.getNumSamples() < numSamples)
            scratch.setSize (2, numSamples, false, true, true);

        float* mono = buffer.getWritePointer (0);
        float* left = scratch.getWritePointer (0);
        float* right = scratch.getWritePointer (1);
        juce::FloatVectorOperations::copy (left, mono, numSamples);
        juce::FloatVectorOperations::copy (right, mono, numSamples);
        engine.process (left, right, numSamples, controls, bpm, ppq, playing);

        for (int i = 0; i < numSamples; ++i)
            mono[i] = 0.5f * (left[i] + right[i]);
    }

    beats.fetch_add (1, std::memory_order_relaxed);
}

void PulsarProcessor::setDepth (int axis, int param, float depth)
{
    if (param < 0 || param >= pz::numParams || ! pz::specs()[(size_t) param].mod)
        return;

    (axis == 0 ? depthX : depthY)[(size_t) param].store (juce::jlimit (-1.0f, 1.0f, depth), std::memory_order_relaxed);
    updateHostDisplay (juce::AudioProcessorListener::ChangeDetails().withNonParameterStateChanged (true));
}

void PulsarProcessor::loadPreset (int bankIndex, int presetIndex)
{
    const auto& banks = pulsar::getBanks();
    bankIndex = juce::jlimit (0, (int) banks.size() - 1, bankIndex);
    const auto& list = banks[(size_t) bankIndex].presets;
    presetIndex = juce::jlimit (0, (int) list.size() - 1, presetIndex);

    const auto set = [this] (int param, float normalised)
    {
        auto* p = params[(size_t) param];

        if (p != nullptr && std::abs (p->getValue() - normalised) > 1.0e-6f)
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (normalised);
            p->endChangeGesture();
        }
    };

    // 1. Tout au neutre, sauf le gain d'entrée ; le portail ne pilote plus rien.
    for (int p = 0; p < pz::numParams; ++p)
    {
        if (p != pz::P::inGain && params[(size_t) p] != nullptr)
            set (p, params[(size_t) p]->getDefaultValue());

        depthX[(size_t) p].store (0.0f, std::memory_order_relaxed);
        depthY[(size_t) p].store (0.0f, std::memory_order_relaxed);
    }

    // 2. Les réglages du style.
    for (const auto& s : list[(size_t) presetIndex].settings)
    {
        if (auto* p = params[(size_t) s.param])
            set (s.param, p->convertTo0to1 (s.value));

        if (pz::specs()[(size_t) s.param].mod)
        {
            depthX[(size_t) s.param].store (juce::jlimit (-1.0f, 1.0f, s.x * 0.01f), std::memory_order_relaxed);
            depthY[(size_t) s.param].store (juce::jlimit (-1.0f, 1.0f, s.y * 0.01f), std::memory_order_relaxed);
        }
    }

    bank.store (bankIndex);
    preset.store (presetIndex);
    updateHostDisplay (juce::AudioProcessorListener::ChangeDetails().withNonParameterStateChanged (true));
}

void PulsarProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("bank", bank.load(), nullptr);
    state.setProperty ("preset", preset.load(), nullptr);

    // Ce que le portail pilote : "x_<réglage>" et "y_<réglage>".
    for (int p = 0; p < pz::numParams; ++p)
    {
        const auto& spec = pz::specs()[(size_t) p];

        if (spec.mod)
        {
            state.setProperty ("x_" + juce::String (spec.id), (double) depthX[(size_t) p].load(), nullptr);
            state.setProperty ("y_" + juce::String (spec.id), (double) depthY[(size_t) p].load(), nullptr);
        }
    }

    if (const auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void PulsarProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (const auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        const auto state = juce::ValueTree::fromXml (*xml);

        if (state.isValid() && state.hasType (apvts.state.getType()))
        {
            bank.store ((int) state.getProperty ("bank", 0));
            preset.store ((int) state.getProperty ("preset", 0));

            for (int p = 0; p < pz::numParams; ++p)
            {
                const auto& spec = pz::specs()[(size_t) p];

                if (spec.mod)
                {
                    depthX[(size_t) p].store (juce::jlimit (-1.0f, 1.0f, (float) (double) state.getProperty ("x_" + juce::String (spec.id), 0.0)));
                    depthY[(size_t) p].store (juce::jlimit (-1.0f, 1.0f, (float) (double) state.getProperty ("y_" + juce::String (spec.id), 0.0)));
                }
            }

            apvts.replaceState (state);
        }
    }
}

juce::AudioProcessorEditor* PulsarProcessor::createEditor()
{
    return new PulsarEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PulsarProcessor();
}
