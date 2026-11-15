// Q-TIP - filter design (pure C++17, no JUCE).
//
// Every filter shape is described as a cascade of analog first/second-order
// sections. Each analog section is turned into a digital biquad by one of two
// transforms:
//
//   * Matched ("Analog" phase mode): poles by impulse invariance, numerator
//     solved so the digital magnitude equals the analog magnitude exactly at
//     DC, at Nyquist and at the section's own frequency. This removes the
//     high-frequency "cramping" of the bilinear transform (after M. Vicanek,
//     "Matched Second Order Digital Filters", 2016).
//   * Bilinear ("Zero Latency" phase mode): the classic pre-warped bilinear
//     transform, kept for comparison.
//
// All maths is double precision.
#pragma once

#include <array>
#include <cmath>
#include <complex>

namespace qtip
{

constexpr double kPi = 3.14159265358979323846;

enum class FilterType : int
{
    Bell = 0,
    LowShelf,
    HighShelf,
    LowCut,        // high pass, variable slope
    HighCut,       // low pass, variable slope
    Notch,
    BandPass,
    TiltShelf,
    FlatTilt,
    FlatTop,
    Sword,
    BrickwallLow,  // 96 dB/oct high pass
    BrickwallHigh, // 96 dB/oct low pass
    Allpass,
    ProportionalBell,
    Count
};

inline const char* filterTypeName (FilterType t)
{
    static const char* names[] = { "Bell", "Low Shelf", "High Shelf", "Low Cut", "High Cut",
                                   "Notch", "Band Pass", "Tilt Shelf", "Flat Tilt", "Flat Top",
                                   "Sword", "Brickwall Low", "Brickwall High", "Allpass",
                                   "Proportional Bell" };
    const int i = (int) t;
    return (i >= 0 && i < (int) FilterType::Count) ? names[i] : "?";
}

inline const char* filterTypeShort (FilterType t)
{
    static const char* names[] = { "Bell", "LS", "HS", "LC", "HC", "Notch", "BP", "Tilt", "FTilt",
                                   "FTop", "Sword", "BW-L", "BW-H", "AP", "PBell" };
    const int i = (int) t;
    return (i >= 0 && i < (int) FilterType::Count) ? names[i] : "?";
}

/** True for shapes whose gain parameter does something (and that dynamics can drive). */
inline bool typeUsesGain (FilterType t)
{
    switch (t)
    {
        case FilterType::LowCut: case FilterType::HighCut: case FilterType::Notch:
        case FilterType::BandPass: case FilterType::BrickwallLow: case FilterType::BrickwallHigh:
        case FilterType::Allpass:
            return false;
        default:
            return true;
    }
}

inline bool typeUsesSlope (FilterType t) { return t == FilterType::LowCut || t == FilterType::HighCut; }

enum class Transform : int { Bilinear = 0, Matched = 1 };

struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;

    std::complex<double> response (double w) const
    {
        const std::complex<double> z1 = std::polar (1.0, -w), z2 = z1 * z1;
        return (b0 + b1 * z1 + b2 * z2) / (1.0 + a1 * z1 + a2 * z2);
    }
};

/** A designed band: up to kMaxSections biquads plus a scalar gain. */
struct Cascade
{
    static constexpr int kMaxSections = 10;
    std::array<Biquad, kMaxSections> s {};
    int count = 0;
    double scale = 1.0;
    bool isAllpassOnly = false;

    void clear() { count = 0; scale = 1.0; isAllpassOnly = false; }
    void push (const Biquad& b) { if (count < kMaxSections) s[(size_t) count++] = b; }

    std::complex<double> response (double w) const
    {
        std::complex<double> h (scale, 0.0);
        for (int i = 0; i < count; ++i) h *= s[(size_t) i].response (w);
        return h;
    }

    double magnitudeDb (double w) const
    {
        return 20.0 * std::log10 (std::max (1e-12, std::abs (response (w))));
    }
};

/** Analog section n(s)/d(s) with s in rad/sample units (fs = 1). Second order:
    n2 s^2 + n1 s + n0 over d2 s^2 + d1 s + d0. First order: n2 = d2 = 0. */
struct AnalogSection
{
    double n2, n1, n0, d2, d1, d0;
    double matchW;    // frequency (rad/sample) where the matched design is pinned
    bool allpass = false;

    bool firstOrder() const { return d2 == 0.0; }

    double magSq (double w) const
    {
        const double nr = n0 - n2 * w * w, ni = n1 * w;
        const double dr = d0 - d2 * w * w, di = d1 * w;
        return (nr * nr + ni * ni) / std::max (1e-300, dr * dr + di * di);
    }
};

