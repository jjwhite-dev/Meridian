#include "EqDisplay.h"
#include "Theme.h"
#include "../PluginProcessor.h"

namespace
{
    constexpr double kMinF = 20.0, kMaxF = 20000.0;
    constexpr float kSpecTop = 6.0f, kSpecBottom = -96.0f, kSpecTilt = 4.5f; // dB/oct around 1 kHz
    constexpr float kNodeRadius = 11.0f;
    namespace pb = params::band;
}

EqDisplay::EqDisplay (MeridianProcessor& p) : proc (p)
{
    setAnalyzerOrder (12);
    setOpaque (true);
}

void EqDisplay::setAnalyzerOrder (int order)
{
    fftOrder = juce::jlimit (10, 14, order);
    const int n = 1 << fftOrder;
    fft = std::make_unique<juce::dsp::FFT> (fftOrder);
    window = std::make_unique<juce::dsp::WindowingFunction<float>> ((size_t) n, juce::dsp::WindowingFunction<float>::hann, false);
    fftData.assign ((size_t) n * 2, 0.0f);
    scratch.assign (1 << 15, 0.0f);
    for (auto* s : { &pre, &post, &side })
    {
        s->ring.assign ((size_t) n, 0.0f);
        s->db.assign ((size_t) n / 2 + 1, -120.0f);
        s->write = 0;
    }
}

void EqDisplay::setSelected (int band)
{
    selected = band;
    repaint();
}

// ------------------------------------------------------------------ mapping
float EqDisplay::xForFreq (double f) const
{
    return (float) (std::log (f / kMinF) / std::log (kMaxF / kMinF) * getWidth());
}

double EqDisplay::freqForX (float x) const
{
    return kMinF * std::pow (kMaxF / kMinF, juce::jlimit (0.0, 1.0, (double) x / juce::jmax (1, getWidth())));
}

float EqDisplay::yForDb (double db) const
{
    const float h = (float) getHeight(), pad = 26.0f;
    return h * 0.5f - (float) db * (h * 0.5f - pad) / dbRange;
}

double EqDisplay::dbForY (float y) const
{
    const float h = (float) getHeight(), pad = 26.0f;
    return (h * 0.5f - y) * dbRange / (h * 0.5f - pad);
}

float EqDisplay::ySpec (double db, double f) const
{
    const double tilted = db + kSpecTilt * std::log2 (f / 1000.0);
    return (float) ((kSpecTop - tilted) / (kSpecTop - kSpecBottom) * getHeight());
}

double EqDisplay::specDbAt (const Spectrum& s, double f) const
{
    const int n = 1 << fftOrder;
    const double bin = f * n / specRate;
    const int i = juce::jlimit (0, (int) s.db.size() - 2, (int) bin);
    const double t = juce::jlimit (0.0, 1.0, bin - i);
    return s.db[(size_t) i] * (1 - t) + s.db[(size_t) i + 1] * t;
}

// ------------------------------------------------------------------ analyzer
void EqDisplay::pull (AnalyzerFifo& fifo, Spectrum& s)
{
    const int got = fifo.pull (scratch.data(), (int) scratch.size());
    const int n = (int) s.ring.size();
    for (int i = 0; i < got; ++i)
    {
        s.ring[(size_t) s.write] = scratch[(size_t) i];
        if (++s.write >= n) s.write = 0;
    }
    if (got > 0) { s.active = true; s.silentFrames = 0; }
    else if (++s.silentFrames > 30) s.active = false;
}

void EqDisplay::analyse (Spectrum& s)
{
    const int n = 1 << fftOrder;
    for (int i = 0; i < n; ++i) fftData[(size_t) i] = s.ring[(size_t) ((s.write + i) % n)];
    std::fill (fftData.begin() + n, fftData.end(), 0.0f);
    window->multiplyWithWindowingTable (fftData.data(), (size_t) n);
    fft->performFrequencyOnlyForwardTransform (fftData.data());

    const float norm = 4.0f / (float) n; // hann coherent gain 0.5, one-sided spectrum
    for (size_t k = 0; k < s.db.size(); ++k)
    {
        const float db = 20.0f * std::log10 (fftData[k] * norm + 1e-9f);
        float& v = s.db[k];
        v = db > v ? db : v + (db - v) * 0.22f;  // fast rise, smooth fall
    }
}

