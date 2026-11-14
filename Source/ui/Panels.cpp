#include "Panels.h"
#include "../PluginProcessor.h"

namespace pb = params::band;

void PanelBase::paintPanel (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (theme::panel);
    g.fillRoundedRectangle (r, 12.0f);
    g.setColour (theme::line);
    g.drawRoundedRectangle (r, 12.0f, 1.0f);
}

// ================================================================== HeaderBar
HeaderBar::HeaderBar (MeridianProcessor& p)
{
    phase.attach (p.apvts.getParameter (params::phase));
    phase.setOptionEnabled (2, false);
    phase.setOptionEnabled (3, false);
    phase.setTooltip ("Zero Latency: classic bilinear filters. Analog: Nyquist-matched filters that follow the analog "
                      "curve up to Nyquist. Linear and Mixed phase come in a later build.");
    addAndMakeVisible (phase);

    osAttachment = std::make_unique<juce::ButtonParameterAttachment> (*p.apvts.getParameter (params::oversample), oversampling, nullptr);
    oversampling.setTooltip ("Runs the EQ at twice the sample rate (adds a little latency)");
    addAndMakeVisible (oversampling);

    settings.onClick = [this] { if (onSettings) onSettings(); };
    addAndMakeVisible (settings);
}

void HeaderBar::paint (juce::Graphics& g)
{
    g.setColour (theme::line);
    g.fillRect (0, getHeight() - 1, getWidth(), 1);

    juce::Path logo;
    logo.startNewSubPath (18, 30);
    logo.cubicTo (22, 30, 23, 21, 27, 21);
    logo.cubicTo (31, 21, 32, 33, 36, 27);
    g.setColour (theme::accent);
    g.strokePath (logo, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (theme::text);
    g.setFont (theme::font (15.0f, true));
    g.drawText ("Meridian EQ", 46, 0, 140, getHeight(), juce::Justification::centredLeft);
    g.setColour (theme::muted);
    g.setFont (theme::font (11.0f));
    g.drawText ("prototype", 150, 0, 80, getHeight(), juce::Justification::centredLeft);

    theme::caption (g, "Phase", phase.getBounds().translated (-56, 0).withWidth (50), juce::Justification::centredRight);
}

void HeaderBar::resized()
{
    auto r = getLocalBounds().reduced (16, 10);
    settings.setBounds (r.removeFromRight (32));
    r.removeFromRight (10);
    oversampling.setBounds (r.removeFromRight (150));
    phase.setBounds (juce::Rectangle<int> (360, 32).withCentre ({ getWidth() / 2 + 20, getHeight() / 2 }));
}

// ================================================================== BandStrip
BandStrip::BandStrip (MeridianProcessor& p) : proc (p) {}

std::vector<BandStrip::Chip> BandStrip::layoutChips() const
{
    std::vector<Chip> chips;
    float x = 120.0f;
    const auto f = theme::font (11.0f, true);
    for (int b = 0; b < meq::kNumBands; ++b)
    {
        const auto d = proc.readBand (b);
        if (! d.enabled) continue;
        juce::String label = juce::String (b + 1) + " " + meq::filterTypeShort (d.type);
        if (d.channel != meq::Channel::Stereo) label << " " << params::channels[(int) d.channel];
        const float w = (float) juce::GlyphArrangement::getStringWidthInt (f, label) + (d.dynOn ? 28.0f : 20.0f);
        if (x + w > (float) getWidth() - 120.0f) break;
        chips.push_back ({ b, { x, 8.0f, w, (float) getHeight() - 16.0f } });
        x += w + 6.0f;
    }
    return chips;
}

juce::Rectangle<float> BandStrip::addButtonArea (const std::vector<Chip>& chips) const
{
    const float x = chips.empty() ? 120.0f : chips.back().r.getRight() + 6.0f;
    return { x, 8.0f, 84.0f, (float) getHeight() - 16.0f };
}

void BandStrip::paint (juce::Graphics& g)
{
    g.setColour (theme::line);
    g.fillRect (0, getHeight() - 1, getWidth(), 1);

    int count = 0;
    for (int b = 0; b < meq::kNumBands; ++b) count += proc.bandValue (b, pb::on) > 0.5f ? 1 : 0;
    theme::caption (g, "Bands " + juce::String (count) + " / 32", { 16, 0, 100, getHeight() });

    const auto chips = layoutChips();
    g.setFont (theme::font (11.0f, true));
    for (const auto& c : chips)
    {
        const auto d = proc.readBand (c.band);
        const bool on = c.band == selected;
        const auto col = theme::channelColour ((int) d.channel);
        g.setColour (on ? col : theme::panel);
        g.fillRoundedRectangle (c.r, 7.0f);
        g.setColour (on ? col : col.withAlpha (0.55f));
        g.drawRoundedRectangle (c.r, 7.0f, 1.0f);
        juce::String label = juce::String (c.band + 1) + " " + meq::filterTypeShort (d.type);
        if (d.channel != meq::Channel::Stereo) label << " " << params::channels[(int) d.channel];
        g.setColour (on ? theme::onAccent : (d.bypass ? theme::faint : theme::textDim));
        g.drawText (label, c.r.withTrimmedLeft (10).withTrimmedRight (d.dynOn ? 18 : 10), juce::Justification::centredLeft);
        if (d.dynOn)
        {
            g.setColour (theme::amber);
            g.fillEllipse (c.r.getRight() - 13.0f, c.r.getCentreY() - 2.5f, 5.0f, 5.0f);
        }
    }

    const auto add = addButtonArea (chips);
    const float dashes[] = { 3.0f, 3.0f };
    juce::Path outline, dashed;
    outline.addRoundedRectangle (add, 7.0f);
    juce::PathStrokeType (1.0f).createDashedStroke (dashed, outline, dashes, 2);
    g.setColour (theme::faint);
    g.fillPath (dashed);
    g.setColour (theme::textDim);
    g.drawText ("+ Add band", add, juce::Justification::centred);

    g.setColour (theme::muted);
    g.setFont (theme::font (11.0f));
    g.drawText ("Speaker above a dot: audition  /  drag to move  /  wheel for Q", getLocalBounds().withTrimmedRight (16),
                juce::Justification::centredRight);
}

void BandStrip::mouseDown (const juce::MouseEvent& e)
{
    const auto chips = layoutChips();
    for (const auto& c : chips)
        if (c.r.contains (e.position)) { if (onSelect) onSelect (c.band); return; }
    if (addButtonArea (chips).contains (e.position))
    {
        const int b = proc.addBand (meq::FilterType::Bell, 1000.0f, 0.0f, 1.0f);
        if (b >= 0 && onSelect) onSelect (b);
    }
}

// ================================================================== BandPanel
BandPanel::BandPanel (MeridianProcessor& p) : proc (p)
{
    type.addItemList (params::filterTypeNames(), 1);
    addAndMakeVisible (type);
    for (auto* k : { &freq, &gain, &q, &slope }) addAndMakeVisible (*k);
    addAndMakeVisible (channel);
    channel.setOptionColours ({ theme::channelColour (0), theme::channelColour (1), theme::channelColour (2),
                                theme::channelColour (3), theme::channelColour (4) });

    solo.setClickingTogglesState (true);
    solo.setTooltip ("Audition: hear only this band's range (same as the speaker above the band's dot)");
    solo.onClick = [this]
    {
        if (band < 0) return;
        proc.lockedSolo = solo.getToggleState() ? band : -1;
        proc.auditionBand = proc.lockedSolo;
    };
    addAndMakeVisible (solo);

    bypass.setClickingTogglesState (true);
    addAndMakeVisible (bypass);
    remove.onClick = [this]
    {
        if (band < 0) return;
        proc.removeBand (band);
        if (onSelect) onSelect (-1);
    };
    addAndMakeVisible (remove);
    setBand (-1);
}

void BandPanel::setBand (int b)
{
    band = b;
    typeAttachment.reset();
    bypassAttachment.reset();
    const bool valid = b >= 0;
    for (juce::Component* c : { (juce::Component*) &type, (juce::Component*) &freq, (juce::Component*) &gain, (juce::Component*) &q,
                                (juce::Component*) &slope, (juce::Component*) &channel, (juce::Component*) &solo,
                                (juce::Component*) &bypass, (juce::Component*) &remove })
        c->setVisible (valid);
    if (! valid) { repaint(); return; }

    typeAttachment = std::make_unique<juce::ComboBoxParameterAttachment> (*proc.bandParam (b, pb::type), type, nullptr);
    bypassAttachment = std::make_unique<juce::ButtonParameterAttachment> (*proc.bandParam (b, pb::bypass), bypass, nullptr);
    freq.attach (proc.bandParam (b, pb::freq));
    gain.attach (proc.bandParam (b, pb::gain));
    q.attach (proc.bandParam (b, pb::q));
    slope.attach (proc.bandParam (b, pb::slope));
    channel.attach (proc.bandParam (b, pb::channel));
    update();
    repaint();
}

void BandPanel::update()
{
    if (band < 0) return;
    const auto t = (meq::FilterType) juce::roundToInt (proc.bandValue (band, pb::type));
    gain.setEnabled (meq::typeUsesGain (t));
    slope.setEnabled (meq::typeUsesSlope (t));
    solo.setToggleState (proc.lockedSolo == band, juce::dontSendNotification);
}

void BandPanel::paint (juce::Graphics& g)
{
    paintPanel (g);
    if (band < 0)
    {
        theme::caption (g, "Band", { 16, 14, 200, 30 });
        g.setColour (theme::muted);
        g.setFont (theme::font (13.0f));
        g.drawText ("Double-click the display to add a band", getLocalBounds(), juce::Justification::centred);
        return;
    }
    theme::caption (g, "Band " + juce::String (band + 1), { 16, 14, 70, 30 });
    theme::caption (g, "Process", channel.getBounds().withX (16).withWidth (60));
}

void BandPanel::resized()
{
    auto r = getLocalBounds().reduced (16, 14);
    auto top = r.removeFromTop (30);
    top.removeFromLeft (74);
    type.setBounds (top.removeFromLeft (170));
    remove.setBounds (top.removeFromRight (30));
    top.removeFromRight (6);
    bypass.setBounds (top.removeFromRight (30));
    top.removeFromRight (6);
    solo.setBounds (top.removeFromRight (30));

    r.removeFromTop (8);
    auto bottom = r.removeFromBottom (32);
    bottom.removeFromLeft (64);
    channel.setBounds (bottom.withWidth (juce::jmin (bottom.getWidth(), 250)));

    r.removeFromBottom (8);
    const int kw = r.getWidth() / 4;
    for (auto* k : { &freq, &gain, &q, &slope }) k->setBounds (r.removeFromLeft (kw).reduced (4, 0));
}

// ================================================================== DynamicsPanel
DynamicsPanel::DynamicsPanel (MeridianProcessor& p) : proc (p)
{
    enable.getProperties().set ("switch", true);
    enable.getProperties().set ("amber", true);
    enable.setTooltip ("Dynamic EQ on this band");
    addAndMakeVisible (enable);
    mode.setTooltip ("Above: react to signal over the threshold. Below: under it. Two-way: both, in opposite directions.");
    detect.setTooltip ("What the band listens to: its own range, another range of the same signal, or the sidechain input");
    for (juce::Component* c : { (juce::Component*) &mode, (juce::Component*) &threshold, (juce::Component*) &range,
                                (juce::Component*) &ratio, (juce::Component*) &attack, (juce::Component*) &release,
                                (juce::Component*) &onset, (juce::Component*) &body, (juce::Component*) &detect,
                                (juce::Component*) &detFreq, (juce::Component*) &detQ })
        addChildComponent (c);
    setBand (-1);
}

void DynamicsPanel::setBand (int b)
{
    band = b;
    enableAttachment.reset();
    enable.setVisible (b >= 0);
    if (b < 0) { showControls = false; update(); return; }
    enableAttachment = std::make_unique<juce::ButtonParameterAttachment> (*proc.bandParam (b, pb::dyn), enable, nullptr);
    mode.attach (proc.bandParam (b, pb::dynMode));
    threshold.attach (proc.bandParam (b, pb::thr));
    range.attach (proc.bandParam (b, pb::range));
    ratio.attach (proc.bandParam (b, pb::ratio));
    attack.attach (proc.bandParam (b, pb::attack));
    release.attach (proc.bandParam (b, pb::release));
    onset.attach (proc.bandParam (b, pb::onset));
    body.attach (proc.bandParam (b, pb::body));
    detect.attach (proc.bandParam (b, pb::detect));
    detFreq.attach (proc.bandParam (b, pb::detFreq));
    detQ.attach (proc.bandParam (b, pb::detQ));
    update();
}

void DynamicsPanel::update()
{
    bool show = false;
    juce::String newNote;
    if (band >= 0)
    {
        const auto t = (meq::FilterType) juce::roundToInt (proc.bandValue (band, pb::type));
        const bool usable = meq::typeUsesGain (t);
        enable.setEnabled (usable);
        show = usable && proc.bandValue (band, pb::dyn) > 0.5f;
        if (! usable) newNote = juce::String (meq::filterTypeName (t)) + " has no gain to move, so it has no dynamics.";
        else if (! show) newNote = "Turn on to compress or expand above or below a threshold, split transient from body, "
                                   "or key the band from another frequency range or the sidechain input.";
        const int det = juce::roundToInt (proc.bandValue (band, pb::detect));
        detFreq.setEnabled (det != 0);
        detQ.setEnabled (det != 0);
        if (show && det == 2 && ! proc.sidechainConnected())
            newNote = "No sidechain signal: route a track to Meridian EQ's sidechain input in your DAW.";
    }
    if (show != showControls || newNote != note)
    {
        showControls = show;
        note = newNote;
        for (juce::Component* c : { (juce::Component*) &mode, (juce::Component*) &threshold, (juce::Component*) &range,
                                    (juce::Component*) &ratio, (juce::Component*) &attack, (juce::Component*) &release,
                                    (juce::Component*) &onset, (juce::Component*) &body, (juce::Component*) &detect,
                                    (juce::Component*) &detFreq, (juce::Component*) &detQ })
            c->setVisible (show);
        repaint();
    }
}

void DynamicsPanel::paint (juce::Graphics& g)
{
    paintPanel (g);
    theme::caption (g, "Dynamics", { 16, 14, 100, 30 });
    if (note.isNotEmpty())
    {
        g.setColour (theme::muted);
        g.setFont (theme::font (12.0f));
        auto area = showControls ? getLocalBounds().removeFromBottom (26).reduced (16, 0)
                                 : getLocalBounds().reduced (24, 60);
        g.drawFittedText (note, area, showControls ? juce::Justification::centredLeft : juce::Justification::centred, 3);
    }
    if (showControls) theme::caption (g, "Detection", detect.getBounds().translated (0, -18).withHeight (16));
}

void DynamicsPanel::resized()
{
    auto r = getLocalBounds().reduced (16, 14);
    auto top = r.removeFromTop (30);
    top.removeFromLeft (90);
    enable.setBounds (top.removeFromLeft (44));
    mode.setBounds (top.removeFromRight (220));

    r.removeFromTop (8);
    auto knobs = r.removeFromTop (juce::jmin (100, r.getHeight() / 2));
    const int kw = knobs.getWidth() / 5;
    for (auto* k : { &threshold, &range, &ratio, &attack, &release }) k->setBounds (knobs.removeFromLeft (kw).reduced (3, 0));

    r.removeFromTop (12);
    auto left = r.removeFromLeft (r.getWidth() / 2 - 10);
    onset.setBounds (left.removeFromTop (34));
    left.removeFromTop (8);
    body.setBounds (left.removeFromTop (34));

    r.removeFromLeft (20);
    r.removeFromTop (18);
    detect.setBounds (r.removeFromTop (30));
    r.removeFromTop (4);
    auto det = r.removeFromTop (juce::jmin (70, r.getHeight()));
    detFreq.setBounds (det.removeFromLeft (det.getWidth() / 2));
    detQ.setBounds (det);
}

// ================================================================== GlobalPanel
GlobalPanel::GlobalPanel (MeridianProcessor& p) : proc (p)
{
    gainScale.attach (p.apvts.getParameter (params::gainScale));
    width.attach (p.apvts.getParameter (params::width));
    output.attach (p.apvts.getParameter (params::output));
    lookahead.attach (p.apvts.getParameter (params::lookahead));
    gainScale.slider.setTooltip ("Scales every band's gain: 100% as set, 0% flat, -100% inverted, up to 200%");
    lookahead.slider.setTooltip ("Delays the audio so dynamic bands can react before a peak arrives (adds latency)");
    for (auto* s : { &gainScale, &width, &output, &lookahead }) addAndMakeVisible (*s);

    autoGain.getProperties().set ("switch", true);
    autoAttachment = std::make_unique<juce::ButtonParameterAttachment> (*p.apvts.getParameter (params::autoGain), autoGain, nullptr);
    autoGain.setTooltip ("Compensates the overall level change of the curve");
    addAndMakeVisible (autoGain);

    monitor.attach (p.apvts.getParameter (params::monitor));
    monitor.setTooltip ("Listen to the mid or side signal only");
    addAndMakeVisible (monitor);
}

void GlobalPanel::update()
{
    const int lat = proc.getLatencySamples();
    juce::String s = "Latency " + juce::String (lat) + " smp";
    if (proc.apvts.getRawParameterValue (params::autoGain)->load() > 0.5f)
        s << "  /  auto " << params::formatDb (proc.autoGainDb());
    s << "  /  64-bit  /  " << (rendererName ? rendererName() : juce::String ("OpenGL"));
    if (s != status) { status = s; repaint(); }
}

void GlobalPanel::paint (juce::Graphics& g)
{
    paintPanel (g);
    theme::caption (g, "Global", { 16, 14, 100, 30 });
    g.setColour (theme::muted);
    g.setFont (theme::font (11.0f));
    g.drawText ("M/S monitor", monitor.getBounds().withX (16).withWidth (90), juce::Justification::centredLeft);
    g.setFont (theme::mono (11.0f));
    g.drawFittedText (status, getLocalBounds().removeFromBottom (30).reduced (16, 0), juce::Justification::centredLeft, 2);
}

void GlobalPanel::resized()
{
    auto r = getLocalBounds().reduced (16, 14);
    r.removeFromTop (34);
    r.removeFromBottom (24);
    for (auto* s : { &gainScale, &width, &output, &lookahead })
    {
        s->setBounds (r.removeFromTop (32));
        r.removeFromTop (6);
    }
    autoGain.setBounds (r.removeFromTop (26));
    r.removeFromTop (6);
    auto row = r.removeFromTop (28);
    monitor.setBounds (row.removeFromRight (150));
}
