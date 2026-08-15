#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"

class MothBiteAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit MothBiteAudioProcessorEditor (MothBiteAudioProcessor&);

    void resized() override;

private:
    MothBiteAudioProcessor& processor;

    juce::Label titleLabel;

    juce::Slider driveSlider;
    juce::Label driveLabel;
    juce::AudioProcessorValueTreeState::SliderAttachment driveAttachment;

    juce::ComboBox attackBox;
    juce::Label attackLabel;
    juce::AudioProcessorValueTreeState::ComboBoxAttachment attackAttachment;

    juce::Slider brightSlider;
    juce::Label brightLabel;
    juce::AudioProcessorValueTreeState::SliderAttachment brightAttachment;

    juce::Slider volumeSlider;
    juce::Label volumeLabel;
    juce::AudioProcessorValueTreeState::SliderAttachment volumeAttachment;

    juce::ToggleButton bypassButton { "Bypass" };
    juce::AudioProcessorValueTreeState::ButtonAttachment bypassAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MothBiteAudioProcessorEditor)
};
