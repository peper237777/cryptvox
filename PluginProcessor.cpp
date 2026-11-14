#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
inline float dB2g (float db) { return std::pow (10.f, db / 20.f); }

inline float softLimit (float x)
{
    const float a = std::abs (x);
    if (a <= 0.9f) return x;
    const float o = 0.9f + 0.1f * std::tanh ((a - 0.9f) * 10.f);
    return x < 0 ? -o : o;
}
}

juce::AudioProcessorValueTreeState::ParameterLayout CryptvoxProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "preset", 1 }, "PRESET",
                                                   StringArray::fromTokens (presetNames, "|", ""), 0));
    for (auto& s : specs)
    {
        if (s.choices != nullptr)
            l.add (std::make_unique<AudioParameterChoice> (ParameterID { s.id, 1 }, s.name,
                       StringArray::fromTokens (s.choices, "|", ""), (int) s.def));
        else
        {
            NormalisableRange<float> r (s.lo, s.hi, s.step);
            if (s.skewMid > 0) r.setSkewForCentre (s.skewMid);
            l.add (std::make_unique<AudioParameterFloat> (ParameterID { s.id, 1 }, s.name, r, s.def));
        }
    }
    return l;
}

CryptvoxProcessor::CryptvoxProcessor()
    : AudioProcessor (BusesProperties()
          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    for (int i = 0; i < Px::NP; ++i) P[i] = apvts.getRawParameterValue (specs[i].id);
    apvts.addParameterListener ("preset", this);
}

CryptvoxProcessor::~CryptvoxProcessor()
{
    apvts.removeParameterListener ("preset", this);
}

void CryptvoxProcessor::parameterChanged (const juce::String& id, float v)
{
    if (id != "preset" || loadingState) return;
    const int p = (int) v;
    if (p < 1 || p > 9) return;
    for (int i = 0; i < Px::NP; ++i)
        if (auto* prm = apvts.getParameter (specs[i].id))
            prm->setValueNotifyingHost (prm->convertTo0to1 (presets[p - 1][i]));
}

bool CryptvoxProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void CryptvoxProcessor::updateFilters (float hpfF, float lpfF, float toneF)
{
    using C = juce::dsp::IIR::Coefficients<float>;
    const float nyq = (float) sr * 0.45f;
    auto cHp = C::makeHighPass (sr, juce::jmin (hpfF, nyq), 0.7071f);
    auto cLp = C::makeLowPass (sr, juce::jmin (lpfF, nyq), 0.7071f);
    auto cTn = C::makeLowPass (sr, juce::jmin (toneF, nyq), 0.7071f);
    for (int c = 0; c < 2; ++c)
    {
        hp[c].coefficients = cHp; lp[c].coefficients = cLp; toneLp[c].coefficients = cTn;
    }
    lastHpf = hpfF; lastLpf = lpfF; lastTone = toneF;
}

void CryptvoxProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    dblM.init ((int) (sr * 0.1) + 8);
    dlyL.init ((int) (sr * 4.5));
    dlyR.init ((int) (sr * 4.5));
    for (auto& p : ps) p.init ((int) (sr * 0.12) + 8);
    reverb.setSampleRate (sr); reverb.reset();
    dryBuf.setSize (2, juce::jmax (samplesPerBlock, 1));
    revBuf.setSize (2, juce::jmax (samplesPerBlock, 1));

    svf.prepare ({ sr, (juce::uint32) juce::jmax (samplesPerBlock, 1), 2 });
    svf.reset();
    for (int c = 0; c < 2; ++c)
    {
        hp[c].reset(); lp[c].reset(); toneLp[c].reset();
        holdVal[c] = 0; holdCnt[c] = 0; dcx[c] = dcy[c] = 0;
    }
    e2 = lfo = lpl = lpr = ringPh = fPh = gPh = 0; gGate = 1.f; gDrop = false;
    updateFilters (*P[Px::hpf], *P[Px::lpf], *P[Px::dtone]);
}

float CryptvoxProcessor::distort (float x, int mode, float g, float comp) const
{
    float y = x * g;
    switch (mode)
    {
        case 0:  y = std::atan (y) * 0.6366f * 1.6f; break;                       // soft
        case 1:  y = juce::jlimit (-1.f, 1.f, y); break;                          // hard
        case 2:  y = 0.6366f * std::asin (std::sin (y)); break;                   // fold
        default: y = y >= 0 ? 1.f - std::exp (-y)                                // fuzz (asymetryczny)
                            : -0.7f * (1.f - std::exp (1.8f * y)); break;
    }
    return y * comp;
}

void CryptvoxProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2) return;

    if (dryBuf.getNumSamples() < n) { dryBuf.setSize (2, n); revBuf.setSize (2, n); }
    dryBuf.copyFrom (0, 0, buffer, 0, 0, n);
    dryBuf.copyFrom (1, 0, buffer, 1, 0, n);

    auto v = [this] (int i) { return P[i]->load(); };
    const float fsr = (float) sr;

    // ---- makro DESTROY ----
    const float dest = v (Px::destroy) * 0.01f;

    const float gin  = dB2g (v (Px::gain));
    const float outg = dB2g (v (Px::outg));
    const float mix  = v (Px::mix) * 0.01f;

    if (v (Px::hpf) != lastHpf || v (Px::lpf) != lastLpf || v (Px::dtone) != lastTone)
        updateFilters (v (Px::hpf), v (Px::lpf), v (Px::dtone));

    // pitch
    const float ptune = v (Px::ptune);
    const float pmixE = v (Px::pmix) * 0.01f;
    const bool doPitch = std::abs (ptune) > 0.01f && pmixE > 0.001f;
    const float pratio = std::pow (2.f, ptune / 12.f);
    const float pwin = 0.05f * fsr;

    // distortion
    const int dmode = (int) v (Px::dmode);
    const float driveE = juce::jmin (100.f, v (Px::drive) + dest * 60.f);
    const float dg = 1.f + driveE * 0.3f;
    const float dcomp = 1.f / std::pow (dg, 0.35f);
    const float dmixE = juce::jmin (1.f, v (Px::dmix) * 0.01f + dest);

    // crush
    const float cmixE = juce::jmin (1.f, v (Px::cmix) * 0.01f + dest * 0.6f);
    const float bitsE = juce::jmax (3.f, v (Px::bits) - dest * 8.f);
    const int downE = juce::jmax (1, (int) std::round (v (Px::down) + dest * 8.f));
    const float qlv = std::pow (2.f, bitsE - 1.f);

    // ring
    const float ringE = juce::jmin (1.f, v (Px::ringmix) * 0.01f + dest * 0.25f);
    const float rfreq = v (Px::rfreq);

    // tempo
    double bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm())
                if (*b > 20.0) bpm = *b;
    const float beatSamples = (float) (60.0 / bpm * sr);

    // gate
    static const float gdivs[4] = { 1.f, 0.5f, 0.25f, 0.125f };
    const float gperiod = juce::jmax (16.f, beatSamples * gdivs[juce::jlimit (0, 3, (int) v (Px::gdiv))]);
    const float gdE = juce::jmin (1.f, v (Px::gdepth) * 0.01f + dest * 0.3f);
    const float gChaos = juce::jmin (1.f, v (Px::gshape) * 0.01f + dest * 0.3f);
    const float gcoef = 1.f - std::exp (-1.f / (0.001f * fsr));

    // filter
    static const float frates[5] = { 4.f, 2.f, 1.f, 0.5f, 0.25f };
    const float fInc = (float) ((bpm / 60.0) / frates[juce::jlimit (0, 4, (int) v (Px::frate))] / sr);
    const float fmixE = v (Px::fmix) * 0.01f;
    const float fcut = v (Px::fcut);
    const float fsweep = v (Px::flfo) * 0.01f;
    {
        const int ft = (int) v (Px::fmode);
        svf.setType (ft == 0 ? juce::dsp::StateVariableTPTFilterType::lowpass
                   : ft == 1 ? juce::dsp::StateVariableTPTFilterType::bandpass
                             : juce::dsp::StateVariableTPTFilterType::highpass);
        svf.setResonance (0.7f + v (Px::freso) * 0.01f * 9.3f);
    }

    // doubler
    const float dblAmt = v (Px::dbl) * 0.01f * 0.6f;

    // delay
    static const float dbeats[5] = { 0.25f, 0.5f, 0.75f, 1.f, 2.f };
    float dtime = beatSamples * dbeats[juce::jlimit (0, 4, (int) v (Px::dldiv))];
    dtime = juce::jmin (dtime, (float) dlyL.b.size() - 4.f);
    const float dmixDl = v (Px::dlmix) * 0.01f;
    const float dfb = juce::jmin (0.98f, v (Px::dlfb) * 0.01f);
    const float ddirt = v (Px::dldirt) * 0.01f;
    const float ddg = 1.f + ddirt * 6.f;
    const float ddn = 1.f / std::atan (ddg);

    // reverb
    const float rmix = v (Px::rvmix) * 0.01f * 0.8f;
    juce::Reverb::Parameters rp;
    rp.roomSize = 0.3f + 0.7f * v (Px::rvsize) * 0.01f;
    rp.damping = v (Px::rvdamp) * 0.01f;
    rp.wetLevel = 0.33f; rp.dryLevel = 0.f; rp.width = 1.f; rp.freezeMode = 0.f;
    reverb.setParameters (rp);

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);
    float* rvL = revBuf.getWritePointer (0);
    float* rvR = revBuf.getWritePointer (1);

    for (int i = 0; i < n; ++i)
    {
        // generatory
        ringPh += rfreq / fsr; ringPh -= std::floor (ringPh);
        const float ringOsc = std::sin (6.2831853f * ringPh);
        fPh += fInc; fPh -= std::floor (fPh);
        if (fmixE > 0.001f && (i & 3) == 0)
        {
            const float cut = fcut * std::pow (2.f, fsweep * 3.f * std::sin (6.2831853f * fPh));
            svf.setCutoffFrequency (juce::jlimit (30.f, fsr * 0.45f, cut));
        }

        // glitch gate
        float gg = 1.f;
        if (gdE > 0.001f)
        {
            gPh += 1.f / gperiod;
            if (gPh >= 1.f) { gPh -= 1.f; gDrop = rng.nextFloat() < gChaos * 0.6f; }
            const float tgt = (gPh < 1.f - 0.5f * gdE && ! gDrop) ? 1.f : 1.f - gdE;
            gGate += gcoef * (tgt - gGate);
            gg = gGate;
        }

        float xs[2] = { L[i] * gin, R[i] * gin };

        for (int c = 0; c < 2; ++c)
        {
            float s = lp[c].processSample (hp[c].processSample (xs[c]));

            if (doPitch)
            {
                const float lay = ps[c].process (s, pratio, pwin);
                s = s * (1.f - pmixE) + lay * pmixE;
            }

            if (dmixE > 0.001f)
            {
                float y = distort (s, dmode, dg, dcomp);
                y = toneLp[c].processSample (y);
                const float yd = y - dcx[c] + 0.995f * dcy[c];
                dcx[c] = y; dcy[c] = yd;
                s = s * (1.f - dmixE) + yd * dmixE;
            }

            if (cmixE > 0.001f)
            {
                if (++holdCnt[c] >= downE)
                {
                    holdCnt[c] = 0;
                    holdVal[c] = std::round (s * qlv) / qlv;
                }
                s = s * (1.f - cmixE) + holdVal[c] * cmixE;
            }

            if (ringE > 0.001f)
                s = s * (1.f - ringE) + s * ringOsc * ringE;

            if (fmixE > 0.001f)
                s = s * (1.f - fmixE) + svf.processSample (c, s) * fmixE;

            xs[c] = s * gg;
        }

        float l = xs[0], r = xs[1];

        // doubler
        dblM.push ((l + r) * 0.5f);
        lfo += 6.2831853f * 0.35f / fsr;
        if (lfo > 6.2831853f) lfo -= 6.2831853f;
        if (dblAmt > 0.001f)
        {
            l += dblM.read ((0.014f + 0.0015f * std::sin (lfo)) * fsr) * dblAmt;
            r += dblM.read ((0.022f + 0.0015f * std::sin (lfo * 1.31f + 1.f)) * fsr) * dblAmt;
        }

        // ping-pong delay (brudny feedback + ducking)
        const float ml = (l + r) * 0.5f;
        e2 += 0.0005f * (juce::jmax (std::abs (l), std::abs (r)) - e2);
        const float duck = 1.f - 0.75f * juce::jmin (1.f, e2 * 4.f);
        const float dL = dlyL.read (dtime), dR = dlyR.read (dtime);
        float fbL = dL * dfb, fbR = dR * dfb;
        if (ddirt > 0.01f)
        {
            fbL = std::atan (fbL * ddg) * ddn;
            fbR = std::atan (fbR * ddg) * ddn;
        }
        lpl += 0.35f * (fbL - lpl);
        lpr += 0.35f * (fbR - lpr);
        dlyL.push (ml + lpr);
        dlyR.push (lpl);
        l += dL * dmixDl * duck;
        r += dR * dmixDl * duck;

        rvL[i] = rvR[i] = ml;
        L[i] = l; R[i] = r;
    }

    reverb.processStereo (rvL, rvR, n);

    const float* d0 = dryBuf.getReadPointer (0);
    const float* d1 = dryBuf.getReadPointer (1);
    float peak = 0.f;
    for (int i = 0; i < n; ++i)
    {
        const float wl = L[i] + rvL[i] * rmix;
        const float wr = R[i] + rvR[i] * rmix;
        L[i] = softLimit ((d0[i] * (1.f - mix) + wl * mix) * outg);
        R[i] = softLimit ((d1[i] * (1.f - mix) + wr * mix) * outg);
        peak = juce::jmax (peak, std::abs (L[i]), std::abs (R[i]));
    }
    level.store (juce::jmax (peak, level.load() * 0.85f));
}

juce::AudioProcessorEditor* CryptvoxProcessor::createEditor()
{
    return new CryptvoxEditor (*this);
}

void CryptvoxProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void CryptvoxProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            loadingState = true;
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            loadingState = false;
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CryptvoxProcessor();
}
