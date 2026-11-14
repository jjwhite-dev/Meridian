#include "PluginEditor.h"

namespace
{
    // GUI settings live in the plugin state (saved with the session)
    constexpr const char* kRefresh = "uiRefresh";   // Hz
    constexpr const char* kFftOrder = "uiFft";      // 11..14
    constexpr const char* kBright = "uiBright";     // 0.5..1.5
    constexpr const char* kGpu = "uiGpu";           // bool: draw the editor with OpenGL
    constexpr const char* kRange = "uiRange";       // dB
    constexpr const char* kWidth = "uiW";
    constexpr const char* kHeight = "uiH";
}

MeridianEditor::MeridianEditor (MeridianProcessor& p)
    : AudioProcessorEditor (p), proc (p), header (p), display (p), strip (p), bandPanel (p), dynamicsPanel (p), globalPanel (p)
{
    setLookAndFeel (&lnf);

    // GPU rendering: the whole editor is drawn through OpenGL (can be switched off in the settings menu)
    glContext.setComponentPaintingEnabled (true);
    glContext.setContinuousRepainting (false);
    globalPanel.rendererName = [this] { return glContext.isAttached() ? juce::String ("OpenGL") : juce::String ("CPU renderer"); };

    header.onSettings = [this] { showSettings(); };
    display.onSelect = [this] (int b) { selectBand (b); };
    strip.onSelect = [this] (int b) { selectBand (b); };
    bandPanel.onSelect = [this] (int b) { selectBand (b); };

    for (juce::Component* c : { (juce::Component*) &header, (juce::Component*) &display, (juce::Component*) &strip,
                                (juce::Component*) &bandPanel, (juce::Component*) &dynamicsPanel, (juce::Component*) &globalPanel })
        addAndMakeVisible (c);

    // start on the first band that is on, if any
    for (int b = 0; b < meq::kNumBands; ++b)
        if (proc.bandValue (b, params::band::on) > 0.5f) { selectBand (b); break; }

    setResizable (true, true);
    setResizeLimits (1000, 660, 2560, 1600);
    const auto& st = proc.apvts.state;
    setSize ((int) st.getProperty (kWidth, 1240), (int) st.getProperty (kHeight, 800));
    applySettings();
}

MeridianEditor::~MeridianEditor()
{
    stopTimer();
    if (glContext.isAttached()) glContext.detach();
    setLookAndFeel (nullptr);
}

void MeridianEditor::applySettings()
{
    const auto& st = proc.apvts.state;
    display.setAnalyzerOrder ((int) st.getProperty (kFftOrder, 12));
    display.setBrightness ((float) (double) st.getProperty (kBright, 1.0));
    setGpuRendering ((bool) st.getProperty (kGpu, true));
    display.setDbRange ((float) (double) st.getProperty (kRange, 18.0));
    startTimerHz (juce::jlimit (15, 144, (int) st.getProperty (kRefresh, 60)));
}

void MeridianEditor::setGpuRendering (bool shouldUseGpu)
{
    if (shouldUseGpu && ! glContext.isAttached()) glContext.attachTo (*this);
    else if (! shouldUseGpu && glContext.isAttached()) glContext.detach();
    repaint();
}

void MeridianEditor::showSettings()
{
    auto& st = proc.apvts.state;
    const int refresh = st.getProperty (kRefresh, 60);
    const int order = st.getProperty (kFftOrder, 12);
    const double bright = st.getProperty (kBright, 1.0);
    const bool gpu = st.getProperty (kGpu, true);
    const double range = st.getProperty (kRange, 18.0);

    juce::PopupMenu rate, res, light, scale, menu;
    for (int hz : { 30, 60, 120 }) rate.addItem (100 + hz, juce::String (hz) + " fps", true, refresh == hz);
    const char* resNames[] = { "Low (2048)", "Medium (4096)", "High (8192)", "Ultra (16384)" };
    for (int o = 11; o <= 14; ++o) res.addItem (200 + o, resNames[o - 11], true, order == o);
    const double levels[] = { 0.5, 0.75, 1.0, 1.5 };
    for (int i = 0; i < 4; ++i) light.addItem (300 + i, juce::String (juce::roundToInt (levels[i] * 100)) + "%", true, std::abs (bright - levels[i]) < 0.01);
    const int ranges[] = { 6, 12, 18, 30 };
    for (int i = 0; i < 4; ++i)
        scale.addItem (400 + i, juce::String (juce::CharPointer_UTF8 ("\xc2\xb1")) + juce::String (ranges[i]) + " dB", true, std::abs (range - ranges[i]) < 0.1);

    menu.addSectionHeader ("Display");
    menu.addSubMenu ("Spectrum refresh rate", rate);
    menu.addSubMenu ("Spectrum resolution", res);
    menu.addSubMenu ("Spectrum brightness", light);
    menu.addSubMenu ("EQ scale", scale);
    menu.addSeparator();
    menu.addSectionHeader ("Renderer");
    menu.addItem (1, "GPU (OpenGL)", true, gpu);
    menu.addItem (2, "CPU (software)", true, ! gpu);

    juce::Component::SafePointer<MeridianEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&header), [safe] (int r)
    {
        if (safe == nullptr || r == 0) return;
        auto& state = safe->proc.apvts.state;
        const double lv[] = { 0.5, 0.75, 1.0, 1.5 };
        const int rg[] = { 6, 12, 18, 30 };
        if (r == 1 || r == 2) state.setProperty (kGpu, r == 1, nullptr);
        else if (r >= 400) state.setProperty (kRange, rg[r - 400], nullptr);
        else if (r >= 300) state.setProperty (kBright, lv[r - 300], nullptr);
        else if (r >= 200) state.setProperty (kFftOrder, r - 200, nullptr);
        else if (r >= 100) state.setProperty (kRefresh, r - 100, nullptr);
        safe->applySettings();
    });
}

void MeridianEditor::selectBand (int band)
{
    selected = band;
    display.setSelected (band);
    strip.selected = band;
    strip.repaint();
    bandPanel.setBand (band);
    dynamicsPanel.setBand (band);
}

void MeridianEditor::timerCallback()
{
    if (selected >= 0 && proc.bandValue (selected, params::band::on) < 0.5f)
        selectBand (-1);
    display.refresh();
    strip.repaint();
    bandPanel.update();
    dynamicsPanel.update();
    globalPanel.update();
}

void MeridianEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::bg);
}

void MeridianEditor::resized()
{
    auto r = getLocalBounds();
    header.setBounds (r.removeFromTop (52));
    const int bottomH = juce::jlimit (280, 330, (int) (r.getHeight() * 0.38));
    auto bottom = r.removeFromBottom (bottomH);
    strip.setBounds (r.removeFromBottom (44));
    display.setBounds (r);

    bottom.reduce (12, 12);
    const int w = bottom.getWidth();
    bandPanel.setBounds (bottom.removeFromLeft ((int) (w * 0.36)));
    bottom.removeFromLeft (12);
    dynamicsPanel.setBounds (bottom.removeFromLeft ((int) (w * 0.40)));
    bottom.removeFromLeft (12);
    globalPanel.setBounds (bottom);

    proc.apvts.state.setProperty (kWidth, getWidth(), nullptr);
    proc.apvts.state.setProperty (kHeight, getHeight(), nullptr);
}
