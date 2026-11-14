// Filter design tests: no JUCE needed.
//   c++ -std=c++17 -O2 -I../Source tests/test_design.cpp -o test_design && ./test_design
#include "dsp/FilterDesign.h"
#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace meq;

static int failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { ++failures; std::printf ("FAIL %s:%d: ", __FILE__, __LINE__); std::printf (__VA_ARGS__); std::printf ("\n"); } } while (0)

static double analogDb (const AnalogSection& a, double w) { return 10.0 * std::log10 (std::max (1e-300, a.magSq (w))); }

static bool stable (const Biquad& b)
{
    // roots of z^2 + a1 z + a2 inside the unit circle
    return std::abs (b.a2) < 1.0 && std::abs (b.a1) < 1.0 + b.a2;
}

static void testMatchedBellTracksAnalog()
{
    const double fs = 48000.0;
    for (double f : { 100.0, 1000.0, 8000.0, 15000.0, 19000.0 })
        for (double g : { -12.0, 6.0, 18.0 })
            for (double Q : { 0.5, 1.0, 4.0 })
            {
                const double w0 = 2 * kPi * f / fs;
                const auto a = detail::bell (w0, g, Q);
                Cascade mc;
                designBand (mc, { FilterType::Bell, f, g, Q, 12 }, fs, Transform::Matched);
                const auto m = mc;
                const auto b = detail::bilinear (a);
                CHECK (stable (m.s[0]), "matched bell unstable f=%g g=%g Q=%g", f, g, Q);
                double worstM = 0, worstB = 0;
                for (int i = 1; i < 400; ++i)
                {
                    const double w = kPi * i / 400.0;
                    const double ref = analogDb (a, w);
                    worstM = std::max (worstM, std::abs (20 * std::log10 (std::abs (m.response (w))) - ref));
                    worstB = std::max (worstB, std::abs (20 * std::log10 (std::abs (b.response (w))) - ref));
                }
                // matched is pinned at DC, w0 and Nyquist; it must beat bilinear clearly at high f
                CHECK (std::abs (20 * std::log10 (std::abs (m.response (w0))) - g) < 0.05,
                       "matched bell peak f=%g g=%g Q=%g: %.3f dB", f, g, Q, 20 * std::log10 (std::abs (m.response (w0))));
                CHECK (worstM < (f > 0.35 * fs ? 2.5 : 1.6), "matched bell worst error %.2f dB (f=%g g=%g Q=%g)", worstM, f, g, Q);
                if (f == 15000.0 && Q == 1.0)
                    std::printf ("  bell %5.0f Hz %+5.1f dB Q1: worst error matched %.2f dB, bilinear %.2f dB\n", f, g, worstM, worstB);
                if (f >= 8000.0 && std::abs (g) > 6.0)
                    CHECK (worstM < worstB, "matched not better than bilinear at f=%g g=%g Q=%g (%.2f vs %.2f)", f, g, Q, worstM, worstB);
            }
}

static void testAllTypesSane()
{
    for (double fs : { 44100.0, 96000.0 })
        for (int ti = 0; ti < (int) FilterType::Count; ++ti)
            for (int tr = 0; tr < 2; ++tr)
                for (double f : { 20.0, 250.0, 3000.0, 20000.0 })
                    for (double g : { -18.0, 0.0, 9.0 })
                        for (int slope : { 6, 12, 18, 48, 96 })
                        {
                            Cascade c;
                            BandDesign d { (FilterType) ti, f, g, 1.2, slope };
                            designBand (c, d, fs, (Transform) tr);
                            CHECK (c.count > 0, "no sections type %d", ti);
                            for (int i = 0; i < c.count; ++i)
                            {
                                const auto& b = c.s[(size_t) i];
                                CHECK (std::isfinite (b.b0) && std::isfinite (b.b1) && std::isfinite (b.b2)
                                         && std::isfinite (b.a1) && std::isfinite (b.a2),
                                       "non-finite coeffs type %s f=%g g=%g", filterTypeName ((FilterType) ti), f, g);
                                CHECK (stable (b), "unstable type %s f=%g g=%g slope=%d tr=%d", filterTypeName ((FilterType) ti), f, g, slope, tr);
                            }
                            CHECK (std::isfinite (c.magnitudeDb (0.3)), "non-finite response %s", filterTypeName ((FilterType) ti));
                        }
}

