#include "PluginEditor.h"
#include "BinaryData.h"

using namespace juce;
static const Colour copper (0xffd68646), teal (0xff62e6d8), amber (0xffffb45a);

Image ctui::loadImage (const String& n)
{
    int size = 0;
    auto* d = BinaryData::getNamedResource ((n.replaceCharacter ('.', '_') + "_png").toRawUTF8(), size);
    if (d == nullptr) { jassertfalse; return {}; }
    return ImageCache::getFromMemory (d, size);
}

// ---------------------------------------------------------------- Knob
ctui::Knob::Knob (AudioProcessorValueTreeState& a, const String& id)
{
    setSliderStyle (Slider::RotaryVerticalDrag);
    setTextBoxStyle (Slider::NoTextBox, true, 0, 0);
    setRotaryParameters (degreesToRadians (225.f), degreesToRadians (495.f), true);
    setMouseDragSensitivity (180);
    auto* p = a.getParameter (id);
    setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    textFromValueFunction = [p] (double v) { return p->getText (p->convertTo0to1 ((float) v), 12) + " " + p->getLabel(); };
}

void ctui::Knob::paint (Graphics& g)
{
    static Image body = loadImage ("knob");
    auto b = getLocalBounds().toFloat();
    g.drawImage (body, b, RectanglePlacement::stretchToFit);
    const auto c = b.getCentre();
    const float pos = (float) valueToProportionOfLength (getValue());
    const float a = degreesToRadians (-135.f + 270.f * pos), r = (float) layout::knobD * 0.5f;
    // illuminated arc
    const bool hot = isMouseOverOrDragging();
    Path arc; arc.addCentredArc (c.x, c.y, r + 9.5f, r + 9.5f, 0.f, degreesToRadians (-135.f), a, true);
    g.setColour (copper.withAlpha (hot ? 0.28f : 0.16f)); g.strokePath (arc, PathStrokeType (5.f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (copper.withAlpha (hot ? 1.f : 0.9f));    g.strokePath (arc, PathStrokeType (1.7f, PathStrokeType::curved, PathStrokeType::rounded));
    // pointer on the aluminium cap
    Path p; p.addRoundedRectangle (-1.2f, -r * 0.60f, 2.4f, r * 0.40f, 1.1f);
    g.setColour (Colour (0xff1d1d20));
    g.fillPath (p, AffineTransform::rotation (a).translated (c.x, c.y));
}

// ---------------------------------------------------------------- ParamButton
ctui::ParamButton::ParamButton (AudioProcessorValueTreeState& a, const String& id, const String& asset, Kind k, bool alwaysToggle, RangedAudioParameter* latch)
    : param (a.getParameter (id)), latchParam (latch), kind (k), toggle (alwaysToggle)
{
    if (k == Kind::Pad) { up = loadImage ("pad_up"); down = loadImage ("pad_down"); }
    else { up = loadImage (asset + "_up"); down = loadImage (asset + "_down"); }
    setMouseCursor (MouseCursor::PointingHandCursor);
    startTimerHz (30);
}

void ctui::ParamButton::mouseDown (const MouseEvent&)
{
    const bool latched = latchParam != nullptr && latchParam->getValue() > 0.5f;
    param->beginChangeGesture();
    set ((toggle || latched) ? ! isOn() : true);
    repaint();
}
void ctui::ParamButton::mouseUp (const MouseEvent&)
{
    const bool latched = latchParam != nullptr && latchParam->getValue() > 0.5f;
    if (! toggle && ! latched) set (false);
    param->endChangeGesture();
    repaint();
}

void ctui::ParamButton::paint (Graphics& g)
{
    const bool on = isOn();
    auto b = getLocalBounds().toFloat();
    g.drawImage (on ? down : up, b, RectanglePlacement::stretchToFit);
    const float lvl = glow ? glow() : 0.f;
    if (kind == Kind::Pad)
    {
        const auto c = b.getCentre(); const float r = 75.f;
        const float k = on ? 1.f : jlimit (0.f, 1.f, lvl);
        for (float w : { 9.f, 5.5f, 2.6f })
        {
            g.setColour ((on ? amber : copper).withAlpha ((on ? 0.14f : 0.06f + 0.10f * k) * (w > 5.f ? 1.f : (w > 3.f ? 1.8f : 3.4f)) + (on ? 0.f : 0.f)));
            g.drawEllipse (c.x - r, c.y - r, r * 2.f, r * 2.f, w);
        }
        g.setColour (on ? Colour (0xffffe3b0) : copper.withAlpha (0.25f + 0.5f * k));
        g.drawEllipse (c.x - r, c.y - r, r * 2.f, r * 2.f, 1.4f);
    }
    else
    {
        const float cx = b.getWidth() - 22.f, cy = b.getHeight() * 0.5f;
        if (on)
        {
            ColourGradient gl (amber.withAlpha (0.8f), cx, cy, Colours::transparentBlack, cx, cy + 16.f, true);
            g.setGradientFill (gl); g.fillEllipse (cx - 16.f, cy - 16.f, 32.f, 32.f);
            g.setColour (Colour (0xffffe6b8)); g.fillEllipse (cx - 3.6f, cy - 3.6f, 7.2f, 7.2f);
        }
        else { g.setColour (Colour (0xff3a2a20)); g.fillEllipse (cx - 3.6f, cy - 3.6f, 7.2f, 7.2f); }
    }
}

// ---------------------------------------------------------------- Editor
static Font findSignatureFont()
{
    const StringArray wanted { "Snell Roundhand", "Zapfino", "Apple Chancery", "Brush Script MT", "Savoye LET",
                               "Bradley Hand", "Segoe Script", "Lucida Handwriting", "URW Chancery L", "Z003" };
    const auto all = Font::findAllTypefaceNames();
    for (auto& w : wanted) if (all.contains (w)) return Font (FontOptions (w, 30.f, Font::bold));
    return Font (FontOptions (all.contains ("Georgia") ? "Georgia" : Font::getDefaultSerifFontName(), 28.f, Font::bold | Font::italic));
}

namespace
{
    //                       catch  atk  rel  duck eLvl eDiv eFb  hLvl decay sLvl sPit tone  low  width ret
    const float presets[6][15] = {
        {  700,   4,  400, .55f, .65f, 2, .42f, .55f, 3.2f, .12f, 3,  8500, 200, 1.1f, -3 },   // lead vox
        {  400,  60,  900, .40f, .30f, 3, .35f, .85f, 5.5f, .45f, 3,  6500, 250, 1.4f, -2 },   // backing
        {  500,  10,  600, .30f, .55f, 1, .50f, .60f, 2.6f, .20f, 2,  9500, 160, 1.3f, -3 },   // guitar
        {  120,   0,  350, .20f, .25f, 0, .30f, .90f, 1.4f, .00f, 3, 11000, 300, 1.2f, -4 },   // drums
        {  600, 120, 1400, .15f, .35f, 3, .40f, .80f, 7.0f, .55f, 5, 12000, 120, 1.5f, -3 },   // keys
        { 1200,   0, 3500, .00f, .90f, 4, .85f, .90f, 12.f, .80f, 0,  6000, 100, 1.5f,  0 } }; // wild
}

void CatapultEditor::applyPreset (int idx)
{
    for (int i = 0; i < layout::numKnobs; ++i)
        if (auto* p = proc.apvts.getParameter (layout::knobIds[i]))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (presets[idx][i]));
            p->endChangeGesture();
        }
    activeChip = idx;
    for (int i = 0; i < chips.size(); ++i) chips[i]->setLit (i == idx);
}

