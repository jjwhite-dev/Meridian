#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_opengl/juce_opengl.h>

#include "PluginProcessor.h"
#include "ui/EqDisplay.h"
#include "ui/GlobalSettings.h"
#include "ui/LookAndFeel.h"
#include "ui/Panels.h"
#include "ui/SettingsPanel.h"

class MeridianEditor : public juce::AudioProcessorEditor,
                       private juce::Timer,
                       private juce::ChangeListener
{
public:
    explicit MeridianEditor (MeridianProcessor&);
    ~MeridianEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    /** Everything is laid out at "logical" size inside this component, which is
        scaled by the interface size setting. */
    class Root : public juce::Component
    {
    public:
        std::function<void()> onResize, onBackgroundClick;
        void resized() override { if (onResize) onResize(); }
        void mouseDown (const juce::MouseEvent&) override { if (onBackgroundClick) onBackgroundClick(); }
    };

    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void selectBand (int band);
    void applySettings();
    void layoutRoot();
    void setGpuRendering (bool shouldUseGpu);
    void undo();
    void redo();

    MeridianProcessor& proc;
    juce::SharedResourcePointer<GlobalSettings> settings; // must exist before the look and feel (it applies the theme)
    MeridianLookAndFeel lnf;
    juce::OpenGLContext glContext;
    juce::TooltipWindow tooltips { this, 700 };

    Root root;
    HeaderBar header;
    EqDisplay display;
    BandStrip strip;
    BandPanel bandPanel;
    DynamicsPanel dynamicsPanel;
    GlobalPanel globalPanel;
    SettingsPanel settingsPanel;

    int selected = -1;
    float appliedScale = 0.0f;
    int appliedFftOrder = 0, appliedRefresh = 0;
    bool resizingFromScale = false;

    static constexpr int minLogicalW = 1100, minLogicalH = 680;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MeridianEditor)
};
