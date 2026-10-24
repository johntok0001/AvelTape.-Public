#pragma once

#include "Widgets.h"
#include "../PluginProcessor.h"

namespace avel::tape::ui
{
    //==============================================================================
    // INPUT / OUTPUT LED meter module (132 x 262).
    class MeterView : public juce::Component
    {
    public:
        MeterView (AvelTapeProcessor& p, bool input) : proc (p), isInput (input) { setOpaque (false); }

        void update (float dt)
        {
            auto* src = isInput ? proc.inPeak : proc.outPeak;
            bool changed = false;
            for (int c = 0; c < 2; ++c)
            {
                const float peak = src[c].exchange (0.0f);
                const float db = juce::Decibels::gainToDecibels (peak, -100.0f);
                const float nd = juce::jmax (db, disp[c] - 26.0f * dt);
                if (std::abs (nd - disp[c]) > 0.05f) { disp[c] = nd; changed = true; }
            }
            const bool sig = juce::jmax (disp[0], disp[1]) > -60.0f;
            if (changed || sig != signal) { signal = sig; repaint(); }
        }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();   // the dark well itself is drawn by the backdrop

            text (g, isInput ? "INPUT" : "OUTPUT", font (F::montMedium, 11.0f, 0.2f), { 0, 14, r.getWidth(), 13 }, juce::Justification::centred, col::cream2);

            // LED housing
            const float bx = 28.0f, by = 56.0f;
            auto housing = juce::Rectangle<float> (bx, by, 36.0f, 138.5f);
            g.setColour (juce::Colour (0xff080807));
            g.fillRoundedRectangle (housing, 4.0f);
            innerShadow (g, housing, 4.0f, juce::Colours::black, 3, { 0, 1 });

            static const float thr[14] = { 6, 4, 2, 0, -2, -4, -6, -8, -10, -12, -15, -18, -21, -24 };
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 14; ++i)
                {
                    const bool lit = disp[c] >= thr[i];
                    const juce::Colour on  = i < 2 ? juce::Colour (0xffff3b2a) : i < 4 ? juce::Colour (0xffff7a24) : juce::Colour (0xffffd23a);
                    const juce::Colour off = i < 2 ? juce::Colour (0xff3a1310) : i < 4 ? juce::Colour (0xff3a1d0e) : juce::Colour (0xff3a3211);
                    auto seg = juce::Rectangle<float> (bx + 4.0f + (float) c * 16.0f, by + 4.0f + (float) i * 9.5f, 12.0f, 7.0f);
                    if (lit)
                    {
                        g.setColour (on.withAlpha (0.35f));
                        g.fillRoundedRectangle (seg.expanded (2.0f), 3.0f);
                    }
                    g.setColour (lit ? on : off);
                    g.fillRoundedRectangle (seg, 1.5f);
                }

            // scale
            const char* labels[] = { "+6", "0", "\xe2\x88\x92" "6", "\xe2\x88\x92" "12", "\xe2\x88\x92" "24" };
            const int idx[] = { 0, 3, 6, 9, 13 };
            for (int k = 0; k < 5; ++k)
            {
                const float y = by + 4.0f + (float) idx[k] * 9.5f + 3.5f;
                g.setColour (juce::Colour (0xff6c665d));
                g.fillRect (bx + 42.0f, y - 0.5f, 5.0f, 1.0f);
                text (g, juce::String::fromUTF8 (labels[k]), font (F::interMedium, 10.5f), { bx + 51.0f, y - 7.0f, 30.0f, 14.0f },
                      juce::Justification::centredLeft, col::label);
            }