void EqDisplay::refresh()
{
    specRate = proc.baseSampleRate();
    pull (proc.preFifo, pre);
    pull (proc.postFifo, post);
    pull (proc.sidechainFifo, side);
    analyse (pre);
    analyse (post);
    if (side.active) analyse (side);
    rebuildCurves();
    repaint();
}

void EqDisplay::rebuildCurves()
{
    rate = proc.engineSampleRate();
    gainScale = proc.apvts.getRawParameterValue (params::gainScale)->load() / 100.0;
    const auto transform = proc.currentTransform();

    const size_t np = pointFreqs.size();
    totalDb.assign (np, 0.0f);
    liveDb.assign (np, 0.0f);
    selDb.assign (np, 0.0f);
    selRangeDb.clear();
    anyDynamic = false;

    meq::Cascade live;
    for (int b = 0; b < meq::kNumBands; ++b)
    {
        bands[(size_t) b] = proc.readBand (b);
        const auto& p = bands[(size_t) b];
        auto& c = cascades[(size_t) b];
        if (! p.enabled) { c.clear(); continue; }
        meq::Engine::designStatic (c, p, gainScale, rate, transform);

        const bool dyn = p.dynOn && meq::typeUsesGain (p.type) && ! p.bypass;
        if (dyn)
        {
            anyDynamic = true;
            meq::Engine::designStatic (live, p, gainScale, rate, transform, proc.liveDynDb (b));
        }

        const bool counts = ! p.bypass && ! c.isAllpassOnly;
        for (size_t i = 0; i < np; ++i)
        {
            const double w = 2.0 * meq::kPi * pointFreqs[i] / rate;
            const float d = (pointFreqs[i] < 0.5 * rate) ? (float) c.magnitudeDb (w) : -120.0f;
            if (counts) totalDb[i] += d;
            if (counts) liveDb[i] += dyn ? (pointFreqs[i] < 0.5 * rate ? (float) live.magnitudeDb (w) : -120.0f) : d;
            if (b == selected) selDb[i] = d;
        }
    }

    // area the selected band's dynamics can sweep (static curve vs. static + range)
    if (juce::isPositiveAndBelow (selected, meq::kNumBands))
    {
        const auto& p = bands[(size_t) selected];
        if (p.enabled && p.dynOn && meq::typeUsesGain (p.type))
        {
            meq::Cascade ranged;
            meq::Engine::designStatic (ranged, p, gainScale, rate, transform, proc.bandValue (selected, pb::range));
            selRangeDb.resize (np);
            for (size_t i = 0; i < np; ++i)
            {
                const double w = 2.0 * meq::kPi * pointFreqs[i] / rate;
                selRangeDb[i] = totalDb[i] - selDb[i] + (pointFreqs[i] < 0.5 * rate ? (float) ranged.magnitudeDb (w) : -120.0f);
            }
        }
    }
}

void EqDisplay::resized()
{
    const int np = juce::jmax (2, getWidth() / 2);
    pointFreqs.resize ((size_t) np);
    for (int i = 0; i < np; ++i) pointFreqs[(size_t) i] = freqForX ((float) i * getWidth() / (float) (np - 1));
    dbLabelArea = getLocalBounds().removeFromRight (44);
    rebuildCurves();
}

