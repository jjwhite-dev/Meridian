// Meridian EQ - processing engine (pure C++17, no JUCE).
//
// 32 bands, each with its own filter shape, channel (stereo / left / right /
// mid / side) and optional dynamics. Runs in double precision. The JUCE
// processor only converts parameters and buffers; everything audible is here,
// so it can be unit tested without a host.
#pragma once

#include "FilterDesign.h"

#include <array>
#include <atomic>
#include <vector>

namespace meq
{

constexpr int kNumBands = 32;

enum class Channel : int { Stereo = 0, Left, Right, Mid, Side };
enum class DynMode : int { Above = 0, Below, TwoWay };
enum class Detect  : int { Self = 0, Relative, External };
enum class Monitor : int { Off = 0, Mid, Side };

struct BandParams
{
    bool enabled = false;
    bool bypass = false;
    FilterType type = FilterType::Bell;
    double freq = 1000.0;
    double gain = 0.0;
    double q = 0.707;
    int slope = 12;
    Channel channel = Channel::Stereo;

    bool dynOn = false;
    DynMode dynMode = DynMode::Above;
    double threshold = -24.0;  // dB
    double range = -6.0;       // dB the band moves by when triggered (sign = direction)
    double ratio = 2.0;
    double attackMs = 10.0;
    double releaseMs = 120.0;
    double onset = 0.0;        // -100..100 %, extra move on transients
    double body = 0.0;         // -100..100 %, extra move on the sustained part
    Detect detect = Detect::Self;
    double detFreq = 1000.0;   // centre of the detector band for Relative / External
    double detQ = 1.0;

    bool staticEquals (const BandParams& o) const
    {
        return enabled == o.enabled && bypass == o.bypass && type == o.type && freq == o.freq
            && gain == o.gain && q == o.q && slope == o.slope && channel == o.channel;
    }
};

struct GlobalParams
{
    Transform transform = Transform::Matched; // Analog phase mode = Matched, Zero Latency = Bilinear
    double gainScale = 1.0;   // -2..2 (= -200..200 %)
    double outputDb = 0.0;
    double width = 1.0;       // 0..2
    bool autoGain = false;
    Monitor monitor = Monitor::Off;
    double lookaheadMs = 0.0;
    int solo = -1;            // band being auditioned, -1 = none
};

class Engine
{
public:
    static constexpr int kSubBlock = 16;
    static constexpr double kMaxLookaheadMs = 20.0;

    /** Allocates. maxSampleRate bounds every later setSampleRate (oversampling included). */
    void prepare (double sampleRate, double maxSampleRate);
    /** No allocation; resets filter state. */
    void setSampleRate (double sampleRate);
    void reset();

    void setParams (const std::array<BandParams, kNumBands>& bands, const GlobalParams& global);

    /** In-place stereo processing. scL/scR may be null (no sidechain). */
    void process (double* L, double* R, const double* scL, const double* scR, int numSamples);

    int latencySamples() const { return lookahead; }
    double sampleRate() const { return fs; }

    /** Current gain of a band including its dynamic movement, for the display. */
    float liveGainDb (int band) const { return live[(size_t) band].load (std::memory_order_relaxed); }
    float liveDynDb (int band) const  { return liveDyn[(size_t) band].load (std::memory_order_relaxed); }
    float autoGainDb() const { return liveAuto.load (std::memory_order_relaxed); }

    /** Static design of one band at this engine's rate (used by tests and the display). */
    static void designStatic (Cascade& c, const BandParams& p, double gainScale, double fs, Transform t, double extraGainDb = 0.0);

private:
    struct SectionState { double z1 = 0, z2 = 0; };

    struct Band
    {
        BandParams target, cur;
        bool initialised = false;
        double logF = 3.0, gain = 0.0, logQ = 0.0;
        FilterType designedType = FilterType::Bell;
        int designedSlope = -1;
        double designedF = -1, designedG = 1e9, designedQ = -1;
        Cascade cas;
        std::array<std::array<SectionState, Cascade::kMaxSections>, 2> st {};

        // dynamics
        Biquad det;
        std::array<SectionState, 2> detSt {};
        double env = 0, fastEnv = 0, slowEnv = 0;
        double dynDb = 0, dynTarget = 0;

        void clearState()
        {
            for (auto& ch : st) for (auto& s : ch) s = {};
        }
    };

    struct Audition
    {
        Cascade cas;
        std::array<std::array<SectionState, Cascade::kMaxSections>, 2> st {};
        double mix = 0;
        int band = -1;
        bool usePre = false;
    };

    void updateBand (Band& b, int index, int n);
    void runDetector (Band& b, const double* inL, const double* inR, const double* scL, const double* scR, int n);
    void runCascade (const Cascade& c, std::array<SectionState, Cascade::kMaxSections>& st, double* x, int n) const;
    void processBand (Band& b, double* L, double* R, int n);
    void updateAudition (int n);
    void computeAutoGain();

    double fs = 48000.0, maxFs = 48000.0;
    std::array<Band, kNumBands> bands;
    GlobalParams global;
    Transform designedTransform = Transform::Matched;
    double designedFs = 0.0;

    // lookahead delay on the main path (detectors see the undelayed signal)
    std::vector<double> delayL, delayR;
    int delayPos = 0, lookahead = 0;

    // scratch for one sub-block
    std::array<double, kSubBlock> tmpA {}, tmpB {}, preL {}, preR {};

    Audition audition;

    double outGain = 1.0, autoComp = 0.0, autoTarget = 0.0;
    bool staticDirty = true;
    int autoCountdown = 0;

    std::array<std::atomic<float>, kNumBands> live {}, liveDyn {};
    std::atomic<float> liveAuto { 0.0f };
};

} // namespace meq
