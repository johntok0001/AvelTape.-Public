#include "PluginEditor.h"

using namespace avel::tape;
using namespace avel::tape::ui;

namespace
{
    // knob-row geometry (design units)
    constexpr float rowY = 382.0f;          // top of the knob titles
    constexpr float knobCY = rowY + 80.0f;  // knob centre line
    constexpr float pillY = rowY + 146.0f;
    constexpr float extraY = rowY + 182.0f;
    constexpr float stripY = 628.0f;

    const char* iconPaths[] = {
        "M20 12 C16 6 8 6 8 12 C8 18 16 18 20 12 C24 6 32 6 32 12 C32 18 24 18 20 12",                       // link / feedback
        "M4 12 C10 2 14 2 20 12 C26 22 30 22 36 12",                                                          // wow
        "M4 12 Q7 5 10 12 Q13 19 16 12 Q19 5 22 12 Q25 19 28 12 Q31 5 34 12",                                 // flutter
        "M8 9 A6 6 0 1 0 20 9 A6 6 0 1 0 8 9 Z M20 9 A6 6 0 1 0 32 9 A6 6 0 1 0 20 9 Z M14 17 A6 6 0 1 0 26 17 A6 6 0 1 0 14 17 Z", // saturation
        "M2 3 L2 22 L42 22 M2 19 C12 19 15 7 21 7 C27 7 29 15 42 15 M12 22 L12 6 M22 22 L22 6 M32 22 L32 6",   // tone
        "M20 0 L20 3 M12 4 L28 6 M12 8 L28 10 M12 12 L28 14 M12 16 L28 18 M12 20 L28 22 M20 22 L20 24"         // spring
    };

    juce::Path svg (const char* d) { return juce::Drawable::parseSVGPath (d); }

    juce::Path gearPath()
    {
        juce::Path p;
        const juce::Point<float> c (12.0f, 12.0f);
        for (int i = 0; i < 8; ++i)
        {
            const float a0 = juce::degreesToRadians ((float) i * 45.0f);
            const float pts[5][2] = { { -13, 8.2f }, { -9, 11 }, { 9, 11 }, { 13, 8.2f }, { 32, 8.2f } };
            for (int j = 0; j < 5; ++j)
            {
                auto pt = c.getPointOnCircumference (pts[j][1], a0 + juce::degreesToRadians (pts[j][0]));
                if (i == 0 && j == 0) p.startNewSubPath (pt); else p.lineTo (pt);
            }
        }
        p.closeSubPath();
        p.addEllipse (9.0f, 9.0f, 6.0f, 6.0f);
        p.setUsingNonZeroWinding (false);
        return p;
    }

    void drawChevron (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, bool left)
    {
        juce::Path p;
        const auto m = r.getCentre();
        p.startNewSubPath (m.x + (left ? 3.0f : -3.0f), m.y - 6.0f);
        p.lineTo (m.x + (left ? -3.0f : 3.0f), m.y);
        p.lineTo (m.x + (left ? 3.0f : -3.0f), m.y + 6.0f);
        g.setColour (c);
        g.strokePath (p, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

//==============================================================================
void PresetBox::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff34302b));
    g.fillRoundedRectangle (r, 8.0f);
    auto in = r.reduced (1.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0c0b0a), 0, in.getY(), juce::Colour (0xff151413), 0, in.getBottom(), false));
    g.fillRoundedRectangle (in, 7.0f);
    innerShadow (g, in, 7.0f, juce::Colours::black.withAlpha (0.8f), 6, { 0, 2 });

    g.setColour (juce::Colour (0xff2a2724));
    g.fillRect (44.0f, 1.0f, 1.0f, r.getHeight() - 2.0f);
    g.fillRect (r.getWidth() - 45.0f, 1.0f, 1.0f, r.getHeight() - 2.0f);

    drawChevron (g, { 0, 0, 44, r.getHeight() }, hot == -1 ? col::cream : col::cream2, true);
    drawChevron (g, { r.getWidth() - 44, 0, 44, r.getHeight() }, hot == 1 ? col::cream : col::cream2, false);

    auto mid = r.withTrimmedLeft (45).withTrimmedRight (45);
    if (hot == 0)
    {
        g.setColour (juce::Colours::white.withAlpha (0.03f));
        g.fillRect (mid.reduced (0, 1));
    }
    text (g, name, font (F::interRegular, 16.0f), mid.withTrimmedLeft (30).withTrimmedRight (30), juce::Justification::centred, col::cream);
    for (int i = 0; i < 3; ++i)
    {
        g.setColour (col::cream.withAlpha (0.6f));
        g.fillEllipse (mid.getRight() - 30.0f + (float) i * 6.0f, mid.getCentreY() - 1.5f, 3.0f, 3.0f);
    }
}

