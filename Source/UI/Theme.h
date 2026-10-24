#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "BinaryData.h"
#include <map>

// Avel Tape look: dark lacquer, brushed aluminium, amber light.
namespace avel::tape::ui
{
    namespace col
    {
        const juce::Colour amber     { 0xffffb347 };
        const juce::Colour amberHi   { 0xffffc36b };
        const juce::Colour amberDeep { 0xffff962a };
        const juce::Colour amberLine { 0xffd9933a };
        const juce::Colour cream     { 0xffece6da };
        const juce::Colour cream2    { 0xffd8d2c6 };
        const juce::Colour label     { 0xffbdb5a6 };
        const juce::Colour ink       { 0xff24211e };
        const juce::Colour inkSoft   { 0xff2a2622 };
        const juce::Colour segBg     { 0xff090908 };
        const juce::Colour segBorder { 0xff2c2925 };
        const juce::Colour hairline  { 0x1affffff };
    }

    enum class F { interRegular, interMedium, interSemi, montMedium, montSemi };

    inline juce::Typeface::Ptr typeface (F f)
    {
        static juce::Typeface::Ptr ir = juce::Typeface::createSystemTypefaceFor (BinaryData::InterRegular_otf, BinaryData::InterRegular_otfSize);
        static juce::Typeface::Ptr im = juce::Typeface::createSystemTypefaceFor (BinaryData::InterMedium_otf, BinaryData::InterMedium_otfSize);
        static juce::Typeface::Ptr is = juce::Typeface::createSystemTypefaceFor (BinaryData::InterSemiBold_otf, BinaryData::InterSemiBold_otfSize);
        static juce::Typeface::Ptr mm = juce::Typeface::createSystemTypefaceFor (BinaryData::MontserratMedium_otf, BinaryData::MontserratMedium_otfSize);
        static juce::Typeface::Ptr ms = juce::Typeface::createSystemTypefaceFor (BinaryData::MontserratSemiBold_otf, BinaryData::MontserratSemiBold_otfSize);
        switch (f)
        {
            case F::interRegular: return ir;
            case F::interMedium:  return im;
            case F::interSemi:    return is;
            case F::montMedium:   return mm;
            case F::montSemi:     return ms;
        }
        return im;
    }

    // `size` is the CSS-like em size in px, `trackingEm` the CSS letter-spacing in em
    inline juce::Font font (F f, float size, float trackingEm = 0.0f)
    {
        juce::Font fo (juce::FontOptions (typeface (f)).withPointHeight (size));
        if (std::abs (trackingEm) > 1.0e-4f)
            fo.setExtraKerningFactor (trackingEm * size / fo.getHeight());
        return fo;
    }

    inline float textWidth (const juce::Font& f, const juce::String& s)
    {
        return juce::GlyphArrangement::getStringWidth (f, s);
    }

    // Draws tracked text; when centred, compensates for the trailing letter-space.
    inline void text (juce::Graphics& g, const juce::String& s, const juce::Font& f, juce::Rectangle<float> r,
                      juce::Justification j, juce::Colour c)
    {
        g.setFont (f);
        g.setColour (c);
        if (j.testFlags (juce::Justification::horizontallyCentred))
            r = r.translated (f.getExtraKerningFactor() * f.getHeight() * 0.5f, 0.0f);
        g.drawText (s, r, j, false);
    }

    // engraved text on metal: light line underneath
    inline void engraved (juce::Graphics& g, const juce::String& s, const juce::Font& f, juce::Rectangle<float> r,
                          juce::Justification j, juce::Colour c = col::ink)
    {
        text (g, s, f, r.translated (0, 1.0f), j, juce::Colours::white.withAlpha (0.42f));
        text (g, s, f, r, j, c);
    }

