#include "SettingsPanel.h"

namespace
{
    /** Preview card for one built-in palette. */
    class PaletteCard : public juce::Component
    {
    public:
        PaletteCard (int idx, GlobalSettings& s) : index (idx), settings (s)
        {
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
            setTitle (theme::presets()[(size_t) index].name);
        }

        void paint (juce::Graphics& g) override
        {
            const auto& p = theme::presets()[(size_t) index];
            const bool sel = settings.paletteIndex == index;
            auto r = getLocalBounds().toFloat().reduced (1.0f);
            g.setColour (p.bg);
            g.fillRoundedRectangle (r, 8.0f);
            auto preview = r.reduced (6.0f).withTrimmedBottom (18.0f);
            g.setColour (p.display);
            g.fillRoundedRectangle (preview, 5.0f);

            // spectrum gradient hill
            juce::Path hill;
            hill.startNewSubPath (preview.getX(), preview.getBottom());
            for (int i = 0; i <= 20; ++i)
            {
                const float t = (float) i / 20.0f;
                const float y = preview.getBottom() - preview.getHeight() * (0.25f + 0.25f * std::sin (t * 5.5f + 1.0f) * (1.0f - t * 0.5f));
                hill.lineTo (preview.getX() + preview.getWidth() * t, y);
            }
            hill.lineTo (preview.getRight(), preview.getBottom());
            hill.closeSubPath();
            g.setGradientFill (juce::ColourGradient (p.specLow.withAlpha (0.8f), preview.getX(), 0, p.specHigh.withAlpha (0.8f), preview.getRight(), 0, false));
            g.fillPath (hill);

            // EQ curve in the accent colour, channel dots
            juce::Path curve;
            for (int i = 0; i <= 30; ++i)
            {
                const float t = (float) i / 30.0f;
                const float y = preview.getCentreY() - preview.getHeight() * 0.28f * std::exp (-std::pow ((t - 0.35f) / 0.12f, 2.0f))
                                + preview.getHeight() * 0.18f * std::exp (-std::pow ((t - 0.75f) / 0.08f, 2.0f));
                if (i == 0) curve.startNewSubPath (preview.getX(), y); else curve.lineTo (preview.getX() + preview.getWidth() * t, y);
            }
            g.setColour (p.text);
            g.strokePath (curve, juce::PathStrokeType (1.5f));
            const juce::Colour dots[] = { p.stereo, p.left, p.right, p.mid, p.side, p.amber };
            for (int i = 0; i < 6; ++i)
            {
                g.setColour (dots[i]);
                g.fillEllipse (preview.getX() + 6.0f + (float) i * 10.0f, preview.getY() + 5.0f, 6.0f, 6.0f);
            }

            g.setColour (p.text);
            g.setFont (theme::font (11.0f, true));
            g.drawText (p.name, r.removeFromBottom (20.0f).reduced (8.0f, 0.0f), juce::Justification::centredLeft);

            g.setColour (sel ? theme::accent : (isMouseOver() ? theme::border2 : theme::border));
            g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 8.0f, sel ? 2.0f : 1.0f);
        }

        void mouseEnter (const juce::MouseEvent&) override { repaint(); }
        void mouseExit (const juce::MouseEvent&) override { repaint(); }
        void mouseDown (const juce::MouseEvent&) override
        {
            settings.palette = theme::presets()[(size_t) index];
            settings.paletteIndex = index;
            settings.changed();
        }

