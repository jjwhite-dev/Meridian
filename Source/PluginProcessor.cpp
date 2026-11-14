#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    enum BandSlot { kOn, kType, kFreq, kGain, kQ, kSlope, kChannel, kBypass, kDyn, kDynMode,
                    kThr, kRange, kRatio, kAttack, kRelease, kOnset, kBody, kDetect, kDetFreq, kDetQ };
}

MeridianProcessor::MeridianProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "MeridianEQ", params::createLayout())
{
    for (int b = 0; b < meq::kNumBands; ++b)
        for (size_t s = 0; s < std::size (params::band::all); ++s)
            bandPtrs[(size_t) b].p[s] = apvts.getRawParameterValue (params::bandId (b, params::band::all[s]));

    pPhase     = apvts.getRawParameterValue (params::phase);
    pOs        = apvts.getRawParameterValue (params::oversample);
    pGainScale = apvts.getRawParameterValue (params::gainScale);
    pOut       = apvts.getRawParameterValue (params::output);
    pWidth     = apvts.getRawParameterValue (params::width);
    pAutoGain  = apvts.getRawParameterValue (params::autoGain);
    pMonitor   = apvts.getRawParameterValue (params::monitor);
    pLookahead = apvts.getRawParameterValue (params::lookahead);
}

bool MeridianProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (in != out) return false;
    if (in != juce::AudioChannelSet::stereo() && in != juce::AudioChannelSet::mono()) return false;

    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo())
            return false;
    }
    return true;
}

void MeridianProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    baseRate = sampleRate;
    const int maxBlock = juce::jmax (1, samplesPerBlock);
    work.setSize (2, maxBlock, false, true, false);
    sideWork.setSize (2, maxBlock, false, true, false);
    osMain.initProcessing ((size_t) maxBlock);
    osSide.initProcessing ((size_t) maxBlock);
    osMain.reset();
    osSide.reset();

    osActive = pOs->load() > 0.5f;
    engine.prepare (sampleRate * (osActive ? 2.0 : 1.0), sampleRate * 2.0);
    engineRate = engine.sampleRate();
    pullParams();
    reportedLatency = -1;
    updateLatency();
}

meq::Transform MeridianProcessor::currentTransform() const
{
    // Linear and Mixed are not implemented yet in this prototype: they run as Analog.
    return juce::roundToInt (pPhase->load()) == 0 ? meq::Transform::Bilinear : meq::Transform::Matched;
}

void MeridianProcessor::pullParams()
{
    for (int b = 0; b < meq::kNumBands; ++b)
    {
        const auto& p = bandPtrs[(size_t) b].p;
        auto& d = bandScratch[(size_t) b];
        d.enabled   = p[kOn]->load() > 0.5f;
        d.type      = (meq::FilterType) juce::jlimit (0, (int) meq::FilterType::Count - 1, juce::roundToInt (p[kType]->load()));
        d.freq      = p[kFreq]->load();
        d.gain      = p[kGain]->load();
        d.q         = p[kQ]->load();
        d.slope     = 6 * (1 + juce::roundToInt (p[kSlope]->load()));
        d.channel   = (meq::Channel) juce::jlimit (0, 4, juce::roundToInt (p[kChannel]->load()));
        d.bypass    = p[kBypass]->load() > 0.5f;
        d.dynOn     = p[kDyn]->load() > 0.5f;
        d.dynMode   = (meq::DynMode) juce::jlimit (0, 2, juce::roundToInt (p[kDynMode]->load()));
        d.threshold = p[kThr]->load();
        d.range     = p[kRange]->load();
        d.ratio     = p[kRatio]->load();
        d.attackMs  = p[kAttack]->load();
        d.releaseMs = p[kRelease]->load();
        d.onset     = p[kOnset]->load();
        d.body      = p[kBody]->load();
        d.detect    = (meq::Detect) juce::jlimit (0, 2, juce::roundToInt (p[kDetect]->load()));
        d.detFreq   = p[kDetFreq]->load();
        d.detQ      = p[kDetQ]->load();
    }

    meq::GlobalParams g;
    g.transform   = currentTransform();
    g.gainScale   = pGainScale->load() / 100.0;
    g.outputDb    = pOut->load();
    g.width       = pWidth->load() / 100.0;
    g.autoGain    = pAutoGain->load() > 0.5f;
    g.monitor     = (meq::Monitor) juce::jlimit (0, 2, juce::roundToInt (pMonitor->load()));
    g.lookaheadMs = pLookahead->load();
    g.solo        = auditionBand.load();
    engine.setParams (bandScratch, g);
}