namespace detail
{
    inline double clampW (double w) { return std::min (std::max (w, 1e-7), 0.995 * kPi); }

    inline Biquad bilinear (const AnalogSection& a)
    {
        const double wp = clampW (a.matchW);
        const double K = wp / std::tan (wp * 0.5); // pre-warp at the section's frequency
        Biquad q;
        if (a.firstOrder())
        {
            const double b0 = a.n1 * K + a.n0, b1 = a.n0 - a.n1 * K;
            const double a0 = a.d1 * K + a.d0, a1 = a.d0 - a.d1 * K;
            q.b0 = b0 / a0; q.b1 = b1 / a0; q.b2 = 0; q.a1 = a1 / a0; q.a2 = 0;
            return q;
        }
        const double K2 = K * K;
        const double b0 = a.n2 * K2 + a.n1 * K + a.n0;
        const double b1 = 2.0 * (a.n0 - a.n2 * K2);
        const double b2 = a.n2 * K2 - a.n1 * K + a.n0;
        const double a0 = a.d2 * K2 + a.d1 * K + a.d0;
        const double a1 = 2.0 * (a.d0 - a.d2 * K2);
        const double a2 = a.d2 * K2 - a.d1 * K + a.d0;
        q.b0 = b0 / a0; q.b1 = b1 / a0; q.b2 = b2 / a0; q.a1 = a1 / a0; q.a2 = a2 / a0;
        return q;
    }

    /** Reflects numerator zeros that lie outside the unit circle to the inside.
        The magnitude response is unchanged; the section becomes minimum phase,
        which keeps its inverse stable. */
    inline void makeMinimumPhase (Biquad& q)
    {
        if (q.b0 == 0.0) return;
        const double disc = q.b1 * q.b1 - 4.0 * q.b0 * q.b2;
        if (disc < 0.0)
        {
            if (std::abs (q.b2) > std::abs (q.b0)) std::swap (q.b0, q.b2); // conjugate pair outside
            return;
        }
        const double sq = std::sqrt (disc);
        // roots of z^2 + (b1/b0) z + b2/b0
        double r1 = (-q.b1 + sq) / (2.0 * q.b0), r2 = (-q.b1 - sq) / (2.0 * q.b0);
        double k = q.b0;
        if (std::abs (r1) > 1.0) { k *= r1; r1 = 1.0 / r1; }
        if (std::abs (r2) > 1.0) { k *= r2; r2 = 1.0 / r2; }
        q.b0 = k; q.b1 = -k * (r1 + r2); q.b2 = k * r1 * r2;
    }

    inline Biquad matched (const AnalogSection& a)
    {
        Biquad q;
        if (a.firstOrder())
        {
            const double wp = std::min (a.d0 / a.d1, 0.98 * kPi);
            const double p = std::exp (-wp);
            const double A1 = -p;
            const double t0 = std::sqrt (a.magSq (0.0)), t1 = std::sqrt (a.magSq (kPi));
            q.b0 = 0.5 * (t0 * (1 + A1) + t1 * (1 - A1));
            q.b1 = 0.5 * (t0 * (1 + A1) - t1 * (1 - A1));
            q.b2 = 0; q.a1 = A1; q.a2 = 0;
            return q;
        }

        // poles by impulse invariance of the analog denominator
        double wp = std::sqrt (a.d0 / a.d2);
        const double qp = wp / (a.d1 / a.d2);
        wp = std::min (wp, 0.98 * kPi);
        const double zeta = 1.0 / (2.0 * qp);
        const double e = std::exp (-zeta * wp);
        if (zeta <= 1.0) q.a1 = -2.0 * e * std::cos (std::sqrt (1.0 - zeta * zeta) * wp);
        else             q.a1 = -2.0 * e * std::cosh (std::sqrt (zeta * zeta - 1.0) * wp);
        q.a2 = e * e;

        if (a.allpass) { q.b0 = q.a2; q.b1 = q.a1; q.b2 = 1.0; return q; }

        // |B|^2 = B0 phi0 + B1 phi1 + B2 phi2 with phi1 = sin^2(w/2), phi0 = 1-phi1, phi2 = 4 phi0 phi1
        const double A0 = (1 + q.a1 + q.a2) * (1 + q.a1 + q.a2);
        const double A1 = (1 - q.a1 + q.a2) * (1 - q.a1 + q.a2);
        const double A2 = -4.0 * q.a2;

        const double wm = std::min (std::max (a.matchW, 1e-4), 0.9 * kPi);
        const double s = std::sin (wm * 0.5);
        const double p1 = s * s, p0 = 1.0 - p1, p2 = 4.0 * p0 * p1;

        const double B0 = a.magSq (0.0) * A0;
        const double B1 = a.magSq (kPi) * A1;
        const double Am = A0 * p0 + A1 * p1 + A2 * p2;
        const double B2 = (a.magSq (wm) * Am - B0 * p0 - B1 * p1) / p2;

        const double sB0 = std::sqrt (std::max (0.0, B0)), sB1 = std::sqrt (std::max (0.0, B1));
        const double W = 0.5 * (sB0 + sB1);
        const double disc = std::max (0.0, W * W + B2);
        q.b0 = 0.5 * (W + std::sqrt (disc));
        q.b1 = 0.5 * (sB0 - sB1);
        q.b2 = (q.b0 != 0.0) ? -B2 / (4.0 * q.b0) : 0.0;
        makeMinimumPhase (q);
        return q;
    }

