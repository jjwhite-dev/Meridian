#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "../dsp/EqEngine.h"

class MeridianProcessor;
class AnalyzerFifo;

/** The big curve + analyzer view. Bands are dots: drag to move (frequency /
    gain), wheel for Q, double-click to add or remove, right-click for a menu.
    The speaker button above a dot auditions that band. Clicking near a peak in
    the analyzer grabs it into a new band (Spectrum Grab). Every band's own
    curve is drawn in the colour of its processing channel. */
class EqDisplay : public juce::Component
{
public:
    explicit EqDisplay (MeridianProcessor&);

    /** Pull new analyzer data, rebuild curves and repaint. Called by the editor's timer. */
    void refresh();

    void setSelected (int band);
    int getSelected() const { return selected; }
    std::function<void (int)> onSelect;
    /** Called when the user cycles the dB scale by clicking it. */
    std::function<void (float)> onDbRangeChange;

    void setAnalyzerOrder (int order);
    void setBrightness (float b)        { brightness = b; repaint(); }

    /** 0 = smooth curve, 1 = particles (columns of glowing dots with falling peak dots). */
    void setSpectrumStyle (int style)   { spectrumStyle = style; repaint(); }
    void setParticleLook (float dotSize, int spacing);
    void setFrameRate (int hz)          { frameRate = juce::jmax (15, hz); }
    void setSpectrumGrab (bool b)       { spectrumGrabOn = b; if (! b) grabCandidate = false; }

    /** Freeze: holds the input spectrum (averaged over the last half second) and
        shows it with the current EQ applied as a "ghost" spectrum, so you can EQ
        the frozen sound and see the result. */
    void setFrozen (bool shouldFreeze);
    bool isFrozen() const { return frozen; }
    void setDbRange (float r)           { dbRange = r; repaint(); }
    float getDbRange() const            { return dbRange; }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    /** Number of points of the smoothed, log-spaced spectrum. */
    static constexpr int kGridPoints = 256;

private:
    struct Spectrum
    {
        std::vector<float> ring;  // last N samples
        std::vector<float> db;    // raw bins (for peak finding)
        std::vector<float> grid;  // 1/6-octave smoothed, log-spaced, time-smoothed (for drawing)
        std::vector<float> avg;   // the same, averaged over about half a second (for Freeze)
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
    double gridDbAt (const Spectrum&, double f) const;

    void pull (AnalyzerFifo&, Spectrum&);
    void analyse (Spectrum&);
    void rebuildCurves();
    juce::Path spectrumPath (const Spectrum&, bool closed) const;
    juce::Path curvePath (const std::vector<float>& db) const;
    void updateParticles();
    void buildSprites();
    void drawParticles (juce::Graphics&);
    void computeStatic (juce::uint64 key);
    void computeLive();
    void paintBackground (juce::Graphics&);   // grid + labels (cached)
    void paintCurves (juce::Graphics&);       // band curves, fills, channel sums (cached)
    juce::Colour heatColour (float t) const;
    float heatAt (float y) const;

    int nodeAt (juce::Point<float>) const;
    int auditionButtonAt (juce::Point<float>) const;
    juce::Point<float> nodePosition (int band) const;
    juce::Rectangle<float> auditionButtonBounds (int band) const;
    bool showsAuditionButton (int band) const;
    void toggleAudition (int band);
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
    std::vector<float> fftData, scratch, power;
    std::array<double, kGridPoints> gridFreqs {};
    Spectrum pre, post, side;
    Spectrum frozenInput, ghost;   // Freeze: held input, and held input + current EQ
    bool frozen = false, spectrumGrabOn = false;
    juce::TextButton freezeButton { "Freeze" };
    std::vector<float> weightedDb; // all bands on one curve (stereo/mid full, L/R half, side quarter), for the ghost
    const Spectrum& mainSpectrum() const { return frozen ? ghost : post; }
    void updateGhost();
    float brightness = 1.0f;

    // particle style
    int spectrumStyle = 0, frameRate = 60;
    float dotSize = 2.4f;
    int dotSpacing = 6;
    unsigned frameCount = 0;
    std::vector<float> colTop, colPeak;      // per column: y of the top dot row, y of the falling peak dot
    std::vector<int> colRows;                // per column: number of dot rows
    juce::Image heatColumn;                  // one column of dots, coloured bottom (cool) to top (hot)
    std::array<juce::Image, 16> peakSprites; // falling peak dots at 16 heat levels
    juce::String spriteKey;

    // cached layers: only redrawn when what they show changes
    juce::Image backgroundLayer, curveLayer;
    juce::String backgroundKey, curveKey;
    juce::uint64 staticKey = 0;
    int staticVersion = 0;
    bool anyDynamicBand = false;
    float layerScale() const;
    static constexpr float spritePixelScale = 2.0f;
    double specRate = 48000.0;

    // curves
    double rate = 48000.0;
    double gainScale = 1.0;
    std::array<meq::BandParams, meq::kNumBands> bands {};
    std::array<meq::Cascade, meq::kNumBands> cascades {};
    std::array<std::vector<float>, meq::kNumBands> bandDb;     // each band on its own
    std::array<std::vector<float>, 5> channelDb, channelLiveDb; // per processing channel (stereo bands included)
    std::array<bool, 5> channelUsed {}, channelDynamic {};
    std::vector<double> pointFreqs;
    std::vector<float> selRangeDb;

    // view / interaction
    float dbRange = 18.0f;
    int selected = -1, hoverNode = -1, hoverAudition = -1, dragBand = -1;
    bool hovering = false;
    juce::Point<float> hoverPos;
    bool grabCandidate = false;
    double grabFreq = 0, grabDb = 0;
    juce::Rectangle<int> dbLabelArea;
};