    //==============================================================================
    inline juce::Image makeNoiseImage (int w, int h, juce::int64 seed)
    {
        juce::Image img (juce::Image::ARGB, w, h, true);
        juce::Random r (seed);
        juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
            {
                const float v = r.nextFloat() * 2.0f - 1.0f;
                bd.setPixelColour (x, y, v > 0 ? juce::Colours::white.withAlpha (v * 0.5f) : juce::Colours::black.withAlpha (-v * 0.5f));
            }
        return img;
    }

    // horizontal brushed streaks
    inline juce::Image makeBrushedImage (int w, int h, juce::int64 seed)
    {
        juce::Image img (juce::Image::ARGB, w, h, true);
        juce::Random r (seed);
        juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
        std::vector<float> rows ((size_t) h);
        for (auto& v : rows) v = r.nextFloat() * 2.0f - 1.0f;
        for (int y = 0; y < h; ++y)
        {
            float streak = 0.0f;
            for (int x = 0; x < w; ++x)
            {
                if (r.nextInt (90) == 0) streak = (r.nextFloat() * 2.0f - 1.0f) * 0.6f;
                streak *= 0.985f;
                const float v = juce::jlimit (-1.0f, 1.0f, rows[(size_t) y] * 0.55f + streak + (r.nextFloat() - 0.5f) * 0.25f);
                bd.setPixelColour (x, y, v > 0 ? juce::Colours::white.withAlpha (v * 0.16f) : juce::Colours::black.withAlpha (-v * 0.11f));
            }
        }
        return img;
    }

    inline const juce::Image& grainImage()    { static juce::Image i = makeNoiseImage (192, 192, 42); return i; }
    inline const juce::Image& brushedImage()  { static juce::Image i = makeBrushedImage (640, 160, 7); return i; }

    inline void tile (juce::Graphics& g, const juce::Image& img, juce::Rectangle<float> r, float alpha)
    {
        juce::Graphics::ScopedSaveState s (g);
        g.setTiledImageFill (img, 0, 0, alpha);
        g.fillRect (r);
    }

    // brushed metal panel (gradient + streaks + soft top light)
    inline void metal (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour top, juce::Colour mid, juce::Colour bot)
    {
        juce::ColourGradient grad (top, r.getX(), r.getY(), bot, r.getX(), r.getBottom(), false);
        grad.addColour (0.45, mid);
        g.setGradientFill (grad);
        g.fillRect (r);
        tile (g, brushedImage(), r, 1.0f);
        juce::ColourGradient light (juce::Colours::white.withAlpha (0.2f), r.getCentreX(), r.getY(),
                                    juce::Colours::white.withAlpha (0.0f), r.getCentreX(), r.getY() + r.getHeight() * 0.6f, true);
        g.setGradientFill (light);
        g.fillRect (r);
    }

    inline void innerShadow (juce::Graphics& g, juce::Rectangle<float> r, float radius, juce::Colour c, int blur, juce::Point<int> offset)
    {
        juce::Graphics::ScopedSaveState s (g);
        juce::Path inner;
        inner.addRoundedRectangle (r, radius);
        g.reduceClipRegion (inner);
        juce::Path frame;
        frame.addRectangle (r.expanded ((float) blur * 3.0f));
        frame.addRoundedRectangle (r, radius);
        frame.setUsingNonZeroWinding (false);
        juce::DropShadow (c, blur, offset).drawForPath (g, frame);
    }

