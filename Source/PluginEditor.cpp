#include "PluginEditor.h"

MeridianEditor::MeridianEditor (MeridianProcessor& p)
    : AudioProcessorEditor (p), proc (p), header (p), display (p), strip (p), bandPanel (p), dynamicsPanel (p),
      globalPanel (p), settingsPanel (*settings)
{
    setLookAndFeel (&lnf);
    setWantsKeyboardFocus (true);

    // GPU rendering: the whole editor is drawn through OpenGL (can be switched off in the options)
    glContext.setComponentPaintingEnabled (true);
    glContext.setContinuousRepainting (false);
    globalPanel.rendererName = [this] { return glContext.isAttached() ? juce::String ("OpenGL") : juce::String ("CPU renderer"); };

    header.onSettings = [this] { settingsPanel.isOpen() ? settingsPanel.close() : settingsPanel.open(); };
    header.onUndo = [this] { undo(); };
    header.onRedo = [this] { redo(); };
    header.onBackgroundClick = [this] { selectBand (-1); };
    globalPanel.onBackgroundClick = [this] { selectBand (-1); };
    root.onBackgroundClick = [this] { selectBand (-1); };
    display.onSelect = [this] (int b) { selectBand (b); };
    display.onDbRangeChange = [this] (float r) { settings->dbRange = r; settings->changed(); };
    strip.onSelect = [this] (int b) { selectBand (b); };
    bandPanel.onSelect = [this] (int b) { selectBand (b); };

    addAndMakeVisible (root);
    for (juce::Component* c : { (juce::Component*) &header, (juce::Component*) &display, (juce::Component*) &strip,
                                (juce::Component*) &bandPanel, (juce::Component*) &dynamicsPanel, (juce::Component*) &globalPanel })
        root.addAndMakeVisible (c);
    root.addChildComponent (settingsPanel);
    root.onResize = [this] { layoutRoot(); };

    // every click starts a new undo step; listen to clicks anywhere in the editor
    addMouseListener (this, true);

    for (int b = 0; b < meq::kNumBands; ++b)
        if (proc.bandValue (b, params::band::on) > 0.5f) { selectBand (b); break; }

    setResizable (true, true);
    settings->addChangeListener (this);
    applySettings();
}

MeridianEditor::~MeridianEditor()
{
    settings->removeChangeListener (this);
    stopTimer();
    removeMouseListener (this);
    if (glContext.isAttached()) glContext.detach();
    setLookAndFeel (nullptr);
}

void MeridianEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    applySettings();
}

void MeridianEditor::applySettings()
{
    auto& s = *settings;
    lnf.applyTheme();

    if (s.fftOrder != appliedFftOrder) { display.setAnalyzerOrder (s.fftOrder); appliedFftOrder = s.fftOrder; }
    display.setBrightness (s.brightness);
    display.setSpectrumStyle (s.spectrumStyle);
    display.setParticleLook (s.particleDot, s.particleSpacing);
    display.setFrameRate (s.refreshHz);
    display.setDbRange (s.dbRange);
    if (s.refreshHz != appliedRefresh) { startTimerHz (juce::jlimit (15, 144, s.refreshHz)); appliedRefresh = s.refreshHz; }
    setGpuRendering (s.gpu);

    if (std::abs (s.uiScale - appliedScale) > 0.001f)
    {
        appliedScale = s.uiScale;
        const auto sc = appliedScale;
        resizingFromScale = true;
        setResizeLimits (juce::roundToInt (minLogicalW * sc), juce::roundToInt (minLogicalH * sc),
                         juce::roundToInt (3200 * sc), juce::roundToInt (2000 * sc));
        setSize (juce::roundToInt (juce::jmax (minLogicalW, s.logicalWidth) * sc),
                 juce::roundToInt (juce::jmax (minLogicalH, s.logicalHeight) * sc));
        resizingFromScale = false;
        resized();
    }

    // repaint everything in the new colours
    std::function<void (juce::Component&)> repaintAll = [&] (juce::Component& c)
    {
        c.repaint();
        for (auto* child : c.getChildren()) repaintAll (*child);
    };
    repaintAll (*this);
}

void MeridianEditor::setGpuRendering (bool shouldUseGpu)
{
    if (shouldUseGpu && ! glContext.isAttached()) glContext.attachTo (*this);
    else if (! shouldUseGpu && glContext.isAttached()) glContext.detach();
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

void MeridianEditor::undo()
{
    proc.undoManager.undo();
}

void MeridianEditor::redo()
{
    proc.undoManager.redo();
}

bool MeridianEditor::keyPressed (const juce::KeyPress& key)
{
    const auto mods = key.getModifiers();
    if (mods.isCommandDown() && key.getKeyCode() == 'Z')
    {
        mods.isShiftDown() ? redo() : undo();
        return true;
    }
    if (mods.isCommandDown() && key.getKeyCode() == 'Y') { redo(); return true; }
    if (key == juce::KeyPress::escapeKey)
    {
        if (settingsPanel.isOpen()) settingsPanel.close(); else selectBand (-1);
        return true;
    }
    return false;
}

void MeridianEditor::mouseDown (const juce::MouseEvent&)
{
    // reached for clicks anywhere (we listen to all children): group what follows into one undo step
    proc.undoManager.beginNewTransaction();
}

void MeridianEditor::timerCallback()
{
    proc.presets.tick();
    if (selected >= 0 && proc.bandValue (selected, params::band::on) < 0.5f)
        selectBand (-1);
    display.refresh();
    strip.repaint();
    header.update();
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
    const float sc = appliedScale > 0.0f ? appliedScale : 1.0f;
    const int lw = juce::roundToInt ((float) getWidth() / sc);
    const int lh = juce::roundToInt ((float) getHeight() / sc);
    root.setTransform (juce::AffineTransform::scale (sc));
    root.setBounds (0, 0, lw, lh);

    if (! resizingFromScale && appliedScale > 0.0f && (lw != settings->logicalWidth || lh != settings->logicalHeight))
    {
        settings->logicalWidth = lw;
        settings->logicalHeight = lh;
    }
}

void MeridianEditor::layoutRoot()
{
    auto r = root.getLocalBounds();
    settingsPanel.setBounds (r);

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
}
