#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "../dsp/EqEngine.h"

class MeridianProcessor;
class AnalyzerFifo;

/** The big curve + analyzer view. Bands are dots: drag to move (frequency /
    gain), wheel for Q, double-click to add or remove, right-click for a menu.
    Holding a dot auditions the band. Clicking near a peak in the analyzer
    grabs it into a new band (Spectrum Grab). */
class EqDisplay : public juce::Component
{
public:
    explicit EqDisplay (MeridianProcessor&);

    /** Pull new analyzer data, rebuild curves and repaint. Called by the editor's timer. */
    void refresh();

    void setSelected (int band);
    int getSelected() const { return selected; }
    std::function<void (int)> onSelect;

    void setAnalyzerOrder (int order);
    void setBrightness (float b)        { brightness = b; repaint(); }
    void setDbRange (float r)           { dbRange = r; repaint(); }
    float getDbRange() const            { return dbRange; }
    void setAuditionOnDrag (bool b)     { auditionOnDrag = b; }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    struct Spectrum
    {
        std::vector<float> ring, db;
        int write = 0;
        bool active = false;
        int silentFrames = 0;
    };

    // mapping
    float xForFreq (double f) const;
    double freqForX (float x) const;
    float yForDb (double db) const;
    double dbForY (float y) const;
    float ySpec (double db, double f) const;
    double specDbAt (const Spectrum&, double f) const;

    void pull (AnalyzerFifo&, Spectrum&);
    void analyse (Spectrum&);
    void rebuildCurves();
    juce::Path spectrumPath (const Spectrum&, bool closed) const;
    juce::Path curvePath (const std::vector<float>& db) const;

    int nodeAt (juce::Point<float>) const;
    juce::Point<float> nodePosition (int band) const;
    bool findPeakNear (juce::Point<float> p, double& freq, double& db) const;
    void beginDrag (int band);
    void endDrag();
    void showBandMenu (int band);
    void setParam (int band, const char* suffix, float value);

    MeridianProcessor& proc;

    // analyzer
    int fftOrder = 12;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::unique_ptr<juce::dsp::WindowingFunction<float>> window;
    std::vector<float> fftData, scratch;
    Spectrum pre, post, side;
    float brightness = 1.0f;
    double specRate = 48000.0;

    // curves
    double rate = 48000.0;
    double gainScale = 1.0;
    std::array<meq::BandParams, meq::kNumBands> bands {};
    std::array<meq::Cascade, meq::kNumBands> cascades {};
    std::vector<double> pointFreqs;
    std::vector<float> totalDb, liveDb, selDb, selRangeDb;
    bool anyDynamic = false;

    // view / interaction
    float dbRange = 18.0f;
    int selected = -1, hoverNode = -1, dragBand = -1;
    bool hovering = false, auditionOnDrag = true, auditionFromDrag = false;
    juce::Point<float> hoverPos;
    bool grabCandidate = false;
    double grabFreq = 0, grabDb = 0;
    juce::Rectangle<int> dbLabelArea;
};
