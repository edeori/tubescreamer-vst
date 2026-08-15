#pragma once

#include <chowdsp_wdf/chowdsp_wdf.h>

namespace mothbite
{
/**
 * Wave digital filter model of the Dwarven Hammer's first op-amp gain stage (IC1.1):
 * a Tube Screamer-style clipper with an antiparallel diode pair (D1/D2) in the feedback
 * path, a variable feedback resistance (R15 + Drive pot), a small feedback capacitor (C8),
 * and a switched shunt capacitor bank (C9 fixed + one of 8 Attack-selected capacitors) that
 * sets the gain-stage's low-frequency floor.
 *
 * Modelled as two independent WDF sub-trees per the "op-amp adaptor" technique: since the
 * ideal op-amp forces zero input current, the current pulled through the shunt network (Zg)
 * given the known non-inverting input voltage must flow entirely through the feedback network
 * (Zf), which is solved by injecting that exact current via a Norton current source placed in
 * parallel with the diode pair's passive surroundings.
 */
class ClipperStage
{
public:
    ClipperStage();

    void prepare (double sampleRate);
    void reset();

    /** @param drive01: Drive pot position, 0..1 (audio taper applied internally). */
    void setDrive (float drive01) noexcept;

    /** @param index: Attack rotary switch position, 0..7. */
    void setAttack (int index) noexcept;

    float processSample (float vin) noexcept;
    void process (float* samples, int numSamples) noexcept;

private:
    static double driveIndexToFeedbackOhms (float drive01) noexcept;
    static double attackIndexToShuntFarads (int index) noexcept;

    // Zg: R16 in series with (C9 fixed + Attack-selected capacitor), driven by an ideal voltage
    // source representing the op-amp's virtual-short (V- pinned to the non-inverting input).
    chowdsp::wdft::ResistorT<double> Rg { 1000.0 };
    chowdsp::wdft::CapacitorT<double> Cg { 120.0e-12 };
    chowdsp::wdft::WDFSeriesT<double, decltype (Rg), decltype (Cg)> Zg { Rg, Cg };
    chowdsp::wdft::IdealVoltageSourceT<double, decltype (Zg)> vinSource { Zg };

    // Zf: diode pair (D1||D2) in parallel with (R15 + Drive pot) and C8, driven by the current
    // pulled through Zg via a Norton current injector standing in for "the rest of the circuit".
    chowdsp::wdft::ResistorT<double> Rf { 10000.0 };
    chowdsp::wdft::CapacitorT<double> Cf { 47.0e-12 };
    chowdsp::wdft::WDFParallelT<double, decltype (Rf), decltype (Cf)> RfCf { Rf, Cf };
    chowdsp::wdft::ResistiveCurrentSourceT<double> igInjector { 1.0e12 };
    chowdsp::wdft::WDFParallelT<double, decltype (RfCf), decltype (igInjector)> Zf { RfCf, igInjector };
    chowdsp::wdft::DiodePairT<double, decltype (Zf)> diodes { Zf, 2.52e-9, 25.85e-3, 1.0 };
};
} // namespace mothbite
