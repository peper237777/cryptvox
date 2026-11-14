#include "PluginEditor.h"

namespace
{
const juce::Colour cRed    (0xffe8243c);
const juce::Colour cViolet (0xff9b6bff);
const juce::Colour cCyan   (0xff2fd6c8);
const juce::Colour cAmber  (0xffffb02e);
const juce::Colour cSteel  (0xff8fa0b8);
const juce::Colour cRose   (0xffff5d8f);

float weightOf (int id) { return id == Px::destroy ? 2.f : 1.f; }
}

CryptvoxEditor::CryptvoxEditor (CryptvoxProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    setSize (1040, 680);

    auto add = [this] (const char* t, juce::Colour c, std::vector<int> ids)
    {
        cards.push_back ({ t, c, std::move (ids), {} });
        return (int) cards.size() - 1;
    };

    using namespace Px;
    rows.push_back ({ add ("INPUT",      cSteel,  { gain, hpf, lpf }),
                      add ("PITCH",      cViolet, { ptune, pmix }),
                      add ("DISTORTION", cRed,    { dmode, drive, dtone, dmix }),
                      add ("BITCRUSH",   cAmber,  { bits, down, cmix }),
                      add ("RING MOD",   cCyan,   { rfreq, ringmix }) });
    rows.push_back ({ add ("GLITCH GATE", cAmber, { gdiv, gdepth, gshape }),
                      add ("FILTER",      cCyan,  { fmode, frate, fcut, freso, flfo, fmix }) });
    rows.push_back ({ add ("DELAY",       cViolet, { dldiv, dlmix, dlfb, dldirt }),
                      add ("SPACE",       cViolet, { rvmix, rvsize, rvdamp, dbl }),
                      add ("MASTER",      cRose,   { destroy, outg, mix }) });

    ctls.resize ((size_t) NP);
    for (auto& card : cards)
        for (int id : card.ids)
        {
            const Spec& sp = specs[id];
            auto c = std::make_unique<Ctl>();

            c->label = std::make_unique<juce::Label>();
            c->label->setText (sp.label, juce::dontSendNotification);
            c->label->setJustificationType (juce::Justification::centred);
            c->label->setFont (11.5f);
            c->label->setColour (juce::Label::textColourId, juce::Colour (0xff9a9ab0));
            addAndMakeVisible (*c->label);

            if (sp.choices != nullptr)
            {
                c->combo = std::make_unique<juce::ComboBox>();
                c->combo->addItemList (juce::StringArray::fromTokens (sp.choices, "|", ""), 1);
                addAndMakeVisible (*c->combo);
                c->ca = std::make_unique<CA> (proc.apvts, sp.id, *c->combo);
            }
            else
            {
                c->knob = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag,
                                                          juce::Slider::TextBoxBelow);
                auto& s = *c->knob;
                s.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f,
                                       juce::MathConstants<float>::pi * 2.8f, true);
                s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 74, 16);
                s.setColour (juce::Slider::rotarySliderFillColourId, id == destroy ? cRed.brighter (0.2f) : card.col);
                s.setColour (juce::Slider::textBoxTextColourId, juce::Colours::white.withAlpha (0.85f));
                s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
                s.setNumDecimalPlacesToDisplay (sp.step < 1.f ? 1 : 0);
                s.setTextValueSuffix (sp.unit != nullptr ? sp.unit : "");
                s.setDoubleClickReturnValue (true, sp.def);
                addAndMakeVisible (s);
                c->sa = std::make_unique<SA> (proc.apvts, sp.id, s);
            }
            ctls[(size_t) id] = std::move (c);
        }

    presetBox.addItemList (juce::StringArray::fromTokens (presetNames, "|", ""), 1);
    addAndMakeVisible (presetBox);
    presetAtt = std::make_unique<CA> (proc.apvts, "preset", presetBox);
    addAndMakeVisible (prevBtn);
    addAndMakeVisible (nextBtn);
    prevBtn.onClick = [this] { stepPreset (-1); };
    nextBtn.onClick = [this] { stepPreset (1); };

    startTimerHz (30);
}

CryptvoxEditor::~CryptvoxEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void CryptvoxEditor::stepPreset (int delta)
{
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter ("preset")))
    {
        const int cnt = p->choices.size();
        const int idx = (p->getIndex() + delta + cnt) % cnt;
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 ((float) idx));
        p->endChangeGesture();
    }
}

void CryptvoxEditor::timerCallback()
{
    lvl = juce::jmax (proc.level.load(), lvl * 0.9f);
    ++frame;
    repaint (orbArea.expanded (30));
}