CatapultEditor::CatapultEditor (CatapultProcessor& p) : AudioProcessorEditor (&p), proc (p), sigFont (findSignatureFont())
{
    faceplate = ctui::loadImage ("faceplate");
    setSize (layout::W, layout::H);
    auto* latch = proc.apvts.getParameter ("latch");

    pad = std::make_unique<ctui::ParamButton> (proc.apvts, "throw", "", ctui::ParamButton::Kind::Pad, false, latch);
    pad->setBounds (layout::padCx - layout::padSize / 2, layout::padCy - layout::padSize / 2, layout::padSize, layout::padSize);
    pad->glow = [this] { return proc.sendLevel.load(); };
    addAndMakeVisible (*pad);

    for (auto& k : layout::keys)
    {
        const String id (k.id);
        auto* b = keys.add (new ctui::ParamButton (proc.apvts, id, k.asset, ctui::ParamButton::Kind::Key, id == "latch", id == "latch" ? nullptr : latch));
        b->setBounds (k.rect[0], k.rect[1], k.rect[2], k.rect[3]);
        addAndMakeVisible (b);
    }
    for (int i = 0; i < layout::numChips; ++i)
    {
        auto* c = chips.add (new ctui::Chip (String ("chip_") + layout::chipIds[i]));
        c->setBounds (layout::chipX0 + i * layout::chipDX, layout::chipY, layout::chipW, layout::chipH);
        c->onClick = [this, i] { applyPreset (i); };
        addAndMakeVisible (c);
    }
    for (int i = 0; i < layout::numKnobs; ++i)
    {
        auto* k = knobs.add (new ctui::Knob (proc.apvts, layout::knobIds[i]));
        const int cx = (int) std::lround (layout::knobX0 + i * layout::knobDX), pad2 = 8;
        k->setBounds (cx - layout::knobD / 2 - pad2, layout::knobY - layout::knobD / 2 - pad2, layout::knobD + 2 * pad2, layout::knobD + 2 * pad2);
        addAndMakeVisible (k);
        attach.push_back (std::make_unique<SliderParameterAttachment> (*proc.apvts.getParameter (layout::knobIds[i]), *k));
        k->onDragStart = [this] { activeChip = -1; for (auto* c : chips) c->setLit (false); };
    }
    startTimerHz (40);
}
CatapultEditor::~CatapultEditor() { stopTimer(); }