static void testShapes()
{
    const double fs = 48000.0;
    auto at = [&] (const Cascade& c, double hz) { return c.magnitudeDb (2 * kPi * hz / fs); };
    for (int tr = 0; tr < 2; ++tr)
    {
        const auto T = (Transform) tr;
        Cascade c;
        designBand (c, { FilterType::LowShelf, 200, 6, 0.707, 12 }, fs, T);
        CHECK (std::abs (at (c, 10) - 6) < 0.2 && std::abs (at (c, 10000)) < 0.2, "low shelf %d: %.2f %.2f", tr, at (c, 10), at (c, 10000));
        designBand (c, { FilterType::HighShelf, 5000, -9, 0.707, 12 }, fs, T);
        CHECK (std::abs (at (c, 20)) < 0.2 && std::abs (at (c, 23000) + 9) < 0.6, "high shelf %d: %.2f %.2f", tr, at (c, 20), at (c, 23000));
        designBand (c, { FilterType::LowCut, 1000, 0, 0.707, 24 }, fs, T);
        CHECK (std::abs (at (c, 1000) + 3.01) < 0.3, "LC -3dB point %d: %.2f", tr, at (c, 1000));
        CHECK (std::abs (at (c, 500) + 24.1) < 1.5, "LC 24 dB/oct one octave down %d: %.2f", tr, at (c, 500));
        designBand (c, { FilterType::LowCut, 1000, 0, 0.707, 18 }, fs, T);
        CHECK (std::abs (at (c, 250) + 36.1) < 2.0, "LC 18 dB/oct two octaves %d: %.2f", tr, at (c, 250));
        designBand (c, { FilterType::HighCut, 2000, 0, 0.707, 96 }, fs, T);
        CHECK (at (c, 4000) < -80, "HC 96 dB/oct %d: %.2f", tr, at (c, 4000));
        designBand (c, { FilterType::Notch, 1000, 0, 4, 12 }, fs, T);
        CHECK (at (c, 1000) < -60, "notch depth %d: %.2f", tr, at (c, 1000));
        designBand (c, { FilterType::Allpass, 1000, 0, 1, 12 }, fs, T);
        CHECK (std::abs (at (c, 300)) < 0.01 && std::abs (at (c, 9000)) < 0.01, "allpass flat %d", tr);
        designBand (c, { FilterType::FlatTop, 1000, 6, 1.0, 12 }, fs, T);
        CHECK (std::abs (at (c, 1000) - 6) < 0.8 && std::abs (at (c, 50)) < 0.3 && std::abs (at (c, 18000)) < 0.6,
               "flat top %d: %.2f %.2f %.2f", tr, at (c, 1000), at (c, 50), at (c, 18000));
        designBand (c, { FilterType::FlatTilt, 1000, 12, 1.0, 12 }, fs, T);
        CHECK (std::abs (at (c, 1000)) < 0.05, "flat tilt pivot %d: %.2f", tr, at (c, 1000));
        CHECK (at (c, 20) < -4 && at (c, 16000) > 3.5, "flat tilt direction %d: %.2f %.2f", tr, at (c, 20), at (c, 16000));
        designBand (c, { FilterType::Sword, 3000, -12, 4, 12 }, fs, T);
        CHECK (std::abs (at (c, 3000) + 12) < 0.3, "sword depth %d: %.2f", tr, at (c, 3000));
    }
}

int main()
{
    testMatchedBellTracksAnalog();
    testAllTypesSane();
    testShapes();
    if (failures) { std::printf ("%d failure(s)\n", failures); return 1; }
    std::printf ("design tests passed\n");
    return 0;
}
