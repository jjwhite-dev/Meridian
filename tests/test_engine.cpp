// Engine tests: no JUCE needed.
#include "dsp/EqEngine.h"
#include <cstdio>
#include <random>
#include <memory>
#include <vector>

using namespace qtip;

static int failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { ++failures; std::printf ("FAIL %s:%d: ", __FILE__, __LINE__); std::printf (__VA_ARGS__); std::printf ("\n"); } } while (0)

static const double fs = 48000.0;

struct Run
{
    std::vector<double> L, R;
};

static std::vector<double> sine (double hz, double amp, int n, double phase = 0.0)
{
    std::vector<double> v ((size_t) n);
    for (int i = 0; i < n; ++i) v[(size_t) i] = amp * std::sin (2 * kPi * hz * i / fs + phase);
    return v;
}

static double rmsDb (const std::vector<double>& v, int from)
{
    double s = 0;
    for (size_t i = (size_t) from; i < v.size(); ++i) s += v[i] * v[i];
    return 10 * std::log10 (std::max (1e-30, s / (double) (v.size() - (size_t) from)));
}

static Run process (Engine& e, std::vector<double> L, std::vector<double> R,
                    const std::vector<double>* scL = nullptr, const std::vector<double>* scR = nullptr, int block = 512)
{
    for (size_t s = 0; s < L.size(); s += (size_t) block)
    {
        const int n = (int) std::min ((size_t) block, L.size() - s);
        e.process (L.data() + s, R.data() + s, scL ? scL->data() + s : nullptr, scR ? scR->data() + s : nullptr, n);
    }
    return { L, R };
}

static std::unique_ptr<Engine> makeEngine (const std::array<BandParams, kNumBands>& b, GlobalParams g = {})
{
    auto e = std::make_unique<Engine>();
    e->prepare (fs, fs * 2);
    e->setParams (b, g);
    return e;
}

static void testUnity()
{
    std::array<BandParams, kNumBands> b {};
    auto e = makeEngine (b);
    auto in = sine (440, 0.5, 4800);
    auto out = process (*e, in, in);
    double worst = 0;
    for (size_t i = 0; i < in.size(); ++i) worst = std::max (worst, std::abs (out.L[i] - in[i]));
    CHECK (worst < 1e-12, "no bands should be transparent, worst diff %g", worst);
}

static void testBell()
{
    std::array<BandParams, kNumBands> b {};
    b[0].enabled = true; b[0].type = FilterType::Bell; b[0].freq = 1000; b[0].gain = 6; b[0].q = 1;
    auto e = makeEngine (b);
    auto at1k = process (*e, sine (1000, 0.25, 24000), sine (1000, 0.25, 24000));
    const double g = rmsDb (at1k.L, 12000) - rmsDb (sine (1000, 0.25, 24000), 12000);
    CHECK (std::abs (g - 6) < 0.1, "bell +6 at 1 kHz measured %.2f dB", g);

    auto e2 = makeEngine (b);
    auto at100 = process (*e2, sine (100, 0.25, 24000), sine (100, 0.25, 24000));
    const double g2 = rmsDb (at100.L, 12000) - rmsDb (sine (100, 0.25, 24000), 12000);
    CHECK (std::abs (g2) < 0.3, "bell should leave 100 Hz alone, measured %.2f dB", g2);
}

static void testMidSide()
{
    std::array<BandParams, kNumBands> b {};
    b[0].enabled = true; b[0].type = FilterType::Bell; b[0].freq = 1000; b[0].gain = -12; b[0].q = 1; b[0].channel = Channel::Mid;
    auto e = makeEngine (b);
    auto s = sine (1000, 0.25, 24000);
    std::vector<double> neg (s.size());
    for (size_t i = 0; i < s.size(); ++i) neg[i] = -s[i];
    auto side = process (*e, s, neg); // pure side signal
    const double g = rmsDb (side.L, 12000) - rmsDb (s, 12000);
    CHECK (std::abs (g) < 0.05, "mid band must not touch side signal, measured %.2f dB", g);

    auto e2 = makeEngine (b);
    auto mid = process (*e2, s, s);
    const double g2 = rmsDb (mid.L, 12000) - rmsDb (s, 12000);
    CHECK (std::abs (g2 + 12) < 0.2, "mid band on mono signal, measured %.2f dB", g2);
}

