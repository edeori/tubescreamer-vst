#pragma once

#include <array>
#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

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

    // 4x oversampling around ClipperStage only (the hard diode nonlinearity) - ToneStage is
    // purely linear and doesn't need it. Measured to remove several dB of aliasing-driven excess
    // high-frequency energy at high drive that a plain single-rate WDF clipper introduces.
    static constexpr int oversamplingFactorLog2 = 2; // 2^2 = 4x
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MothBiteAudioProcessor)
};