            // IN / OUT pill
            auto pill = juce::Rectangle<float> (0, 0, 70, 26).withCentre ({ r.getCentreX(), r.getBottom() - 25.0f });
            g.setColour (juce::Colour (0xff0b0a09));
            g.fillRoundedRectangle (pill, 13.0f);
            g.setColour (juce::Colour (0xff2e2b27));
            g.drawRoundedRectangle (pill.reduced (0.5f), 13.0f, 1.0f);
            lamp (g, { pill.getX() + 22.0f, pill.getCentreY() }, 8.0f, signal);
            text (g, isInput ? "IN" : "OUT", font (F::montMedium, 11.0f, 0.14f), pill.withTrimmedLeft (32).withTrimmedRight (8),
                  juce::Justification::centredLeft, col::cream2);
        }

    private:
        AvelTapeProcessor& proc;
        bool isInput;
        float disp[2] { -100.0f, -100.0f };
        bool signal = false;
    };

    //==============================================================================
    // Tape transport module (864 x 262): head pattern, lamps, reels, echo trail, heads, ruler.
    class TransportView : public juce::Component
    {
    public:
        TransportView (AvelTapeProcessor& p)
            : proc (p), heads (*p.apvts.getParameter (pid::heads), headPatternNames(), false)
        {
            addAndMakeVisible (heads);
            heads.setTooltip ("Which playback heads you hear: I = delay time, II = 2x, III = 3x");
            buildReel();
        }

        void resized() override
        {
            const auto f = font (F::montMedium, 11.0f, 0.18f);
            labelW = textWidth (f, "HEAD PATTERN");
            heads.setBounds ((int) (64.0f + labelW + 20.0f), 11, 6 * 58 + 5 * 2 + 4, 32);
        }

        void update (float dt)
        {
            const float t = proc.effectiveTimeMs.load();
            const float rps = juce::jlimit (0.04f, 1.6f, 0.32f * 375.0f / juce::jmax (20.0f, t));
            reelAngle += dt * rps * juce::MathConstants<float>::twoPi;
            if (reelAngle > juce::MathConstants<float>::twoPi) reelAngle -= juce::MathConstants<float>::twoPi;

            const float in = proc.inputPeak.exchange (0.0f);
            const float echo = proc.echoPeak.exchange (0.0f);
            inAct = juce::jmax (in > 0.003f ? 1.0f : 0.0f, inAct - dt * 2.5f);
            echoAct = juce::jmax (juce::jlimit (0.0f, 1.0f, echo * 4.0f), echoAct - dt * 1.8f);
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();   // the dark well itself is drawn by the backdrop

            // top row
            text (g, "HEAD PATTERN", font (F::montMedium, 11.0f, 0.18f), { 64.0f, 10.0f, labelW + 10.0f, 34.0f },
                  juce::Justification::centredLeft, col::label);
            {
                const auto f = font (F::montMedium, 9.5f, 0.16f);
                float x = r.getRight() - 22.0f;
                auto lampItem = [&] (const juce::String& name, bool on, bool sep)
                {
                    lamp (g, { x - 4.5f, 27.0f }, 9.0f, on);
                    x -= 9.0f + 12.0f;
                    const float w = textWidth (f, name);
                    text (g, name, f, { x - w, 20.0f, w + 4.0f, 14.0f }, juce::Justification::centredLeft, col::label);
                    x -= w + 12.0f;
                    if (sep)
                    {
                        g.setColour (juce::Colours::white.withAlpha (0.12f));
                        g.fillRect (x, 20.0f, 1.0f, 14.0f);
                        x -= 13.0f;
                    }
                };
                lampItem ("MOTOR", true, true);
                lampItem ("HEADS", echoAct > 0.15f, true);
                lampItem ("TAPE", inAct > 0.15f, false);
            }

            juce::Graphics::ScopedSaveState ss (g);
            g.addTransform (juce::AffineTransform::translation (12.0f, 50.0f));
            drawTransport (g);
        }

    private:
        void buildReel()
        {
            const int px = 300;
            reel = juce::Image (juce::Image::ARGB, px, px, true);
            juce::Graphics g (reel);
            const float s = (float) px / 140.0f;
            g.addTransform (juce::AffineTransform::scale (s));
            const juce::Point<float> c (70.0f, 70.0f);

            juce::ColourGradient rg (juce::Colour (0xffd2ccc1), 70.0f - 12.0f, 70.0f - 16.0f, juce::Colour (0xff3a3732), 70.0f + 70.0f, 70.0f + 70.0f, true);
            rg.addColour (0.5, juce::Colour (0xff8a857c));
            rg.addColour (0.8, juce::Colour (0xff5e5a53));
            g.setGradientFill (rg);
            g.fillEllipse (0, 0, 140, 140);
            g.setColour (juce::Colour (0xff2a2723));
            g.drawEllipse (1, 1, 138, 138, 2.0f);
            // concentric machining rings
            juce::Random rnd (5);
            for (float rr = 33.0f; rr <= 68.0f; rr += 1.5f)
            {
                const bool light = rnd.nextBool();
                g.setColour (light ? juce::Colours::white.withAlpha (0.08f + rnd.nextFloat() * 0.1f) : juce::Colours::black.withAlpha (0.05f + rnd.nextFloat() * 0.08f));
                g.drawEllipse (juce::Rectangle<float> (rr * 2, rr * 2).withCentre (c), 0.7f);
            }
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.drawEllipse (juce::Rectangle<float> (60, 60).withCentre (c), 1.2f);
            // window hole
            g.setColour (juce::Colour (0xff141210));
            g.fillEllipse (juce::Rectangle<float> (20, 20).withCentre (c.translated (24, 28)));
            g.setColour (juce::Colours::black.withAlpha (0.6f));
            g.drawEllipse (juce::Rectangle<float> (20, 20).withCentre (c.translated (24, 28)), 1.5f);
            g.setColour (juce::Colours::white.withAlpha (0.25f));
            g.drawEllipse (juce::Rectangle<float> (21, 21).withCentre (c.translated (24, 29)), 0.6f);
            // hub
            g.setColour (juce::Colour (0xff151312));
            g.fillEllipse (juce::Rectangle<float> (32, 32).withCentre (c));
            g.setColour (juce::Colour (0xff3a3632));
            g.drawEllipse (juce::Rectangle<float> (32, 32).withCentre (c), 2.0f);
            juce::ColourGradient hub (juce::Colour (0xfffbf8f1), c.x - 2, c.y - 3, juce::Colour (0xff3b3833), c.x + 7, c.y + 7, true);
            g.setGradientFill (hub);
            g.fillEllipse (juce::Rectangle<float> (14, 14).withCentre (c));
            g.setColour (juce::Colour (0xff151312));
            g.fillEllipse (juce::Rectangle<float> (5, 5).withCentre (c));
        }

        void drawReel (juce::Graphics& g, juce::Point<float> c, float angle)
        {
            const float sc = 140.0f / (float) reel.getWidth();
            g.drawImageTransformed (reel, juce::AffineTransform::translation (-(float) reel.getWidth() * 0.5f, -(float) reel.getHeight() * 0.5f)
                                              .scaled (sc).rotated (angle).translated (c));
        }

        static void chromeCircle (juce::Graphics& g, juce::Point<float> c, float r)
        {
            juce::ColourGradient cg (juce::Colour (0xfffbf8f1), c.x - r * 0.3f, c.y - r * 0.4f, juce::Colour (0xff3b3833), c.x + r, c.y + r, true);
            cg.addColour (0.55, juce::Colour (0xff9b978e));
            g.setGradientFill (cg);
            g.fillEllipse (juce::Rectangle<float> (r * 2, r * 2).withCentre (c));
        }

        static void rubberCircle (juce::Graphics& g, juce::Point<float> c, float r)
        {
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3c3834), c.x - r * 0.3f, c.y - r * 0.3f, juce::Colour (0xff0c0b0a), c.x + r, c.y + r, true));
            g.fillEllipse (juce::Rectangle<float> (r * 2, r * 2).withCentre (c));
        }

        static void metalBlock (juce::Graphics& g, juce::Rectangle<float> b, bool vertical)
        {
            if (vertical)
            {
                juce::ColourGradient cg (juce::Colour (0xff4e4a44), b.getX(), 0, juce::Colour (0xff55514b), b.getRight(), 0, false);
                cg.addColour (0.45, juce::Colour (0xffc9c4ba));
                g.setGradientFill (cg);
            }
            else
            {
                juce::ColourGradient cg (juce::Colour (0xffd8d3c9), 0, b.getY(), juce::Colour (0xff4e4a44), 0, b.getBottom(), false);
                cg.addColour (0.5, juce::Colour (0xff8e897f));
                g.setGradientFill (cg);
            }
            g.fillRoundedRectangle (b, 2.5f);
        }

        // echo pulses within the window, from the current settings
        struct Pulse { double beats; float amp; };

        std::vector<Pulse> computePulses (double& rangeBeats) const
        {
            const double bpm = juce::jlimit (20.0, 400.0, proc.getBpm());
            const double beatMs = 60000.0 / bpm;
            const double t = proc.computeTimeMs();
            const int pattern = (int) proc.apvts.getRawParameterValue (pid::heads)->load();
            const float fb = proc.apvts.getRawParameterValue (pid::feedback)->load() / 100.0f;
            const auto hs = headsForPattern (pattern);
            int active = 0, longest = 1;
            for (int h = 0; h < 3; ++h) if (hs[(size_t) h]) { ++active; longest = h + 1; }
            active = juce::jmax (1, active);

            rangeBeats = 2.0;
            while (rangeBeats < 16.0 && rangeBeats * beatMs < t * longest * 2.6) rangeBeats *= 2.0;
            const double rangeMs = rangeBeats * beatMs;

            const float gE = 1.0f / std::sqrt ((float) active);
            const float fbN = fb / (float) active;
            std::vector<Pulse> out;
            std::vector<std::pair<double, float>> rec { { 0.0, 1.0f } };
            for (int iter = 0; iter < 400 && ! rec.empty(); ++iter)
            {
                auto e = rec.front();
                rec.erase (rec.begin());
                for (int h = 0; h < 3; ++h)
                {
                    if (! hs[(size_t) h]) continue;
                    const double tt = e.first + t * (h + 1);
                    if (tt > rangeMs) continue;
                    const float a = e.second;
                    bool merged = false;
                    for (auto& p : out)
                        if (std::abs (p.beats * beatMs - tt) < 0.5) { p.amp += a * gE; merged = true; break; }
                    if (! merged) out.push_back ({ tt / beatMs, a * gE });
                    if (a * fbN > 0.02f && rec.size() < 64)
                        rec.push_back ({ tt, juce::jmin (4.0f, a * fbN) });
                }
            }
            return out;
        }

        static juce::String beatLabel (double b)
        {
            const int whole = (int) std::floor (b + 1e-6);
            const double frac = b - whole;
            juce::String f;
            if (std::abs (frac - 0.25) < 1e-3) f = "1/4";
            else if (std::abs (frac - 0.5) < 1e-3) f = "1/2";
            else if (std::abs (frac - 0.75) < 1e-3) f = "3/4";
            if (whole == 0) return f.isEmpty() ? "0" : f;
            return f.isEmpty() ? juce::String (whole) : juce::String (whole) + " " + f;
        }

        void drawTransport (juce::Graphics& g)
        {
            // reel housings
            for (float x : { 2.0f, 690.0f })
            {
                auto h = juce::Rectangle<float> (x, 18.0f, 148.0f, 156.0f);
                g.setColour (juce::Colour (0xff181614));
                g.fillRoundedRectangle (h, 12.0f);
                g.setColour (juce::Colour (0xff262320));
                g.drawRoundedRectangle (h.reduced (0.5f), 12.0f, 1.0f);
            }

            // window
            auto win = juce::Rectangle<float> (168.0f, 16.0f, 504.0f, 124.0f);
            {
                juce::ColourGradient wg (juce::Colour (0xff1d150d), 0, win.getY(), juce::Colour (0xff120c07), 0, win.getBottom(), false);
                wg.addColour (0.5, juce::Colour (0xff2a1c0f));
                g.setGradientFill (wg);
                g.fillRoundedRectangle (win, 3.0f);
                g.setColour (juce::Colour (0x0dffb464));
                for (float x = 180.0f; x < 672.0f; x += 14.0f) g.fillRect (x, 18.0f, 1.0f, 120.0f);
                juce::ColourGradient haze (juce::Colour (0xffff9a2e).withAlpha (0.22f * (0.6f + 0.4f * echoAct)), 319.0f, 78.0f,
                                           juce::Colour (0xffff9a2e).withAlpha (0.0f), 319.0f + 300.0f, 78.0f, true);
                g.setGradientFill (haze);
                g.fillRoundedRectangle (win, 3.0f);
                g.setColour (juce::Colour (0xff2c2722));
                g.drawRoundedRectangle (win.reduced (0.5f), 3.0f, 1.0f);
            }

            // echo trail
            {
                juce::Graphics::ScopedSaveState s (g);
                g.reduceClipRegion (win.reduced (2.0f).toNearestInt());
                double range = 2.0;
                auto pulses = computePulses (range);
                float maxAmp = 0.0001f;
                for (auto& p : pulses) maxAmp = juce::jmax (maxAmp, p.amp);
                const float norm = juce::jmin (1.0f, 1.0f / maxAmp) * 104.0f;
                const float live = 0.55f + 0.45f * echoAct;
                int idx = 0;
                for (auto& p : pulses)
                {
                    const float x = 180.0f + (float) (p.beats / range) * 480.0f;
                    const float h = juce::jlimit (3.0f, 112.0f, p.amp * norm);
                    const juce::Line<float> ln (x, 78.0f - h * 0.5f, x, 78.0f + h * 0.5f);
                    const float strong = idx < 3 ? 1.0f : 0.75f;
                    g.setColour (juce::Colour (0xffff8a1e).withAlpha (0.10f * live));
                    g.drawLine (ln, 14.0f);
                    g.setColour (juce::Colour (0xffff8a1e).withAlpha (0.22f * live));
                    g.drawLine (ln, 7.0f);
                    g.setColour (juce::Colour (0xffffb24d).withAlpha (0.65f * live * strong));
                    g.drawLine (ln, 3.0f);
                    g.setColour (juce::Colour (0xffffe2b0).withAlpha (strong * (0.6f + 0.4f * live)));
                    g.drawLine (ln, 1.5f);
                    ++idx;
                }
                rulerRange = range;
            }

            // posts
            for (float x : { 174.0f, 654.0f })
            {
                metalBlock (g, { x, 6.0f, 12.0f, 150.0f }, true);
                metalBlock (g, { x - 4.0f, 4.0f, 20.0f, 16.0f }, false);
            }

            // reels
            drawReel (g, { 76.0f, 96.0f }, reelAngle);
            drawReel (g, { 764.0f, 96.0f }, reelAngle + 1.3f);

            // tape
            {
                juce::Path tape;
                tape.startNewSubPath (60.0f, 166.0f);
                tape.lineTo (150.0f, 153.0f);
                tape.lineTo (690.0f, 153.0f);
                tape.lineTo (780.0f, 166.0f);
                g.setColour (juce::Colour (0xff6e4325));
                g.strokePath (tape, juce::PathStrokeType (6.0f, juce::PathStrokeType::mitered, juce::PathStrokeType::butt));
                g.setColour (juce::Colour (0x4dffbe82));
                g.fillRect (150.0f, 150.3f, 540.0f, 1.0f);
            }

            // heads (I, II, III playback + record)
            const auto hs = headsForPattern ((int) proc.apvts.getRawParameterValue (pid::heads)->load());
            const float hx[] = { 250.0f, 400.0f, 530.0f, 640.0f };
            for (int i = 0; i < 4; ++i)
            {
                auto b = juce::Rectangle<float> (40.0f, 34.0f).withCentre ({ hx[i], 137.0f });
                metalBlock (g, b, false);
                g.setColour (juce::Colours::white.withAlpha (0.3f));
                g.fillRect (b.getX(), b.getY() + 8.0f, b.getWidth(), 1.0f);
                g.setColour (juce::Colours::black.withAlpha (0.5f));
                g.fillRect (hx[i] - 0.5f, b.getY() + 10.0f, 1.0f, 10.0f);
                if (i < 3 && hs[(size_t) i])
                {
                    const float a = 0.55f + 0.45f * echoAct;
                    juce::ColourGradient glow (juce::Colour (0xffffa640).withAlpha (0.9f * a), hx[i], 158.0f,
                                               juce::Colour (0xffffa640).withAlpha (0.0f), hx[i] + 16.0f, 158.0f, true);
                    g.setGradientFill (glow);
                    g.fillEllipse (juce::Rectangle<float> (32.0f, 14.0f).withCentre ({ hx[i], 157.0f }));
                }
            }

            // capstan, guide rollers
            rubberCircle (g, { 690.0f, 150.0f }, 13.0f);
            chromeCircle (g, { 690.0f, 150.0f }, 5.0f);
            for (float x : { 24.0f, 816.0f })
            {
                rubberCircle (g, { x, 178.0f }, 10.0f);
                g.setColour (juce::Colour (0xff3a3632));
                g.drawEllipse (juce::Rectangle<float> (20.0f, 20.0f).withCentre ({ x, 178.0f }), 1.0f);
            }

            // ruler
            auto ruler = juce::Rectangle<float> (150.0f, 166.0f, 540.0f, 26.0f);
            g.setColour (juce::Colour (0xff0b0a09));
            g.fillRoundedRectangle (ruler, 2.0f);
            g.setColour (juce::Colour (0xff2a2622));
            g.drawRoundedRectangle (ruler.reduced (0.5f), 2.0f, 1.0f);
            g.setColour (juce::Colour (0xff8a8378));
            for (int k = 0; k <= 16; ++k)
                g.fillRect (180.0f + (float) k * 30.0f - 0.5f, 166.0f, 1.0f, k % 2 == 0 ? 8.0f : 5.0f);
            const double fr[] = { 0.0, 0.125, 0.25, 0.375, 0.5, 0.75, 1.0 };
            const auto lf = font (F::interMedium, 10.5f);
            for (double f : fr)
            {
                const float x = 180.0f + (float) f * 480.0f;
                text (g, beatLabel (f * rulerRange), lf, { x - 30.0f, 176.0f, 60.0f, 14.0f }, juce::Justification::centred, juce::Colour (0xffcfc8ba));
            }
        }

        AvelTapeProcessor& proc;
        Segmented heads;
        juce::Image reel;
        float reelAngle = 0.0f, labelW = 90.0f;
        float inAct = 0.0f, echoAct = 0.0f;
        double rulerRange = 2.0;
    };
}