    inline Biquad toDigital (const AnalogSection& a, Transform t)
    {
        return t == Transform::Matched ? matched (a) : bilinear (a);
    }

    /** 1/H: a cut is designed as the matching boost, inverted (more accurate for the
        matched transform, and exact reciprocity between boost and cut). */
    inline Biquad invert (const Biquad& q)
    {
        Biquad r;
        const double n = q.b0;
        r.b0 = 1.0 / n; r.b1 = q.a1 / n; r.b2 = q.a2 / n;
        r.a1 = q.b1 / n; r.a2 = q.b2 / n;
        return r;
    }

    template <typename MakeSection>
    inline Biquad gainSection (MakeSection make, double gainDb, Transform t)
    {
        if (gainDb >= 0.0) return toDigital (make (gainDb), t);
        return invert (toDigital (make (-gainDb), t));
    }

    // ---- analog prototypes; w0 in rad/sample, A = 10^(gainDb/40)
    inline AnalogSection bell (double w0, double gainDb, double Q)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        return { 1, A * w0 / Q, w0 * w0, 1, w0 / (A * Q), w0 * w0, w0 };
    }
    inline AnalogSection lowShelf (double w0, double gainDb, double Q)
    {
        const double A = std::pow (10.0, gainDb / 40.0), sA = std::sqrt (A);
        return { A, A * sA / Q * w0, A * A * w0 * w0, A, sA / Q * w0, w0 * w0, w0 };
    }
    inline AnalogSection highShelf (double w0, double gainDb, double Q)
    {
        const double A = std::pow (10.0, gainDb / 40.0), sA = std::sqrt (A);
        return { A * A, A * sA / Q * w0, A * w0 * w0, 1, sA / Q * w0, A * w0 * w0, w0 };
    }
    inline AnalogSection highPass2 (double w0, double Q) { return { 1, 0, 0, 1, w0 / Q, w0 * w0, w0 }; }
    inline AnalogSection lowPass2  (double w0, double Q) { return { 0, 0, w0 * w0, 1, w0 / Q, w0 * w0, w0 }; }
    inline AnalogSection highPass1 (double w0) { return { 0, 1, 0, 0, 1, w0, w0 }; }
    inline AnalogSection lowPass1  (double w0) { return { 0, 0, w0, 0, 1, w0, w0 }; }
    inline AnalogSection notch     (double w0, double Q) { return { 1, 0, w0 * w0, 1, w0 / Q, w0 * w0, w0 }; }
    inline AnalogSection bandPass  (double w0, double Q) { return { 0, w0 / Q, 0, 1, w0 / Q, w0 * w0, w0 }; }
    inline AnalogSection allpass2  (double w0, double Q)
    {
        AnalogSection a { 1, -w0 / Q, w0 * w0, 1, w0 / Q, w0 * w0, w0 };
        a.allpass = true;
        return a;
    }

    /** Butterworth cascade of the given order (1..16). Q scales the most resonant section. */
    inline void butterworth (Cascade& c, bool highPass, double w0, int order, double userQ, Transform t)
    {
        const int pairs = order / 2;
        const double qScale = userQ / 0.70710678118654752;
        for (int k = 1; k <= pairs; ++k)
        {
            double q = 1.0 / (2.0 * std::sin ((2.0 * k - 1.0) * kPi / (2.0 * order)));
            if (k == 1) q *= qScale; // k == 1 is the highest-Q pole pair
            c.push (toDigital (highPass ? highPass2 (w0, q) : lowPass2 (w0, q), t));
        }
        if (order % 2 == 1)
            c.push (toDigital (highPass ? highPass1 (w0) : lowPass1 (w0), t));
    }
} // namespace detail

