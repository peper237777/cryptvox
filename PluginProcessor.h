#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "Params.h"

class CryptvoxProcessor : public juce::AudioProcessor,
                          private juce::AudioProcessorValueTreeState::Listener
{
public:
    CryptvoxProcessor();
    ~CryptvoxProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Cryptvox"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> level { 0.f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void parameterChanged (const juce::String& id, float newValue) override;
    void updateFilters (float hpfF, float lpfF, float toneF);

    struct Line
    {
        std::vector<float> b; int w = 0;
        void init (int n) { b.assign ((size_t) n, 0.f); w = 0; }
        void push (float x) { b[(size_t) w] = x; if (++w >= (int) b.size()) w = 0; }
        float read (float d) const
        {
            const int n = (int) b.size();
            float p = (float) w - d;
            while (p < 0) p += (float) n;
            int i = (int) p; const float f = p - (float) i;
            if (i >= n) i -= n;
            const int i1 = i + 1 >= n ? 0 : i + 1;
            return b[(size_t) i] * (1.f - f) + b[(size_t) i1] * f;
        }
    };

    struct PitchShift
    {
        Line line; float phase = 0.f;
        void init (int n) { line.init (n); phase = 0.f; }
        float process (float x, float ratio, float win)
        {
            line.push (x);
            phase += (1.f - ratio) / win;
            phase -= std::floor (phase);
            float p2 = phase + 0.5f; if (p2 >= 1.f) p2 -= 1.f;
            const float s1 = std::sin (3.14159265f * phase);
            const float s2 = std::sin (3.14159265f * p2);
            return line.read (phase * win) * s1 * s1 + line.read (p2 * win) * s2 * s2;
        }
    };

    float distort (float x, int mode, float g, float comp) const;

    using Filt = juce::dsp::IIR::Filter<float>;
    Filt hp[2], lp[2], toneLp[2];
    juce::dsp::StateVariableTPTFilter<float> svf;
    PitchShift ps[2];
    Line dblM, dlyL, dlyR;
    juce::Reverb reverb;
    juce::AudioBuffer<float> dryBuf, revBuf;
    juce::Random rng;

    std::atomic<float>* P[Px::NP] {};
    double sr = 44100.0;
    float holdVal[2] {}; int holdCnt[2] {};
    float dcx[2] {}, dcy[2] {};
    float ringPh = 0, fPh = 0, gPh = 0, gGate = 1.f;
    bool gDrop = false;
    float e2 = 0, lfo = 0, lpl = 0, lpr = 0;
    float lastHpf = -1, lastLpf = -1, lastTone = -1;
    bool loadingState = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CryptvoxProcessor)
};
