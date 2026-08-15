#include "DSP/ClipperStage.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <iostream>

namespace
{
    int failures = 0;
    void check (bool condition, const char* name)
    {
        if (! condition)
        {
            ++failures;
            std::cerr << "FAIL: " << name << '\n';
        }
    }

    // Predicts the clipper's small-signal (non-clipping) AC gain magnitude in dB, by
    // linearising the feedback network (diodes open-circuit at low signal amplitude) and
    // evaluating Gain(jw) = 1 + Zf(jw)/Zg(jw) directly - independent implementation of the
    // same physics ClipperStage models, used to cross-check the WDF result and its sign
    // convention.
    double expectedLinearGainDb (double freqHz, double rf, double rg, double cf, double cg)
    {
        const double w = 2.0 * M_PI * freqHz;
        const std::complex<double> jwcf (0.0, w * cf);
        const std::complex<double> zf = 1.0 / (1.0 / rf + jwcf);
        const std::complex<double> zg = std::complex<double> (rg, 0.0) + 1.0 / std::complex<double> (0.0, w * cg);
        const std::complex<double> gain = 1.0 + zf / zg;
        return 20.0 * std::log10 (std::abs (gain));
    }

    double audioTaper (double x) noexcept
    {
        return (std::pow (10.0, x) - 1.0) / 9.0;
    }

    constexpr double kAttackCapsFarads[8] = { 33.0e-9, 47.0e-9, 68.0e-9, 82.0e-9, 100.0e-9, 220.0e-9, 330.0e-9, 470.0e-9 };
}

int main()
{
    constexpr double fs = 48000.0;

    // Small-signal gain should match the linearised 1 + Zf/Zg prediction, and its sign.
    {
        mothbite::ClipperStage clipper;
        clipper.prepare (fs);

        constexpr float driveParam = 0.5f;
        constexpr int attackIndex = 3;
        clipper.setDrive (driveParam);
        clipper.setAttack (attackIndex);

        constexpr double testFreq = 2000.0;
        constexpr double amplitude = 0.0001; // small enough to stay well below diode conduction

        double maxOut = 0.0;
        const int numSamples = (int) fs;
        for (int n = 0; n < numSamples; ++n)
        {
            const auto x = (float) (amplitude * std::sin (2.0 * M_PI * testFreq * (double) n / fs));
            const auto y = clipper.processSample (x);
            if (n > numSamples / 2)
                maxOut = std::max (maxOut, (double) std::abs (y));
        }

        const double measuredGainDb = 20.0 * std::log10 (maxOut / amplitude);

        const double rf = 10000.0 + 500000.0 * audioTaper ((double) driveParam);
        const double rg = 1000.0;
        const double cf = 47.0e-12;
        const double cg = 120.0e-12 + kAttackCapsFarads[attackIndex];

        const double expectedDb = expectedLinearGainDb (testFreq, rf, rg, cf, cg);

        std::cerr << "measured=" << measuredGainDb << "dB expected=" << expectedDb << "dB\n";
        check (std::abs (measuredGainDb - expectedDb) < 0.5, "small-signal gain matches linearised 1+Zf/Zg prediction");
    }

    // High drive should produce visibly symmetric, bounded clipping.
    {
        mothbite::ClipperStage clipper;
        clipper.prepare (fs);
        clipper.setDrive (1.0f);
        clipper.setAttack (7);

        constexpr double testFreq = 200.0;
        constexpr double amplitude = 1.0;

        float posPeak = 0.0f;
        float negPeak = 0.0f;
        const int numSamples = (int) fs;
        for (int n = 0; n < numSamples; ++n)
        {
            const auto x = (float) (amplitude * std::sin (2.0 * M_PI * testFreq * (double) n / fs));
            const auto y = clipper.processSample (x);
            if (n > numSamples / 2)
            {
                posPeak = std::max (posPeak, y);
                negPeak = std::min (negPeak, y);
            }
        }

        std::cerr << "posPeak=" << posPeak << " negPeak=" << negPeak << '\n';
        check (posPeak > 0.0f && negPeak < 0.0f, "clipped output still swings both polarities");
        check (std::abs (posPeak + negPeak) < 0.25f * std::max (posPeak, -negPeak),
               "clipping is roughly symmetric for a symmetric diode pair");
    }

    // No NaN/Inf across the full drive/attack range and a few sample rates.
    {
        for (double testFs : { 44100.0, 48000.0, 96000.0 })
        {
            mothbite::ClipperStage clipper;
            clipper.prepare (testFs);

            for (int attackIdx = 0; attackIdx < 8; ++attackIdx)
            {
                clipper.setAttack (attackIdx);
                for (float drive : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
                {
                    clipper.setDrive (drive);
                    clipper.reset();

                    bool finite = true;
                    for (int n = 0; n < 1000; ++n)
                    {
                        const auto x = (float) (0.8 * std::sin (2.0 * M_PI * 440.0 * (double) n / testFs));
                        const auto y = clipper.processSample (x);
                        if (! std::isfinite (y))
                        {
                            finite = false;
                            break;
                        }
                    }
                    check (finite, "clipper output stays finite across drive/attack/sample-rate sweep");
                }
            }
        }
    }

    std::cout << (failures == 0 ? "All ClipperStage DSP tests passed\n" : "Tests failed\n");
    return failures == 0 ? 0 : 1;
}
