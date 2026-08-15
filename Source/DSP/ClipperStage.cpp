#include "ClipperStage.h"

#include <algorithm>
#include <cmath>

namespace mothbite
{
namespace
{
    // R15 (fixed) + Drive pot (A500K, wired as a rheostat: 0..500k added in series).
    constexpr double kFeedbackFixedOhms = 10000.0;
    constexpr double kDrivePotMaxOhms = 500000.0;

    // C9 (fixed, always in parallel) + the ATTACK rotary switch's 8 selectable capacitors.
    constexpr double kShuntFixedFarads = 120.0e-12;
    constexpr double kAttackCapsFarads[8] = { 33.0e-9, 47.0e-9, 68.0e-9, 82.0e-9, 100.0e-9, 220.0e-9, 330.0e-9, 470.0e-9 };

    // Approximates a logarithmic (audio-taper) potentiometer's electrical response to a linear
    // 0..1 rotation fraction. A reasonable starting point; refinable later against a measured pot.
    double audioTaper (double x) noexcept
    {
        return (std::pow (10.0, x) - 1.0) / 9.0;
    }
} // namespace

ClipperStage::ClipperStage() = default;

void ClipperStage::prepare (double sampleRate)
{
    Cg.prepare (sampleRate);
    Cf.prepare (sampleRate);
    reset();
}

void ClipperStage::reset()
{
    Cg.reset();
    Cf.reset();
}

double ClipperStage::driveIndexToFeedbackOhms (float drive01) noexcept
{
    return kFeedbackFixedOhms + kDrivePotMaxOhms * audioTaper ((double) drive01);
}

double ClipperStage::attackIndexToShuntFarads (int index) noexcept
{
    const auto clamped = std::min (7, std::max (0, index));
    return kShuntFixedFarads + kAttackCapsFarads[(size_t) clamped];
}

void ClipperStage::setDrive (float drive01) noexcept
{
    Rf.setResistanceValue (driveIndexToFeedbackOhms (drive01));
}

void ClipperStage::setAttack (int index) noexcept
{
    Cg.setCapacitanceValue (attackIndexToShuntFarads (index));
}

float ClipperStage::processSample (float vinSample) noexcept
{
    const double vin = (double) vinSample;

    // Step 1: solve the Zg branch given the known (pinned) V- voltage, yielding the current
    // that must, by KCL, flow entirely through the feedback network Zf.
    vinSource.setVoltage (vin);
    vinSource.incident (Zg.reflected());
    Zg.incident (vinSource.reflected());
    const double ig = chowdsp::wdft::current<double> (Zg);

    // Step 2: drive that exact current into the diode-terminated feedback network and read the
    // resulting voltage across it.
    igInjector.setCurrent (-ig);
    diodes.incident (Zf.reflected());
    Zf.incident (diodes.reflected());
    const double vzf = chowdsp::wdft::voltage<double> (Zf);

    const double vout = vin + vzf;
    return (float) vout;
}

void ClipperStage::process (float* samples, int numSamples) noexcept
{
    for (int i = 0; i < numSamples; ++i)
        samples[i] = processSample (samples[i]);
}
} // namespace mothbite
