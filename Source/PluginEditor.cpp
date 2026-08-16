#include "PluginEditor.h"

#include "BinaryData.h"

#include <array>
#include <cmath>

namespace
{
constexpr int designWidth = 512;
constexpr int designHeight = 768;
constexpr float knobStartAngle = juce::MathConstants<float>::pi * 1.25f;
constexpr float knobEndAngle = juce::MathConstants<float>::pi * 2.75f;

juce::Font makeFont (const juce::Typeface::Ptr& typeface, float height)
{
    if (typeface != nullptr)
        return juce::Font (juce::FontOptions (typeface).withHeight (height));

    return juce::Font (juce::FontOptions (height));
}

juce::Rectangle<int> scaledBounds (juce::Component& component, float x, float y, float width, float height)
{
    const auto scale = juce::jmin (component.getWidth() / (float) designWidth,
                                  component.getHeight() / (float) designHeight);
    const auto contentWidth = designWidth * scale;
    const auto contentHeight = designHeight * scale;
    const auto offsetX = (component.getWidth() - contentWidth) * 0.5f;
    const auto offsetY = (component.getHeight() - contentHeight) * 0.5f;

    return juce::Rectangle<float> (offsetX + x * scale,
                                   offsetY + y * scale,
                                   width * scale,
                                   height * scale).getSmallestIntegerContainer();
}

juce::Image makeTransparentBrandMark()
{
    const auto source = juce::ImageCache::getFromMemory (BinaryData::moth_mark_png,
                                                          BinaryData::moth_mark_pngSize);
    if (! source.isValid())
        return {};

    juce::Image result (juce::Image::ARGB, source.getWidth(), source.getHeight(), true);
    juce::Image::BitmapData sourceData (source, juce::Image::BitmapData::readOnly);
    juce::Image::BitmapData resultData (result, juce::Image::BitmapData::writeOnly);

    for (int y = 0; y < source.getHeight(); ++y)
    {
        for (int x = 0; x < source.getWidth(); ++x)
        {
            const auto pixel = sourceData.getPixelColour (x, y);
            const auto brightness = juce::jmax (pixel.getFloatRed(), pixel.getFloatGreen(), pixel.getFloatBlue());
            const auto alpha = juce::jlimit (0.0f, 1.0f, (brightness - 0.18f) * 1.75f) * pixel.getFloatAlpha();
            resultData.setPixelColour (x, y, juce::Colour (0xffd1b477).withMultipliedAlpha (alpha));
        }
    }

    return result;
}
} // namespace

class MothBiteLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    MothBiteLookAndFeel()
        : knobFace (juce::Drawable::createFromImageData (BinaryData::knob_modern_svg,
                                                         BinaryData::knob_modern_svgSize)),
          knobPointer (juce::Drawable::createFromImageData (BinaryData::knob_modern_pointer_svg,
                                                            BinaryData::knob_modern_pointer_svgSize)),
          footswitch (juce::ImageCache::getFromMemory (BinaryData::footswitch_png,
                                                       BinaryData::footswitch_pngSize)),
          titleTypeface (juce::Typeface::createSystemTypefaceFor (BinaryData::oxanium_bold_ttf,
                                                                  BinaryData::oxanium_bold_ttfSize)),
          controlTypeface (juce::Typeface::createSystemTypefaceFor (BinaryData::barlow_condensed_medium_ttf,
                                                                    BinaryData::barlow_condensed_medium_ttfSize))
    {
        setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xffd2c4ad));
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0x5a090a0b));
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0x886f5130));
        setColour (juce::Label::textColourId, juce::Colour (0xffcbbda6));
    }

    juce::Font titleFont (float height) const { return makeFont (titleTypeface, height); }
    juce::Font controlFont (float height) const { return makeFont (controlTypeface, height); }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider& slider) override
    {
        const auto diameter = (float) juce::jmin (width, height) * 0.88f;
        const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height)
                                .withSizeKeepingCentre (diameter, diameter);

        juce::DropShadow (juce::Colours::black.withAlpha (0.72f), 10, { 0, 5 })
            .drawForRectangle (g, bounds.toNearestInt().reduced (6));

        if (knobFace != nullptr)
            knobFace->drawWithin (g, bounds, juce::RectanglePlacement::centred, 1.0f);

        if (slider.getComponentID() == "attack")
        {
            g.setFont (controlFont (10.5f));
            g.setColour (juce::Colour (0xff9a8260));
            for (int index = 0; index < 8; ++index)
            {
                const auto proportion = index / 7.0f;
                const auto angle = startAngle + proportion * (endAngle - startAngle);
                const auto radius = diameter * 0.55f;
                const auto centre = bounds.getCentre();
                const auto point = centre + juce::Point<float> (std::sin (angle) * radius,
                                                                 -std::cos (angle) * radius);
                g.drawText (juce::String (index + 1),
                            juce::Rectangle<float> (point.x - 8.0f, point.y - 7.0f, 16.0f, 14.0f),
                            juce::Justification::centred, false);
            }
        }

        if (knobPointer != nullptr)
        {
            const auto angle = startAngle + sliderPos * (endAngle - startAngle);
            juce::Graphics::ScopedSaveState saved (g);
            g.addTransform (juce::AffineTransform::rotation (angle, bounds.getCentreX(), bounds.getCentreY()));
            const auto fitToSvgViewBox = juce::AffineTransform::scale (bounds.getWidth() / 100.0f,
                                                                       bounds.getHeight() / 100.0f)
                                                 .translated (bounds.getX(), bounds.getY());
            knobPointer->draw (g, 1.0f, fitToSvgViewBox);
        }
    }

    juce::Label* createSliderTextBox (juce::Slider& slider) override
    {
        auto* label = juce::LookAndFeel_V4::createSliderTextBox (slider);
        label->setFont (controlFont (13.0f));
        label->setJustificationType (juce::Justification::centred);
        return label;
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                           bool highlighted, bool down) override
    {
        auto area = button.getLocalBounds().toFloat();
        const auto switchArea = area.removeFromTop (area.getHeight() - 27.0f).reduced (18.0f, 4.0f);
        const auto imageArea = down ? switchArea.translated (0.0f, 2.0f) : switchArea;

        if (footswitch.isValid())
            g.drawImageWithin (footswitch,
                               juce::roundToInt (imageArea.getX()), juce::roundToInt (imageArea.getY()),
                               juce::roundToInt (imageArea.getWidth()), juce::roundToInt (imageArea.getHeight()),
                               juce::RectanglePlacement::centred);

        const auto effectIsOn = ! button.getToggleState();
        const auto ledCentre = juce::Point<float> (area.getCentreX(), 4.0f);
        if (effectIsOn)
        {
            g.setColour (juce::Colour (0x55d92931));
            g.fillEllipse (juce::Rectangle<float> (20.0f, 20.0f).withCentre (ledCentre));
            g.setColour (juce::Colour (0xffec2932));
        }
        else
        {
            g.setColour (juce::Colour (0xff3c1819));
        }
        g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre (ledCentre));

        g.setFont (controlFont (13.0f));
        g.setColour (juce::Colour (highlighted ? 0xffead9bb : 0xffbcae98));
        g.drawText (button.getButtonText(), area.removeFromBottom (24.0f), juce::Justification::centred, false);
    }

private:
    std::unique_ptr<juce::Drawable> knobFace;
    std::unique_ptr<juce::Drawable> knobPointer;
    juce::Image footswitch;
    juce::Typeface::Ptr titleTypeface;
    juce::Typeface::Ptr controlTypeface;
};

