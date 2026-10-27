#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"

namespace avel::tape::ui
{
    inline float startAngle() { return juce::degreesToRadians (-135.0f); }
    inline float endAngle()   { return juce::degreesToRadians (135.0f); }

    //==============================================================================
    // Big knob: engraved tick ring, black bezel, amber value arc, brushed aluminium cap.
    class AmberKnob : public juce::Slider
    {
    public:
        AmberKnob()
        {
            setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            setMouseDragSensitivity (240);
            setScrollWheelEnabled (true);
            setRotaryParameters (startAngle(), endAngle(), true);
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
        }

        void paint (juce::Graphics& g) override
        {
            const auto b = getLocalBounds().toFloat();
            const float s = juce::jmin (b.getWidth(), b.getHeight());
            const auto c = b.getCentre();
            const float prop = (float) valueToProportionOfLength (getValue());
            const float a0 = startAngle();
            const float a = a0 + prop * (endAngle() - a0);
            const bool on = isEnabled();

            // tick ring
            juce::Path ticks;
            for (int i = 0; i <= 30; ++i)
            {
                const float ang = a0 + (float) i / 30.0f * (endAngle() - a0);
                const float r1 = s * (i % 5 == 0 ? 0.45f : 0.465f), r2 = s * 0.495f;
                ticks.startNewSubPath (c.getPointOnCircumference (r1, ang));
                ticks.lineTo (c.getPointOnCircumference (r2, ang));
            }
            g.setColour (juce::Colours::white.withAlpha (0.35f));
            g.strokePath (ticks, juce::PathStrokeType (s * 0.009f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                          juce::AffineTransform::translation (0, 0.8f));
            g.setColour (col::ink.withAlpha (0.82f));
            g.strokePath (ticks, juce::PathStrokeType (s * 0.009f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // bezel
            const float br = s * 0.43f;
            auto bez = juce::Rectangle<float> (br * 2, br * 2).withCentre (c);
            {
                juce::Path p; p.addEllipse (bez);
                juce::DropShadow (juce::Colours::black.withAlpha (0.5f), (int) (s * 0.12f), { 0, (int) (s * 0.06f) }).drawForPath (g, p);
            }
            g.setColour (juce::Colours::white.withAlpha (0.4f));
            g.fillEllipse (bez.translated (0, 1.0f));
            juce::ColourGradient bg (juce::Colour (0xff2c2926), c.x, c.y - br * 0.3f, juce::Colour (0xff050505), c.x, c.y + br, true);
            bg.addColour (0.55, juce::Colour (0xff121110));
            g.setGradientFill (bg);
            g.fillEllipse (bez);

            // value arc
            const float ar = s * 0.40f;
            juce::Path track; track.addCentredArc (c.x, c.y, ar, ar, 0, a0, endAngle(), true);
            g.setColour (juce::Colours::black.withAlpha (0.6f));
            g.strokePath (track, juce::PathStrokeType (s * 0.04f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            if (a > a0 + 0.01f)
            {
                juce::Path arc; arc.addCentredArc (c.x, c.y, ar, ar, 0, a0, a, true);
                auto glowCol = on ? col::amberDeep : juce::Colour (0xff8a857c);
                const float widths[] = { 0.11f, 0.075f, 0.05f };
                const float alphas[] = { 0.10f, 0.20f, 0.55f };
                for (int i = 0; i < 3; ++i)
                {
                    g.setColour (glowCol.withAlpha (alphas[i]));
                    g.strokePath (arc, juce::PathStrokeType (s * widths[i], juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                }
                g.setColour (on ? col::amberHi : juce::Colour (0xffc9c2b4));
                g.strokePath (arc, juce::PathStrokeType (s * 0.024f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }

            // cap
            const float cr = s * 0.30f;
            auto cap = juce::Rectangle<float> (cr * 2, cr * 2).withCentre (c);
            {
                juce::Path p; p.addEllipse (cap);
                juce::DropShadow (juce::Colours::black.withAlpha (0.75f), (int) (s * 0.07f), { 0, (int) (s * 0.035f) }).drawForPath (g, p);
            }
            const float scale = g.getInternalContext().getPhysicalPixelScaleFactor();
            const int px = juce::jlimit (16, 512, juce::roundToInt (cap.getWidth() * scale));
            g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
            g.drawImage (capImage (px), cap);
            juce::ColourGradient hl (juce::Colours::white.withAlpha (0.3f), cap.getX() + cap.getWidth() * 0.4f, cap.getY() + cap.getHeight() * 0.28f,
                                     juce::Colours::white.withAlpha (0.0f), cap.getX() + cap.getWidth() * 0.4f, cap.getY() + cap.getHeight() * 0.85f, true);
            g.setGradientFill (hl);
            g.fillEllipse (cap);
            g.setColour (juce::Colours::black.withAlpha (0.25f));
            g.drawEllipse (cap.reduced (0.5f), 1.0f);

            // pointer
            const auto p1 = c.getPointOnCircumference (cr * 0.82f, a);
            const auto p2 = c.getPointOnCircumference (cr * 0.52f, a);
            juce::Line<float> ln (p1, p2);
            g.setColour ((on ? col::amber : juce::Colour (0xff6a655d)).withAlpha (0.35f));
            g.drawLine (ln, s * 0.06f);
            g.setColour (on ? col::amber : juce::Colour (0xff6a655d));
            g.drawLine (ln, s * 0.027f);
        }
    };

    //==============================================================================
    // Small dark knob of the bottom strip.
    class MiniKnob : public juce::Slider
    {
    public:
        MiniKnob()
        {
            setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            setMouseDragSensitivity (200);
            setRotaryParameters (startAngle(), endAngle(), true);
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
        }

        void paint (juce::Graphics& g) override
        {
            const auto b = getLocalBounds().toFloat();
            const float s = juce::jmin (b.getWidth(), b.getHeight());
            const auto c = b.getCentre();
            const float a0 = startAngle();
            const float a = a0 + (float) valueToProportionOfLength (getValue()) * (endAngle() - a0);

            juce::Path ticks;
            for (int i = 0; i <= 10; ++i)
            {
                const float ang = a0 + (float) i / 10.0f * (endAngle() - a0);
                ticks.startNewSubPath (c.getPointOnCircumference (s * 0.42f, ang));
                ticks.lineTo (c.getPointOnCircumference (s * 0.49f, ang));
            }
            g.setColour (juce::Colour (0x99cdc6b8));
            g.strokePath (ticks, juce::PathStrokeType (1.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            auto k = juce::Rectangle<float> (s * 0.65f, s * 0.65f).withCentre (c);
            {
                juce::Path p; p.addEllipse (k);
                juce::DropShadow (juce::Colours::black.withAlpha (0.8f), 4, { 0, 2 }).drawForPath (g, p);
            }
            juce::ColourGradient gr (juce::Colour (0xff3e3a36), k.getX() + k.getWidth() * 0.4f, k.getY() + k.getHeight() * 0.3f,
                                     juce::Colour (0xff070707), k.getRight(), k.getBottom(), true);
            gr.addColour (0.7, juce::Colour (0xff151312));
            g.setGradientFill (gr);
            g.fillEllipse (k);
            g.setColour (juce::Colours::white.withAlpha (0.12f));
            g.drawEllipse (k.reduced (0.5f), 1.0f);

            g.setColour (col::cream);
            g.drawLine (juce::Line<float> (c.getPointOnCircumference (k.getWidth() * 0.44f, a), c.getPointOnCircumference (k.getWidth() * 0.16f, a)), 2.0f);
        }
    };

    //==============================================================================
    // Dark value field under a knob: "‹ 375 ms ›". Arrows step the value, a click on the text edits it.
    class ValuePill : public juce::Component, public juce::SettableTooltipClient, private juce::Slider::Listener
    {
    public:
        ValuePill (bool showArrows) : arrows (showArrows) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
        ~ValuePill() override { if (slider != nullptr) slider->removeListener (this); }

        void setSlider (juce::Slider* s)
        {
            if (slider != nullptr) slider->removeListener (this);
            slider = s;
            if (slider != nullptr) slider->addListener (this);
            repaint();
        }

        std::function<void (int)> stepper;   // optional custom step

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (0.5f, 0.5f).withTrimmedBottom (1.0f);
            darkWell (g, r, 6.0f, juce::Colour (0xff1d1c1a), juce::Colour (0xff0f0e0d));
            if (slider == nullptr) return;

            const auto txt = slider->getTextFromValue (slider->getValue());
            const bool word = txt.containsOnly ("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
            text (g, txt, font (F::interMedium, 13.0f, word ? 0.12f : 0.0f), r, juce::Justification::centred, col::cream);

            if (arrows)
            {
                auto chev = [&] (float cx, bool left, bool hot)
                {
                    juce::Path p;
                    const float h = 4.0f, w = 2.6f, cy = r.getCentreY();
                    p.startNewSubPath (cx + (left ? w : -w), cy - h);
                    p.lineTo (cx + (left ? -w : w), cy);
                    p.lineTo (cx + (left ? w : -w), cy + h);
                    g.setColour (col::cream.withAlpha (hot ? 0.85f : 0.42f));
                    g.strokePath (p, juce::PathStrokeType (1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                };
                chev (r.getX() + 12.0f, true, hover == -1);
                chev (r.getRight() - 12.0f, false, hover == 1);
            }
        }

        void mouseMove (const juce::MouseEvent& e) override { setHover (zone (e.x)); }
        void mouseExit (const juce::MouseEvent&) override { setHover (0); }

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (slider == nullptr || ! isEnabled()) return;
            const int z = zone (e.x);
            if (z != 0) step (z);
            else openEditor();
        }

    private:
        int zone (int x) const
        {
            if (! arrows) return 0;
            if (x < 24) return -1;
            if (x > getWidth() - 24) return 1;
            return 0;
        }

        void setHover (int h) { if (h != hover) { hover = h; repaint(); } }

        void step (int dir)
        {
            if (stepper) { stepper (dir); return; }
            const double interval = slider->getInterval();
            const auto range = slider->getRange();
            if (interval >= 1.0 && range.getLength() <= 20.0)
                slider->setValue (juce::jlimit (range.getStart(), range.getEnd(), slider->getValue() + dir * interval));
            else
                slider->setValue (slider->proportionOfLengthToValue (juce::jlimit (0.0, 1.0, slider->valueToProportionOfLength (slider->getValue()) + dir * 0.01)));
        }

        void openEditor()
        {
            editor = std::make_unique<juce::TextEditor>();
            addAndMakeVisible (*editor);
            editor->setBounds (getLocalBounds().reduced (3));
            editor->setJustification (juce::Justification::centred);
            editor->setFont (font (F::interMedium, 13.0f));
            editor->setIndents (2, 3);
            editor->setText (slider->getTextFromValue (slider->getValue()), false);
            editor->selectAll();
            editor->grabKeyboardFocus();
            auto close = [this] (bool apply)
            {
                if (editor == nullptr) return;
                if (apply)
                {
                    const auto t = editor->getText();
                    if (t.trim().isNotEmpty())
                        slider->setValue (slider->getValueFromText (t));
                }
                juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<ValuePill> (this)]
                {
                    if (sp != nullptr) sp->editor.reset();
                });
            };
            editor->onReturnKey = [close] { close (true); };
            editor->onEscapeKey = [close] { close (false); };
            editor->onFocusLost = [close] { close (true); };
        }

        void sliderValueChanged (juce::Slider*) override { repaint(); }

        juce::Slider* slider = nullptr;
        bool arrows;
        int hover = 0;
        std::unique_ptr<juce::TextEditor> editor;
    };

    // plain value text (bottom strip)
    class ValueText : public juce::Component, private juce::Slider::Listener
    {
    public:
        ~ValueText() override { if (slider != nullptr) slider->removeListener (this); }
        void setSlider (juce::Slider* s) { slider = s; if (slider != nullptr) slider->addListener (this); }
        void paint (juce::Graphics& g) override
        {
            if (slider != nullptr)
                text (g, slider->getTextFromValue (slider->getValue()), font (F::interMedium, 11.5f), getLocalBounds().toFloat(),
                      juce::Justification::centredLeft, col::label);
        }
        void sliderValueChanged (juce::Slider*) override { repaint(); }
        juce::Slider* slider = nullptr;
    };

    //==============================================================================
    // Segmented selector bound to a choice parameter (head pattern, tape mode, tape type).
    class Segmented : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        Segmented (juce::RangedAudioParameter& p, juce::StringArray labels, bool montserrat)
            : items (std::move (labels)), mont (montserrat),
              attachment (p, [this] (float v) { index = juce::roundToInt (v); repaint(); }, nullptr)
        {
            attachment.sendInitialUpdate();
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
        }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            g.setColour (col::segBg);
            g.fillRoundedRectangle (r, 7.0f);
            g.setColour (col::segBorder);
            g.drawRoundedRectangle (r.reduced (0.5f), 7.0f, 1.0f);

            const auto f = mont ? font (F::montMedium, 10.5f, 0.1f) : font (F::interMedium, 13.0f);
            for (int i = 0; i < items.size(); ++i)
            {
                auto b = buttonRect (i);
                const bool on = i == index;
                if (on)
                {
                    g.setColour (col::amber.withAlpha (0.10f));
                    g.fillRoundedRectangle (b.expanded (3.0f), 7.0f);
                    g.setColour (juce::Colour (0xff1d1610));
                    g.fillRoundedRectangle (b, 5.0f);
                    g.setColour (col::amberLine);
                    g.drawRoundedRectangle (b.reduced (0.5f), 5.0f, 1.0f);
                }
                else if (i == hover)
                {
                    g.setColour (juce::Colours::white.withAlpha (0.04f));
                    g.fillRoundedRectangle (b, 5.0f);
                }
                text (g, items[i], f, b, juce::Justification::centred, on ? col::amber : col::label);
            }
        }

        void mouseMove (const juce::MouseEvent& e) override { const int h = hit (e.position); if (h != hover) { hover = h; repaint(); } }
        void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
        void mouseDown (const juce::MouseEvent& e) override
        {
            const int i = hit (e.position);
            if (i >= 0 && i != index)
                attachment.setValueAsCompleteGesture ((float) i);
        }

        int getIndex() const { return index; }

    private:
        juce::Rectangle<float> buttonRect (int i) const
        {
            auto r = getLocalBounds().toFloat().reduced (2.0f);
            const float w = (r.getWidth() - (float) (items.size() - 1) * 2.0f) / (float) items.size();
            return { r.getX() + (float) i * (w + 2.0f), r.getY(), w, r.getHeight() };
        }

        int hit (juce::Point<float> p) const
        {
            for (int i = 0; i < items.size(); ++i)
                if (buttonRect (i).contains (p)) return i;
            return -1;
        }

        juce::StringArray items;
        bool mont;
        int index = 0, hover = -1;
        juce::ParameterAttachment attachment;
    };

    //==============================================================================
    // One-button toggle in the segmented style (PING PONG), bound to an Off/On parameter.
    class ChipToggle : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        ChipToggle (juce::RangedAudioParameter& p, juce::String text)
            : label (std::move (text)), attachment (p, [this] (float v) { on = v > 0.5f; repaint(); }, nullptr)
        {
            attachment.sendInitialUpdate();
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
        }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            g.setColour (col::segBg);
            g.fillRoundedRectangle (r, 7.0f);
            g.setColour (col::segBorder);
            g.drawRoundedRectangle (r.reduced (0.5f), 7.0f, 1.0f);
            auto b = r.reduced (2.0f);
            if (on)
            {
                g.setColour (col::amber.withAlpha (0.10f));
                g.fillRoundedRectangle (b.expanded (3.0f), 7.0f);
                g.setColour (juce::Colour (0xff1d1610));
                g.fillRoundedRectangle (b, 5.0f);
                g.setColour (col::amberLine);
                g.drawRoundedRectangle (b.reduced (0.5f), 5.0f, 1.0f);
            }
            else if (hot)
            {
                g.setColour (juce::Colours::white.withAlpha (0.04f));
                g.fillRoundedRectangle (b, 5.0f);
            }
            lamp (g, { b.getX() + 13.0f, b.getCentreY() }, 7.0f, on, 0.6f);
            text (g, label, font (F::montMedium, 10.0f, 0.1f), b.withTrimmedLeft (22.0f).withTrimmedRight (4.0f),
                  juce::Justification::centred, on ? col::amber : col::label);
        }

        void mouseEnter (const juce::MouseEvent&) override { hot = true; repaint(); }
        void mouseExit (const juce::MouseEvent&) override { hot = false; repaint(); }
        void mouseDown (const juce::MouseEvent&) override { attachment.setValueAsCompleteGesture (on ? 0.0f : 1.0f); }
        bool isOn() const { return on; }

    private:
        juce::String label;
        bool on = false, hot = false;
        juce::ParameterAttachment attachment;
    };

    //==============================================================================
    // Pill switch bound to a bool parameter (Wet only, Flutter Mod).
    class PillSwitch : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        PillSwitch (juce::RangedAudioParameter& p, bool framed)
            : frame (framed), attachment (p, [this] (float v) { on = v > 0.5f; repaint(); }, nullptr)
        {
            attachment.sendInitialUpdate();
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
        }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (0.5f);
            const float rad = r.getHeight() * 0.5f;
            if (frame)
            {
                if (on)
                {
                    g.setColour (col::amber.withAlpha (0.12f));
                    g.fillRoundedRectangle (r.expanded (2.0f), rad + 2.0f);
                }
                g.setColour (on ? juce::Colour (0xff1b140c) : juce::Colour (0xff0e0d0c));
                g.fillRoundedRectangle (r, rad);
                g.setColour (on ? juce::Colour (0xffc9852f) : juce::Colour (0xff3a3631));
                g.drawRoundedRectangle (r, rad, 1.0f);
            }
            else
            {
                g.setColour (juce::Colours::white.withAlpha (0.45f));
                g.fillRoundedRectangle (r.translated (0, 1.0f), rad);
                g.setColour (juce::Colour (0xff0e0d0c));
                g.fillRoundedRectangle (r, rad);
                innerShadow (g, r, rad, juce::Colours::black, 4, { 0, 1 });
            }
            const float d = r.getHeight() - 6.0f;
            auto k = juce::Rectangle<float> (d, d).withCentre ({ on ? r.getRight() - 3.0f - d * 0.5f : r.getX() + 3.0f + d * 0.5f, r.getCentreY() });
            if (on)
            {
                g.setColour (col::amber.withAlpha (0.35f));
                g.fillEllipse (k.expanded (3.0f));
            }
            juce::ColourGradient kg (on ? juce::Colour (0xfffff4dc) : juce::Colour (0xffd8d3ca), k.getX() + d * 0.35f, k.getY() + d * 0.3f,
                                     on ? juce::Colour (0xffd98a2c) : juce::Colour (0xff6a665f), k.getRight(), k.getBottom(), true);
            kg.addColour (0.5, on ? juce::Colour (0xffffc066) : juce::Colour (0xffa29d94));
            g.setGradientFill (kg);
            g.fillEllipse (k);
        }

        void mouseDown (const juce::MouseEvent&) override { attachment.setValueAsCompleteGesture (on ? 0.0f : 1.0f); }
        bool isOn() const { return on; }

    private:
        bool frame, on = false;
        juce::ParameterAttachment attachment;
    };

    //==============================================================================
    // Small dark button (SYNC, note, header buttons). Optional icon path in a 24x24 box.
    class SmallButton : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        SmallButton (juce::String t = {}) : label (std::move (t)) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }

        std::function<void()> onClick;
        juce::String label;
        juce::Path icon;
        bool iconFilled = false;
        bool toggled = false;
        bool onMetal = true;          // light bottom edge on aluminium
        float fontSize = 10.5f;
        bool interFont = false;

        void setToggled (bool t) { if (t != toggled) { toggled = t; repaint(); } }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (0.5f);
            if (onMetal) { g.setColour (juce::Colours::white.withAlpha (0.4f)); g.fillRoundedRectangle (r.translated (0, 1.0f), 5.0f); }
            if (toggled)
            {
                g.setColour (col::amber.withAlpha (0.15f));
                g.fillRoundedRectangle (r.expanded (2.5f), 7.0f);
            }
            g.setGradientFill (juce::ColourGradient (toggled ? juce::Colour (0xff221a11) : juce::Colour (0xff2a2724), r.getX(), r.getY(),
                                                     toggled ? juce::Colour (0xff14100b) : juce::Colour (0xff151412), r.getX(), r.getBottom(), false));
            g.fillRoundedRectangle (r, 5.0f);
            g.setColour (toggled ? col::amberLine : (hot ? juce::Colour (0xff57514a) : juce::Colour (0xff3a3631)));
            g.drawRoundedRectangle (r, 5.0f, 1.0f);

            const auto c = toggled ? col::amber : col::cream2;
            if (! icon.isEmpty())
            {
                auto ib = r.withSizeKeepingCentre (18.0f, 18.0f);
                const float sc = ib.getWidth() / 24.0f;
                auto t = juce::AffineTransform::scale (sc).translated (ib.getX(), ib.getY());
                g.setColour (iconFilled ? col::amber : c);
                if (iconFilled) g.fillPath (icon, t);
                else g.strokePath (icon, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), t);
            }
            if (label.isNotEmpty())
                text (g, label, interFont ? font (F::interMedium, fontSize) : font (F::montMedium, fontSize, 0.12f), r, juce::Justification::centred, c);
        }

