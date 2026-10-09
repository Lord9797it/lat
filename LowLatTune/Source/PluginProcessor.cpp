#include "PluginProcessor.h"

static const char* kNoteNames[12] = { "C", "C#", "D", "D#", "E", "F",
                                      "F#", "G", "G#", "A", "A#", "B" };

juce::AudioProcessorValueTreeState::ParameterLayout PitchProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "retune", 1 }, "Retune Speed (ms)",
        NormalisableRange<float> (0.0f, 200.0f, 0.1f, 0.5f), 0.0f));

    for (int i = 0; i < 12; ++i)
        params.push_back (std::make_unique<AudioParameterBool> (
            ParameterID { "note" + String (i), 1 },
            String ("Note ") + kNoteNames[i], true));

    return { params.begin(), params.end() };
}

PitchProcessor::PitchProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    retuneParam = apvts.getRawParameterValue ("retune");
    for (int i = 0; i < 12; ++i)
        noteParams[(size_t) i] = apvts.getRawParameterValue ("note" + juce::String (i));
}

void PitchProcessor::prepareToPlay (double sampleRate, int)
{
    for (auto& t : tuners)
        t.prepare (sampleRate, 70.0f, 32);
}

bool PitchProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void PitchProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int nCh = juce::jmin (numIn, (int) tuners.size());

    std::array<bool, 12> mask {};
    bool any = false;
    for (size_t i = 0; i < 12; ++i)
    {
        mask[i] = noteParams[i]->load() > 0.5f;
        any = any || mask[i];
    }
    if (! any) mask.fill (true);   // scala vuota = cromatico

    const float retune = retuneParam->load();

    for (int ch = 0; ch < nCh; ++ch)
    {
        auto& t = tuners[(size_t) ch];
        t.setScale (mask);
        t.setRetuneMs (retune);
        float* d = buffer.getWritePointer (ch);
        for (int s = 0; s < numSamples; ++s)
            d[s] = t.process (d[s]);
    }

    for (int ch = nCh; ch < numOut; ++ch)
        buffer.clear (ch, 0, numSamples);
}

void PitchProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void PitchProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PitchProcessor();
}