MothBiteAudioProcessorEditor::MothBiteAudioProcessorEditor (MothBiteAudioProcessor& p)
    : AudioProcessorEditor (&p), lookAndFeel (std::make_unique<MothBiteLookAndFeel>()),
      brandMark (makeTransparentBrandMark())
{
    setLookAndFeel (lookAndFeel.get());
    setResizable (true, true);
    setResizeLimits (384, 576, 768, 1152);
    getConstrainer()->setFixedAspectRatio ((double) designWidth / (double) designHeight);

    titleLabel.setText ("MOTHBITE", juce::dontSendNotification);
    titleLabel.setFont (lookAndFeel->titleFont (35.0f));
    titleLabel.setColour (juce::Label::textColourId, juce::Colour (0xffd1b477));
    titleLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (titleLabel);

    subtitleLabel.setText ("DWARVEN HAMMER OVERDRIVE", juce::dontSendNotification);
    subtitleLabel.setFont (lookAndFeel->controlFont (13.0f));
    subtitleLabel.setColour (juce::Label::textColourId, juce::Colour (0xff827664));
    subtitleLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (subtitleLabel);

    setupRotarySlider (driveSlider, driveLabel, "DRIVE");
    setupRotarySlider (attackSlider, attackLabel, "ATTACK / MODE");
    setupRotarySlider (brightSlider, brightLabel, "BRIGHT");
    setupRotarySlider (volumeSlider, volumeLabel, "VOLUME");

    driveSlider.setRange (0.0, 1.0, 0.001);
    brightSlider.setRange (0.0, 1.0, 0.001);
    volumeSlider.setRange (0.0, 1.0, 0.001);

    attackSlider.setComponentID ("attack");
    attackSlider.setRange (0.0, 7.0, 1.0);
    attackSlider.setNumDecimalPlacesToDisplay (0);

    driveAttachment = std::make_unique<SliderAttachment> (p.apvts, "drive", driveSlider);
    attackAttachment = std::make_unique<SliderAttachment> (p.apvts, "attack", attackSlider);
    brightAttachment = std::make_unique<SliderAttachment> (p.apvts, "bright", brightSlider);
    volumeAttachment = std::make_unique<SliderAttachment> (p.apvts, "volume", volumeSlider);

    for (auto* slider : { &driveSlider, &brightSlider, &volumeSlider })
    {
        slider->textFromValueFunction = [] (double value) { return juce::String (juce::roundToInt (value * 100.0)) + "%"; };
        slider->updateText();
    }

    attackSlider.textFromValueFunction = [] (double value)
    {
        constexpr std::array<const char*, 8> capacitors { "33 nF", "47 nF", "68 nF", "82 nF", "100 nF", "220 nF", "330 nF", "470 nF" };
        const auto index = juce::jlimit (0, 7, juce::roundToInt (value));
        return juce::String (index + 1) + "  /  " + capacitors[(size_t) index];
    };
    attackSlider.updateText();

    bypassButton.setClickingTogglesState (true);
    bypassButton.setTooltip ("A teljes effekt ki- és bekapcsolása");
    bypassButton.onStateChange = [this] { updateBypassStatus(); };
    addAndMakeVisible (bypassButton);

    bypassStatusLabel.setFont (lookAndFeel->controlFont (11.0f));
    bypassStatusLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (bypassStatusLabel);

    bypassAttachment = std::make_unique<ButtonAttachment> (p.apvts, "bypass", bypassButton);
    updateBypassStatus();

    brandMarkTarget.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (brandMarkTarget);

#if JUCE_LAYOUT_TUNER
    layoutTuner = std::make_unique<juce_layout_tuner::Overlay> (
        *this, juce::Rectangle<int> (0, 0, designWidth, designHeight), 8);
    layoutTuner->addTarget (titleLabel, "titleLabel");
    layoutTuner->addTarget (subtitleLabel, "subtitleLabel");
    layoutTuner->addTarget (driveLabel, "driveLabel");
    layoutTuner->addTarget (driveSlider, "driveSlider");
    layoutTuner->addTarget (attackLabel, "attackLabel");
    layoutTuner->addTarget (attackSlider, "attackSlider");
    layoutTuner->addTarget (brightLabel, "brightLabel");
    layoutTuner->addTarget (brightSlider, "brightSlider");
    layoutTuner->addTarget (volumeLabel, "volumeLabel");
    layoutTuner->addTarget (volumeSlider, "volumeSlider");
    layoutTuner->addTarget (bypassButton, "bypassButton");
    layoutTuner->addTarget (bypassStatusLabel, "bypassStatusLabel");
    layoutTuner->addTarget (brandMarkTarget, "brandMarkTarget");
    addAndMakeVisible (*layoutTuner);
    layoutTuner->activate (true);
#endif

    setSize (designWidth, designHeight);
}