void CryptvoxEditor::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (84);
    const int cx = getWidth() / 2;
    presetBox.setBounds (cx - 150, 26, 300, 32);
    prevBtn.setBounds (cx - 198, 26, 40, 32);
    nextBtn.setBounds (cx + 158, 26, 40, 32);
    orbArea = { getWidth() - 120, 6, 72, 72 };

    r.reduce (12, 4);
    const int gap = 8;
    const int rowH = (r.getHeight() - 2 * gap) / 3;

    for (auto& row : rows)
    {
        auto rr = r.removeFromTop (rowH);
        r.removeFromTop (gap);

        float totalW = 0.f;
        for (int ci : row) for (int id : cards[(size_t) ci].ids) totalW += weightOf (id);
        const float unit = (float) (rr.getWidth() - gap * ((int) row.size() - 1)) / totalW;

        int x = rr.getX();
        for (int ci : row)
        {
            auto& card = cards[(size_t) ci];
            float cw = 0.f;
            for (int id : card.ids) cw += weightOf (id);
            const int cardW = (int) std::round (unit * cw);
            card.area = { x, rr.getY(), cardW, rr.getHeight() };
            x += cardW + gap;

            auto inner = card.area.withTrimmedTop (28).reduced (4, 0);
            float px = (float) inner.getX();
            for (int id : card.ids)
            {
                const float w = (float) inner.getWidth() * weightOf (id) / cw;
                juce::Rectangle<int> slot ((int) px, inner.getY(), (int) w, inner.getHeight());
                px += w;
                auto& c = *ctls[(size_t) id];
                c.label->setBounds (slot.removeFromTop (16));
                if (c.combo != nullptr)
                    c.combo->setBounds (slot.withSizeKeepingCentre (juce::jmax (40, slot.getWidth() - 8), 28));
                else
                    c.knob->setBounds (slot);
            }
        }
    }
}

void CryptvoxEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0b0b11), 0.f, 0.f,
                                             juce::Colour (0xff040407), 0.f, (float) getHeight(), false));
    g.fillAll();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0x33e8243c), (float) getWidth() * 0.5f, -40.f,
                                             juce::Colours::transparentBlack, (float) getWidth() * 0.5f, 220.f, true));
    g.fillRect (0, 0, getWidth(), 240);

    // logo
    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (juce::FontOptions (32.f)).boldened());
    g.drawText ("CRYPTVOX", 24, 14, 300, 40, juce::Justification::left);
    g.setColour (juce::Colour (0xff7a7a90));
    g.setFont (11.5f);
    g.drawText ("VOCAL DESTRUCTION SYSTEM", 26, 50, 300, 16, juce::Justification::left);

    // karty
    for (auto& c : cards)
    {
        auto a = c.area.toFloat();
        g.setColour (juce::Colour (0xff101017));
        g.fillRoundedRectangle (a, 10.f);
        g.setColour (juce::Colour (0xff1f1f2a));
        g.drawRoundedRectangle (a.reduced (0.5f), 10.f, 1.f);
        g.setColour (c.col);
        g.fillRoundedRectangle (a.getX() + 12.f, a.getY() + 11.f, 3.f, 12.f, 1.5f);
        g.setFont (12.5f);
        g.drawText (c.title, (int) a.getX() + 22, (int) a.getY() + 8, (int) a.getWidth() - 30, 18,
                    juce::Justification::left);
    }

    // orb - reaguje na poziom i DESTROY
    const float dest = proc.apvts.getRawParameterValue ("destroy")->load() * 0.01f;
    const auto c = orbArea.getCentre().toFloat();
    const float base = 22.f;
    g.setColour (cRed.withAlpha (0.10f + 0.35f * lvl));
    g.fillEllipse (c.x - base - 10.f, c.y - base - 10.f, (base + 10.f) * 2.f, (base + 10.f) * 2.f);
    g.setColour (juce::Colour (0xff0b0b11));
    g.fillEllipse (c.x - base, c.y - base, base * 2.f, base * 2.f);

    juce::Path ring;
    const int N = 72;
    for (int k = 0; k <= N; ++k)
    {
        const float a = (float) k / (float) N * juce::MathConstants<float>::twoPi;
        const float noise = std::sin ((float) k * 12.9898f + (float) frame * 0.37f) * 0.5f + 0.5f;
        const float rr = base + 1.f + lvl * 8.f + dest * 7.f * noise * (0.3f + lvl);
        const float px = c.x + std::cos (a) * rr, py = c.y + std::sin (a) * rr;
        if (k == 0) ring.startNewSubPath (px, py); else ring.lineTo (px, py);
    }
    ring.closeSubPath();
    g.setColour (juce::Colour::fromHSV (0.98f - 0.08f * dest, 0.8f, 1.f, 1.f));
    g.strokePath (ring, juce::PathStrokeType (1.6f));
}