// ------------------------------------------------------------------ painting helpers
juce::Path EqDisplay::spectrumPath (const Spectrum& s, bool closed) const
{
    juce::Path path;
    const int n = 1 << fftOrder;
    const float w = (float) getWidth(), h = (float) getHeight();
    bool started = false;
    for (float x = 0; x <= w; x += 2.0f)
    {
        const double f0 = freqForX (x), f1 = freqForX (x + 2.0f);
        int b0 = (int) (f0 * n / specRate), b1 = (int) (f1 * n / specRate);
        b0 = juce::jlimit (1, (int) s.db.size() - 1, b0);
        b1 = juce::jlimit (b0, (int) s.db.size() - 1, b1);
        double db;
        if (b1 > b0) { db = -200; for (int k = b0; k <= b1; ++k) db = juce::jmax (db, (double) s.db[(size_t) k]); }
        else db = specDbAt (s, f0);
        const float y = juce::jlimit (-2.0f, h + 2.0f, ySpec (db, f0));
        if (! started) { path.startNewSubPath (x, closed ? h : y); if (closed) path.lineTo (x, y); started = true; }
        else path.lineTo (x, y);
    }
    if (closed) { path.lineTo (w, h); path.closeSubPath(); }
    return path;
}

juce::Path EqDisplay::curvePath (const std::vector<float>& db) const
{
    juce::Path path;
    const size_t np = db.size();
    const float h = (float) getHeight();
    for (size_t i = 0; i < np; ++i)
    {
        const float x = (float) i * getWidth() / (float) (np - 1);
        const float y = juce::jlimit (-4.0f, h + 4.0f, yForDb (db[i]));
        if (i == 0) path.startNewSubPath (x, y); else path.lineTo (x, y);
    }
    return path;
}

juce::Point<float> EqDisplay::nodePosition (int b) const
{
    const auto& p = bands[(size_t) b];
    const float x = xForFreq (juce::jlimit (kMinF, kMaxF, p.freq));
    double db;
    if (meq::typeUsesGain (p.type) && p.type != meq::FilterType::FlatTilt) db = p.gain * gainScale;
    else db = cascades[(size_t) b].count > 0 && p.freq < 0.5 * rate
                  ? juce::jmax (-dbRange, (float) cascades[(size_t) b].magnitudeDb (2.0 * meq::kPi * p.freq / rate)) : 0.0;
    if (p.type == meq::FilterType::Allpass || p.type == meq::FilterType::FlatTilt) db = 0.0;
    const float y = juce::jlimit (14.0f, (float) getHeight() - 14.0f, yForDb (db));
    return { x, y };
}