MothBiteAudioProcessorEditor::~MothBiteAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void MothBiteAudioProcessorEditor::setupRotarySlider (juce::Slider& slider, juce::Label& label,
                                                       const juce::String& text)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setRotaryParameters (knobStartAngle, knobEndAngle, true);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 112, 22);
    slider.setScrollWheelEnabled (false);
    addAndMakeVisible (slider);

    label.setText (text, juce::dontSendNotification);
    label.setFont (lookAndFeel->controlFont (15.0f));
    label.setColour (juce::Label::textColourId, juce::Colour (0xffc7b99e));
    label.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (label);
}

void MothBiteAudioProcessorEditor::updateBypassStatus()
{
    const auto bypassed = bypassButton.getToggleState();
    bypassStatusLabel.setText (bypassed ? "BYPASSED" : "EFFECT ACTIVE", juce::dontSendNotification);
    bypassStatusLabel.setColour (juce::Label::textColourId,
                                 bypassed ? juce::Colour (0xff695f53) : juce::Colour (0xffb94a43));
}

void MothBiteAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff050505));

    const auto chassis = juce::ImageCache::getFromMemory (BinaryData::pedal_png,
                                                          BinaryData::pedal_pngSize);
    if (chassis.isValid())
        g.drawImageWithin (chassis, 0, 0, getWidth(), getHeight(), juce::RectanglePlacement::centred);

    if (brandMark.isValid())
    {
        const auto bounds = brandMarkTarget.getBounds();
        g.setOpacity (0.76f);
        g.drawImageWithin (brandMark, bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight(),
                           juce::RectanglePlacement::centred);
        g.setOpacity (1.0f);
    }

    const auto scale = getWidth() / (float) designWidth;
    const auto lineY = 178.0f * scale;
    const auto lineLeft = 82.0f * scale;
    const auto lineRight = getWidth() - lineLeft;
    juce::ColourGradient divider (juce::Colour (0x001f1710), lineLeft, lineY,
                                  juce::Colour (0xff6e5433), getWidth() * 0.5f, lineY, false);
    divider.addColour (1.0, juce::Colour (0x001f1710));
    g.setGradientFill (divider);
    g.fillRect (juce::Rectangle<float> (lineLeft, lineY, lineRight - lineLeft, juce::jmax (1.0f, scale)));
}

void MothBiteAudioProcessorEditor::resized()
{
    titleLabel.setBounds (scaledBounds (*this, 90.0f, 76.0f, 332.0f, 43.0f));
    subtitleLabel.setBounds (scaledBounds (*this, 90.0f, 117.0f, 332.0f, 22.0f));
    brandMarkTarget.setBounds (scaledBounds (*this, 220.0f, 136.0f, 72.0f, 48.0f));

    driveLabel.setBounds (scaledBounds (*this, 65.0f, 192.0f, 170.0f, 24.0f));
    driveSlider.setBounds (scaledBounds (*this, 58.0f, 213.0f, 184.0f, 154.0f));
    attackLabel.setBounds (scaledBounds (*this, 277.0f, 192.0f, 170.0f, 24.0f));
    attackSlider.setBounds (scaledBounds (*this, 270.0f, 213.0f, 184.0f, 154.0f));

    brightLabel.setBounds (scaledBounds (*this, 65.0f, 375.0f, 170.0f, 24.0f));
    brightSlider.setBounds (scaledBounds (*this, 58.0f, 396.0f, 184.0f, 154.0f));
    volumeLabel.setBounds (scaledBounds (*this, 277.0f, 375.0f, 170.0f, 24.0f));
    volumeSlider.setBounds (scaledBounds (*this, 270.0f, 396.0f, 184.0f, 154.0f));

    bypassButton.setBounds (scaledBounds (*this, 193.0f, 560.0f, 126.0f, 110.0f));
    bypassStatusLabel.setBounds (scaledBounds (*this, 176.0f, 664.0f, 160.0f, 20.0f));

#if JUCE_LAYOUT_TUNER
    if (layoutTuner != nullptr)
    {
        layoutTuner->setBounds (getLocalBounds());
        layoutTuner->toFront (false);
    }
#endif
}
