#pragma once

#include <juce_dsp/juce_dsp.h>

namespace mothbite
{
/**
 * Linear post-clip filtering: the passive R7/C7 low-pass that follows the clipper op-amp,
 * chained into the IC1.2 "Bright" active high-shelf stage. Both are simple first-order analog
 * networks, translated to digital biquads via the bilinear transform - no nonlinearity here,
 * so plain juce::dsp::IIR filters are used rather than a WDF model.
 */
class ToneStage
{
public:
    ToneStage();

    void prepare (double sampleRate);
    void reset();

    /** @param bright01: Bright pot position, 0..1 (0 = minimum boost, 1 = maximum boost). */
    void setBright (float bright01) noexcept;

    float processSample (float x) noexcept;
    void process (float* samples, int numSamples) noexcept;

private:
    void updatePostClipFilter();
    void updateBrightFilter();

    double fs = 48000.0;
    float bright = 0.0f;

    juce::dsp::IIR::Filter<float> postClipFilter;
    juce::dsp::IIR::Filter<float> brightFilter;
};
} // namespace mothbite
