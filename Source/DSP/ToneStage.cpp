#include "ToneStage.h"

namespace mothbite
{
namespace
{
    // Post-clip low-pass (R7 series, R3||C7 shunt to VREF-AC-ground).
    constexpr double kR7 = 1000.0;
    constexpr double kR3 = 10000.0;
    constexpr double kC7 = 220.0e-9;

    // Bright active high-shelf (IC1.2): R8 feedback resistor, C5+R12 series branch to ground,
    // tapped by the B5K linear-taper Bright pot.
    constexpr double kR8 = 3900.0;
    constexpr double kR12 = 220.0;
    constexpr double kC5 = 220.0e-9;
    constexpr double kBrightPotOhms = 5000.0;

    // Bilinear-transforms a first-order analog stage G0/(1+s/wc) into normalised digital
    // coefficients (b0, b1, a0=1, a1).
    juce::dsp::IIR::Coefficients<float>::Ptr makeFirstOrderLowShelf (double g0, double wc, double fs)
    {
        const double k = 2.0 * fs / wc;
        const double b0 = g0 / (1.0 + k);
        const double b1 = g0 / (1.0 + k);
        const double a1 = (1.0 - k) / (1.0 + k);
        return new juce::dsp::IIR::Coefficients<float> ((float) b0, (float) b1, 1.0f, (float) a1);
    }

    // Bilinear-transforms a first-order analog zero/pole pair (1+s/wz)/(1+s/wp) into normalised
    // digital coefficients (b0, b1, a0=1, a1).
    juce::dsp::IIR::Coefficients<float>::Ptr makeFirstOrderShelf (double wz, double wp, double fs)
    {
        const double kz = 2.0 * fs / wz;
        const double kp = 2.0 * fs / wp;
        const double b0 = (1.0 + kz) / (1.0 + kp);
        const double b1 = (1.0 - kz) / (1.0 + kp);
        const double a1 = (1.0 - kp) / (1.0 + kp);
        return new juce::dsp::IIR::Coefficients<float> ((float) b0, (float) b1, 1.0f, (float) a1);
    }
} // namespace

ToneStage::ToneStage() = default;

void ToneStage::prepare (double sampleRate)
{
    fs = sampleRate;
    updatePostClipFilter();
    updateBrightFilter();
    reset();
}

void ToneStage::reset()
{
    postClipFilter.reset();
    brightFilter.reset();
}

void ToneStage::updatePostClipFilter()
{
    const double g0 = kR3 / (kR3 + kR7);
    const double rShunt = (kR3 * kR7) / (kR3 + kR7);
    const double wc = 1.0 / (kC7 * rShunt);
    postClipFilter.coefficients = makeFirstOrderLowShelf (g0, wc, fs);
}

void ToneStage::updateBrightFilter()
{
    const double rB = (1.0 - (double) bright) * kBrightPotOhms;
    const double wz = 1.0 / (kC5 * (rB + kR12 + kR8));
    const double wp = 1.0 / (kC5 * (rB + kR12));
    brightFilter.coefficients = makeFirstOrderShelf (wz, wp, fs);
}

void ToneStage::setBright (float bright01) noexcept
{
    bright = bright01;
    updateBrightFilter();
}

float ToneStage::processSample (float x) noexcept
{
    return brightFilter.processSample (postClipFilter.processSample (x));
}

void ToneStage::process (float* samples, int numSamples) noexcept
{
    for (int i = 0; i < numSamples; ++i)
        samples[i] = processSample (samples[i]);
}
} // namespace mothbite
