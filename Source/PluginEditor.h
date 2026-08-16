#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"

#if JUCE_LAYOUT_TUNER
#include "Debug/JuceLayoutTuner.h"
#endif

class MothBiteLookAndFeel;

class MothBiteAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit MothBiteAudioProcessorEditor (MothBiteAudioProcessor&);
    ~MothBiteAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void setupRotarySlider (juce::Slider&, juce::Label&, const juce::String&);
    void updateBypassStatus();

    std::unique_ptr<MothBiteLookAndFeel> lookAndFeel;

    juce::Label titleLabel;
    juce::Label subtitleLabel;

    juce::Slider driveSlider;
    juce::Label driveLabel;
    std::unique_ptr<SliderAttachment> driveAttachment;

    juce::Slider attackSlider;
    juce::Label attackLabel;
    std::unique_ptr<SliderAttachment> attackAttachment;

    juce::Slider brightSlider;
    juce::Label brightLabel;
    std::unique_ptr<SliderAttachment> brightAttachment;

    juce::Slider volumeSlider;
    juce::Label volumeLabel;
    std::unique_ptr<SliderAttachment> volumeAttachment;

    juce::ToggleButton bypassButton { "BYPASS" };
    juce::Label bypassStatusLabel;
    std::unique_ptr<ButtonAttachment> bypassAttachment;

    juce::Component brandMarkTarget;
    juce::Image brandMark;

#if JUCE_LAYOUT_TUNER
    std::unique_ptr<juce_layout_tuner::Overlay> layoutTuner;
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MothBiteAudioProcessorEditor)
};