        void mouseEnter (const juce::MouseEvent&) override { hot = true; repaint(); }
        void mouseExit (const juce::MouseEvent&) override { hot = false; repaint(); }
        void mouseUp (const juce::MouseEvent& e) override
        {
            if (e.mouseWasClicked() && getLocalBounds().contains (e.getPosition()) && onClick) onClick();
        }

    private:
        bool hot = false;
    };

    // icon-only header button (chevrons, gear, dots, link)
    class IconButton : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        std::function<void()> onClick;
        std::function<void (juce::Graphics&, juce::Rectangle<float>, juce::Colour)> draw;
        juce::Colour colour { col::cream2 };

        IconButton() { setMouseCursor (juce::MouseCursor::PointingHandCursor); }

        void paint (juce::Graphics& g) override
        {
            if (draw) draw (g, getLocalBounds().toFloat(), hot ? colour.brighter (0.4f) : colour);
        }
        void mouseEnter (const juce::MouseEvent&) override { hot = true; repaint(); }
        void mouseExit (const juce::MouseEvent&) override { hot = false; repaint(); }
        void mouseUp (const juce::MouseEvent& e) override
        {
            if (e.mouseWasClicked() && getLocalBounds().contains (e.getPosition()) && onClick) onClick();
        }

    private:
        bool hot = false;
    };

    // a lamp whose state comes from a function, polled by the editor timer
    class LampView : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        std::function<bool()> state;
        bool onMetal = true;

        void update() { const bool s = state ? state() : false; if (s != on) { on = s; repaint(); } }

        void paint (juce::Graphics& g) override
        {
            auto c = getLocalBounds().toFloat().getCentre();
            if (onMetal && ! on)
            {
                g.setColour (juce::Colours::white.withAlpha (0.4f));
                g.fillEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre (c.translated (0, 1.0f)));
            }
            lamp (g, c, 9.0f, on);
        }

    private:
        bool on = false;
    };
}
