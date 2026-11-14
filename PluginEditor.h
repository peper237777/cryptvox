#pragma once
#include "PluginProcessor.h"

class CryptLnF : public juce::LookAndFeel_V4
{
public:
    CryptLnF()
    {
        using juce::Colour;
        setColour (juce::ComboBox::backgroundColourId, Colour (0xff16161e));
        setColour (juce::ComboBox::textColourId, Colour (0xffe8e8f0));
        setColour (juce::ComboBox::outlineColourId, Colour (0xff2a2a36));
        setColour (juce::ComboBox::arrowColourId, Colour (0xff9a9ab0));
        setColour (juce::PopupMenu::backgroundColourId, Colour (0xff12121a));
        setColour (juce::PopupMenu::textColourId, Colour (0xffe8e8f0));
        setColour (juce::PopupMenu::highlightedBackgroundColourId, Colour (0xff3a1520));
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour (juce::TextButton::buttonColourId, Colour (0xff16161e));
        setColour (juce::TextButton::textColourOffId, Colour (0xffe8e8f0));
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float a0, float a1, juce::Slider& s) override
    {
        auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (5.f);
        const float d = juce::jmin (b.getWidth(), b.getHeight());
        const auto c = b.getCentre();
        const float r = d * 0.5f;
        const auto col = s.findColour (juce::Slider::rotarySliderFillColourId);
        const float ang = a0 + pos * (a1 - a0);

        g.setColour (col.withAlpha (0.07f));
        g.fillEllipse (c.x - r, c.y - r, d, d);

        juce::Path track;
        track.addCentredArc (c.x, c.y, r - 3.f, r - 3.f, 0.f, a0, a1, true);
        g.setColour (juce::Colour (0xff262631));
        g.strokePath (track, juce::PathStrokeType (4.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path val;
        val.addCentredArc (c.x, c.y, r - 3.f, r - 3.f, 0.f, a0, ang, true);
        g.setColour (col);
        g.strokePath (val, juce::PathStrokeType (4.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float rb = r - 10.f;
        g.setColour (juce::Colour (0xff0c0c11));
        g.fillEllipse (c.x - rb, c.y - rb, rb * 2.f, rb * 2.f);
        g.setColour (juce::Colour (0xff23232d));
        g.drawEllipse (c.x - rb, c.y - rb, rb * 2.f, rb * 2.f, 1.f);

        juce::Path p;
        p.addRoundedRectangle (-1.5f, -rb + 2.f, 3.f, rb * 0.45f, 1.5f);
        g.setColour (juce::Colours::white.withAlpha (0.92f));
        g.fillPath (p, juce::AffineTransform::rotation (ang).translated (c.x, c.y));
    }
};

class CryptvoxEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit CryptvoxEditor (CryptvoxProcessor&);
    ~CryptvoxEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void stepPreset (int delta);

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    struct Ctl
    {
        std::unique_ptr<juce::Slider> knob;
        std::unique_ptr<juce::ComboBox> combo;
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<SA> sa;
        std::unique_ptr<CA> ca;
    };
    struct Card { juce::String title; juce::Colour col; std::vector<int> ids; juce::Rectangle<int> area; };

    CryptvoxProcessor& proc;
    CryptLnF lnf;
    std::vector<std::unique_ptr<Ctl>> ctls;
    std::vector<Card> cards;
    std::vector<std::vector<int>> rows;
    juce::ComboBox presetBox;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" };
    std::unique_ptr<CA> presetAtt;
    juce::Rectangle<int> orbArea;
    float lvl = 0.f;
    int frame = 0;
};
