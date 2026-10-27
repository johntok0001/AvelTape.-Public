#pragma once

#include "PluginProcessor.h"
#include "UI/Panels.h"

namespace avel::tape::ui
{
    // Preset field in the header: < | Warm Echoes ... | >
    class PresetBox : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        std::function<void()> onPrev, onNext, onMenu;
        juce::String name;

        PresetBox() { setMouseCursor (juce::MouseCursor::PointingHandCursor); }

        void paint (juce::Graphics& g) override;
        void mouseMove (const juce::MouseEvent& e) override { setHot (zone (e.x)); }
        void mouseExit (const juce::MouseEvent&) override { setHot (-2); }
        void mouseUp (const juce::MouseEvent& e) override
        {
            if (! e.mouseWasClicked()) return;
            const int z = zone (e.x);
            if (z < 0 && onPrev) onPrev();
            else if (z > 0 && onNext) onNext();
            else if (z == 0 && onMenu) onMenu();
        }

    private:
        int zone (int x) const { return x < 44 ? -1 : (x > getWidth() - 44 ? 1 : 0); }
        void setHot (int h) { if (h != hot) { hot = h; repaint(); } }
        int hot = -2;
    };

    // Static artwork: header, metal panels, engraved labels, icons, strip, footer.
    class Backdrop : public juce::Component
    {
    public:
        Backdrop() { setInterceptsMouseClicks (false, false); setBufferedToImage (true); }
        void paint (juce::Graphics& g) override;

        struct Col { float x, w; juce::String title; int icon; };   // icon: -1 none, 0..5
        std::vector<Col> columns;
        struct StripItem { juce::Rectangle<float> r; juce::String label; bool separator; };
        std::vector<StripItem> stripItems;
        float wordmarkW = 300.0f;
    };
}

class AvelTapeEditor : public juce::AudioProcessorEditor,
                       private juce::Timer,
                       private juce::ChangeListener
{
public:
    explicit AvelTapeEditor (AvelTapeProcessor&);
    ~AvelTapeEditor() override;

    void paint (juce::Graphics&) override {}
    void resized() override;

    static constexpr int designW = 1180, designH = 740;

private:
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void layoutDesign();
    void updateSyncVisibility();
    void showPresetMenu();
    void showMoreMenu();
    void showSettingsMenu();
    void showDivisionMenu();
    void savePresetDialog();
    void setScale (int percent);

    AvelTapeProcessor& proc;
    avel::tape::ui::LookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 700 };

    juce::Component root;   // design-size content, scaled with a transform
    avel::tape::ui::Backdrop backdrop;
    avel::tape::ui::PresetBox presetBox;
    avel::tape::ui::SmallButton btnA { "A" }, btnB { "B" };
    avel::tape::ui::IconButton linkBtn, gearBtn, moreBtn;

    avel::tape::ui::MeterView inMeter, outMeter;
    avel::tape::ui::TransportView transport;

    avel::tape::ui::AmberKnob timeKnob, divKnob, feedbackKnob, mixKnob, wowKnob, flutterKnob, satKnob, toneKnob, reverbKnob;
    avel::tape::ui::ValuePill timePill { true }, feedbackPill { true }, mixPill { true }, wowPill { false }, flutterPill { false },
                              satPill { true }, tonePill { true }, reverbPill { true };

    avel::tape::ui::SmallButton noteBtn, syncBtn { "SYNC" };
    avel::tape::ui::LampView syncLamp, feedbackLamp, wowLamp, flutterLamp, reverbLamp;
    avel::tape::ui::PillSwitch wetSwitch, flutterModSwitch;

    avel::tape::ui::Segmented modeSeg, typeSeg;
    avel::tape::ui::MiniKnob noiseKnob, ageKnob;
    avel::tape::ui::ValueText noiseText, ageText;

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::vector<std::unique_ptr<SA>> sliderAttachments;
    std::unique_ptr<juce::ParameterAttachment> syncAttachment;
    bool syncOn = false;

    std::unique_ptr<juce::AlertWindow> alert;
    juce::ComponentBoundsConstrainer constrainer;
    double lastTime = 0.0;
    bool sizeRestored = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AvelTapeEditor)
};