//==============================================================================
void Backdrop::paint (juce::Graphics& g)
{
    const float W = (float) AvelTapeEditor::designW, H = (float) AvelTapeEditor::designH;
    g.fillAll (juce::Colour (0xff141312));

    // header
    {
        auto h = juce::Rectangle<float> (0, 0, W, 72);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff232120), 0, 0, juce::Colour (0xff161514), 0, 72, false));
        g.fillRect (h);
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.fillRect (0.0f, 0.0f, W, 1.0f);
        g.setColour (juce::Colour (0xff070707));
        g.fillRect (0.0f, 71.0f, W, 1.0f);

        const auto f1 = font (F::montMedium, 27.0f, 0.42f);
        const auto f2 = font (F::montMedium, 9.5f, 0.62f);
        auto block = juce::Rectangle<float> (30.0f, 14.0f, wordmarkW, 44.0f);
        text (g, "AVEL TAPE", f1, block.withHeight (30.0f), juce::Justification::centred, col::cream);
        text (g, "TAPE DELAY", f2, block.withY (43.0f).withHeight (13.0f), juce::Justification::centred, col::cream.withAlpha (0.62f));
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.fillRect (block.getRight() + 26.0f, 16.0f, 1.0f, 40.0f);
    }

    // upper metal band
    metal (g, { 0, 72, W, 290 }, juce::Colour (0xffbdb8ae), juce::Colour (0xffa29d94), juce::Colour (0xff8a857c));
    // dark wells of the meters and the transport (static, so they live in the cached backdrop)
    darkWell (g, { 14, 86, 132, 262 }, 12.0f);
    darkWell (g, { 158, 86, 864, 262 }, 12.0f, juce::Colour (0xff1a1918), juce::Colour (0xff0f0e0d));
    darkWell (g, { 1034, 86, 132, 262 }, 12.0f);

    // groove
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff050505), 0, 362, juce::Colour (0xff2a2724), 0, 366, false));
    g.fillRect (0.0f, 362.0f, W, 4.0f);
    // lower metal panel
    metal (g, { 0, 366, W, H - 366 }, juce::Colour (0xffc6c1b7), juce::Colour (0xffaaa59b), juce::Colour (0xff8e897f));
    screw (g, { 16.5f, 382.5f }, 6.5f, 35.0f);
    screw (g, { W - 16.5f, 382.5f }, 6.5f, -50.0f);
    screw (g, { 16.5f, H - 16.5f }, 6.5f, 80.0f);
    screw (g, { W - 16.5f, H - 16.5f }, 6.5f, 10.0f);

    // knob titles, separators, icons
    const auto tf = font (F::montSemi, 12.0f, 0.16f);
    for (auto& c : columns)
    {
        engraved (g, c.title, tf, { c.x - 10.0f, rowY, c.w + 20.0f, 15.0f }, juce::Justification::centred);
        if (c.icon >= 0)
        {
            const bool lampToo = c.icon == 0 || c.icon == 1 || c.icon == 2 || c.icon == 5;
            const float total = lampToo ? 59.0f : 40.0f;
            auto ir = juce::Rectangle<float> (c.x + (c.w - total) * 0.5f, extraY + 2.0f, 40.0f, 24.0f);
            auto p = svg (iconPaths[c.icon]);
            strokeIcon (g, p, ir.translated (0, 1.0f), juce::Colours::white.withAlpha (0.4f));
            strokeIcon (g, p, ir, col::inkSoft);
        }
    }
    for (float x : { 461.0f, 707.0f, 1001.0f })
    {
        g.setColour (juce::Colours::black.withAlpha (0.28f));
        g.fillRect (x, rowY + 4.0f, 1.0f, 226.0f);
        g.setColour (juce::Colours::white.withAlpha (0.4f));
        g.fillRect (x + 1.0f, rowY + 4.0f, 1.0f, 226.0f);
    }
    // DRY / WET labels around the switch (mix column = 3rd)
    if (columns.size() > 2)
    {
        const auto& c = columns[2];
        const auto f = font (F::montSemi, 11.0f, 0.12f);
        const float cx = c.x + c.w * 0.5f;
        engraved (g, "DRY", f, { cx - 22.0f - 10.0f - 40.0f, extraY, 40.0f, 28.0f }, juce::Justification::centredRight);
        engraved (g, "WET", f, { cx + 22.0f + 10.0f, extraY, 40.0f, 28.0f }, juce::Justification::centredLeft);
    }

    // bottom strip
    auto strip = juce::Rectangle<float> (16.0f, stripY, W - 32.0f, 58.0f);
    darkWell (g, strip, 10.0f, juce::Colour (0xff151413), juce::Colour (0xff0d0c0b));
    const auto sf = font (F::montMedium, 11.0f, 0.16f);
    for (auto& it : stripItems)
    {
        if (it.separator)
        {
            g.setColour (juce::Colours::white.withAlpha (0.1f));
            g.fillRect (it.r.getCentreX(), stripY + 14.0f, 1.0f, 30.0f);
        }
        else
            text (g, it.label, sf, it.r, juce::Justification::centredLeft, col::label);
    }

    // footer
    engraved (g, "AVEL AUDIO", font (F::montSemi, 14.0f, 0.5f), { 0, 697.0f, W, 18.0f }, juce::Justification::centred, juce::Colour (0xff2e2a26));
    engraved (g, "MAGNETIC IDEAS FOR MODERN CREATORS", font (F::montMedium, 7.5f, 0.45f), { 0, 717.0f, W, 10.0f },
              juce::Justification::centred, juce::Colour (0xb8282420));

    // fine grain over everything
    tile (g, grainImage(), { 0, 0, W, H }, 0.05f);
}

