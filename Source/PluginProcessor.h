#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "AnalyzerFifo.h"
#include "Params.h"
#include "dsp/EqEngine.h"

class MeridianProcessor : public juce::AudioProcessor
{
public:
    MeridianProcessor();
    ~MeridianProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;
    bool supportsDoublePrecisionProcessing() const override { return true; }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Meridian EQ"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- used by the editor (message thread)
    juce::AudioProcessorValueTreeState apvts;
    AnalyzerFifo preFifo, postFifo, sidechainFifo;

    /** Band being auditioned (-1 = none). Written by the editor. */
    std::atomic<int> auditionBand { -1 };
    /** Band whose solo button is locked on (message thread only). */
    int lockedSolo = -1;

    juce::RangedAudioParameter* bandParam (int band, const char* suffix) const;
    float bandValue (int band, const char* suffix) const;
    void setBandValue (int band, const char* suffix, float value);

    /** Turns on the first free band with the given settings. Returns its index or -1. */
    int addBand (meq::FilterType type, float freq, float gain, float q);
    void removeBand (int band);
    void resetBandToDefaults (int band);

    /** A snapshot of one band's settings for drawing. */
    meq::BandParams readBand (int band) const;

    float liveGainDb (int band) const   { return engine.liveGainDb (band); }
    float liveDynDb (int band) const    { return engine.liveDynDb (band); }
    float autoGainDb() const            { return engine.autoGainDb(); }
    double engineSampleRate() const     { return engineRate.load(); }
    double baseSampleRate() const       { return baseRate.load(); }
    bool sidechainConnected() const     { return sidechainLive.load(); }
    meq::Transform currentTransform() const;

private:
    struct BandPtrs
    {
        std::array<std::atomic<float>*, std::size (params::band::all)> p {};
    };

    template <typename T> void processT (juce::AudioBuffer<T>& buffer);
    void pullParams();
    void updateLatency();

    meq::Engine engine;
    std::array<BandPtrs, meq::kNumBands> bandPtrs;
    std::atomic<float>* pPhase = nullptr;
    std::atomic<float>* pOs = nullptr;
    std::atomic<float>* pGainScale = nullptr;
    std::atomic<float>* pOut = nullptr;
    std::atomic<float>* pWidth = nullptr;
    std::atomic<float>* pAutoGain = nullptr;
    std::atomic<float>* pMonitor = nullptr;
    std::atomic<float>* pLookahead = nullptr;

    std::array<meq::BandParams, meq::kNumBands> bandScratch {};

    juce::dsp::Oversampling<double> osMain { 2, 1, juce::dsp::Oversampling<double>::filterHalfBandPolyphaseIIR, true, false };
    juce::dsp::Oversampling<double> osSide { 2, 1, juce::dsp::Oversampling<double>::filterHalfBandPolyphaseIIR, true, false };
    juce::AudioBuffer<double> work, sideWork;
    bool osActive = false;
    int reportedLatency = -1;

    std::atomic<double> baseRate { 48000.0 }, engineRate { 48000.0 };
    std::atomic<bool> sidechainLive { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MeridianProcessor)
};
