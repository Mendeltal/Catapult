#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

// ============================================================ DSP helpers
void ct::Biquad::lowpass (double sr, double f, double q)
{
    f = jlimit (20.0, sr * 0.45, f);
    const double w = 2.0 * MathConstants<double>::pi * f / sr, c = std::cos (w), s = std::sin (w), al = s / (2.0 * q);
    const double a0 = 1.0 + al;
    b0 = (float) (((1.0 - c) * 0.5) / a0); b1 = (float) ((1.0 - c) / a0); b2 = b0;
    a1 = (float) (-2.0 * c / a0); a2 = (float) ((1.0 - al) / a0);
}
void ct::Biquad::highpass (double sr, double f, double q)
{
    f = jlimit (10.0, sr * 0.45, f);
    const double w = 2.0 * MathConstants<double>::pi * f / sr, c = std::cos (w), s = std::sin (w), al = s / (2.0 * q);
    const double a0 = 1.0 + al;
    b0 = (float) (((1.0 + c) * 0.5) / a0); b1 = (float) (-(1.0 + c) / a0); b2 = b0;
    a1 = (float) (-2.0 * c / a0); a2 = (float) ((1.0 - al) / a0);
}

void ct::FDN::prepare (double s, float lengthScale)
{
    sampleRate = s; lenScale = lengthScale;
    static const int base[N] = { 1327, 1559, 1787, 2053, 2371, 2749, 3109, 3539 };
    for (int i = 0; i < N; ++i)
    {
        len[i] = (int) (base[i] * (s / 44100.0) * lenScale);
        line[i].assign ((size_t) len[i] + 64, 0.f);
        wpos[i] = 0; lp[i] = 0.f; phase[i] = (float) i / (float) N;
        rate[i] = (0.17f + 0.043f * (float) i) / (float) s;    // Hz / sr  -> cycles per sample
    }
    static const int apBase[4] = { 142, 107, 379, 277 };
    for (int i = 0; i < 4; ++i)
    {
        apLen[i] = jmax (8, (int) (apBase[i] * (s / 44100.0)));
        apBuf[i].assign ((size_t) apLen[i] * 2, 0.f); apPos[i] = 0;
    }
    preLen = (int) (0.020 * s);
    for (auto& p : pre) p.assign ((size_t) preLen + 1, 0.f);
    prePos = 0; frz = 0.f;
    setDecay (t60, false);
}

void ct::FDN::setDecay (float t, bool freeze)
{
    t60 = t;
    for (int i = 0; i < N; ++i)
        gain[i] = freeze ? 1.0f : std::pow (10.f, -3.f * ((float) len[i] / (float) sampleRate) / jmax (0.05f, t));
    frzTarget = freeze ? 1.f : 0.f;
}

void ct::FDN::tick (float inL, float inR, float& outL, float& outR)
{
    frz += (frzTarget - frz) * 0.0015f;
    const float inScale = 1.f - frz;
    // pre-delay
    pre[0][(size_t) prePos] = inL; pre[1][(size_t) prePos] = inR;
    const int rp = (prePos + 1) % (preLen + 1);
    float x[2] = { pre[0][(size_t) rp] * inScale, pre[1][(size_t) rp] * inScale };
    prePos = (prePos + 1) % (preLen + 1);
    // input diffusion
    for (int a = 0; a < 4; ++a)
        for (int c = 0; c < 2; ++c)
        {
            float& slot = apBuf[a][(size_t) (c * apLen[a] + apPos[a])];
            const float d = slot, y = -0.6f * x[c] + d;
            slot = x[c] + 0.6f * y; x[c] = y;
        }
    for (int a = 0; a < 4; ++a) apPos[a] = (apPos[a] + 1) % apLen[a];

    float v[N];
    const float dampEff = damp * (1.f - frz);
    for (int i = 0; i < N; ++i)
    {
        phase[i] += rate[i]; if (phase[i] >= 1.f) phase[i] -= 1.f;
        const float d = (float) len[i] - 3.f + 3.f * std::sin (6.2831853f * phase[i]);
        float rp2 = (float) wpos[i] - d; const int n = (int) line[i].size();
        while (rp2 < 0) rp2 += (float) n;
        const int i0 = (int) rp2; const float fr = rp2 - (float) i0;
        const float s0 = line[i][(size_t) (i0 % n)], s1 = line[i][(size_t) ((i0 + 1) % n)];
        float y = s0 + (s1 - s0) * fr;
        lp[i] += (y - lp[i]) * (1.f - dampEff); y = lp[i];
        v[i] = y;
    }
    // output taps (before mixing)
    outL = 0.32f * (v[0] - v[2] + v[4] - v[6] + v[1]);
    outR = 0.32f * (v[1] - v[3] + v[5] - v[7] + v[0]);
    // Hadamard mix (fast Walsh-Hadamard, normalised)
    float m[N]; for (int i = 0; i < N; ++i) m[i] = v[i];
    for (int h = 1; h < N; h <<= 1)
        for (int i = 0; i < N; i += h << 1)
            for (int j = i; j < i + h; ++j) { const float a = m[j], b = m[j + h]; m[j] = a + b; m[j + h] = a - b; }
    const float norm = 0.35355339f;
    for (int i = 0; i < N; ++i)
    {
        const float in = x[i & 1] * 0.35f;
        float w = in + gain[i] * m[i] * norm;
        if (std::abs (w) < 1.0e-20f) w = 0.f;
        line[i][(size_t) wpos[i]] = w;
        wpos[i] = (wpos[i] + 1) % (int) line[i].size();
    }
}