//==============================================================================
AvelTapeEditor::AvelTapeEditor (AvelTapeProcessor& p)
    : AudioProcessorEditor (p), proc (p),
      inMeter (p, true), outMeter (p, false), transport (p),
      wetSwitch (*p.apvts.getParameter (pid::wetOnly), false),
      flutterModSwitch (*p.apvts.getParameter (pid::flutterMod), true),
      modeSeg (*p.apvts.getParameter (pid::tapeMode), tapeModeNames(), true),
      typeSeg (*p.apvts.getParameter (pid::tapeType), tapeTypeNames(), true)
{
    setLookAndFeel (&lnf);
    juce::LookAndFeel::setDefaultLookAndFeel (&lnf);

    addAndMakeVisible (root);
    root.addAndMakeVisible (backdrop);

    // header
    root.addAndMakeVisible (presetBox);
    presetBox.setTooltip ("Presets: click the name for the list, arrows for previous / next");
    presetBox.onPrev = [this] { proc.stepPreset (-1); };
    presetBox.onNext = [this] { proc.stepPreset (1); };
    presetBox.onMenu = [this] { showPresetMenu(); };

    for (auto* b : { &btnA, &btnB })
    {
        b->interFont = true;
        b->fontSize = 17.0f;
        b->onMetal = false;
        root.addAndMakeVisible (*b);
    }
    btnA.setTooltip ("Compare: setting A");
    btnB.setTooltip ("Compare: setting B");
    btnA.onClick = [this] { proc.selectAB (0); };
    btnB.onClick = [this] { proc.selectAB (1); };

    linkBtn.colour = juce::Colour (0xff6c665d);
    linkBtn.setTooltip ("Copy the current setting to the other slot (A -> B or B -> A)");
    linkBtn.draw = [] (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c)
    {
        auto linkPath = svg ("M10 14 L14 10 M8.5 11.5 L6.5 13.5 C5 15 5 17.5 6.5 18.5 C7.8 19.6 9.6 19.4 10.8 18.2 L12.5 16.5 "
                      "M15.5 12.5 L17.5 10.5 C19 9 19 6.5 17.5 5.5 C16.2 4.4 14.4 4.6 13.2 5.8 L11.5 7.5");
        const float s = 20.0f / 24.0f;
        g.setColour (c);
        g.strokePath (linkPath, juce::PathStrokeType (1.7f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                      juce::AffineTransform::scale (s).translated (r.getCentreX() - 10.0f, r.getCentreY() - 10.0f));
    };
    linkBtn.onClick = [this] { proc.copyToOtherSlot(); };

    gearBtn.setTooltip ("Settings: window size");
    gearBtn.draw = [] (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c)
    {
        static const juce::Path gp = gearPath();
        const float s = 28.0f / 24.0f;
        g.setColour (c);
        g.fillPath (gp, juce::AffineTransform::scale (s).translated (r.getCentreX() - 14.0f, r.getCentreY() - 14.0f));
    };
    gearBtn.onClick = [this] { showSettingsMenu(); };

    moreBtn.setTooltip ("Save preset, init, presets folder");
    moreBtn.draw = [] (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c)
    {
        g.setColour (c);
        for (int i = -1; i <= 1; ++i)
            g.fillEllipse (juce::Rectangle<float> (4.0f, 4.0f).withCentre (r.getCentre().translated (0, (float) i * 8.0f)));
    };
    moreBtn.onClick = [this] { showMoreMenu(); };
    for (auto* b : { &linkBtn, &gearBtn, &moreBtn }) root.addAndMakeVisible (*b);

    root.addAndMakeVisible (inMeter);
    root.addAndMakeVisible (outMeter);
    root.addAndMakeVisible (transport);

    // knobs
    auto attach = [this] (juce::Slider& s, const char* id, const juce::String& tip)
    {
        root.addAndMakeVisible (s);
        sliderAttachments.push_back (std::make_unique<SA> (proc.apvts, id, s));
        s.setTooltip (tip);
    };
    attach (timeKnob, pid::time, "Delay time of head I (head II = 2x, head III = 3x). Double-click: default");
    attach (divKnob, pid::division, "Note value of head I when SYNC is on");
    attach (feedbackKnob, pid::feedback, "Repeats. Above 100 % the tape loop runs away into self-oscillation");
    attach (mixKnob, pid::mix, "Dry / echo balance");
    attach (wowKnob, pid::wow, "Slow tape speed drift (pitch wobble)");
    attach (flutterKnob, pid::flutter, "Fast tape speed jitter");
    attach (satKnob, pid::saturation, "Tape saturation on the record head");
    attach (toneKnob, pid::tone, "Tone of the repeats: dark to airy");
    attach (reverbKnob, pid::reverb, "Spring reverb on the input and the echoes");
    attach (noiseKnob, pid::noise, "Tape hiss level (recorded, so it repeats with the echoes)");
    attach (ageKnob, pid::age, "Tape age: duller highs, drop-outs, unsteady speed");

    timePill.setSlider (&timeKnob);
    feedbackPill.setSlider (&feedbackKnob);
    mixPill.setSlider (&mixKnob);
    wowPill.setSlider (&wowKnob);
    flutterPill.setSlider (&flutterKnob);
    satPill.setSlider (&satKnob);
    tonePill.setSlider (&toneKnob);
    reverbPill.setSlider (&reverbKnob);
    tonePill.stepper = [this] (int dir)
    {
        const float marks[] = { 10.0f, 35.0f, 53.0f, 72.0f, 90.0f };
        const float v = (float) toneKnob.getValue();
        int cur = 0;
        for (int i = 0; i < 5; ++i) if (std::abs (marks[i] - v) < std::abs (marks[cur] - v)) cur = i;
        toneKnob.setValue (marks[juce::jlimit (0, 4, cur + dir)]);
    };
    for (auto* pl : { &timePill, &feedbackPill, &mixPill, &wowPill, &flutterPill, &satPill, &tonePill, &reverbPill })
    {
        root.addAndMakeVisible (*pl);
        pl->setTooltip ("Click to type a value; arrows step it");
    }
    noiseText.setSlider (&noiseKnob);
    ageText.setSlider (&ageKnob);
    root.addAndMakeVisible (noiseText);
    root.addAndMakeVisible (ageText);

    // delay extras
    noteBtn.icon = svg ("M9 18 L9 5 L17 4 L17 7");
    noteBtn.icon.addEllipse (3.9f, 15.4f, 5.2f, 5.2f);
    noteBtn.iconFilled = false;
    noteBtn.setTooltip ("Choose the note value (turns SYNC on)");
    noteBtn.onClick = [this] { showDivisionMenu(); };
    root.addAndMakeVisible (noteBtn);
    noteBtn.setToggled (true);

    syncBtn.setTooltip ("Sync the delay time to the project tempo");
    root.addAndMakeVisible (syncBtn);
    syncAttachment = std::make_unique<juce::ParameterAttachment> (*proc.apvts.getParameter (pid::sync),
        [this] (float v) { syncOn = v > 0.5f; syncBtn.setToggled (syncOn); updateSyncVisibility(); }, nullptr);
    syncBtn.onClick = [this] { syncAttachment->setValueAsCompleteGesture (syncOn ? 0.0f : 1.0f); };
    syncAttachment->sendInitialUpdate();

    auto raw = [this] (const char* id) { return proc.apvts.getRawParameterValue (id); };
    syncLamp.state = [this] { return syncOn; };
    syncLamp.setTooltip ("Lit when the delay follows the project tempo");
    feedbackLamp.state = [this, fb = raw (pid::feedback)]
    {
        const float v = fb->load();
        if (v > 100.0f) return (juce::Time::getMillisecondCounter() / 250) % 2 == 0;   // blinks: runaway
        return v > 0.5f;
    };
    feedbackLamp.setTooltip ("Feedback loop active; blinks above 100 % (self-oscillation)");
    wowLamp.state = [wow = raw (pid::wow), m = raw (pid::flutterMod)] { return wow->load() > 0.5f && m->load() > 0.5f; };
    flutterLamp.state = [fl = raw (pid::flutter), m = raw (pid::flutterMod)] { return fl->load() > 0.5f && m->load() > 0.5f; };
    wowLamp.setTooltip ("Wow active (needs Flutter Mod on)");
    flutterLamp.setTooltip ("Flutter active (needs Flutter Mod on)");
    reverbLamp.state = [rv = raw (pid::reverb)] { return rv->load() > 0.5f; };
    reverbLamp.setTooltip ("Spring reverb active");
    for (auto* l : { &syncLamp, &feedbackLamp, &wowLamp, &flutterLamp, &reverbLamp }) root.addAndMakeVisible (*l);

    wetSwitch.setTooltip ("Wet only: removes the dry signal (for send / aux tracks)");
    flutterModSwitch.setTooltip ("Tape speed modulation (wow & flutter) on / off");
    modeSeg.setTooltip ("Tape condition: Clean (hi-fi), Vintage, Worn (dull, unsteady, drop-outs)");
    typeSeg.setTooltip ("Tape formulation: Normal, High (bright, more headroom), Low (dark, saturates early)");
    for (juce::Component* c : { (juce::Component*) &wetSwitch, (juce::Component*) &flutterModSwitch, (juce::Component*) &modeSeg, (juce::Component*) &typeSeg })
        root.addAndMakeVisible (*c);

    proc.addChangeListener (this);
    changeListenerCallback (&proc);

    layoutDesign();

    constrainer.setFixedAspectRatio ((double) designW / (double) designH);
    constrainer.setSizeLimits (designW / 2, designH / 2, designW * 2, designH * 2);
    setConstrainer (&constrainer);
    setResizable (true, true);
    const int pct = juce::jlimit (50, 200, proc.uiScalePercent.load());
    setSize (designW * pct / 100, designH * pct / 100);
    sizeRestored = true;

    lastTime = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (60);
}

AvelTapeEditor::~AvelTapeEditor()
{
    proc.removeChangeListener (this);
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void AvelTapeEditor::layoutDesign()
{
    root.setBounds (0, 0, designW, designH);
    backdrop.setBounds (0, 0, designW, designH);

    // header
    backdrop.wordmarkW = textWidth (font (F::montMedium, 27.0f, 0.42f), "AVEL TAPE");
    presetBox.setBounds ((designW - 420) / 2, 13, 420, 46);
    moreBtn.setBounds (1126, 16, 28, 40);
    gearBtn.setBounds (1072, 16, 40, 40);
    btnB.setBounds (1006, 16, 48, 40);
    linkBtn.setBounds (970, 22, 24, 28);
    btnA.setBounds (910, 16, 48, 40);

    inMeter.setBounds (14, 86, 132, 262);
    transport.setBounds (158, 86, 864, 262);
    outMeter.setBounds (1034, 86, 132, 262);

    // knob columns: x, width, knob size
    struct ColDef { float x, w, size; juce::Slider* knob; ValuePill* pill; float pillW; const char* title; int icon; };
    const ColDef cols[] = {
        { 34.0f,   136.0f, 112.0f, &timeKnob,     &timePill,     116.0f, "DELAY TIME",    -1 },
        { 176.0f,  136.0f, 112.0f, &feedbackKnob, &feedbackPill, 116.0f, "FEEDBACK",       0 },
        { 318.0f,  136.0f, 112.0f, &mixKnob,      &mixPill,      116.0f, "MIX",           -1 },
        { 470.0f,  112.0f, 88.0f,  &wowKnob,      &wowPill,      88.0f,  "WOW",            1 },
        { 588.0f,  112.0f, 88.0f,  &flutterKnob,  &flutterPill,  88.0f,  "FLUTTER",        2 },
        { 716.0f,  136.0f, 108.0f, &satKnob,      &satPill,      116.0f, "SATURATION",     3 },
        { 858.0f,  136.0f, 104.0f, &toneKnob,     &tonePill,     116.0f, "TONE",           4 },
        { 1010.0f, 136.0f, 100.0f, &reverbKnob,   &reverbPill,   104.0f, "SPRING REVERB",  5 },
    };
    backdrop.columns.clear();
    for (auto& c : cols)
    {
        const float cx = c.x + c.w * 0.5f;
        c.knob->setBounds (juce::Rectangle<float> (c.size, c.size).withCentre ({ cx, knobCY }).toNearestInt());
        c.pill->setBounds (juce::Rectangle<float> (c.pillW, 26.0f).withCentre ({ cx, pillY + 13.0f }).toNearestInt());
        backdrop.columns.push_back ({ c.x, c.w, c.title, c.icon });

        if (c.icon == 0 || c.icon == 1 || c.icon == 2 || c.icon == 5)
        {
            LampView* l = c.icon == 0 ? &feedbackLamp : c.icon == 1 ? &wowLamp : c.icon == 2 ? &flutterLamp : &reverbLamp;
            const float left = c.x + (c.w - 59.0f) * 0.5f;
            l->setBounds (juce::Rectangle<float> (14.0f, 14.0f).withCentre ({ left + 54.5f, extraY + 14.0f }).toNearestInt());
        }
    }
    divKnob.setBounds (timeKnob.getBounds());

    // delay extras: [note] [SYNC] (lamp)
    {
        const float syncW = textWidth (font (F::montMedium, 10.5f, 0.12f), "SYNC") + 24.0f;
        const float total = 30.0f + 10.0f + syncW + 10.0f + 9.0f;
        float x = 34.0f + (136.0f - total) * 0.5f;
        noteBtn.setBounds (juce::Rectangle<float> (x, extraY + 1.0f, 30.0f, 26.0f).toNearestInt());
        x += 40.0f;
        syncBtn.setBounds (juce::Rectangle<float> (x, extraY + 2.0f, syncW, 24.0f).toNearestInt());
        x += syncW + 10.0f;
        syncLamp.setBounds (juce::Rectangle<float> (14.0f, 14.0f).withCentre ({ x + 4.5f, extraY + 14.0f }).toNearestInt());
    }
    // mix switch
    wetSwitch.setBounds (juce::Rectangle<float> (44.0f, 24.0f).withCentre ({ 318.0f + 68.0f, extraY + 14.0f }).toNearestInt());

    // bottom strip, laid out left to right
    backdrop.stripItems.clear();
    const auto lf = font (F::montMedium, 11.0f, 0.16f);
    float x = 16.0f + 22.0f;
    const float cy = stripY + 29.0f;
    auto label = [&] (const juce::String& s)
    {
        const float w = textWidth (lf, s);
        backdrop.stripItems.push_back ({ { x, cy - 8.0f, w + 6.0f, 16.0f }, s, false });
        x += w + 12.0f;
    };
    auto sep = [&] { backdrop.stripItems.push_back ({ { x, cy - 15.0f, 1.0f, 30.0f }, {}, true }); x += 14.0f; };
    auto place = [&] (juce::Component& c, float w, float h, float gapAfter)
    {
        c.setBounds (juce::Rectangle<float> (x, cy - h * 0.5f, w, h).toNearestInt());
        x += w + gapAfter;
    };
    label ("TAPE MODE");   place (modeSeg, 200.0f, 34.0f, 14.0f); sep();
    label ("TAPE TYPE");   place (typeSeg, 194.0f, 34.0f, 14.0f); sep();
    label ("NOISE");       place (noiseKnob, 40.0f, 40.0f, 8.0f); place (noiseText, 44.0f, 16.0f, 8.0f); sep();
    label ("AGE");         place (ageKnob, 40.0f, 40.0f, 8.0f);   place (ageText, 38.0f, 16.0f, 8.0f); sep();
    label ("FLUTTER MOD"); place (flutterModSwitch, 44.0f, 24.0f, 0.0f);

    backdrop.repaint();
}

void AvelTapeEditor::resized()
{
    const float s = (float) getWidth() / (float) designW;
    root.setTransform (juce::AffineTransform::scale (s));
    if (sizeRestored)
        proc.uiScalePercent.store (juce::jlimit (50, 200, juce::roundToInt (s * 100.0f)));
}

void AvelTapeEditor::setScale (int percent)
{
    setSize (designW * percent / 100, designH * percent / 100);
}

void AvelTapeEditor::updateSyncVisibility()
{
    timeKnob.setVisible (! syncOn);
    divKnob.setVisible (syncOn);
    timePill.setSlider (syncOn ? (juce::Slider*) &divKnob : (juce::Slider*) &timeKnob);
}

void AvelTapeEditor::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const float dt = (float) juce::jlimit (0.0, 0.1, (now - lastTime) / 1000.0);
    lastTime = now;
    inMeter.update (dt);
    outMeter.update (dt);
    transport.update (dt);
    for (auto* l : { &syncLamp, &feedbackLamp, &wowLamp, &flutterLamp, &reverbLamp }) l->update();
}

void AvelTapeEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    presetBox.name = proc.getPresetName();
    presetBox.repaint();
    btnA.setToggled (proc.getABSlot() == 0);
    btnB.setToggled (proc.getABSlot() == 1);
}