void CatapultEditor::timerCallback()
{
    auto toNorm = [] (float lin) { const float db = 20.f * std::log10 (jmax (lin, 1.0e-5f)); return std::pow (jlimit (0.f, 1.f, (db + 66.f) / 66.f), 1.5f); };
    const float w = toNorm (proc.wetLevel.load());
    wetShown += (w - wetShown) * (w > wetShown ? 0.6f : 0.25f);
    retShown += (wetShown - retShown) * 0.4f;
    for (int i = 0; i + 1 < N; ++i) { wetHist[(size_t) i] = wetHist[(size_t) i + 1]; sendHist[(size_t) i] = sendHist[(size_t) i + 1]; }
    wetHist[N - 1] = wetShown; sendHist[N - 1] = proc.throwOn.load() ? proc.sendLevel.load() : 0.f;
    freezeFade += ((proc.apvts.getRawParameterValue ("freeze")->load() > 0.5f ? 1.f : 0.f) - freezeFade) * 0.15f;
    repaint (layout::display[0], layout::display[1], layout::display[2], layout::display[3]);
    repaint (layout::retMeter[0], layout::retMeter[1] - 4, layout::retMeter[2], layout::retMeter[3] + 8);
}

void CatapultEditor::drawDisplay (Graphics& g)
{
    Rectangle<float> r ((float) layout::display[0], (float) layout::display[1], (float) layout::display[2], (float) layout::display[3]);
    Graphics::ScopedSaveState ss (g);
    Path clip; clip.addRoundedRectangle (r.reduced (1.f), 11.f); g.reduceClipRegion (clip);
    const float l = r.getX() + 16.f, rt = r.getRight() - 16.f, top = r.getY() + 34.f, bot = r.getBottom() - 24.f, h = bot - top;

    // throw-active bands
    for (int i = 0; i < N; ++i)
    {
        const float s = sendHist[(size_t) i]; if (s < 0.02f) continue;
        const float x = l + (rt - l) * (float) i / (float) (N - 1);
        g.setColour (amber.withAlpha (0.10f * s)); g.fillRect (x, top, (rt - l) / (float) N + 0.6f, h);
    }
    // wet curve
    Path line, fill;
    for (int i = 0; i < N; ++i)
    {
        const float x = l + (rt - l) * (float) i / (float) (N - 1), y = bot - wetHist[(size_t) i] * h * 0.94f;
        if (i == 0) { line.startNewSubPath (x, y); } else { const float px = l + (rt - l) * (float) (i - 1) / (float) (N - 1), py = bot - wetHist[(size_t) i - 1] * h * 0.94f;
            line.quadraticTo (px, py, (x + px) * 0.5f, (y + py) * 0.5f); }
    }
    fill = line; fill.lineTo (rt, bot); fill.lineTo (l, bot); fill.closeSubPath();
    ColourGradient gf (teal.withAlpha (0.55f), 0.f, top, teal.withAlpha (0.02f), 0.f, bot, false);
    g.setGradientFill (gf); g.fillPath (fill);
    for (auto [w, a] : { std::pair<float, float> { 7.f, 0.08f }, { 3.5f, 0.18f }, { 1.5f, 0.95f } })
    { g.setColour (Colour (0xff9ffbf2).withAlpha (a)); g.strokePath (line, PathStrokeType (w, PathStrokeType::curved, PathStrokeType::rounded)); }
    // send gate trace along the bottom
    Path gate; for (int i = 0; i < N; ++i) { const float x = l + (rt - l) * (float) i / (float) (N - 1), y = bot - 2.f - sendHist[(size_t) i] * 9.f; if (i == 0) gate.startNewSubPath (x, y); else gate.lineTo (x, y); }
    g.setColour (amber.withAlpha (0.9f)); g.strokePath (gate, PathStrokeType (1.4f));
    // freeze tint
    if (freezeFade > 0.01f)
    {
        g.setColour (Colour (0xff8fd8ff).withAlpha (0.10f * freezeFade)); g.fillRect (r);
        g.setColour (Colour (0xffbfeaff).withAlpha (0.8f * freezeFade));
        g.setFont (Font (FontOptions (11.f, Font::bold)));
        g.drawText ("F R O Z E N", r.withTrimmedTop (r.getHeight() * 0.4f).withHeight (20.f), Justification::centred);
    }
    // glass
    ColourGradient glass (Colours::white.withAlpha (0.07f), r.getX(), r.getY(), Colours::transparentWhite, r.getX() + r.getWidth() * 0.55f, r.getBottom(), false);
    g.setGradientFill (glass); g.fillRect (r);
}