void MeridianProcessor::updateLatency()
{
    const int engineLatency = engine.latencySamples();
    const int latency = osActive ? (int) std::lround (engineLatency / 2.0 + osMain.getLatencyInSamples())
                                 : engineLatency;
    if (latency != reportedLatency)
    {
        reportedLatency = latency;
        setLatencySamples (latency);
    }
}

void MeridianProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)  { processT (buffer); }
void MeridianProcessor::processBlock (juce::AudioBuffer<double>& buffer, juce::MidiBuffer&) { processT (buffer); }

template <typename T>
void MeridianProcessor::processT (juce::AudioBuffer<T>& buffer)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n == 0) return;

    auto main = getBusBuffer (buffer, true, 0);
    const int mainCh = main.getNumChannels();
    if (mainCh == 0) return;

    if (work.getNumSamples() < n)
    {
        // host sent a bigger block than announced; grow (rare)
        work.setSize (2, n, false, true, true);
        sideWork.setSize (2, n, false, true, true);
        osMain.initProcessing ((size_t) n);
        osSide.initProcessing ((size_t) n);
    }

    for (int c = 0; c < 2; ++c)
    {
        const T* src = main.getReadPointer (juce::jmin (c, mainCh - 1));
        double* dst = work.getWritePointer (c);
        for (int i = 0; i < n; ++i) dst[i] = (double) src[i];
    }

    bool haveSide = false;
    if (getBusCount (true) > 1 && getBus (true, 1) != nullptr && getBus (true, 1)->isEnabled())
    {
        auto side = getBusBuffer (buffer, true, 1);
        const int sc = side.getNumChannels();
        if (sc > 0)
        {
            haveSide = true;
            for (int c = 0; c < 2; ++c)
            {
                const T* src = side.getReadPointer (juce::jmin (c, sc - 1));
                double* dst = sideWork.getWritePointer (c);
                for (int i = 0; i < n; ++i) dst[i] = (double) src[i];
            }
        }
    }
    sidechainLive = haveSide;

    preFifo.push (work.getReadPointer (0), work.getReadPointer (1), n);
    if (haveSide) sidechainFifo.push (sideWork.getReadPointer (0), sideWork.getReadPointer (1), n);

    const bool wantOs = pOs->load() > 0.5f;
    if (wantOs != osActive)
    {
        osActive = wantOs;
        engine.setSampleRate (baseRate.load() * (osActive ? 2.0 : 1.0));
        engineRate = engine.sampleRate();
        osMain.reset();
        osSide.reset();
    }

    pullParams();

    if (osActive)
    {
        juce::dsp::AudioBlock<double> block (work.getArrayOfWritePointers(), 2, (size_t) n);
        auto up = osMain.processSamplesUp (block);
        const double* sl = nullptr;
        const double* sr = nullptr;
        if (haveSide)
        {
            juce::dsp::AudioBlock<double> sblock (sideWork.getArrayOfWritePointers(), 2, (size_t) n);
            auto upSide = osSide.processSamplesUp (sblock);
            sl = upSide.getChannelPointer (0);
            sr = upSide.getChannelPointer (1);
        }
        engine.process (up.getChannelPointer (0), up.getChannelPointer (1), sl, sr, (int) up.getNumSamples());
        osMain.processSamplesDown (block);
    }
    else
    {
        engine.process (work.getWritePointer (0), work.getWritePointer (1),
                        haveSide ? sideWork.getReadPointer (0) : nullptr,
                        haveSide ? sideWork.getReadPointer (1) : nullptr, n);
    }

    postFifo.push (work.getReadPointer (0), work.getReadPointer (1), n);

    for (int c = 0; c < mainCh; ++c)
    {
        T* dst = main.getWritePointer (c);
        const double* src = work.getReadPointer (juce::jmin (c, 1));
        if (mainCh == 1)
        {
            const double* r = work.getReadPointer (1);
            for (int i = 0; i < n; ++i) dst[i] = (T) (0.5 * (src[i] + r[i]));
        }
        else
        {
            for (int i = 0; i < n; ++i) dst[i] = (T) src[i];
        }
    }
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, n);

    updateLatency();
}