void ct::PitchShifter::prepare (double)
{
    buf.assign (8192, 0.f); mask = 8191; wp = 0; ph = 0.f;
}
float ct::PitchShifter::process (float x)
{
    buf[(size_t) wp] = x;
    float out = 0.f;
    for (int h = 0; h < 2; ++h)
    {
        float p = ph + 0.5f * (float) h; if (p >= 1.f) p -= 1.f;
        const float d = p * (float) win + 2.f;
        const float rp = (float) wp - d;
        const int i0 = (int) std::floor (rp); const float fr = rp - (float) i0;
        const float a = buf[(size_t) (i0 & mask)], b = buf[(size_t) ((i0 + 1) & mask)];
        const float s = std::sin (3.14159265f * p);
        out += (a + (b - a) * fr) * s * s;
    }
    ph += (1.f - ratio) / (float) win;
    ph -= std::floor (ph);
    wp = (wp + 1) & mask;
    return out;
}

// ============================================================ Processor
AudioProcessorValueTreeState::ParameterLayout CatapultProcessor::createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout l;
    auto addBool = [&] (const char* id, const char* name, bool def)
        { l.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def)); };
    auto addF = [&] (const char* id, const char* name, NormalisableRange<float> r, float def, const char* unit = "")
        { l.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def,
                 AudioParameterFloatAttributes().withLabel (unit))); };
    addBool ("throw", "Throw", false);
    addBool ("freeze", "Freeze", false);
    addBool ("latch", "Latch", false);
    addF ("catch",   "Catch",   { 0.f, 1500.f, 1.f, 0.6f }, 0.f, "ms");
    addF ("attack",  "Attack",  { 0.f, 800.f, 1.f, 0.4f }, 4.f, "ms");
    addF ("release", "Release", { 10.f, 6000.f, 1.f, 0.4f }, 250.f, "ms");
    addF ("duck",    "Duck",    { 0.f, 1.f }, 0.35f);
    addF ("echoLvl", "Echo Level", { 0.f, 1.f }, 0.6f);
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "echoDiv", 1 }, "Echo Div",
           StringArray { "1/16", "1/8", "1/8.", "1/4", "1/4.", "1/2" }, 2));
    addF ("echoFb",  "Echo Feedback", { 0.f, 0.92f }, 0.45f);
    addF ("hallLvl", "Hall Level", { 0.f, 1.f }, 0.7f);
    addF ("decay",   "Hall Decay", { 0.4f, 14.f, 1.f, 0.45f }, 3.5f, "s");
    addF ("shimLvl", "Shimmer Level", { 0.f, 1.f }, 0.3f);
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "shimPitch", 1 }, "Shimmer Pitch",
           StringArray { "-12", "+5", "+7", "+12", "+19", "+24" }, 3));
    addF ("tone",    "Tone", { 1500.f, 20000.f, 1.f, 0.35f }, 9000.f, "Hz");
    addF ("lowcut",  "Low Cut", { 20.f, 1000.f, 1.f, 0.4f }, 150.f, "Hz");
    addF ("width",   "Width", { 0.f, 1.5f }, 1.1f);
    addF ("ret",     "Return", { -24.f, 6.f }, -3.f, "dB");
    return l;
}

CatapultProcessor::CatapultProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    auto g = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    pThrow = g ("throw"); pFreeze = g ("freeze"); pCatch = g ("catch"); pAttack = g ("attack"); pRelease = g ("release");
    pDuck = g ("duck"); pEchoLvl = g ("echoLvl"); pEchoDiv = g ("echoDiv"); pEchoFb = g ("echoFb");
    pHallLvl = g ("hallLvl"); pDecay = g ("decay"); pShimLvl = g ("shimLvl"); pShimPitch = g ("shimPitch");
    pTone = g ("tone"); pLow = g ("lowcut"); pWidth = g ("width"); pRet = g ("ret");
}

