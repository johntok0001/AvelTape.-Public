#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Params.h"
#include "TapeEngine.h"

class AvelTapeProcessor : public juce::AudioProcessor,
                          public juce::ChangeBroadcaster
{
public:
    AvelTapeProcessor();
    ~AvelTapeProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Avel Tape"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return juce::jmax (0, currentFactory); }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    juce::AudioProcessorValueTreeState apvts;

    // presets (factory + user files), sent as change messages to the editor
    struct PresetEntry { juce::String name; bool factory; int index; juce::File file; };
    std::vector<PresetEntry> getPresetList() const;
    void loadPreset (const PresetEntry& p);
    void loadPresetByIndex (int indexInList);
    void stepPreset (int delta);
    bool saveUserPreset (const juce::String& name);
    void initPreset();
    juce::String getPresetName() const;
    static juce::File getUserPresetFolder();

    // A/B compare
    int getABSlot() const { return abSlot; }
    void selectAB (int slot);
    void copyToOtherSlot();

    // metering / display (written on the audio thread, read by the editor)
    std::atomic<float> inPeak[2] {}, outPeak[2] {};
    std::atomic<float> echoPeak { 0.0f }, inputPeak { 0.0f };
    std::atomic<double> hostBpm { 120.0 };
    std::atomic<float> effectiveTimeMs { 375.0f };
    std::atomic<int> uiScalePercent { 100 };

    double getBpm() const { return hostBpm.load(); }
    float computeTimeMs() const;   // from the parameters (sync aware), for the display

private:
    void setParamPlain (const juce::String& id, float value);
    void applyFactory (int index);

    avel::tape::TapeEngine engine;

    std::atomic<float>* pTime = nullptr; std::atomic<float>* pSync = nullptr; std::atomic<float>* pDiv = nullptr;
    std::atomic<float>* pFeedback = nullptr; std::atomic<float>* pMix = nullptr; std::atomic<float>* pWetOnly = nullptr;
    std::atomic<float>* pHeads = nullptr; std::atomic<float>* pWow = nullptr; std::atomic<float>* pFlutter = nullptr;
    std::atomic<float>* pSat = nullptr; std::atomic<float>* pTone = nullptr; std::atomic<float>* pReverb = nullptr;
    std::atomic<float>* pMode = nullptr; std::atomic<float>* pType = nullptr; std::atomic<float>* pNoise = nullptr;
    std::atomic<float>* pAge = nullptr; std::atomic<float>* pFlutterMod = nullptr; std::atomic<float>* pPingPong = nullptr;

    int currentFactory = 0;
    int abSlot = 0;
    juce::ValueTree abSlots[2];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AvelTapeProcessor)
};