    private:
        int index;
        GlobalSettings& settings;
    };

    /** Colour picker shown in a call-out box; edits one palette slot live. */
    class ColourPicker : public juce::Component, private juce::ChangeListener
    {
    public:
        ColourPicker (GlobalSettings& s, juce::Colour theme::Palette::* m) : settings (s), member (m)
        {
            selector.setCurrentColour (settings.palette.*member, juce::dontSendNotification);
            selector.addChangeListener (this);
            addAndMakeVisible (selector);
            setSize (300, 320);
        }
        ~ColourPicker() override { selector.removeChangeListener (this); }
        void resized() override { selector.setBounds (getLocalBounds()); }

    private:
        void changeListenerCallback (juce::ChangeBroadcaster*) override
        {
            settings.palette.*member = selector.getCurrentColour();
            settings.palette.name = "Custom";
            settings.paletteIndex = -1;
            settings.changed();
        }

        GlobalSettings& settings;
        juce::Colour theme::Palette::* member;
        juce::ColourSelector selector { juce::ColourSelector::showColourAtTop | juce::ColourSelector::showSliders
                                        | juce::ColourSelector::showColourspace };
    };

    /** One editable colour row: swatch, name, hex. */
    class ColourRow : public juce::Component
    {
    public:
        ColourRow (GlobalSettings& s, const theme::Slot& sl) : settings (s), slot (sl)
        {
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
        }

        void paint (juce::Graphics& g) override
        {
            const auto c = settings.palette.*(slot.member);
            auto r = getLocalBounds();
            if (isMouseOver())
            {
                g.setColour (theme::well);
                g.fillRoundedRectangle (r.toFloat(), 6.0f);
            }
            auto sw = r.removeFromLeft (34).reduced (6, 5).toFloat();
            g.setColour (c);
            g.fillRoundedRectangle (sw, 4.0f);
            g.setColour (theme::border2);
            g.drawRoundedRectangle (sw, 4.0f, 1.0f);
            g.setColour (theme::text);
            g.setFont (theme::font (12.0f));
            g.drawText (slot.label, r.withTrimmedLeft (6), juce::Justification::centredLeft);
            g.setColour (theme::muted);
            g.setFont (theme::mono (11.0f));
            g.drawText ("#" + c.toDisplayString (false), r.withTrimmedRight (8), juce::Justification::centredRight);
        }

        void mouseEnter (const juce::MouseEvent&) override { repaint(); }
        void mouseExit (const juce::MouseEvent&) override { repaint(); }
        void mouseDown (const juce::MouseEvent&) override
        {
            juce::CallOutBox::launchAsynchronously (std::make_unique<ColourPicker> (settings, slot.member), getScreenBounds(), nullptr);
        }

    private:
        GlobalSettings& settings;
        theme::Slot slot;
    };
}

