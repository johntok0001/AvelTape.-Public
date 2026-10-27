// Offline checks for Avel Tape: DSP behaviour, presets/state, CPU, and editor snapshots (PNG).
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <iostream>

using namespace avel::tape;

static int failures = 0;
static void check (bool ok, const juce::String& what)
{
    std::cout << (ok ? "  ok    " : "  FAIL  ") << what << std::endl;
    if (! ok) ++failures;
}

static void set (AvelTapeProcessor& p, const char* id, float plain)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
}

static void cleanSetup (AvelTapeProcessor& p)
{
    set (p, pid::sync, 0); set (p, pid::time, 250); set (p, pid::heads, 0); set (p, pid::feedback, 0);
    set (p, pid::mix, 50); set (p, pid::wetOnly, 0); set (p, pid::wow, 0); set (p, pid::flutter, 0);
    set (p, pid::flutterMod, 0); set (p, pid::pingPong, 0); set (p, pid::saturation, 0); set (p, pid::tone, 100); set (p, pid::reverb, 0);
    set (p, pid::tapeMode, 0); set (p, pid::tapeType, 0); set (p, pid::noise, -90); set (p, pid::age, 0);
}

// renders `seconds` of output for an impulse (or the given generator) at 48 kHz
static juce::AudioBuffer<float> render (AvelTapeProcessor& p, double seconds, std::function<float (int)> gen)
{
    const double sr = 48000.0;
    const int block = 256;
    const int total = (int) (seconds * sr);
    p.setPlayConfigDetails (2, 2, sr, block);
    p.prepareToPlay (sr, block);
    juce::AudioBuffer<float> out (2, total);
    juce::AudioBuffer<float> buf (2, block);
    juce::MidiBuffer midi;
    for (int pos = 0; pos < total; pos += block)
    {
        const int n = juce::jmin (block, total - pos);
        buf.setSize (2, n, false, false, true);
        for (int i = 0; i < n; ++i)
        {
            const float v = gen (pos + i);
            buf.setSample (0, i, v);
            buf.setSample (1, i, v);
        }
        p.processBlock (buf, midi);
        for (int ch = 0; ch < 2; ++ch) out.copyFrom (ch, pos, buf, ch, 0, n);
    }
    return out;
}

static float peakAround (const juce::AudioBuffer<float>& b, double ms, double windowMs = 4.0)
{
    const int c = (int) (ms * 48.0), w = (int) (windowMs * 48.0);
    float m = 0;
    for (int i = juce::jmax (0, c - w); i < juce::jmin (b.getNumSamples(), c + w); ++i)
        m = juce::jmax (m, std::abs (b.getSample (0, i)));
    return m;
}