// ------------------------------------------------------------------ paint
void EqDisplay::paint (juce::Graphics& g)
{
    const float w = (float) getWidth(), h = (float) getHeight();
    g.fillAll (theme::display);

    // grid
    const double minor[] = { 30, 40, 60, 70, 80, 90, 300, 400, 600, 700, 800, 900, 3000, 4000, 6000, 7000, 8000, 9000 };
    const double major[] = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
    g.setColour (theme::gridMinor);
    for (double f : minor) g.drawVerticalLine (juce::roundToInt (xForFreq (f)), 0.0f, h);
    g.setColour (theme::gridMajor);
    for (double f : major) g.drawVerticalLine (juce::roundToInt (xForFreq (f)), 0.0f, h);
    const float step = dbRange <= 6 ? 2.0f : dbRange <= 12 ? 3.0f : dbRange <= 18 ? 6.0f : 10.0f;
    for (float d = step; d < dbRange; d += step)
    {
        g.drawHorizontalLine (juce::roundToInt (yForDb (d)), 0.0f, w);
        g.drawHorizontalLine (juce::roundToInt (yForDb (-d)), 0.0f, w);
    }
    g.setColour (juce::Colour (0xff2e333d));
    g.drawHorizontalLine (juce::roundToInt (yForDb (0)), 0.0f, w);

    // analyzer
    const float bright = brightness;
    if (pre.active)
    {
        g.setColour (juce::Colour (0xff2c3039).withMultipliedAlpha (bright));
        g.strokePath (spectrumPath (pre, false), juce::PathStrokeType (1.0f));
    }
    if (side.active && proc.sidechainConnected())
    {
        g.setColour (theme::amber.withAlpha (0.35f * bright));
        g.strokePath (spectrumPath (side, false), juce::PathStrokeType (1.0f));
    }
    if (post.active)
    {
        g.setColour (theme::muted.withAlpha (0.10f * bright));
        g.fillPath (spectrumPath (post, true));
        g.setColour (juce::Colour (0xff555b67).withMultipliedAlpha (bright));
        g.strokePath (spectrumPath (post, false), juce::PathStrokeType (1.0f));
    }

    const float y0 = yForDb (0);
    if (! totalDb.empty())
    {
        // selected band's own shape
        if (juce::isPositiveAndBelow (selected, meq::kNumBands) && bands[(size_t) selected].enabled)
        {
            auto sel = curvePath (selDb);
            sel.lineTo (w, y0); sel.lineTo (0, y0); sel.closeSubPath();
            g.setColour (theme::accent.withAlpha (0.10f));
            g.fillPath (sel);
        }
        // dynamic range of the selected band
        if (! selRangeDb.empty())
        {
            auto a = curvePath (totalDb);
            const size_t np = selRangeDb.size();
            for (size_t i = np; i-- > 0;)
                a.lineTo ((float) i * w / (float) (np - 1), juce::jlimit (-4.0f, h + 4.0f, yForDb (selRangeDb[i])));
            a.closeSubPath();
            g.setColour (theme::amber.withAlpha (0.22f));
            g.fillPath (a);
        }

        auto total = curvePath (totalDb);
        auto fill = total;
        fill.lineTo (w, y0); fill.lineTo (0, y0); fill.closeSubPath();
        g.setColour (theme::accent.withAlpha (0.05f));
        g.fillPath (fill);
        g.setColour (theme::accent);
        g.strokePath (total, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        if (anyDynamic)
        {
            g.setColour (theme::text.withAlpha (0.75f));
            g.strokePath (curvePath (liveDb), juce::PathStrokeType (1.2f));
        }
    }

    // audition: dim everything outside the band's range
    const int aud = proc.auditionBand.load();
    if (juce::isPositiveAndBelow (aud, meq::kNumBands) && bands[(size_t) aud].enabled)
    {
        const auto& p = bands[(size_t) aud];
        double lo = 0, hi = 0;
        using FT = meq::FilterType;
        switch (p.type)
        {
            case FT::LowCut: case FT::BrickwallLow:   lo = kMinF; hi = p.freq; break;
            case FT::HighCut: case FT::BrickwallHigh: lo = p.freq; hi = kMaxF; break;
            case FT::LowShelf:  lo = kMinF; hi = p.freq * 1.5; break;
            case FT::HighShelf: lo = p.freq / 1.5; hi = kMaxF; break;
            default:
            {
                const double bw = juce::jmax (0.12, 0.7 / p.q);
                lo = p.freq * std::pow (2.0, -bw); hi = p.freq * std::pow (2.0, bw);
                break;
            }
        }
        const float xl = juce::jmax (0.0f, xForFreq (lo)), xh = juce::jmin (w, xForFreq (hi));
        g.setColour (juce::Colour (0xa807080a));
        g.fillRect (0.0f, 0.0f, xl, h);
        g.fillRect (xh, 0.0f, w - xh, h);
        g.setColour (theme::accent.withAlpha (0.7f));
        if (xl > 0.5f) g.drawVerticalLine (juce::roundToInt (xl), 0.0f, h);
        if (xh < w - 0.5f) g.drawVerticalLine (juce::roundToInt (xh), 0.0f, h);

        juce::String text = "Auditioning band " + juce::String (aud + 1) + "  " + params::formatFrequency ((float) p.freq);
        if (meq::typeUsesGain (p.type)) text << "  Q " << juce::String (p.q, 2);
        if (proc.lockedSolo == aud && dragBand < 0) text << "  (solo locked)";
        g.setFont (theme::mono (11.0f));
        const int tw = juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), text) + 40;
        auto banner = juce::Rectangle<int> (tw, 26).withCentre ({ getWidth() / 2, 25 });
        g.setColour (juce::Colour (0xff16222a));
        g.fillRoundedRectangle (banner.toFloat(), 13.0f);
        g.setColour (juce::Colour (0xff2b4b5c));
        g.drawRoundedRectangle (banner.toFloat(), 13.0f, 1.0f);
        g.setColour (theme::text);
        g.drawText (text, banner.withTrimmedLeft (18), juce::Justification::centred);
    }

    // labels
    g.setFont (theme::mono (11.0f));
    g.setColour (theme::muted);
    for (double f : major)
    {
        const float x = juce::jmin (w - 34.0f, xForFreq (f) + 4.0f);
        g.drawText (f >= 1000 ? juce::String ((int) (f / 1000)) + "k" : juce::String ((int) f),
                    juce::Rectangle<float> (x, h - 18.0f, 40.0f, 14.0f), juce::Justification::centredLeft);
    }
    for (float d = -dbRange + step; d < dbRange; d += step)
    {
        const auto label = (d > 0 ? "+" : "") + juce::String ((int) d);
        g.drawText (label, juce::Rectangle<float> (w - 42.0f, yForDb (d) - 15.0f, 34.0f, 14.0f), juce::Justification::centredRight);
    }

    // spectrum grab hint
    if (grabCandidate && dragBand < 0)
    {
        const float gx = xForFreq (grabFreq), gy = ySpec (grabDb, grabFreq);
        g.setColour (theme::text.withAlpha (0.7f));
        const float dashes[] = { 2.0f, 3.0f };
        juce::Path ring;
        ring.addEllipse (gx - 9, gy - 9, 18, 18);
        juce::Path dashed;
        juce::PathStrokeType (1.0f).createDashedStroke (dashed, ring, dashes, 2);
        g.fillPath (dashed);
        const auto text = "Drag to grab peak  " + params::formatFrequency ((float) grabFreq);
        g.setFont (theme::font (11.0f));
        auto r = juce::Rectangle<float> (gx + 14, gy - 32, (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), text) + 16, 22);
        if (r.getRight() > w - 50) r.setX (gx - 14 - r.getWidth());
        r.setY (juce::jmax (4.0f, r.getY()));
        g.setColour (theme::well);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (theme::border2);
        g.drawRoundedRectangle (r, 6.0f, 1.0f);
        g.setColour (theme::textDim);
        g.drawText (text, r, juce::Justification::centred);
    }

    // nodes
    for (int b = 0; b < meq::kNumBands; ++b)
    {
        const auto& p = bands[(size_t) b];
        if (! p.enabled) continue;
        const auto pos = nodePosition (b);
        const bool dyn = p.dynOn && meq::typeUsesGain (p.type);
        const bool isSel = b == selected;

        if (dyn)
        {
            // live gain movement: amber stem from the static to the current position
            const float liveY = juce::jlimit (6.0f, h - 6.0f, yForDb (proc.liveGainDb (b)));
            g.setColour (theme::amber);
            g.drawLine (pos.x, pos.y, pos.x, liveY, 2.0f);
            g.fillEllipse (pos.x - 3, liveY - 3, 6, 6);
        }

        auto r = juce::Rectangle<float> (kNodeRadius * 2, kNodeRadius * 2).withCentre (pos);
        const float alpha = p.bypass ? 0.4f : 1.0f;
        g.setColour ((isSel ? theme::accent : juce::Colour (0xff16191f)).withMultipliedAlpha (alpha));
        g.fillEllipse (r);
        g.setColour ((isSel ? theme::accent : dyn ? theme::amber : (b == hoverNode ? theme::textDim : theme::faint)).withMultipliedAlpha (alpha));
        g.drawEllipse (r.reduced (0.75f), 1.5f);
        g.setColour ((isSel ? theme::onAccent : theme::text).withMultipliedAlpha (alpha));
        g.setFont (theme::font (11.0f, true));
        g.drawText (juce::String (b + 1), r, juce::Justification::centred);
    }

    // readout for the selected node
    if (juce::isPositiveAndBelow (selected, meq::kNumBands) && bands[(size_t) selected].enabled)
    {
        const auto& p = bands[(size_t) selected];
        const auto pos = nodePosition (selected);
        juce::String text = params::formatFrequency ((float) p.freq);
        if (meq::typeUsesGain (p.type)) text << "  " << params::formatDb ((float) p.gain) << "  Q " << juce::String (p.q, 2);
        else if (meq::typeUsesSlope (p.type)) text << "  " << p.slope << " dB/oct";
        g.setFont (theme::mono (11.0f));
        const float tw = (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), text) + 18.0f;
        auto r = juce::Rectangle<float> (tw, 24.0f).withCentre ({ pos.x, pos.y > 70 ? pos.y - 36 : pos.y + 36 });
        r.setX (juce::jlimit (6.0f, w - tw - 50.0f, r.getX()));
        g.setColour (theme::text);
        g.fillRoundedRectangle (r, 7.0f);
        g.setColour (theme::onAccent);
        g.drawText (text, r, juce::Justification::centred);
    }

    // hover readout
    if (hovering && dragBand < 0 && hoverNode < 0 && ! grabCandidate)
    {
        const double f = freqForX (hoverPos.x);
        g.setColour (theme::text.withAlpha (0.3f));
        g.drawVerticalLine (juce::roundToInt (hoverPos.x), 0.0f, h);
        size_t idx = (size_t) juce::jlimit (0, (int) totalDb.size() - 1, juce::roundToInt (hoverPos.x / w * (float) (totalDb.size() - 1)));
        juce::String text = params::formatFrequency ((float) f);
        if (post.active) text << "  " << juce::String (specDbAt (post, f), 1) << " dBFS";
        if (! totalDb.empty()) text << "  EQ " << params::formatDb (totalDb[idx]);
        g.setFont (theme::mono (11.0f));
        const float tw = (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), text) + 16.0f;
        auto r = juce::Rectangle<float> (hoverPos.x + 12, juce::jlimit (40.0f, h - 60.0f, hoverPos.y - 34), tw, 22);
        if (r.getRight() > w - 46) r.setX (hoverPos.x - 12 - tw);
        g.setColour (theme::well);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (theme::border2);
        g.drawRoundedRectangle (r, 6.0f, 1.0f);
        g.setColour (theme::text);
        g.drawText (text, r, juce::Justification::centred);
    }

    // legend
    g.setFont (theme::font (11.0f));
    float lx = 14;
    auto legend = [&] (juce::Colour c, const juce::String& s, bool box)
    {
        g.setColour (c);
        if (box) g.fillRect (lx, 14.0f, 10.0f, 8.0f); else g.fillRect (lx, 17.0f, 10.0f, 2.0f);
        g.setColour (theme::muted);
        const int tw = juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), s);
        g.drawText (s, juce::Rectangle<float> (lx + 16, 10, (float) tw + 4, 16), juce::Justification::centredLeft);
        lx += 16 + (float) tw + 18;
    };
    legend (theme::accent, "EQ curve", false);
    if (anyDynamic) legend (theme::text.withAlpha (0.75f), "Live (dynamics)", false);
    if (! selRangeDb.empty()) legend (theme::amber.withAlpha (0.6f), "Dynamic range", true);
    if (side.active && proc.sidechainConnected()) legend (theme::amber.withAlpha (0.5f), "Sidechain", false);

    if (std::none_of (bands.begin(), bands.end(), [] (const meq::BandParams& p) { return p.enabled; }))
    {
        g.setColour (theme::muted);
        g.setFont (theme::font (13.0f));
        g.drawText ("Double-click to add a band, or click near a peak in the analyzer to grab it",
                    getLocalBounds().withSizeKeepingCentre (600, 24), juce::Justification::centred);
    }
}

