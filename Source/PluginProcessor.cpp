#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>

namespace
{
    // Same audio-taper approximation used for the Drive pot (ClipperStage.cpp), applied here to
    // the Volume pot (A100K).
    float audioTaperGain (float x) noexcept
    {
        return (float) ((std::pow (10.0, (double) x) - 1.0) / 9.0);
    }
} // namespace

MothBiteAudioProcessor::MothBiteAudioProcessor()
    : AudioProcessor (BusesProperties()
                           .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                           .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout MothBiteAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterBool> ("bypass", "Bypass", false));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "drive", "Drive", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        "attack",
        "Attack",
        juce::StringArray { "1 (33n)", "2 (47n)", "3 (68n)", "4 (82n)", "5 (100n)", "6 (220n)", "7 (330n)", "8 (470n)" },
        3));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "bright", "Bright", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "volume", "Volume", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));

    return layout;
}

void MothBiteAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
        2, oversamplingFactorLog2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
    oversampling->initProcessing ((size_t) samplesPerBlock);
    setLatencySamples ((int) oversampling->getLatencyInSamples());

    const auto oversampledRate = sampleRate * (double) oversampling->getOversamplingFactor();

    for (auto& clipper : clippers)
        clipper.prepare (oversampledRate);

    for (auto& tone : toneStages)
        tone.prepare (sampleRate);
}

void MothBiteAudioProcessor::releaseResources()
{
}

bool MothBiteAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainOut = layouts.getMainOutputChannelSet();

    if (mainOut != juce::AudioChannelSet::mono() && mainOut != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == mainOut;
}

void MothBiteAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto value = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    if (value ("bypass") > 0.5f)
        return;

    const auto drive = value ("drive");
    const auto attackIndex = (int) value ("attack");
    const auto bright = value ("bright");
    const auto volumeGain = audioTaperGain (value ("volume"));

    const auto numChannels = buffer.getNumChannels();
    const auto numSamples = buffer.getNumSamples();

    juce::dsp::AudioBlock<float> block (buffer);
    auto oversampledBlock = oversampling->processSamplesUp (block);
    const auto numOversampledSamples = (int) oversampledBlock.getNumSamples();

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto& clipper = clippers[(size_t) ch];
        clipper.setDrive (drive);
        clipper.setAttack (attackIndex);
        clipper.process (oversampledBlock.getChannelPointer ((size_t) ch), numOversampledSamples);
    }

    oversampling->processSamplesDown (block);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto& tone = toneStages[(size_t) ch];
        tone.setBright (bright);

        auto* data = buffer.getWritePointer (ch);
        tone.process (data, numSamples);

        for (int i = 0; i < numSamples; ++i)
            data[i] *= volumeGain;
    }
}

juce::AudioProcessorEditor* MothBiteAudioProcessor::createEditor()
{
    return new MothBiteAudioProcessorEditor (*this);
}

void MothBiteAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState().createXml())
        copyXmlToBinary (*state, destData);
}

void MothBiteAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MothBiteAudioProcessor();
}
