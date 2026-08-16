// Scratch measurement tool - not part of the shipped plugin. Measures how long AdaptiveGate
// takes to CLOSE (hold+release) after a note/palm-mute stops, chained ahead of MothBite's
// clipper, over the real DI file - the user reports the gate doesn't shut fast enough for tight
// rhythm playing, letting noise bleed through between mutes. Sweeps hold/release multipliers to
// find a tighter setting. Remove this file and its CMake target when the tuning session is done.

#include "DSP/ClipperStage.h"
#include "DSP/ToneStage.h"

#include "DSP/AdaptiveGateEngine.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

using namespace mothbite;

namespace
{
    double toDb (double linear) { return 20.0 * std::log10 (std::max (linear, 1.0e-12)); }

    struct WindowStats
    {
        double rms = 0.0;
        double peak = 0.0;
    };

    WindowStats computeStats (const float* data, int numSamples)
    {
        double sumSq = 0.0;
        double peak = 0.0;
        for (int i = 0; i < numSamples; ++i)
        {
            const double v = (double) data[i];
            sumSq += v * v;
            peak = std::max (peak, std::abs (v));
        }
        return { std::sqrt (sumSq / std::max (1, numSamples)), peak };
    }

    // Finds the sharpest loud-to-quiet transition in the file (biggest short-term RMS drop),
    // as a proxy for a real note/palm-mute stop - rather than a fixed threshold that might not
    // exactly match this particular recording's dynamics.
    int findNoteOff (const std::vector<float>& mono, double sampleRate)
    {
        const int shortWin = (int) std::round (sampleRate * 0.02); // 20ms
        const int hop = (int) std::round (sampleRate * 0.005);     // 5ms scan resolution

        double bestDrop = -1.0e9;
        int bestStart = -1;

        for (int start = shortWin; start + shortWin * 4 <= (int) mono.size(); start += hop)
        {
            const auto before = computeStats (mono.data() + start - shortWin, shortWin);
            const auto after2 = computeStats (mono.data() + start + shortWin * 2, shortWin);
            const auto beforeDb = toDb (before.rms);
            const auto afterDb = toDb (after2.rms);

            if (beforeDb < -25.0)
                continue; // require the "before" side to be genuinely playing

            const auto drop = beforeDb - afterDb;
            if (drop > bestDrop)
            {
                bestDrop = drop;
                bestStart = start;
            }
        }

        if (bestStart >= 0)
            printf ("(sharpest transition found: %.1f dB drop)\n", bestDrop);

        return bestStart;
    }

    std::vector<float> runGate (const std::vector<float>& mono, double sampleRate, float attackMul,
                                 float holdMul, float releaseMul, float thresholdOffsetDb)
    {
        constexpr int blockSize = 512;

        adaptivegate::dsp::AdaptiveGateEngine gate;
        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) blockSize, 1 };
        gate.prepare (spec);
        gate.setSourceProfile (adaptivegate::presets::SourceType::Guitar);
        gate.setAttackHoldReleaseMultiplier (attackMul, holdMul, releaseMul);
        gate.setThresholdOffsetDb (thresholdOffsetDb);
        gate.setMix (1.0f);
        gate.setBypassed (false);
        gate.reset();

        std::vector<float> output ((size_t) mono.size());
        juce::AudioBuffer<float> buffer (1, blockSize);

        int done = 0;
        while (done < (int) mono.size())
        {
            const int thisBlock = std::min (blockSize, (int) mono.size() - done);
            buffer.setSize (1, thisBlock, false, false, true);
            std::copy (mono.begin() + done, mono.begin() + done + thisBlock, buffer.getWritePointer (0));
            gate.process (buffer);
            std::copy (buffer.getReadPointer (0), buffer.getReadPointer (0) + thisBlock, output.begin() + done);
            done += thisBlock;
        }

        return output;
    }

    std::vector<float> runClipperChain (const std::vector<float>& input, double sampleRate, float drive, int attackIndex)
    {
        ClipperStage clipper;
        ToneStage tone;
        clipper.prepare (sampleRate);
        tone.prepare (sampleRate);
        clipper.setDrive (drive);
        clipper.setAttack (attackIndex);
        tone.setBright (0.0f);

        std::vector<float> output = input;
        clipper.process (output.data(), (int) output.size());
        tone.process (output.data(), (int) output.size());
        return output;
    }

    // Measures the time (ms) from noteOffStart until the post-clipper output RMS (5ms windows)
    // first drops below thresholdDb and STAYS below it.
    double measureCloseTimeMs (const std::vector<float>& clippedOut, int noteOffStart, double sampleRate, double thresholdDb)
    {
        const int stepSamples = (int) std::round (sampleRate * 0.005); // 5ms steps
        const int maxSteps = (int) std::round (sampleRate * 0.5) / stepSamples; // scan up to 500ms

        for (int step = 0; step < maxSteps; ++step)
        {
            const int pos = noteOffStart + step * stepSamples;
            if (pos + stepSamples > (int) clippedOut.size())
                break;

            const auto stats = computeStats (clippedOut.data() + pos, stepSamples);
            if (toDb (stats.rms) < thresholdDb)
            {
                // Confirm it stays down for the next 30ms (not just a brief dip in a decaying transient).
                bool staysDown = true;
                for (int checkStep = 1; checkStep <= 6; ++checkStep)
                {
                    const int checkPos = pos + checkStep * stepSamples;
                    if (checkPos + stepSamples > (int) clippedOut.size())
                        break;
                    if (toDb (computeStats (clippedOut.data() + checkPos, stepSamples).rms) >= thresholdDb)
                    {
                        staysDown = false;
                        break;
                    }
                }
                if (staysDown)
                    return step * 5.0;
            }
        }
        return -1.0; // never closed within the scan window
    }
} // namespace