//==============================================================================
void AvelTapeEditor::showPresetMenu()
{
    auto list = proc.getPresetList();
    juce::PopupMenu m;
    m.addSectionHeader ("Factory");
    const auto cur = proc.getPresetName();
    bool userHeader = false;
    for (int i = 0; i < (int) list.size(); ++i)
    {
        if (! list[(size_t) i].factory && ! userHeader)
        {
            m.addSectionHeader ("User");
            userHeader = true;
        }
        m.addItem (i + 1, list[(size_t) i].name, true, list[(size_t) i].name == cur);
    }
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetBox).withMinimumWidth (presetBox.getWidth()),
                     [this] (int r) { if (r > 0) proc.loadPresetByIndex (r - 1); });
}

void AvelTapeEditor::showMoreMenu()
{
    juce::PopupMenu m;
    m.addItem (1, "Save preset...");
    m.addItem (2, "Init (default settings)");
    m.addItem (3, "Open presets folder");
    m.addSeparator();
    m.addItem (4, juce::String ("Avel Tape ") + AVEL_VERSION, false, false);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&moreBtn), [this] (int r)
    {
        if (r == 1) savePresetDialog();
        else if (r == 2) proc.initPreset();
        else if (r == 3)
        {
            auto f = AvelTapeProcessor::getUserPresetFolder();
            f.createDirectory();
            f.startAsProcess();
        }
    });
}

