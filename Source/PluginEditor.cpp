#include "PluginEditor.h"

namespace
{
    void setupRotarySlider (juce::Slider& slider, juce::Label& label, const juce::String& text, juce::Component& parent)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 20);
        parent.addAndMakeVisible (slider);

        label.setText (text, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.attachToComponent (&slider, false);
        parent.addAndMakeVisible (label);
    }
} // namespace

MothBiteAudioProcessorEditor::MothBiteAudioProcessorEditor (MothBiteAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      driveAttachment (p.apvts, "drive", driveSlider),
      attackAttachment (p.apvts, "attack", attackBox),
      brightAttachment (p.apvts, "bright", brightSlider),
      volumeAttachment (p.apvts, "volume", volumeSlider),
      bypassAttachment (p.apvts, "bypass", bypassButton)
{
    titleLabel.setText ("MothBite", juce::dontSendNotification);
    titleLabel.setJustificationType (juce::Justification::centred);
    titleLabel.setFont (juce::Font (juce::FontOptions (24.0f, juce::Font::bold)));
    addAndMakeVisible (titleLabel);

    setupRotarySlider (driveSlider, driveLabel, "Drive", *this);
    setupRotarySlider (brightSlider, brightLabel, "Bright", *this);
    setupRotarySlider (volumeSlider, volumeLabel, "Volume", *this);

    attackBox.addItemList ({ "1 (33n)", "2 (47n)", "3 (68n)", "4 (82n)", "5 (100n)", "6 (220n)", "7 (330n)", "8 (470n)" }, 1);
    addAndMakeVisible (attackBox);

    attackLabel.setText ("Attack", juce::dontSendNotification);
    attackLabel.setJustificationType (juce::Justification::centred);
    attackLabel.attachToComponent (&attackBox, false);
    addAndMakeVisible (attackLabel);

    addAndMakeVisible (bypassButton);

    setSize (480, 260);
}

void MothBiteAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (16);

    titleLabel.setBounds (area.removeFromTop (32));
    area.removeFromTop (24); // room for the knob labels

    auto knobRow = area.removeFromTop (140);
    const auto knobWidth = knobRow.getWidth() / 4;

    driveSlider.setBounds (knobRow.removeFromLeft (knobWidth).reduced (8));
    attackBox.setBounds (knobRow.removeFromLeft (knobWidth).reduced (8).withHeight (24));
    brightSlider.setBounds (knobRow.removeFromLeft (knobWidth).reduced (8));
    volumeSlider.setBounds (knobRow.removeFromLeft (knobWidth).reduced (8));

    area.removeFromTop (16);
    bypassButton.setBounds (area.removeFromTop (24).withSizeKeepingCentre (100, 24));
}