// ------------------------------------------------------------------ Content
class SettingsPanel::Content : public juce::Component
{
public:
    explicit Content (GlobalSettings& s) : settings (s)
    {
        refresh.onChange = [this] (int i) { const int hz[] = { 30, 60, 120, 144 }; settings.refreshHz = hz[i]; settings.changed(); };
        resolution.onChange = [this] (int i) { settings.fftOrder = 11 + i; settings.changed(); };
        scale.onChange = [this] (int i) { const float r[] = { 6, 12, 18, 30 }; settings.dbRange = r[i]; settings.changed(); };
        size.onChange = [this] (int i) { settings.uiScale = GlobalSettings::uiScales()[(size_t) i]; settings.changed(); };
        renderer.onChange = [this] (int i) { settings.gpu = i == 0; settings.changed(); };
        style.onChange = [this] (int i) { settings.spectrumStyle = i; settings.changed(); resized(); };
        grab.onChange = [this] (int i) { settings.spectrumGrab = i == 1; settings.changed(); };
        saveDefaults.onClick = [this] { if (onSave) onSave(); saved = juce::Time::getMillisecondCounter(); repaint(); };
        resetDefaults.onClick = [this] { if (onReset) onReset(); };

        dot.slider.setRange (1.4, 4.0, 0.1);
        dot.slider.textFromValueFunction = [] (double v) { return juce::String (v, 1) + " px"; };
        dot.slider.onDragEnd = [] {};
        dot.slider.onValueChange = [this] { dot.repaint(); settings.particleDot = (float) dot.slider.getValue(); settings.sendChangeMessage(); };
        spacing.slider.setRange (4.0, 10.0, 1.0);
        spacing.slider.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v)) + " px"; };
        spacing.slider.onDragEnd = [] {};
        spacing.slider.onValueChange = [this] { spacing.repaint(); settings.particleSpacing = juce::roundToInt (spacing.slider.getValue()); settings.sendChangeMessage(); };

        brightness.slider.setRange (50.0, 150.0, 1.0);
        brightness.slider.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v)) + "%"; };
        brightness.slider.onDragEnd = [] {};
        brightness.slider.onValueChange = [this]
        {
            brightness.repaint();
            settings.brightness = (float) brightness.slider.getValue() / 100.0f;
            settings.sendChangeMessage();
        };

        fill.slider.setRange (0.0, 60.0, 1.0);
        fill.slider.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v)) + "%"; };
        fill.slider.onValueChange = [this]
        {
            fill.repaint();
            if (std::abs (settings.palette.specFill * 100.0f - (float) fill.slider.getValue()) < 0.5f) return;
            settings.palette.specFill = (float) fill.slider.getValue() / 100.0f;
            settings.paletteIndex = -1;
            settings.palette.name = "Custom";
            settings.changed();
        };

        reset.onClick = [this]
        {
            const int base = juce::jlimit (0, (int) theme::presets().size() - 1, lastPreset);
            settings.palette = theme::presets()[(size_t) base];
            settings.paletteIndex = base;
            settings.changed();
        };

        for (juce::Component* c : { (juce::Component*) &refresh, (juce::Component*) &resolution, (juce::Component*) &brightness,
                                    (juce::Component*) &scale, (juce::Component*) &size, (juce::Component*) &renderer,
                                    (juce::Component*) &fill, (juce::Component*) &reset, (juce::Component*) &style,
                                    (juce::Component*) &dot, (juce::Component*) &spacing, (juce::Component*) &grab,
                                    (juce::Component*) &saveDefaults, (juce::Component*) &resetDefaults })
            addAndMakeVisible (c);

        for (int i = 0; i < (int) theme::presets().size(); ++i)
            addAndMakeVisible (cards.add (new PaletteCard (i, settings)));
        for (const auto& sl : theme::slots())
            addAndMakeVisible (rows.add (new ColourRow (settings, sl)));

        sync();
        setSize (panelWidth - 12, 1400);
    }

    /** Show the current settings. */
    void sync()
    {
        const int hz[] = { 30, 60, 120, 144 };
        for (int i = 0; i < 4; ++i) if (settings.refreshHz == hz[i]) refresh.setSelected (i);
        resolution.setSelected (settings.fftOrder - 11);
        const float r[] = { 6, 12, 18, 30 };
        for (int i = 0; i < 4; ++i) if (std::abs (settings.dbRange - r[i]) < 0.1f) scale.setSelected (i);
        const auto& sc = GlobalSettings::uiScales();
        for (int i = 0; i < (int) sc.size(); ++i) if (std::abs (settings.uiScale - sc[(size_t) i]) < 0.01f) size.setSelected (i);
        renderer.setSelected (settings.gpu ? 0 : 1);
        style.setSelected (settings.spectrumStyle);
        grab.setSelected (settings.spectrumGrab ? 1 : 0);
        dot.slider.setValue (settings.particleDot, juce::dontSendNotification);
        spacing.slider.setValue (settings.particleSpacing, juce::dontSendNotification);
        dot.setEnabled (settings.spectrumStyle == 1);
        spacing.setEnabled (settings.spectrumStyle == 1);
        brightness.slider.setValue (settings.brightness * 100.0, juce::dontSendNotification);
        fill.slider.setValue (settings.palette.specFill * 100.0, juce::dontSendNotification);
        if (settings.paletteIndex >= 0) lastPreset = settings.paletteIndex;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        for (auto& h : headings) theme::caption (g, h.text, h.area);
        g.setColour (theme::muted);
        g.setFont (theme::font (12.0f));
        for (auto& l : labels) g.drawText (l.text, l.area, juce::Justification::centredLeft);
        g.setColour (theme::muted.withAlpha (0.8f));
        g.setFont (theme::font (11.0f));
        g.drawText ("Current: " + settings.palette.name, customNote, juce::Justification::centredLeft);
        if (saved != 0 && juce::Time::getMillisecondCounter() - saved < 4000)
        {
            g.setColour (theme::accent);
            g.drawText ("Saved as default", saveDefaults.getBounds().translated (0, 34).withHeight (14), juce::Justification::centred);
        }
    }

    void resized() override
    {
        headings.clear();
        labels.clear();
        const int w = getWidth() - 32;
        int y = 8;
        auto heading = [&] (const juce::String& t) { headings.push_back ({ t, { 16, y, w, 20 } }); y += 28; };
        auto row = [&] (const juce::String& label, juce::Component& c, int h = 30)
        {
            labels.push_back ({ label, { 16, y, w, 16 } });
            y += 18;
            c.setBounds (16, y, w, h);
            y += h + 14;
        };

        heading ("Display");
        row ("Spectrum style", style);
        dot.setBounds (16, y, (w - 12) / 2, 34);
        spacing.setBounds (16 + (w - 12) / 2 + 12, y, (w - 12) / 2, 34);
        y += 48;
        row ("Spectrum refresh rate (fps)", refresh);
        row ("Spectrum resolution", resolution);
        brightness.setBounds (16, y, w, 34); y += 48;
        row ("EQ scale", scale);
        row ("Spectrum Grab (click a peak to add a band on it)", grab);

        y += 6;
        heading ("Interface");
        row ("Size", size);
        row ("Renderer", renderer);

        y += 6;
        heading ("Colour presets");
        const int cw = (w - 10) / 2, ch = 70;
        for (int i = 0; i < cards.size(); ++i)
            cards[i]->setBounds (16 + (i % 2) * (cw + 10), y + (i / 2) * (ch + 10), cw, ch);
        y += ((cards.size() + 1) / 2) * (ch + 10) + 10;

        heading ("Custom colours");
        customNote = { 16, y - 8, w, 16 };
        y += 12;
        for (auto* r : rows) { r->setBounds (12, y, w + 8, 28); y += 30; }
        y += 8;
        fill.setBounds (16, y, w, 34); y += 48;
        reset.setBounds (16, y, w, 30); y += 44;

        heading ("Startup");
        labels.push_back ({ "New instances start with the saved default EQ and options.", { 16, y - 6, w, 16 } });
        y += 16;
        saveDefaults.setBounds (16, y, w, 32); y += 40;
        resetDefaults.setBounds (16, y, w, 30); y += 54;

        if (getHeight() != y) setSize (getWidth(), y);
    }

    GlobalSettings& settings;
    SegmentedControl refresh { { "30", "60", "120", "144" } };
    SegmentedControl resolution { { "Low", "Medium", "High", "Ultra" } };
    BarSlider brightness { "Spectrum brightness" };
    SegmentedControl scale { { juce::String (juce::CharPointer_UTF8 ("\xc2\xb1" "6 dB")), juce::String (juce::CharPointer_UTF8 ("\xc2\xb1" "12")),
                               juce::String (juce::CharPointer_UTF8 ("\xc2\xb1" "18")), juce::String (juce::CharPointer_UTF8 ("\xc2\xb1" "30")) } };
    SegmentedControl size { { "80%", "100%", "125%", "150%", "175%", "200%" } };
    SegmentedControl renderer { { "GPU (OpenGL)", "CPU (software)" } };
    SegmentedControl style { { "Smooth", "Particles" } };
    BarSlider dot { "Dot size" }, spacing { "Dot spacing" };
    SegmentedControl grab { { "Off", "On" } };
    juce::TextButton saveDefaults { "Save current EQ and options as default" };
    juce::TextButton resetDefaults { "Reset defaults to factory" };
    std::function<void()> onSave, onReset;
    juce::uint32 saved = 0;
    BarSlider fill { "Spectrum fill opacity" };
    juce::TextButton reset { "Reset colours to preset" };
    juce::OwnedArray<juce::Component> cards, rows;
    int lastPreset = 0;

    struct Text { juce::String text; juce::Rectangle<int> area; };
    std::vector<Text> headings, labels;
    juce::Rectangle<int> customNote;
};