// ------------------------------------------------------------------ interaction
int EqDisplay::nodeAt (juce::Point<float> p) const
{
    int best = -1;
    float bestD = kNodeRadius + 4.0f;
    for (int b = 0; b < meq::kNumBands; ++b)
    {
        if (! bands[(size_t) b].enabled) continue;
        const float d = nodePosition (b).getDistanceFrom (p);
        if (d < bestD || (b == selected && d < kNodeRadius + 4.0f)) { bestD = d; best = b; }
    }
    return best;
}

bool EqDisplay::findPeakNear (juce::Point<float> p, double& freq, double& db) const
{
    if (! post.active) return false;
    const int n = 1 << fftOrder;
    const double f = freqForX (p.x);
    const int b0 = juce::jmax (2, (int) (f * std::pow (2.0, -0.25) * n / specRate));
    const int b1 = juce::jmin ((int) post.db.size() - 2, (int) (f * std::pow (2.0, 0.25) * n / specRate));
    int best = -1;
    float bestDb = -200;
    for (int k = b0; k <= b1; ++k)
    {
        const float v = post.db[(size_t) k];
        if (v > post.db[(size_t) k - 1] && v >= post.db[(size_t) k + 1] && v > bestDb) { bestDb = v; best = k; }
    }
    if (best < 0 || bestDb < -80.0f) return false;
    // parabolic interpolation of the peak bin
    const double a = post.db[(size_t) best - 1], b = post.db[(size_t) best], c = post.db[(size_t) best + 1];
    const double denom = a - 2 * b + c;
    const double off = std::abs (denom) > 1e-9 ? 0.5 * (a - c) / denom : 0.0;
    freq = (best + off) * specRate / n;
    db = b - 0.25 * (a - c) * off;
    const juce::Point<float> peak { xForFreq (freq), ySpec (db, freq) };
    return freq > kMinF && freq < kMaxF && peak.getDistanceFrom (p) < 22.0f;
}