void CatapultEditor::paint (Graphics& g)
{
    g.drawImage (faceplate, getLocalBounds().toFloat(), RectanglePlacement::stretchToFit);
    drawDisplay (g);

    // return meter
    Rectangle<float> m ((float) layout::retMeter[0], (float) layout::retMeter[1], (float) layout::retMeter[2], (float) layout::retMeter[3]);
    const int segs = 24;
    for (int i = 0; i < segs; ++i)
    {
        const float x = m.getX() + (m.getWidth() / (float) segs) * (float) i;
        const bool on = ((float) i + 0.5f) / (float) segs < retShown;
        const Colour c = i > 20 ? Colour (0xffff5a4a) : (i > 15 ? amber : teal);
        g.setColour (on ? c : Colour (0xff1c1f21));
        g.fillRect (x + 0.8f, m.getY() + 1.f, m.getWidth() / (float) segs - 1.6f, m.getHeight() - 2.f);
    }

    // knob value readouts
    g.setFont (Font (FontOptions (9.f)));
    g.setColour (copper.withAlpha (0.9f));
    for (int i = 0; i < layout::numKnobs; ++i)
    {
        const int cx = (int) std::lround (layout::knobX0 + i * layout::knobDX);
        g.drawText (knobs[i]->getTextFromValue (knobs[i]->getValue()), cx - 30, layout::knobY + 48, 60, 12, Justification::centred);
    }

    // signature
    auto pr = Rectangle<int> (layout::plate[0], layout::plate[1], layout::plate[2], layout::plate[3]).reduced (20, 2).toFloat();
    g.setFont (sigFont.withHeight (pr.getHeight() * 0.78f));
    g.setColour (Colour (0x66ffd9b0)); g.drawText ("Tal Mendel", pr.translated (0.f, 1.f), Justification::centred);
    g.setColour (Colour (0xff35190a)); g.drawText ("Tal Mendel", pr, Justification::centred);
}