bool CatapultProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainInputChannelSet() == AudioChannelSet::stereo() && l.getMainOutputChannelSet() == AudioChannelSet::stereo();
}

void CatapultProcessor::prepareToPlay (double s, int)
{
    sr = s;
    auto pow2 = [] (double n) { int p = 1; while (p < n) p <<= 1; return p; };
    const int hs = pow2 (s * 3.0); hMask = hs - 1;
    const int es = pow2 (s * 8.0); eMask = es - 1;
    for (int c = 0; c < 2; ++c) { hist[c].assign ((size_t) hs, 0.f); eBuf[c].assign ((size_t) es, 0.f); }
    hw = ew = 0; retroActive = false; prevThrow = false; sendEnv = 0.f; duckEnv = 0.f; duckGain = 1.f;
    hall.prepare (s, 1.35f); shim.prepare (s, 0.9f);
    for (auto& p : ps) p.prepare (s);
    shimFbL = shimFbR = 0.f;
    for (int c = 0; c < 2; ++c)
    {
        eLp[c].lowpass (s, 5200.0); eHp[c].highpass (s, 170.0); eLp[c].reset(); eHp[c].reset();
        for (int k = 0; k < 2; ++k) { lp[c][k].reset(); hp[c][k].reset(); }
    }
    lastTone = lastLow = lastPitch = lastDecay = -1.f; lastFreeze = false;
    retSm.reset (s, 0.03); widthSm.reset (s, 0.03); echoSm.reset (s, 0.03); hallSm.reset (s, 0.03); shimSm.reset (s, 0.03);
    retSm.setCurrentAndTargetValue (Decibels::decibelsToGain (pRet->load()));
    widthSm.setCurrentAndTargetValue (pWidth->load());
    echoSm.setCurrentAndTargetValue (pEchoLvl->load()); hallSm.setCurrentAndTargetValue (pHallLvl->load()); shimSm.setCurrentAndTargetValue (pShimLvl->load());
    eDelay = -1.f;
}

void CatapultProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals nd;
    const int n = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2) return;
    float* L = buffer.getWritePointer (0); float* R = buffer.getWritePointer (1);

    // host tempo
    double bpm = 120.0;
    if (auto* ph = getPlayHead()) if (auto pos = ph->getPosition()) if (auto b = pos->getBpm()) bpm = jlimit (30.0, 300.0, *b);

    // block-rate parameter updates
    const bool thr = pThrow->load() > 0.5f, frz = pFreeze->load() > 0.5f;
    if (thr && ! prevThrow)
    {
        const int cl = (int) (pCatch->load() * 0.001 * sr);
        if (cl > 64) { retroLen = jmin (cl, hMask - 64); retroStart = (hw - retroLen) & hMask; retroPos = 0; retroActive = true; }
    }
    prevThrow = thr;
    throwOn.store (thr || sendEnv > 0.01f || retroActive);

    const float tone = pTone->load(), low = pLow->load(), decay = pDecay->load();
    if (tone != lastTone) { for (int c = 0; c < 2; ++c) for (int k = 0; k < 2; ++k) lp[c][k].lowpass (sr, tone, k == 0 ? 0.5412 : 1.3066); lastTone = tone; }
    if (low != lastLow) { for (int c = 0; c < 2; ++c) for (int k = 0; k < 2; ++k) hp[c][k].highpass (sr, low, 0.7071); lastLow = low; }
    if (decay != lastDecay || frz != lastFreeze) { hall.setDecay (decay, frz); shim.setDecay (jmax (2.5f, decay * 0.8f), frz); lastDecay = decay; lastFreeze = frz; }
    hall.setDamping (jmap (jlimit (0.f, 1.f, (20000.f - tone) / 18500.f), 0.12f, 0.55f));
    shim.setDamping (0.42f);
    static const float pitches[] = { -12.f, 5.f, 7.f, 12.f, 19.f, 24.f };
    const int pi = jlimit (0, 5, (int) pShimPitch->load());
    if (pitches[pi] != lastPitch) { ps[0].setSemitones (pitches[pi]); ps[1].setSemitones (pitches[pi]); lastPitch = pitches[pi]; }

    static const float beats[] = { 0.25f, 0.5f, 0.75f, 1.f, 1.5f, 2.f };
    const float targetDelay = jlimit (64.f, (float) eMask - 16.f, (float) (beats[jlimit (0, 5, (int) pEchoDiv->load())] * 60.0 / bpm * sr));
    if (eDelay < 0.f) eDelay = targetDelay;
    const float fb = pEchoFb->load();
    retSm.setTargetValue (Decibels::decibelsToGain (pRet->load())); widthSm.setTargetValue (pWidth->load());
    echoSm.setTargetValue (pEchoLvl->load()); hallSm.setTargetValue (pHallLvl->load()); shimSm.setTargetValue (pShimLvl->load());

    const float attStep = 1.f / jmax (1.f, pAttack->load() * 0.001f * (float) sr);
    const float relStep = 1.f / jmax (1.f, pRelease->load() * 0.001f * (float) sr);
    const float duckAmt = pDuck->load();
    const float duckA = 1.f - std::exp (-1.f / (0.003f * (float) sr)), duckR = 1.f - std::exp (-1.f / (0.16f * (float) sr));
    const int fadeN = (int) (0.006 * sr);
    float wetPeak = 0.f, inPeak = 0.f;

    for (int i = 0; i < n; ++i)
    {
        const float dl = L[i], dr = R[i];
        inPeak = jmax (inPeak, std::abs (dl), std::abs (dr));
        hist[0][(size_t) hw] = dl; hist[1][(size_t) hw] = dr; hw = (hw + 1) & hMask;

        // send gate
        sendEnv = thr ? jmin (1.f, sendEnv + attStep) : jmax (0.f, sendEnv - relStep);
        float sL = dl * sendEnv, sR = dr * sendEnv;
        if (retroActive)
        {
            const int idx = (retroStart + retroPos) & hMask;
            const float f = (float) jmin (jmin (retroPos, retroLen - retroPos), fadeN) / (float) fadeN;
            sL += hist[0][(size_t) idx] * f; sR += hist[1][(size_t) idx] * f;
            if (++retroPos >= retroLen) retroActive = false;
        }

        // echo (ping-pong)
        eDelay += (targetDelay - eDelay) * 0.0004f;
        float rpos = (float) ew - eDelay; const int e0 = (int) std::floor (rpos); const float ef = rpos - (float) e0;
        auto rd = [&] (const std::vector<float>& b) { const float a = b[(size_t) (e0 & eMask)], c = b[(size_t) ((e0 + 1) & eMask)]; return a + (c - a) * ef; };
        const float eoL = rd (eBuf[0]), eoR = rd (eBuf[1]);
        const float fL = std::tanh (eLp[0].process (eHp[0].process (eoL))), fR = std::tanh (eLp[1].process (eHp[1].process (eoR)));
        eBuf[0][(size_t) ew] = (sL + sR) * 0.5f + fb * fR;
        eBuf[1][(size_t) ew] = fb * fL;
        ew = (ew + 1) & eMask;

        // reverbs
        float hoL, hoR, soL, soR;
        hall.tick (sL + eoL * 0.45f, sR + eoR * 0.45f, hoL, hoR);
        shim.tick (sL * 0.8f + shimFbL, sR * 0.8f + shimFbR, soL, soR);
        shimFbL = std::tanh (ps[0].process (soL) * 0.55f); shimFbR = std::tanh (ps[1].process (soR) * 0.55f);

        float wl = echoSm.getNextValue() * eoL + hallSm.getNextValue() * hoL * 1.4f + shimSm.getNextValue() * soL * 1.6f;
        float wr = echoSm.getCurrentValue() * eoR + hallSm.getCurrentValue() * hoR * 1.4f + shimSm.getCurrentValue() * soR * 1.6f;
        for (int k = 0; k < 2; ++k) { wl = hp[0][k].process (wl); wr = hp[1][k].process (wr); }
        for (int k = 0; k < 2; ++k) { wl = lp[0][k].process (wl); wr = lp[1][k].process (wr); }
        const float w = widthSm.getNextValue(), m = (wl + wr) * 0.5f, s = (wl - wr) * 0.5f * w;
        wl = m + s; wr = m - s;

        // ducking under the dry signal
        const float lv = (std::abs (dl) + std::abs (dr)) * 0.5f;
        duckEnv += (lv - duckEnv) * (lv > duckEnv ? duckA : duckR);
        const float tg = 1.f - duckAmt * jlimit (0.f, 1.f, duckEnv * 4.f);
        duckGain += (tg - duckGain) * 0.01f;
        const float rg = retSm.getNextValue() * duckGain;
        wl *= rg; wr *= rg;
        wetPeak = jmax (wetPeak, std::abs (wl), std::abs (wr));
        L[i] = dl + wl; R[i] = dr + wr;
    }
    wetLevel.store (wetPeak); sendLevel.store (sendEnv); inLevel.store (inPeak);
}

void CatapultProcessor::getStateInformation (MemoryBlock& d)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, d);
}
void CatapultProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size)) if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (ValueTree::fromXml (*xml));
}

AudioProcessorEditor* CatapultProcessor::createEditor() { return new CatapultEditor (*this); }
AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new CatapultProcessor(); }