void EqDisplay::setParam (int band, const char* suffix, float value)
{
    if (auto* p = proc.bandParam (band, suffix))
        p->setValueNotifyingHost (p->convertTo0to1 (value));
}

void EqDisplay::beginDrag (int band)
{
    dragBand = band;
    for (auto* s : { pb::freq, pb::gain })
        if (auto* p = proc.bandParam (band, s)) p->beginChangeGesture();
    if (auditionOnDrag)
    {
        proc.auditionBand = band;
        auditionFromDrag = true;
    }
}

void EqDisplay::endDrag()
{
    if (dragBand < 0) return;
    for (auto* s : { pb::freq, pb::gain })
        if (auto* p = proc.bandParam (dragBand, s)) p->endChangeGesture();
    if (auditionFromDrag) proc.auditionBand = proc.lockedSolo;
    auditionFromDrag = false;
    dragBand = -1;
}

void EqDisplay::mouseMove (const juce::MouseEvent& e)
{
    hovering = true;
    hoverPos = e.position;
    hoverNode = nodeAt (e.position);
    grabCandidate = hoverNode < 0 && findPeakNear (e.position, grabFreq, grabDb);
    setMouseCursor (hoverNode >= 0 || grabCandidate ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::CrosshairCursor);
    repaint();
}