static void dspTests()
{
    std::cout << "DSP" << std::endl;
    auto impulse = [] (int i) { return i == 0 ? 0.5f : 0.0f; };

    {
        AvelTapeProcessor p; cleanSetup (p);
        auto out = render (p, 1.2, impulse);
        const float e1 = peakAround (out, 250), e2 = peakAround (out, 500), quiet = peakAround (out, 125, 60);
        check (e1 > 0.2f, "head I echo at 250 ms (" + juce::String (e1, 3) + ")");
        check (e2 < 0.01f && quiet < 0.01f, "no feedback -> single echo");
    }
    {
        AvelTapeProcessor p; cleanSetup (p); set (p, pid::heads, 5);
        auto out = render (p, 1.2, impulse);
        check (peakAround (out, 250) > 0.1f && peakAround (out, 500) > 0.1f && peakAround (out, 750) > 0.1f, "ALL heads -> 250 / 500 / 750 ms");
        check (peakAround (out, 1000, 30) < 0.01f, "nothing after the last head without feedback");
    }
    {
        AvelTapeProcessor p; cleanSetup (p); set (p, pid::feedback, 60);
        auto out = render (p, 1.4, impulse);
        const float e1 = peakAround (out, 250), e2 = peakAround (out, 500), e3 = peakAround (out, 750);
        check (e2 > 0.05f && e2 < e1 && e3 < e2, "feedback repeats decay (" + juce::String (e1, 3) + ", " + juce::String (e2, 3) + ", " + juce::String (e3, 3) + ")");
    }
    {
        AvelTapeProcessor p; cleanSetup (p); set (p, pid::mix, 0);
        juce::Random r (3);
        std::vector<float> in (48000);
        for (auto& v : in) v = (r.nextFloat() * 2 - 1) * 0.3f;
        auto out = render (p, 1.0, [&] (int i) { return in[(size_t) i]; });
        float diff = 0;
        for (int i = 0; i < 48000; ++i) diff = juce::jmax (diff, std::abs (out.getSample (0, i) - in[(size_t) i]));
        check (diff < 1.0e-5f, "mix 0 % = dry signal untouched");
    }
    {
        AvelTapeProcessor p; cleanSetup (p); set (p, pid::wetOnly, 1); set (p, pid::mix, 50);
        auto out = render (p, 0.4, impulse);
        check (std::abs (out.getSample (0, 0)) < 1.0e-4f, "wet only removes the dry signal");
    }
    {
        AvelTapeProcessor p;   // runaway: feedback 120 %, hot saturation, everything on
        set (p, pid::feedback, 120); set (p, pid::saturation, 100); set (p, pid::heads, 5); set (p, pid::reverb, 100);
        set (p, pid::time, 60); set (p, pid::mix, 100); set (p, pid::tapeMode, 2); set (p, pid::age, 100); set (p, pid::wow, 100); set (p, pid::flutter, 100);
        juce::Random r (5);
        auto out = render (p, 20.0, [&] (int i) { return i < 4800 ? (r.nextFloat() * 2 - 1) * 0.8f : 0.0f; });
        float m = 0; bool finite = true;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < out.getNumSamples(); ++i)
            {
                const float v = out.getSample (ch, i);
                finite = finite && std::isfinite (v);
                m = juce::jmax (m, std::abs (v));
            }
        check (finite && m < 4.0f, "self-oscillation stays bounded (peak " + juce::String (m, 2) + ")");
        check (peakAround (out, 19500, 400) > 0.01f, "self-oscillation keeps running");
    }
    {
        AvelTapeProcessor p;   // random settings never produce NaN
        juce::Random r (9);
        bool finite = true;
        for (int round = 0; round < 12; ++round)
        {
            for (auto* prm : p.getParameters()) prm->setValueNotifyingHost (r.nextFloat());
            auto out = render (p, 0.5, [&] (int) { return (r.nextFloat() * 2 - 1) * 0.5f; });
            for (int i = 0; i < out.getNumSamples(); ++i) finite = finite && std::isfinite (out.getSample (0, i)) && std::isfinite (out.getSample (1, i));
        }
        check (finite, "random settings: output always finite");
    }
    {
        AvelTapeProcessor p; cleanSetup (p); set (p, pid::pingPong, 1); set (p, pid::feedback, 60); set (p, pid::wetOnly, 1);
        auto out = render (p, 1.4, impulse);
        auto peakCh = [&] (int ch, double ms)
        {
            float m = 0;
            for (int i = (int) ((ms - 4) * 48); i < (int) ((ms + 4) * 48); ++i) m = juce::jmax (m, std::abs (out.getSample (ch, i)));
            return m;
        };
        const float l1 = peakCh (0, 250), r1 = peakCh (1, 250), l2 = peakCh (0, 500), r2 = peakCh (1, 500), l3 = peakCh (0, 750), r3 = peakCh (1, 750);
        check (l1 > 0.1f && r1 < 0.005f, "ping pong: 1st repeat left only");
        check (r2 > 0.05f && l2 < 0.005f, "ping pong: 2nd repeat right only");
        check (l3 > 0.02f && r3 < 0.005f, "ping pong: 3rd repeat left again");
    }
    {
        AvelTapeProcessor p;
        set (p, pid::sync, 1); set (p, pid::division, 6);   // 1/4 at 120 BPM (no host) = 500 ms
        check (std::abs (p.computeTimeMs() - 500.0f) < 0.5f, "sync 1/4 at 120 BPM = 500 ms");
    }
    {
        AvelTapeProcessor p;   // CPU
        set (p, pid::heads, 5); set (p, pid::reverb, 50); set (p, pid::wow, 50); set (p, pid::flutter, 50);
        juce::Random r (1);
        const double t0 = juce::Time::getMillisecondCounterHiRes();
        render (p, 10.0, [&] (int) { return (r.nextFloat() * 2 - 1) * 0.3f; });
        const double ms = juce::Time::getMillisecondCounterHiRes() - t0;
        std::cout << "  info  10 s of stereo audio processed in " << ms << " ms (" << (ms / 100.0) << " % of real time)" << std::endl;
        check (ms < 2000.0, "CPU well below real time");
    }
}