static void testLeftOnly()
{
    std::array<BandParams, kNumBands> b {};
    b[0].enabled = true; b[0].type = FilterType::HighShelf; b[0].freq = 2000; b[0].gain = -10; b[0].q = 0.7; b[0].channel = Channel::Left;
    auto e = makeEngine (b);
    auto s = sine (12000, 0.25, 24000);
    auto out = process (*e, s, s);
    CHECK (rmsDb (out.L, 12000) - rmsDb (s, 12000) < -9, "left shelf should cut left");
    CHECK (std::abs (rmsDb (out.R, 12000) - rmsDb (s, 12000)) < 0.01, "left shelf must not touch right");
}

static void testDynamics()
{
    std::array<BandParams, kNumBands> b {};
    auto& p = b[0];
    p.enabled = true; p.type = FilterType::Bell; p.freq = 1000; p.gain = 0; p.q = 1;
    p.dynOn = true; p.dynMode = DynMode::Above; p.threshold = -30; p.range = -9; p.ratio = 20; p.attackMs = 2; p.releaseMs = 50;

    auto e = makeEngine (b);
    auto loud = sine (1000, 0.5, 48000);
    auto out = process (*e, loud, loud);
    const double g = rmsDb (out.L, 24000) - rmsDb (loud, 24000);
    CHECK (g < -8.0 && g > -9.5, "loud signal should be cut by ~9 dB, measured %.2f dB", g);
    CHECK (e->liveDynDb (0) < -8.5, "live dynamic gain %.2f", e->liveDynDb (0));

    auto e2 = makeEngine (b);
    auto quiet = sine (1000, 0.002, 48000);
    auto out2 = process (*e2, quiet, quiet);
    const double g2 = rmsDb (out2.L, 24000) - rmsDb (quiet, 24000);
    CHECK (std::abs (g2) < 0.2, "quiet signal below threshold untouched, measured %.2f dB", g2);

    // two-way: below the threshold the band moves the other way
    p.dynMode = DynMode::TwoWay;
    auto e3 = makeEngine (b);
    auto out3 = process (*e3, quiet, quiet);
    const double g3 = rmsDb (out3.L, 24000) - rmsDb (quiet, 24000);
    CHECK (g3 > 8.0, "two-way should lift a quiet signal, measured %.2f dB", g3);
}

static void testExternalSidechain()
{
    std::array<BandParams, kNumBands> b {};
    auto& p = b[0];
    p.enabled = true; p.type = FilterType::Bell; p.freq = 1000; p.gain = 0; p.q = 0.7;
    p.dynOn = true; p.threshold = -30; p.range = -12; p.ratio = 20; p.attackMs = 1; p.releaseMs = 50;
    p.detect = Detect::External; p.detFreq = 60; p.detQ = 1;

    auto main = sine (1000, 0.25, 48000);
    auto kick = sine (60, 0.7, 48000);
    std::vector<double> silence (main.size(), 0.0);

    auto e = makeEngine (b);
    auto ducked = process (*e, main, main, &kick, &kick);
    const double g = rmsDb (ducked.L, 24000) - rmsDb (main, 24000);
    CHECK (g < -10.5, "sidechain should duck the main signal, measured %.2f dB", g);

    auto e2 = makeEngine (b);
    auto free = process (*e2, main, main, &silence, &silence);
    const double g2 = rmsDb (free.L, 24000) - rmsDb (main, 24000);
    CHECK (std::abs (g2) < 0.2, "silent sidechain must not duck, measured %.2f dB (self level would trigger)", g2);

    // a sidechain outside the detector band must not trigger either
    auto hat = sine (8000, 0.7, 48000);
    auto e3 = makeEngine (b);
    auto out3 = process (*e3, main, main, &hat, &hat);
    CHECK (std::abs (rmsDb (out3.L, 24000) - rmsDb (main, 24000)) < 1.0, "sidechain outside detector band triggered");
}

static void testLookahead()
{
    std::array<BandParams, kNumBands> b {};
    GlobalParams g;
    g.lookaheadMs = 5.0;
    auto e = makeEngine (b, g);
    CHECK (e->latencySamples() == 240, "lookahead latency %d", e->latencySamples());
    std::vector<double> imp (2000, 0.0);
    imp[10] = 1.0;
    auto out = process (*e, imp, imp, nullptr, nullptr, 100);
    CHECK (std::abs (out.L[250] - 1.0) < 1e-12, "impulse should arrive 240 samples late");
}