    // dark recessed module on metal (meters, transport, value pills, bottom strip)
    inline void darkWell (juce::Graphics& g, juce::Rectangle<float> r, float radius,
                          juce::Colour top = juce::Colour (0xff1a1918), juce::Colour bot = juce::Colour (0xff0e0d0c), float shadow = 1.0f)
    {
        g.setColour (juce::Colours::white.withAlpha (0.45f * shadow));
        g.fillRoundedRectangle (r.translated (0, 1.0f), radius);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (r.expanded (1.0f), radius + 1.0f);
        g.setGradientFill (juce::ColourGradient (top, r.getX(), r.getY(), bot, r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, radius);
        innerShadow (g, r, radius, juce::Colours::black.withAlpha (0.8f), 8, { 0, 2 });
    }

    inline void screw (juce::Graphics& g, juce::Point<float> c, float r, float angleDeg)
    {
        g.setColour (juce::Colours::white.withAlpha (0.4f));
        g.fillEllipse (juce::Rectangle<float> (r * 2, r * 2).withCentre (c.translated (0, 1)));
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff2eee6), c.x - r * 0.4f, c.y - r * 0.4f, juce::Colour (0xff3a3733), c.x + r, c.y + r, true));
        g.fillEllipse (juce::Rectangle<float> (r * 2, r * 2).withCentre (c));
        juce::Path slot;
        slot.addRectangle (-r * 0.85f, -r * 0.15f, r * 1.7f, r * 0.3f);
        g.setColour (juce::Colour (0xcc1e1c19));
        g.fillPath (slot, juce::AffineTransform::rotation (juce::degreesToRadians (angleDeg)).translated (c));
    }

    inline void lamp (juce::Graphics& g, juce::Point<float> c, float d, bool on, float glow = 1.0f)
    {
        const float r = d * 0.5f;
        if (on && glow > 0.0f)
        {
            juce::ColourGradient halo (col::amber.withAlpha (0.55f * glow), c.x, c.y, col::amber.withAlpha (0.0f), c.x + r * 2.6f, c.y, true);
            g.setGradientFill (halo);
            g.fillEllipse (juce::Rectangle<float> (r * 5.2f, r * 5.2f).withCentre (c));
        }
        juce::ColourGradient grad (on ? juce::Colour (0xfffff1d0) : juce::Colour (0xff5a5650), c.x - r * 0.35f, c.y - r * 0.4f,
                                   on ? juce::Colour (0xffb8601c) : juce::Colour (0xff1d1c1a), c.x + r, c.y + r, true);
        grad.addColour (0.45, on ? col::amber : juce::Colour (0xff35322e));
        g.setGradientFill (grad);
        g.fillEllipse (juce::Rectangle<float> (d, d).withCentre (c));
    }

    // stroke icon from a 40x24 design box
    inline void strokeIcon (juce::Graphics& g, const juce::Path& p, juce::Rectangle<float> r, juce::Colour c, float thick = 1.7f)
    {
        const float s = juce::jmin (r.getWidth() / 40.0f, r.getHeight() / 24.0f);
        auto t = juce::AffineTransform::scale (s).translated (r.getCentreX() - 20.0f * s, r.getCentreY() - 12.0f * s);
        g.setColour (c);
        g.strokePath (p, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), t);
    }

    //==============================================================================
    // Brushed aluminium knob cap (conic highlights), cached per pixel size.
    inline juce::Image capImage (int px)
    {
        static std::map<int, juce::Image> cache;
        auto it = cache.find (px);
        if (it != cache.end()) return it->second;

        juce::Image img (juce::Image::ARGB, px, px, true);
        {
            juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
            juce::Random rnd (99);
            std::vector<float> streak (720);
            for (auto& s : streak) s = (rnd.nextFloat() - 0.5f) * 0.10f;
            const float stops[] = { 0.86f, 0.56f, 0.92f, 0.52f, 0.84f, 0.54f, 0.91f, 0.50f, 0.86f };
            const float c = (float) px * 0.5f;
            for (int y = 0; y < px; ++y)
                for (int x = 0; x < px; ++x)
                {
                    const float dx = (float) x + 0.5f - c, dy = (float) y + 0.5f - c;
                    const float dist = std::sqrt (dx * dx + dy * dy);
                    if (dist > c) continue;
                    float a = std::atan2 (dx, -dy) - juce::degreesToRadians (20.0f);
                    while (a < 0) a += juce::MathConstants<float>::twoPi;
                    const float pos = a / juce::MathConstants<float>::twoPi * 8.0f;
                    const int i = juce::jlimit (0, 7, (int) pos);
                    const float t = pos - (float) i;
                    const float sm = t * t * (3.0f - 2.0f * t);
                    float v = stops[i] + (stops[i + 1] - stops[i]) * sm;
                    v += streak[(size_t) ((int) (a / juce::MathConstants<float>::twoPi * 720.0f) % 720)];
                    v += (rnd.nextFloat() - 0.5f) * 0.03f;
                    // darker toward the rim
                    v *= 1.0f - 0.18f * juce::jlimit (0.0f, 1.0f, (dist / c - 0.75f) * 4.0f);
                    v = juce::jlimit (0.0f, 1.0f, v);
                    const float aa = juce::jlimit (0.0f, 1.0f, c - dist);
                    auto col = juce::Colour::fromFloatRGBA (v * 0.98f, v * 0.96f, v * 0.92f, aa);
                    bd.setPixelColour (x, y, col);
                }
        }
        cache[px] = img;
        return img;
    }

    //==============================================================================
    class LookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        LookAndFeel()
        {
            setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1c1b19));
            setColour (juce::PopupMenu::textColourId, col::cream);
            setColour (juce::PopupMenu::headerTextColourId, col::label);
            setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff3a2a16));
            setColour (juce::PopupMenu::highlightedTextColourId, col::amber);
            setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xf0201f1d));
            setColour (juce::TooltipWindow::textColourId, col::cream);
            setColour (juce::TooltipWindow::outlineColourId, juce::Colour (0xff3a3631));
            setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff0d0c0b));
            setColour (juce::TextEditor::textColourId, col::cream);
            setColour (juce::TextEditor::highlightColourId, col::amberDeep.withAlpha (0.35f));
            setColour (juce::TextEditor::outlineColourId, col::amberLine);
            setColour (juce::TextEditor::focusedOutlineColourId, col::amberLine);
            setColour (juce::CaretComponent::caretColourId, col::amber);
            setColour (juce::AlertWindow::backgroundColourId, juce::Colour (0xff1c1b19));
            setColour (juce::AlertWindow::textColourId, col::cream);
            setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2a2724));
            setColour (juce::TextButton::textColourOffId, col::cream);
            setColour (juce::ResizableWindow::backgroundColourId, juce::Colour (0xff1c1b19));
        }

        juce::Font getPopupMenuFont() override { return font (F::interRegular, 14.5f); }
        juce::Font getAlertWindowMessageFont() override { return font (F::interRegular, 14.0f); }
        juce::Font getTextButtonFont (juce::TextButton&, int) override { return font (F::interMedium, 13.0f); }

        juce::Rectangle<int> getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea) override
        {
            const auto f = font (F::interRegular, 13.0f);
            const int w = (int) textWidth (f, tipText) + 22;
            const int h = 26;
            return juce::Rectangle<int> (screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 24,
                                         screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 6) : screenPos.y + 6, w, h)
                       .constrainedWithin (parentArea);
        }

        void drawTooltip (juce::Graphics& g, const juce::String& t, int w, int h) override
        {
            auto r = juce::Rectangle<float> ((float) w, (float) h);
            g.setColour (findColour (juce::TooltipWindow::backgroundColourId));
            g.fillRoundedRectangle (r, 6.0f);
            g.setColour (findColour (juce::TooltipWindow::outlineColourId));
            g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
            g.setColour (col::cream);
            g.setFont (font (F::interRegular, 13.0f));
            g.drawText (t, r.reduced (10, 0), juce::Justification::centredLeft, true);
        }

        void drawPopupMenuBackground (juce::Graphics& g, int w, int h) override
        {
            g.fillAll (findColour (juce::PopupMenu::backgroundColourId));
            g.setColour (juce::Colour (0xff3a3631));
            g.drawRect (0, 0, w, h);
        }
    };
}