static void stateTests()
{
    std::cout << "State / presets" << std::endl;
    AvelTapeProcessor p;
    p.setCurrentProgram (4);
    check (p.getPresetName() == "Worn Cassette", "factory program loads");
    const float age = p.apvts.getRawParameterValue (pid::age)->load();
    juce::MemoryBlock mb;
    p.getStateInformation (mb);
    AvelTapeProcessor q;
    q.setStateInformation (mb.getData(), (int) mb.getSize());
    check (std::abs (q.apvts.getRawParameterValue (pid::age)->load() - age) < 0.01f && q.getPresetName() == "Worn Cassette", "state round trip");

    // A/B
    set (p, pid::mix, 10);
    p.selectAB (1);
    set (p, pid::mix, 90);
    p.selectAB (0);
    check (std::abs (p.apvts.getRawParameterValue (pid::mix)->load() - 10.0f) < 0.5f, "A/B keeps setting A");
    p.selectAB (1);
    check (std::abs (p.apvts.getRawParameterValue (pid::mix)->load() - 90.0f) < 0.5f, "A/B keeps setting B");
    for (int i = 0; i < p.getNumPrograms(); ++i) p.setCurrentProgram (i);
    check (p.getNumPrograms() == 11, "11 factory presets");

    {   // pluginval-style: random normalised values must survive a state round trip
        AvelTapeProcessor x;
        juce::Random r (4);
        for (auto* prm : x.getParameters()) prm->setValueNotifyingHost (r.nextFloat());
        std::vector<float> before;
        for (auto* prm : x.getParameters()) before.push_back (prm->getValue());
        juce::MemoryBlock st; x.getStateInformation (st);
        for (auto* prm : x.getParameters()) prm->setValueNotifyingHost (r.nextFloat());
        x.setStateInformation (st.getData(), (int) st.getSize());
        bool ok = true; int i = 0;
        for (auto* prm : x.getParameters())
        {
            if (std::abs (prm->getValue() - before[(size_t) i]) > 0.01f)
            {
                ok = false;
                std::cout << "        " << prm->getName (30) << " " << before[(size_t) i] << " -> " << prm->getValue() << std::endl;
            }
            ++i;
        }
        check (ok, "random parameter values restored from state");
    }
}

static void snapshot (AvelTapeProcessor& p, const juce::String& name, int scalePct = 100)
{
    p.uiScalePercent.store (scalePct);
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    ed->setVisible (true);
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    juce::Random r (2);
    p.setPlayConfigDetails (2, 2, 48000.0, 512);
    p.prepareToPlay (48000.0, 512);
    for (int k = 0; k < 50; ++k)
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
                buf.setSample (ch, i, (r.nextFloat() * 2 - 1) * 0.45f * (float) std::sin (k * 0.3 + i * 0.0001));
        p.processBlock (buf, midi);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (17);
    }
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
    auto dir = juce::File::getCurrentWorkingDirectory().getChildFile ("shots");
    dir.createDirectory();
    auto f = dir.getChildFile (name + ".png");
    f.deleteFile();
    juce::FileOutputStream os (f);
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::cout << "  shot  " << f.getFullPathName() << " (" << ed->getWidth() << "x" << ed->getHeight() << ")" << std::endl;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const bool shotsOnly = argc > 1 && juce::String (argv[1]) == "--shots";

    if (! shotsOnly)
    {
        dspTests();
        stateTests();
    }

    std::cout << "Snapshots" << std::endl;
    {
        AvelTapeProcessor p;
        snapshot (p, "01_default");
    }
    {
        AvelTapeProcessor p;
        p.setCurrentProgram (1);
        snapshot (p, "02_dub_chamber_sync");
    }
    {
        AvelTapeProcessor p;
        p.setCurrentProgram (4);
        snapshot (p, "03_worn_75pct", 75);
    }
    {
        AvelTapeProcessor p;
        p.setCurrentProgram (9);
        snapshot (p, "04_ping_pong");
    }

    std::cout << (failures == 0 ? "ALL TESTS PASSED" : juce::String (failures) + " TEST(S) FAILED") << std::endl;
    return failures == 0 ? 0 : 1;
}