static void testAudition()
{
    std::array<BandParams, kNumBands> b {};
    b[3].enabled = true; b[3].type = FilterType::Bell; b[3].freq = 4000; b[3].gain = -3; b[3].q = 2;
    GlobalParams g;
    g.solo = 3;
    auto e = makeEngine (b, g);
    auto low = sine (200, 0.25, 24000);
    auto out = process (*e, low, low);
    CHECK (rmsDb (out.L, 12000) - rmsDb (low, 12000) < -30, "audition should remove content far from the band");
    auto e2 = makeEngine (b, g);
    auto at = sine (4000, 0.25, 24000);
    auto out2 = process (*e2, at, at);
    CHECK (std::abs (rmsDb (out2.L, 12000) - rmsDb (at, 12000) + 3) < 1.0, "audition passes the band's own region (with its cut)");
}

static void testAutoGainAndWidth()
{
    std::array<BandParams, kNumBands> b {};
    b[0].enabled = true; b[0].type = FilterType::Bell; b[0].freq = 1000; b[0].gain = 6; b[0].q = 0.3;
    GlobalParams g;
    g.autoGain = true;
    auto e = makeEngine (b, g);
    auto s = sine (1000, 0.1, 48000);
    process (*e, s, s);
    CHECK (e->autoGainDb() < -1.0, "auto gain should compensate a broad boost, got %.2f", e->autoGainDb());

    std::array<BandParams, kNumBands> none {};
    GlobalParams w;
    w.width = 0.0;
    auto e2 = makeEngine (none, w);
    auto l = sine (500, 0.3, 4800), r = sine (700, 0.3, 4800);
    auto out = process (*e2, l, r);
    double diff = 0;
    for (size_t i = 0; i < out.L.size(); ++i) diff = std::max (diff, std::abs (out.L[i] - out.R[i]));
    CHECK (diff < 1e-12, "width 0 must give mono");
}

static void testStress()
{
    std::mt19937 rng (7);
    std::uniform_real_distribution<double> u (0, 1);
    std::array<BandParams, kNumBands> b {};
    for (auto& p : b)
    {
        p.enabled = true;
        p.type = (FilterType) (int) (u (rng) * (int) FilterType::Count);
        p.freq = 20 * std::pow (1000.0, u (rng));
        p.gain = -18 + 36 * u (rng);
        p.q = 0.1 * std::pow (300.0, u (rng));
        p.slope = 6 * (1 + (int) (u (rng) * 16));
        p.channel = (Channel) (int) (u (rng) * 5);
        p.dynOn = u (rng) > 0.5;
        p.detect = (Detect) (int) (u (rng) * 3);
        p.detFreq = 20 * std::pow (1000.0, u (rng));
        p.dynMode = (DynMode) (int) (u (rng) * 3);
        p.onset = -100 + 200 * u (rng);
        p.body = -100 + 200 * u (rng);
    }
    for (int tr = 0; tr < 2; ++tr)
    {
        GlobalParams g;
        g.transform = (Transform) tr;
        g.gainScale = tr == 0 ? 2.0 : -1.0;
        g.lookaheadMs = 3;
        auto e = makeEngine (b, g);
        std::vector<double> L (96000), R (96000), sc (96000);
        for (size_t i = 0; i < L.size(); ++i) { L[i] = u (rng) * 2 - 1; R[i] = u (rng) * 2 - 1; sc[i] = u (rng) * 2 - 1; }
        auto out = process (*e, L, R, &sc, &sc, 333);
        bool finite = true;
        double peak = 0;
        for (size_t i = 0; i < out.L.size(); ++i)
        {
            finite = finite && std::isfinite (out.L[i]) && std::isfinite (out.R[i]);
            peak = std::max (peak, std::abs (out.L[i]));
        }
        CHECK (finite, "stress produced non-finite output (transform %d)", tr);
        CHECK (peak < 1e6, "stress output exploded: %g", peak);
    }
}

int main()
{
    testUnity();
    testBell();
    testMidSide();
    testLeftOnly();
    testDynamics();
    testExternalSidechain();
    testLookahead();
    testAudition();
    testAutoGainAndWidth();
    testStress();
    if (failures) { std::printf ("%d failure(s)\n", failures); return 1; }
    std::printf ("engine tests passed\n");
    return 0;
}