void AvelTapeEditor::showSettingsMenu()
{
    juce::PopupMenu size;
    const int cur = juce::roundToInt ((float) getWidth() * 100.0f / (float) designW);
    for (int pct : { 60, 75, 90, 100, 125, 150 })
        size.addItem (pct, juce::String (pct) + " %", true, std::abs (cur - pct) <= 1);
    juce::PopupMenu m;
    m.addSubMenu ("Window size", size);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&gearBtn), [this] (int r) { if (r > 0) setScale (r); });
}

void AvelTapeEditor::showDivisionMenu()
{
    juce::PopupMenu m;
    const auto names = divisionNames();
    const int cur = (int) proc.apvts.getRawParameterValue (pid::division)->load();
    for (int i = 0; i < names.size(); ++i)
        m.addItem (i + 1, names[i], true, syncOn && i == cur);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&noteBtn), [this] (int r)
    {
        if (r <= 0) return;
        if (auto* p = proc.apvts.getParameter (pid::division))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 ((float) (r - 1)));
            p->endChangeGesture();
        }
        if (! syncOn) syncAttachment->setValueAsCompleteGesture (1.0f);
    });
}

void AvelTapeEditor::savePresetDialog()
{
    alert = std::make_unique<juce::AlertWindow> ("Save preset", "Preset name:", juce::MessageBoxIconType::NoIcon, this);
    alert->addTextEditor ("name", proc.getPresetName());
    alert->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    alert->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    alert->enterModalState (true, juce::ModalCallbackFunction::create ([this] (int r)
    {
        if (r == 1 && alert != nullptr)
            proc.saveUserPreset (alert->getTextEditorContents ("name"));
        alert.reset();
    }), false);
}
