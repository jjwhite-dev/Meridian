#pragma once

#include "Controls.h"
#include "GlobalSettings.h"

/** Options panel that slides in from the right edge (where the options
    button is). Covers its parent; clicking outside the panel closes it. */
class SettingsPanel : public juce::Component, private juce::Timer, private juce::ChangeListener
{
public:
    explicit SettingsPanel (GlobalSettings&);
    ~SettingsPanel() override;

    void open();
    void close();
    bool isOpen() const { return target > 0.5f; }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

    static constexpr int panelWidth = 340;

private:
    class Content;
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    juce::Rectangle<int> panelBounds() const;

    GlobalSettings& settings;
    float amount = 0.0f, target = 0.0f;
    IconButton closeButton { "Close", icons::cross() };
    juce::Viewport viewport;
    std::unique_ptr<Content> content;
};