int main()
{
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    const juce::File wavFile ("/Users/mothproduction/Documents/VSCode/tubescreamer-vst/Measurements/di - 25 Earth Elemental - rythm gtr 2.wav");
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (wavFile));
    if (reader == nullptr)
    {
        printf ("Could not open WAV file\n");
        return 1;
    }

    const double sampleRate = reader->sampleRate;
    const int numSamples = (int) reader->lengthInSamples;

    juce::AudioBuffer<float> fileBuffer ((int) reader->numChannels, numSamples);
    reader->read (&fileBuffer, 0, numSamples, 0, true, true);

    std::vector<float> mono ((size_t) numSamples);
    for (int i = 0; i < numSamples; ++i)
    {
        float sum = 0.0f;
        for (int ch = 0; ch < fileBuffer.getNumChannels(); ++ch)
            sum += fileBuffer.getSample (ch, i);
        mono[(size_t) i] = sum / (float) fileBuffer.getNumChannels();
    }

    const int noteOffStart = findNoteOff (mono, sampleRate);
    if (noteOffStart < 0)
    {
        printf ("Could not find a clean note-off in the file\n");
        return 1;
    }
    printf ("Note-off point: %.3fs\n\n", noteOffStart / sampleRate);

    constexpr float drive = 1.0f;
    constexpr int attackIndex = 7;

    const int analysisStart = noteOffStart - (int) std::round (sampleRate * 0.02);
    const int analysisLen = (int) std::round (sampleRate * 0.6);

    // Reference: no gate at all, straight into the clipper.
    std::vector<float> rawIn (mono.begin() + analysisStart, mono.begin() + analysisStart + analysisLen);
    const auto ungatedOut = runClipperChain (rawIn, sampleRate, drive, attackIndex);
    const auto ungatedCloseMs = measureCloseTimeMs (ungatedOut, (int) std::round (sampleRate * 0.02), sampleRate, -50.0);

    printf ("Ungated close time to -50dBFS (natural decay only): %.1f ms\n\n", ungatedCloseMs);

    struct Setting
    {
        const char* label;
        float holdMul, releaseMul, thresholdOffsetDb;
    };

    const std::vector<Setting> settings = {
        { "Guitar defaults (1.0x/1.0x)", 1.0f, 1.0f, 0.0f },
        { "0.5x hold, 0.5x release",     0.5f, 0.5f, 0.0f },
        { "0.25x hold, 0.25x release",   0.25f, 0.25f, 0.0f },
        { "0.1x hold, 0.1x release",     0.1f, 0.1f, 0.0f },
        { "0.1x hold, 0.1x release, +6dB stricter", 0.1f, 0.1f, 6.0f },
        { "0.1x hold, 0.1x release, +12dB stricter", 0.1f, 0.1f, 12.0f },
    };

    printf ("%-42s %14s %14s\n", "Setting", "CloseMs(-50dB)", "CloseMs(-60dB)");

    for (const auto& s : settings)
    {
        const auto gated = runGate (mono, sampleRate, 1.0f, s.holdMul, s.releaseMul, s.thresholdOffsetDb);
        std::vector<float> gatedWindow (gated.begin() + analysisStart, gated.begin() + analysisStart + analysisLen);
        const auto out = runClipperChain (gatedWindow, sampleRate, drive, attackIndex);

        const int localNoteOff = (int) std::round (sampleRate * 0.02);
        const auto closeMs50 = measureCloseTimeMs (out, localNoteOff, sampleRate, -50.0);
        const auto closeMs60 = measureCloseTimeMs (out, localNoteOff, sampleRate, -60.0);

        printf ("%-42s %14.1f %14.1f\n", s.label, closeMs50, closeMs60);
    }

    return 0;
}
