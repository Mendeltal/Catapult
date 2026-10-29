#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"
#include "Layout.h"

namespace ctui
{
    juce::Image loadImage (const juce::String& resourceName);

    class Knob : public juce::Slider
    {
    public:
        Knob (juce::AudioProcessorValueTreeState& a, const juce::String& id);
        void paint (juce::Graphics&) override;
    };

    // Momentary/toggle button bound to a bool parameter. Honors the Latch parameter if given.
    class ParamButton : public juce::Component, private juce::Timer
    {
    public:
        enum class Kind { Pad, Key };
        ParamButton (juce::AudioProcessorValueTreeState& a, const juce::String& id, const juce::String& asset,
                     Kind k, bool alwaysToggle, juce::RangedAudioParameter* latch);
        ~ParamButton() override { stopTimer(); }
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        bool isOn() const { return param->getValue() > 0.5f; }
        std::function<float()> glow;
    private:
        void timerCallback() override { repaint(); }
        void set (bool on) { param->setValueNotifyingHost (on ? 1.f : 0.f); }
        juce::RangedAudioParameter* param; juce::RangedAudioParameter* latchParam;
        juce::Image up, down; Kind kind; bool toggle;
    };

    class Chip : public juce::Component
    {
    public:
        Chip (const juce::String& asset) { up = loadImage (asset + "_up"); down = loadImage (asset + "_down"); setMouseCursor (juce::MouseCursor::PointingHandCursor); }
        void paint (juce::Graphics& g) override { g.drawImage (lit ? down : up, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit); }
        void mouseDown (const juce::MouseEvent&) override { if (onClick) onClick(); }
        std::function<void()> onClick;
        void setLit (bool b) { if (lit != b) { lit = b; repaint(); } }
    private:
        juce::Image up, down; bool lit = false;
    };
}

class CatapultEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit CatapultEditor (CatapultProcessor&);
    ~CatapultEditor() override;
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override;
    void applyPreset (int idx);
    void drawDisplay (juce::Graphics&);

    CatapultProcessor& proc;
    juce::Image faceplate;
    juce::OwnedArray<ctui::Knob> knobs;
    juce::OwnedArray<ctui::Chip> chips;
    std::unique_ptr<ctui::ParamButton> pad;
    juce::OwnedArray<ctui::ParamButton> keys;
    std::vector<std::unique_ptr<juce::SliderParameterAttachment>> attach;
    juce::Font sigFont;
    static constexpr int N = 216;
    std::array<float, N> wetHist {}, sendHist {};
    float wetShown = 0.f, retShown = 0.f; int activeChip = -1; float freezeFade = 0.f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CatapultEditor)
};