juce::RangedAudioParameter* MeridianProcessor::bandParam (int band, const char* suffix) const
{
    return apvts.getParameter (params::bandId (band, suffix));
}

float MeridianProcessor::bandValue (int band, const char* suffix) const
{
    if (auto* v = apvts.getRawParameterValue (params::bandId (band, suffix))) return v->load();
    return 0.0f;
}

void MeridianProcessor::setBandValue (int band, const char* suffix, float value)
{
    if (auto* p = bandParam (band, suffix))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (value));
        p->endChangeGesture();
    }
}

void MeridianProcessor::resetBandToDefaults (int band)
{
    for (auto* suffix : params::band::all)
        if (auto* p = bandParam (band, suffix))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->getDefaultValue());
            p->endChangeGesture();
        }
}

int MeridianProcessor::addBand (meq::FilterType type, float freq, float gain, float q)
{
    for (int b = 0; b < meq::kNumBands; ++b)
    {
        if (bandValue (b, params::band::on) > 0.5f) continue;
        resetBandToDefaults (b);
        setBandValue (b, params::band::type, (float) (int) type);
        setBandValue (b, params::band::freq, freq);
        setBandValue (b, params::band::gain, gain);
        setBandValue (b, params::band::q, q);
        setBandValue (b, params::band::detFreq, freq);
        setBandValue (b, params::band::on, 1.0f);
        return b;
    }
    return -1;
}

void MeridianProcessor::removeBand (int band)
{
    if (lockedSolo == band) lockedSolo = -1;
    if (auditionBand.load() == band) auditionBand = lockedSolo;
    setBandValue (band, params::band::on, 0.0f);
}

meq::BandParams MeridianProcessor::readBand (int b) const
{
    meq::BandParams d;
    const auto& p = bandPtrs[(size_t) b].p;
    d.enabled = p[kOn]->load() > 0.5f;
    d.type    = (meq::FilterType) juce::jlimit (0, (int) meq::FilterType::Count - 1, juce::roundToInt (p[kType]->load()));
    d.freq    = p[kFreq]->load();
    d.gain    = p[kGain]->load();
    d.q       = p[kQ]->load();
    d.slope   = 6 * (1 + juce::roundToInt (p[kSlope]->load()));
    d.channel = (meq::Channel) juce::jlimit (0, 4, juce::roundToInt (p[kChannel]->load()));
    d.bypass  = p[kBypass]->load() > 0.5f;
    d.dynOn   = p[kDyn]->load() > 0.5f;
    d.range   = p[kRange]->load();
    d.detect  = (meq::Detect) juce::jlimit (0, 2, juce::roundToInt (p[kDetect]->load()));
    d.detFreq = p[kDetFreq]->load();
    d.detQ    = p[kDetQ]->load();
    return d;
}

void MeridianProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void MeridianProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* MeridianProcessor::createEditor()
{
    return new MeridianEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MeridianProcessor();
}
