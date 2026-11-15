#include "EqEngine.h"

#include <algorithm>
#include <cmath>

namespace qtip
{

namespace
{
    double dbToGain (double db) { return std::pow (10.0, db / 20.0); }
    double gainToDb (double g)  { return 20.0 * std::log10 (std::max (g, 1e-12)); }
    double timeCoef (double ms, double rate) { return std::exp (-1.0 / std::max (1e-6, ms * 0.001 * rate)); }

    Biquad bandPassFor (double freq, double q, double rate, Transform t)
    {
        Cascade c;
        designBand (c, { FilterType::BandPass, freq, 0.0, std::min (std::max (q, 0.3), 30.0), 12 }, rate, t);
        return c.s[0];
    }
}

void Engine::designStatic (Cascade& c, const BandParams& p, double gainScale, double rate, Transform t, double extraGainDb)
{
    BandDesign d;
    d.type = p.type;
    d.freqHz = p.freq;
    d.gainDb = std::min (36.0, std::max (-36.0, p.gain * gainScale + extraGainDb));
    d.q = p.q;
    d.slopeDbPerOct = p.slope;
    designBand (c, d, rate, t);
}

void Engine::prepare (double sampleRate, double maxSampleRate)
{
    maxFs = std::max (sampleRate, maxSampleRate);
    const auto maxDelay = (size_t) std::ceil (kMaxLookaheadMs * 0.001 * maxFs) + 4;
    delayL.assign (maxDelay, 0.0);
    delayR.assign (maxDelay, 0.0);
    setSampleRate (sampleRate);
}

void Engine::setSampleRate (double sampleRate)
{
    fs = std::min (sampleRate, maxFs);
    reset();
}

void Engine::reset()
{
    for (auto& b : bands)
    {
        b.clearState();
        b.detSt = {};
        b.env = b.fastEnv = b.slowEnv = 0;
        b.dynDb = b.dynTarget = 0;
        b.initialised = false;
        b.designedSlope = -1;
    }
    for (auto& ch : audition.st) for (auto& s : ch) s = {};
    audition.mix = 0;
    std::fill (delayL.begin(), delayL.end(), 0.0);
    std::fill (delayR.begin(), delayR.end(), 0.0);
    delayPos = 0;
    designedFs = 0.0;
    staticDirty = true;
}

void Engine::setParams (const std::array<BandParams, kNumBands>& newBands, const GlobalParams& g)
{
    for (int i = 0; i < kNumBands; ++i)
    {
        auto& b = bands[(size_t) i];
        if (! b.target.staticEquals (newBands[(size_t) i])) staticDirty = true;
        b.target = newBands[(size_t) i];
    }
    if (g.gainScale != global.gainScale || g.transform != global.transform || g.autoGain != global.autoGain)
        staticDirty = true;
    global = g;

    const int maxLa = (int) delayL.size() - 2;
    lookahead = std::min (maxLa, std::max (0, (int) std::lround (global.lookaheadMs * 0.001 * fs)));
}

void Engine::runCascade (const Cascade& c, std::array<SectionState, Cascade::kMaxSections>& st, double* x, int n) const
{
    for (int k = 0; k < c.count; ++k)
    {
        const auto& q = c.s[(size_t) k];
        auto& s = st[(size_t) k];
        double z1 = s.z1, z2 = s.z2;
        for (int i = 0; i < n; ++i)
        {
            const double in = x[i];
            const double y = q.b0 * in + z1;
            z1 = q.b1 * in - q.a1 * y + z2;
            z2 = q.b2 * in - q.a2 * y;
            x[i] = y;
        }
        // flush denormals
        s.z1 = std::abs (z1) < 1e-30 ? 0.0 : z1;
        s.z2 = std::abs (z2) < 1e-30 ? 0.0 : z2;
    }
    if (c.scale != 1.0)
        for (int i = 0; i < n; ++i) x[i] *= c.scale;
}

void Engine::runDetector (Band& b, const double* inL, const double* inR, const double* scL, const double* scR, int n)
{
    const auto& p = b.cur;
    const bool external = p.detect == Detect::External;
    const double* srcL = external ? scL : inL;
    const double* srcR = external ? scR : inR;

    const double att = timeCoef (p.attackMs, fs), rel = timeCoef (p.releaseMs, fs);
    const double fAtt = timeCoef (0.3, fs), fRel = timeCoef (15.0, fs);
    const double sAtt = timeCoef (25.0, fs), sRel = timeCoef (150.0, fs);

    auto filt = [&] (int ch, double x)
    {
        auto& s = b.detSt[(size_t) ch];
        const double y = b.det.b0 * x + s.z1;
        s.z1 = b.det.b1 * x - b.det.a1 * y + s.z2;
        s.z2 = b.det.b2 * x - b.det.a2 * y;
        return y;
    };

    for (int i = 0; i < n; ++i)
    {
        double l = 0, r = 0;
        if (srcL != nullptr && srcR != nullptr) { l = srcL[i]; r = srcR[i]; }

        double x;
        switch (p.channel)
        {
            case Channel::Left:  x = std::abs (filt (0, l)); break;
            case Channel::Right: x = std::abs (filt (0, r)); break;
            case Channel::Mid:   x = std::abs (filt (0, 0.5 * (l + r))); break;
            case Channel::Side:  x = std::abs (filt (0, 0.5 * (l - r))); break;
            case Channel::Stereo:
            default:             x = std::max (std::abs (filt (0, l)), std::abs (filt (1, r))); break;
        }

        b.env     = x > b.env     ? att  * b.env     + (1 - att)  * x : rel  * b.env     + (1 - rel)  * x;
        b.fastEnv = x > b.fastEnv ? fAtt * b.fastEnv + (1 - fAtt) * x : fRel * b.fastEnv + (1 - fRel) * x;
        b.slowEnv = x > b.slowEnv ? sAtt * b.slowEnv + (1 - sAtt) * x : sRel * b.slowEnv + (1 - sRel) * x;
    }
    for (auto& s : b.detSt)
    {
        if (std::abs (s.z1) < 1e-30) s.z1 = 0;
        if (std::abs (s.z2) < 1e-30) s.z2 = 0;
    }

    // gain computer (two-way thresholds)
    const double lvl = gainToDb (b.env);
    const double over = lvl - p.threshold;
    const double k = 1.0 - 1.0 / std::max (1.0, p.ratio);
    const double R = std::abs (p.range);
    const double sgn = p.range < 0 ? -1.0 : 1.0;
    const double up = std::min (R, std::max (0.0, over) * k);
    const double down = std::min (R, std::max (0.0, -over) * k);

    double d = 0.0;
    switch (p.dynMode)
    {
        case DynMode::Above:  d = sgn * up; break;
        case DynMode::Below:  d = sgn * down; break;
        case DynMode::TwoWay: d = sgn * up - sgn * down; break;
    }

    // onset / body split: how transient the detector signal is right now (0..1)
    const double t = std::min (1.0, std::max (0.0, (gainToDb (b.fastEnv) - gainToDb (b.slowEnv)) / 9.0));
    const double active = std::min (1.0, std::max (0.0, over / 6.0 + 1.0));
    d += (p.onset / 100.0) * R * t * active;
    d += (p.body / 100.0) * R * (1.0 - t) * active;

    b.dynTarget = std::min (24.0, std::max (-24.0, d));
}

void Engine::updateBand (Band& b, int index, int n)
{
    const auto& tg = b.target;
    const bool typeChanged = ! b.initialised || tg.type != b.cur.type || tg.slope != b.cur.slope
                          || tg.channel != b.cur.channel || tg.enabled != b.cur.enabled;
    if (typeChanged)
    {
        b.clearState();
        b.logF = std::log10 (tg.freq);
        b.gain = tg.gain;
        b.logQ = std::log10 (tg.q);
        b.initialised = true;
        b.designedSlope = -1;
    }

    const double wasDyn = b.dynDb;
    b.cur = tg;

    if (! tg.enabled) { live[(size_t) index].store (0.0f); liveDyn[(size_t) index].store (0.0f); return; }

    // parameter glide (25 ms)
    const double a = timeCoef (25.0, fs / n);
    const double tF = std::log10 (tg.freq), tQ = std::log10 (tg.q);
    b.logF = tF + (b.logF - tF) * a;
    b.gain = tg.gain + (b.gain - tg.gain) * a;
    b.logQ = tQ + (b.logQ - tQ) * a;
    if (std::abs (b.logF - tF) < 1e-5) b.logF = tF;
    if (std::abs (b.gain - tg.gain) < 1e-4) b.gain = tg.gain;
    if (std::abs (b.logQ - tQ) < 1e-5) b.logQ = tQ;

    // dynamics glide (1 ms) towards the gain computer's target
    const bool dynActive = tg.dynOn && typeUsesGain (tg.type) && ! tg.bypass;
    if (! dynActive) b.dynTarget = 0.0;
    const double ad = timeCoef (1.0, fs / n);
    b.dynDb = b.dynTarget + (b.dynDb - b.dynTarget) * ad;
    if (std::abs (b.dynDb - b.dynTarget) < 1e-4) b.dynDb = b.dynTarget;

    const double f = std::pow (10.0, b.logF), q = std::pow (10.0, b.logQ);
    const double gEff = b.gain * global.gainScale + b.dynDb;

    const bool needs = b.designedSlope != tg.slope || b.designedType != tg.type
                    || std::abs (f - b.designedF) > 1e-6 * f || std::abs (q - b.designedQ) > 1e-6
                    || std::abs (gEff - b.designedG) > 1e-4 || designedTransform != global.transform
                    || designedFs != fs || wasDyn != b.dynDb;
    if (needs)
    {
        BandParams p = tg;
        p.freq = f; p.q = q; p.gain = b.gain;
        designStatic (b.cas, p, global.gainScale, fs, global.transform, b.dynDb);
        b.designedType = tg.type; b.designedSlope = tg.slope;
        b.designedF = f; b.designedQ = q; b.designedG = gEff;
    }

    if (dynActive)
    {
        const bool self = tg.detect == Detect::Self;
        double dq = self ? std::max (0.5, q) : tg.detQ;
        double df = self ? f : tg.detFreq;
        if (self && (tg.type == FilterType::LowShelf || tg.type == FilterType::HighShelf
                     || tg.type == FilterType::TiltShelf || tg.type == FilterType::FlatTilt))
            dq = 0.5;
        b.det = bandPassFor (df, dq, fs, Transform::Matched);
    }

    live[(size_t) index].store ((float) gEff);
    liveDyn[(size_t) index].store ((float) b.dynDb);
}

void Engine::processBand (Band& b, double* L, double* R, int n)
{
    if (! b.cur.enabled || b.cur.bypass || b.cas.count == 0) return;
    switch (b.cur.channel)
    {
        case Channel::Stereo: runCascade (b.cas, b.st[0], L, n); runCascade (b.cas, b.st[1], R, n); break;
        case Channel::Left:   runCascade (b.cas, b.st[0], L, n); break;
        case Channel::Right:  runCascade (b.cas, b.st[0], R, n); break;
        case Channel::Mid:
        case Channel::Side:
        {
            for (int i = 0; i < n; ++i) { tmpA[(size_t) i] = 0.5 * (L[i] + R[i]); tmpB[(size_t) i] = 0.5 * (L[i] - R[i]); }
            runCascade (b.cas, b.st[0], b.cur.channel == Channel::Mid ? tmpA.data() : tmpB.data(), n);
            for (int i = 0; i < n; ++i) { L[i] = tmpA[(size_t) i] + tmpB[(size_t) i]; R[i] = tmpA[(size_t) i] - tmpB[(size_t) i]; }
            break;
        }
    }
}

void Engine::updateAudition (int n)
{
    const int solo = global.solo;
    const bool on = solo >= 0 && solo < kNumBands && bands[(size_t) solo].cur.enabled;
    if (on)
    {
        const auto& b = bands[(size_t) solo];
        if (audition.band != solo)
        {
            for (auto& ch : audition.st) for (auto& s : ch) s = {};
            audition.band = solo;
        }
        const double f = std::pow (10.0, b.logF), q = std::pow (10.0, b.logQ);
        auto& c = audition.cas;
        c.clear();
        audition.usePre = false;
        auto add = [&] (FilterType t, double freq, double qq)
        {
            Cascade tmp;
            designBand (tmp, { t, freq, 0.0, qq, 12 }, fs, Transform::Matched);
            for (int i = 0; i < tmp.count; ++i) c.push (tmp.s[(size_t) i]);
        };
        switch (b.cur.type)
        {
            case FilterType::LowCut: case FilterType::BrickwallLow:   // hear what is being removed
                audition.usePre = true; add (FilterType::HighCut, f, 0.707); add (FilterType::HighCut, f, 0.707); break;
            case FilterType::HighCut: case FilterType::BrickwallHigh:
                audition.usePre = true; add (FilterType::LowCut, f, 0.707); add (FilterType::LowCut, f, 0.707); break;
            case FilterType::LowShelf:  add (FilterType::HighCut, f * 1.5, 0.707); add (FilterType::HighCut, f * 1.5, 0.707); break;
            case FilterType::HighShelf: add (FilterType::LowCut, f / 1.5, 0.707); add (FilterType::LowCut, f / 1.5, 0.707); break;
            case FilterType::Notch: case FilterType::BandPass: case FilterType::Allpass:
                audition.usePre = true; add (FilterType::BandPass, f, std::max (0.7, q)); add (FilterType::BandPass, f, std::max (0.7, q)); break;
            case FilterType::TiltShelf: case FilterType::FlatTilt:
                add (FilterType::BandPass, f, 0.5); break;
            default:
            {
                const double qa = std::min (30.0, std::max (0.7, b.cur.type == FilterType::Sword ? q * 2.0 : q));
                add (FilterType::BandPass, f, qa); add (FilterType::BandPass, f, qa);
                break;
            }
        }
    }
    const double target = on ? 1.0 : 0.0;
    const double a = timeCoef (15.0, fs / n);
    audition.mix = target + (audition.mix - target) * a;
    if (std::abs (audition.mix - target) < 1e-4) audition.mix = target;
    if (audition.mix == 0.0 && ! on) audition.band = -1;
}

void Engine::computeAutoGain()
{
    // average of the static curve over 20 Hz..20 kHz on a log axis (equal weight per octave)
    std::array<double, 48> sum {};
    Cascade c;
    for (const auto& b : bands)
    {
        const auto& p = b.target;
        if (! p.enabled || p.bypass || ! typeUsesGain (p.type)) continue;
        const double w = p.channel == Channel::Stereo || p.channel == Channel::Mid ? 1.0
                       : p.channel == Channel::Side ? 0.25 : 0.5;
        designStatic (c, p, global.gainScale, fs, global.transform);
        for (size_t i = 0; i < sum.size(); ++i)
        {
            const double hz = 20.0 * std::pow (1000.0, (double) i / (double) (sum.size() - 1));
            if (hz >= 0.49 * fs) break;
            sum[i] += w * c.magnitudeDb (2.0 * kPi * hz / fs);
        }
    }
    double avg = 0;
    int count = 0;
    for (size_t i = 0; i < sum.size(); ++i)
    {
        const double hz = 20.0 * std::pow (1000.0, (double) i / (double) (sum.size() - 1));
        if (hz >= 0.49 * fs) break;
        avg += sum[i];
        ++count;
    }
    autoTarget = count > 0 ? -avg / count : 0.0;
}

void Engine::process (double* L, double* R, const double* scL, const double* scR, int numSamples)
{
    if (designedFs != fs) { designedFs = 0.0; }

    for (int start = 0; start < numSamples; start += kSubBlock)
    {
        const int n = std::min (kSubBlock, numSamples - start);
        double* l = L + start;
        double* r = R + start;
        const double* sl = scL != nullptr ? scL + start : nullptr;
        const double* sr = scR != nullptr ? scR + start : nullptr;

        // 1. detectors look at the undelayed input
        for (auto& b : bands)
            if (b.cur.enabled && b.cur.dynOn && typeUsesGain (b.cur.type) && ! b.cur.bypass)
                runDetector (b, l, r, sl, sr, n);

        // 2. glide parameters / redesign
        for (int i = 0; i < kNumBands; ++i) updateBand (bands[(size_t) i], i, n);
        designedTransform = global.transform;
        designedFs = fs;

        // 3. lookahead delay on the main path
        if (lookahead > 0)
        {
            const int size = (int) delayL.size();
            for (int i = 0; i < n; ++i)
            {
                delayL[(size_t) delayPos] = l[i];
                delayR[(size_t) delayPos] = r[i];
                int rp = delayPos - lookahead;
                if (rp < 0) rp += size;
                l[i] = delayL[(size_t) rp];
                r[i] = delayR[(size_t) rp];
                if (++delayPos >= size) delayPos = 0;
            }
        }

        for (int i = 0; i < n; ++i) { preL[(size_t) i] = l[i]; preR[(size_t) i] = r[i]; }

        // 4. the bands, in order
        for (auto& b : bands) processBand (b, l, r, n);

        // 5. audition (band solo)
        updateAudition (n);
        if (audition.mix > 0.0)
        {
            const double* srcL = audition.usePre ? preL.data() : l;
            const double* srcR = audition.usePre ? preR.data() : r;
            for (int i = 0; i < n; ++i) { tmpA[(size_t) i] = srcL[i]; tmpB[(size_t) i] = srcR[i]; }
            runCascade (audition.cas, audition.st[0], tmpA.data(), n);
            runCascade (audition.cas, audition.st[1], tmpB.data(), n);
            const double m = audition.mix;
            for (int i = 0; i < n; ++i)
            {
                l[i] += m * (tmpA[(size_t) i] - l[i]);
                r[i] += m * (tmpB[(size_t) i] - r[i]);
            }
        }

        // 6. width, monitoring, output gain, auto gain
        if (staticDirty && --autoCountdown <= 0)
        {
            if (global.autoGain) computeAutoGain(); else autoTarget = 0.0;
            staticDirty = false;
            autoCountdown = (int) (0.02 * fs / kSubBlock); // at most every 20 ms
        }
        const double ag = timeCoef (60.0, fs / n);
        autoComp = autoTarget + (autoComp - autoTarget) * ag;
        liveAuto.store ((float) autoComp);
        const double targetGain = dbToGain (global.outputDb + (global.autoGain ? autoComp : 0.0));
        const double og = timeCoef (10.0, fs);

        for (int i = 0; i < n; ++i)
        {
            double m = 0.5 * (l[i] + r[i]);
            double s = 0.5 * (l[i] - r[i]) * global.width;
            if (global.monitor == Monitor::Mid) s = 0.0;
            else if (global.monitor == Monitor::Side) { m = s; s = 0.0; }
            outGain = targetGain + (outGain - targetGain) * og;
            l[i] = (m + s) * outGain;
            r[i] = (m - s) * outGain;
        }
    }
}

} // namespace qtip