void EqDisplay::mouseExit (const juce::MouseEvent&)
{
    hovering = false;
    hoverNode = -1;
    grabCandidate = false;
    repaint();
}

void EqDisplay::mouseDown (const juce::MouseEvent& e)
{
    if (dbLabelArea.contains (e.getPosition()) && nodeAt (e.position) < 0)
    {
        // click the dB scale to cycle the display range
        const float ranges[] = { 6, 12, 18, 30 };
        int i = 0;
        while (i < 4 && ranges[i] <= dbRange) ++i;
        dbRange = ranges[i % 4];
        proc.apvts.state.setProperty ("uiRange", dbRange, nullptr);
        resized();
        repaint();
        return;
    }

    const int node = nodeAt (e.position);
    if (node >= 0)
    {
        if (onSelect) onSelect (node);
        if (e.mods.isPopupMenu()) { showBandMenu (node); return; }
        beginDrag (node);
        return;
    }

    if (e.mods.isPopupMenu()) return;

    double f, db;
    if (findPeakNear (e.position, f, db))
    {
        // Spectrum Grab: a narrow bell on the peak, then keep dragging it
        const int b = proc.addBand (meq::FilterType::Bell, (float) f, 0.0f, 6.0f);
        if (b >= 0)
        {
            bands[(size_t) b] = proc.readBand (b);
            if (onSelect) onSelect (b);
            beginDrag (b);
        }
    }
}

void EqDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (dragBand < 0) return;
    const auto type = bands[(size_t) dragBand].type;
    // shift: gain only (frequency stays where it is)
    if (! e.mods.isShiftDown())
        setParam (dragBand, pb::freq, (float) juce::jlimit (10.0, 22000.0, freqForX (e.position.x)));
    if (meq::typeUsesGain (type))
    {
        const double g = dbForY (e.position.y) / juce::jmax (0.05, std::abs (gainScale)) * (gainScale < 0 ? -1 : 1);
        setParam (dragBand, pb::gain, (float) juce::jlimit (-30.0, 30.0, std::round (g * 10.0) / 10.0));
    }
    bands[(size_t) dragBand] = proc.readBand (dragBand);
    hoverPos = e.position;
    rebuildCurves();
    repaint();
}

void EqDisplay::mouseUp (const juce::MouseEvent&)
{
    endDrag();
    repaint();
}

void EqDisplay::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int node = nodeAt (e.position);
    if (node >= 0)
    {
        endDrag();
        proc.removeBand (node);
        if (selected == node && onSelect) onSelect (-1);
        return;
    }
    const double f = freqForX (e.position.x);
    const double g = juce::jlimit (-30.0, 30.0, dbForY (e.position.y));
    const int b = proc.addBand (meq::FilterType::Bell, (float) f, (float) (std::round (g * 10.0) / 10.0), 1.0f);
    if (b >= 0 && onSelect) onSelect (b);
}

void EqDisplay::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    int node = nodeAt (e.position);
    if (node < 0) node = (dragBand >= 0 ? dragBand : -1);
    if (node < 0) return;
    if (auto* p = proc.bandParam (node, pb::q))
    {
        const float q = proc.bandValue (node, pb::q) * std::exp (wheel.deltaY * (wheel.isReversed ? -1.0f : 1.0f) * 1.5f);
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (juce::jlimit (0.025f, 40.0f, q)));
        p->endChangeGesture();
    }
}

void EqDisplay::showBandMenu (int band)
{
    juce::PopupMenu types, channels, menu;
    const int curType = juce::roundToInt (proc.bandValue (band, pb::type));
    const int curCh = juce::roundToInt (proc.bandValue (band, pb::channel));
    for (int t = 0; t < (int) meq::FilterType::Count; ++t)
        types.addItem (100 + t, meq::filterTypeName ((meq::FilterType) t), true, t == curType);
    const char* chNames[] = { "Stereo", "Left", "Right", "Mid", "Side" };
    for (int c = 0; c < 5; ++c) channels.addItem (200 + c, chNames[c], true, c == curCh);

    menu.addSectionHeader ("Band " + juce::String (band + 1));
    menu.addSubMenu ("Type", types);
    menu.addSubMenu ("Process", channels);
    menu.addItem (1, "Dynamic", meq::typeUsesGain ((meq::FilterType) curType), proc.bandValue (band, pb::dyn) > 0.5f);
    menu.addItem (2, "Bypass", true, proc.bandValue (band, pb::bypass) > 0.5f);
    menu.addItem (3, "Solo (audition)", true, proc.lockedSolo == band);
    menu.addSeparator();
    menu.addItem (4, "Delete band");

    juce::Component::SafePointer<EqDisplay> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [safe, band] (int r)
    {
        if (safe == nullptr || r == 0) return;
        auto& pr = safe->proc;
        if (r >= 100 && r < 200) pr.setBandValue (band, pb::type, (float) (r - 100));
        else if (r >= 200)       pr.setBandValue (band, pb::channel, (float) (r - 200));
        else if (r == 1)         pr.setBandValue (band, pb::dyn, pr.bandValue (band, pb::dyn) > 0.5f ? 0.0f : 1.0f);
        else if (r == 2)         pr.setBandValue (band, pb::bypass, pr.bandValue (band, pb::bypass) > 0.5f ? 0.0f : 1.0f);
        else if (r == 3)
        {
            pr.lockedSolo = pr.lockedSolo == band ? -1 : band;
            pr.auditionBand = pr.lockedSolo;
        }
        else if (r == 4)
        {
            pr.removeBand (band);
            if (safe->onSelect) safe->onSelect (-1);
        }
    });
}