// ------------------------------------------------------------------ SettingsPanel
SettingsPanel::SettingsPanel (GlobalSettings& s) : settings (s)
{
    content = std::make_unique<Content> (settings);
    content->onSave = [this] { if (onSaveDefaults) onSaveDefaults(); };
    content->onReset = [this] { if (onResetDefaults) onResetDefaults(); };
    viewport.setViewedComponent (content.get(), false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (8);
    addAndMakeVisible (viewport);
    closeButton.onClick = [this] { close(); };
    addAndMakeVisible (closeButton);
    settings.addChangeListener (this);
    setVisible (false);
    setInterceptsMouseClicks (true, true);
}

SettingsPanel::~SettingsPanel()
{
    settings.removeChangeListener (this);
}

void SettingsPanel::changeListenerCallback (juce::ChangeBroadcaster*)
{
    content->sync();
    repaint();
}

void SettingsPanel::open()
{
    content->sync();
    target = 1.0f;
    setVisible (true);
    toFront (false);
    startTimerHz (60);
}

void SettingsPanel::close()
{
    target = 0.0f;
    startTimerHz (60);
}

juce::Rectangle<int> SettingsPanel::panelBounds() const
{
    const float ease = 1.0f - std::pow (1.0f - amount, 3.0f);
    const int x = getWidth() - juce::roundToInt (panelWidth * ease);
    return { x, 0, panelWidth, getHeight() };
}

void SettingsPanel::timerCallback()
{
    const float step = 1.0f / 12.0f; // about 200 ms
    amount = target > amount ? juce::jmin (target, amount + step) : juce::jmax (target, amount - step);
    if (amount == target) stopTimer();
    if (amount == 0.0f) setVisible (false);
    resized();
    repaint();
}

void SettingsPanel::paint (juce::Graphics& g)
{
    g.setColour (theme::bg.withAlpha (0.45f * amount));
    g.fillRect (getLocalBounds().withRight (panelBounds().getX()));

    const auto r = panelBounds();
    g.setColour (juce::Colours::black.withAlpha (0.25f * amount));
    g.fillRect (r.translated (-6, 0).withWidth (6));
    g.setColour (theme::panel);
    g.fillRect (r);
    g.setColour (theme::line);
    g.fillRect (r.getX(), 0, 1, getHeight());
    g.setColour (theme::text);
    g.setFont (theme::font (15.0f, true));
    g.drawText ("Options", r.getX() + 16, 0, 200, 52, juce::Justification::centredLeft);
    g.setColour (theme::line);
    g.fillRect (r.getX(), 51, r.getWidth(), 1);
}

void SettingsPanel::resized()
{
    const auto r = panelBounds();
    closeButton.setBounds (r.getRight() - 46, 10, 32, 32);
    viewport.setBounds (r.getX() + 4, 52, r.getWidth() - 4, getHeight() - 52);
    content->setSize (viewport.getMaximumVisibleWidth(), content->getHeight());
}

void SettingsPanel::mouseDown (const juce::MouseEvent& e)
{
    if (! panelBounds().contains (e.getPosition())) close();
}
