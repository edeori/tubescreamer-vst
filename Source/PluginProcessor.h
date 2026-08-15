#pragma once

#include <array>

#include <juce_audio_processors/juce_audio_processors.h>

#include "DSP/ClipperStage.h"
#include "DSP/ToneStage.h"

class MothBiteAudioProcessor final : public juce::AudioProcessor
{
public:
    MothBiteAudioProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override { return apvts.getParameter ("bypass"); }

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Fixed to mono/stereo (see isBusesLayoutSupported) so a plain array works - the WDF stages
    // hold internal cross-references between sibling members and must never be copied/reassigned.
    std::array<mothbite::ClipperStage, 2> clippers;
    std::array<mothbite::ToneStage, 2> toneStages;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MothBiteAudioProcessor)
};
