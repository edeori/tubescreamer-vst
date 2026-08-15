#include "DSP/ToneStage.h"

#include <algorithm>
#include <cmath>
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

    double measureGainDb (mothbite::ToneStage& stage, double freqHz, double fs)
    {
        constexpr double amplitude = 0.5;
        double maxOut = 0.0;
        const int numSamples = (int) fs;
        for (int n = 0; n < numSamples; ++n)
        {
            const auto x = (float) (amplitude * std::sin (2.0 * M_PI * freqHz * (double) n / fs));
            const auto y = stage.processSample (x);
            if (n > numSamples / 2)
                maxOut = std::max (maxOut, (double) std::abs (y));
        }
        return 20.0 * std::log10 (maxOut / amplitude);
    }
}

int main()
{
    constexpr double fs = 48000.0;

    // Post-clip filter: low-frequency gain should match R3/(R3+R7) = 10k/11k = -0.83dB, and
    // roll off well above its ~796Hz corner.
    {
        mothbite::ToneStage tone;
        tone.prepare (fs);
        tone.setBright (0.0f);

        const double lowFreqDb = measureGainDb (tone, 20.0, fs);
        std::cerr << "post-clip lowFreqDb=" << lowFreqDb << '\n';
        check (std::abs (lowFreqDb - (-0.83)) < 0.5, "post-clip filter low-frequency gain matches R3/(R3+R7)");

        tone.reset();
        const double highFreqDb = measureGainDb (tone, 8000.0, fs);
        std::cerr << "post-clip highFreqDb=" << highFreqDb << '\n';
        check (highFreqDb < lowFreqDb - 6.0, "post-clip filter rolls off well above its corner frequency");
    }

    // Bright shelf: flat (no bass boost) regardless of pot position, but a clear high-frequency
    // boost that grows with the bright parameter.
    {
        mothbite::ToneStage toneMin;
        toneMin.prepare (fs);
        toneMin.setBright (0.0f);
        const double minHighFreqDb = measureGainDb (toneMin, 10000.0, fs);

        mothbite::ToneStage toneMax;
        toneMax.prepare (fs);
        toneMax.setBright (1.0f);
        const double maxHighFreqDb = measureGainDb (toneMax, 10000.0, fs);

        std::cerr << "bright=0 10kHz=" << minHighFreqDb << "dB, bright=1 10kHz=" << maxHighFreqDb << "dB\n";
        check (maxHighFreqDb > minHighFreqDb + 5.0, "bright=1 boosts high frequencies more than bright=0");

        mothbite::ToneStage toneLow;
        toneLow.prepare (fs);
        toneLow.setBright (1.0f);
        const double lowFreqDbAtMaxBright = measureGainDb (toneLow, 50.0, fs);
        std::cerr << "bright=1 50Hz=" << lowFreqDbAtMaxBright << "dB\n";
        check (lowFreqDbAtMaxBright < 1.0, "bright shelf does not boost bass even at maximum setting");
    }

    std::cout << (failures == 0 ? "All ToneStage DSP tests passed\n" : "Tests failed\n");
    return failures == 0 ? 0 : 1;
}