struct BandDesign
{
    FilterType type = FilterType::Bell;
    double freqHz = 1000.0;
    double gainDb = 0.0;
    double q = 0.707;
    int slopeDbPerOct = 12; // 6..96 in 6 dB steps
};

/** Designs a band into a cascade. sampleRate is the rate the filter runs at. */
inline void designBand (Cascade& c, const BandDesign& d, double sampleRate, Transform t)
{
    using namespace detail;
    c.clear();
    const double nyq = 0.5 * sampleRate;
    const double f = std::min (std::max (d.freqHz, 5.0), 0.49 * sampleRate);
    const double w0 = 2.0 * kPi * f / sampleRate;
    const double Q = std::min (std::max (d.q, 0.025), 60.0);
    const double g = d.gainDb;

    const auto B  = [t] (double w, double gg, double qq) { return gainSection ([&] (double x) { return bell (w, x, qq); }, gg, t); };
    const auto LS = [t] (double w, double gg, double qq) { return gainSection ([&] (double x) { return lowShelf (w, x, qq); }, gg, t); };
    const auto HS = [t] (double w, double gg, double qq) { return gainSection ([&] (double x) { return highShelf (w, x, qq); }, gg, t); };

    switch (d.type)
    {
        case FilterType::Bell:
            c.push (B (w0, g, Q));
            break;

        case FilterType::ProportionalBell:
        {
            // analog-console style: narrower as the gain grows
            const double qp = Q * (0.45 + 0.55 * std::min (1.0, std::abs (g) / 12.0)) * 1.6;
            c.push (B (w0, g, qp));
            break;
        }

        case FilterType::LowShelf:  c.push (LS (w0, g, Q));  break;
        case FilterType::HighShelf: c.push (HS (w0, g, Q)); break;

        case FilterType::LowCut:
        case FilterType::HighCut:
        {
            const int order = std::min (16, std::max (1, d.slopeDbPerOct / 6));
            butterworth (c, d.type == FilterType::LowCut, w0, order, Q, t);
            break;
        }

        case FilterType::BrickwallLow:  butterworth (c, true,  w0, 16, 0.70710678118654752, t); break;
        case FilterType::BrickwallHigh: butterworth (c, false, w0, 16, 0.70710678118654752, t); break;

        case FilterType::Notch:    c.push (toDigital (notch (w0, Q), t)); break;
        case FilterType::BandPass: c.push (toDigital (bandPass (w0, Q), t)); break;

        case FilterType::Allpass:
            c.push (toDigital (allpass2 (w0, Q), t));
            c.isAllpassOnly = true;
            break;

        case FilterType::TiltShelf:
            c.push (LS (w0, -0.5 * g, 0.5));
            c.push (HS (w0, 0.5 * g, 0.5));
            break;

        case FilterType::FlatTilt:
        {
            // a straight dB/oct line across the audio band: six gentle shelves
            // spread over 20 Hz..20 kHz, re-centred so the pivot (freq) stays at 0 dB.
            // gainDb = total tilt from 20 Hz to 20 kHz.
            const double centres[] = { 35.0, 110.0, 350.0, 1100.0, 3500.0, 11000.0 };
            const double each = g / 6.0;
            for (double fc : centres)
            {
                const double wc = 2.0 * kPi * std::min (fc, 0.45 * sampleRate) / sampleRate;
                c.push (HS (wc, each, 0.4));
            }
            c.scale = 1.0;
            const double atPivot = c.magnitudeDb (w0);
            c.scale = std::pow (10.0, -atPivot / 20.0);
            break;
        }

        case FilterType::FlatTop:
        {
            // plateau of `g` dB, width 2/Q octaves, built from two shelves
            const double width = std::min (6.0, std::max (0.2, 2.0 / Q));
            const double fLo = f * std::pow (2.0, -0.5 * width), fHi = std::min (f * std::pow (2.0, 0.5 * width), 0.49 * sampleRate);
            c.push (HS (2.0 * kPi * fLo / sampleRate, g, 0.9));
            c.push (HS (2.0 * kPi * fHi / sampleRate, -g, 0.9));
            break;
        }

        case FilterType::Sword:
            // sharp tip on a broader base: digs out resonances without hollowing the area around them
            c.push (B (w0, 0.72 * g, Q * 3.0));
            c.push (B (w0, 0.28 * g, Q * 0.6));
            break;

        case FilterType::Count:
            break;
    }
    (void) nyq;
}

} // namespace qtip
