#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <vector>

namespace ct
{
    struct Biquad
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        void lowpass (double sr, double f, double q = 0.7071);
        void highpass (double sr, double f, double q = 0.7071);
        inline float process (float x) noexcept { float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
        void reset() { z1 = z2 = 0; }
    };

    // 8-line feedback delay network with Hadamard mixing, per-line damping and slow modulation.
    class FDN
    {
    public:
        void prepare (double sr, float lengthScale);
        void setDecay (float t60Seconds, bool freeze);
        void setDamping (float coef) { damp = coef; }   // 0..1, higher = darker
        void tick (float inL, float inR, float& outL, float& outR);
    private:
        static constexpr int N = 8;
        std::array<std::vector<float>, N> line;
        std::array<int, N> len {}, wpos {};
        std::array<float, N> lp {}, gain {};
        std::array<float, N> phase {}, rate {};
        float damp = 0.3f, frz = 0.f;
        double sampleRate = 48000;
        float lenScale = 1.f;
        float diffZ[4][2] {};  // input allpass states
        std::array<std::vector<float>, 4> apBuf;
        std::array<int, 4> apLen {}, apPos {};
        std::vector<float> pre[2]; int preLen = 0, prePos = 0;
        float t60 = 3.f, frzTarget = 0.f;
    };

    // Two-head crossfading delay-line pitch shifter (good enough inside a reverb loop).
    class PitchShifter
    {
    public:
        void prepare (double sr);
        void setSemitones (float st) { ratio = std::pow (2.f, st / 12.f); }
        float process (float x);
    private:
        std::vector<float> buf; int mask = 0, wp = 0; float ph = 0.f, ratio = 2.f; int win = 2048;
    };
}

class CatapultProcessor : public juce::AudioProcessor
{
public:
    CatapultProcessor();
    ~CatapultProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Catapult"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 14.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    std::atomic<float> wetLevel { 0.f }, sendLevel { 0.f }, inLevel { 0.f };
    std::atomic<bool> throwOn { false };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    double sr = 48000;
    // dry history for retro catch
    std::vector<float> hist[2]; int hMask = 0, hw = 0;
    int retroStart = 0, retroPos = 0, retroLen = 0; bool retroActive = false;
    bool prevThrow = false;
    float sendEnv = 0.f, duckEnv = 0.f, duckGain = 1.f;

    // echo
    std::vector<float> eBuf[2]; int eMask = 0, ew = 0; float eDelay = 20000.f;
    ct::Biquad eLp[2], eHp[2];
    // reverbs
    ct::FDN hall, shim; ct::PitchShifter ps[2]; float shimFbL = 0, shimFbR = 0;
    // output shaping
    ct::Biquad lp[2][2], hp[2][2];
    float lastTone = -1.f, lastLow = -1.f, lastPitch = -100.f, lastDecay = -1.f; bool lastFreeze = false;
    juce::SmoothedValue<float> retSm, widthSm, echoSm, hallSm, shimSm;

    std::atomic<float> *pThrow, *pFreeze, *pCatch, *pAttack, *pRelease, *pDuck, *pEchoLvl, *pEchoDiv, *pEchoFb,
        *pHallLvl, *pDecay, *pShimLvl, *pShimPitch, *pTone, *pLow, *pWidth, *pRet;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CatapultProcessor)
};
