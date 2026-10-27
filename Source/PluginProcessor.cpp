#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace avel::tape;

AvelTapeProcessor::AvelTapeProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "AvelTape", createParameterLayout())
{
    auto raw = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    pTime = raw (pid::time); pSync = raw (pid::sync); pDiv = raw (pid::division);
    pFeedback = raw (pid::feedback); pMix = raw (pid::mix); pWetOnly = raw (pid::wetOnly);
    pHeads = raw (pid::heads); pWow = raw (pid::wow); pFlutter = raw (pid::flutter);
    pSat = raw (pid::saturation); pTone = raw (pid::tone); pReverb = raw (pid::reverb);
    pMode = raw (pid::tapeMode); pType = raw (pid::tapeType); pNoise = raw (pid::noise);
    pAge = raw (pid::age); pFlutterMod = raw (pid::flutterMod); pPingPong = raw (pid::pingPong);

    apvts.state.setProperty ("preset", factoryPresets()[0].name, nullptr);
}

bool AvelTapeProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void AvelTapeProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
}

float AvelTapeProcessor::computeTimeMs() const
{
    if (pSync->load() > 0.5f)
    {
        const auto& divs = divisions();
        const int d = juce::jlimit (0, (int) divs.size() - 1, (int) pDiv->load());
        const double bpm = juce::jlimit (20.0, 400.0, hostBpm.load());
        return (float) juce::jlimit (20.0, TapeEngine::maxDelayMs, divs[(size_t) d].beats * 60000.0 / bpm);
    }
    return pTime->load();
}

void AvelTapeProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels();
    for (int ch = numIn; ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, n);
    if (n == 0) return;

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                if (*bpm > 1.0)
                    hostBpm.store (*bpm);

    TapeParams p;
    p.timeMs     = computeTimeMs();
    p.pattern    = (int) pHeads->load();
    p.feedback   = pFeedback->load() / 100.0f;
    p.mix        = pMix->load() / 100.0f;
    p.wetOnly    = pWetOnly->load() > 0.5f;
    p.wow        = pWow->load() / 100.0f;
    p.flutter    = pFlutter->load() / 100.0f;
    p.saturation = pSat->load() / 100.0f;
    p.tone       = pTone->load() / 100.0f;
    p.reverb     = pReverb->load() / 100.0f;
    p.mode       = (int) pMode->load();
    p.type       = (int) pType->load();
    p.noiseDb    = pNoise->load();
    p.age        = pAge->load() / 100.0f;
    p.modulation = pFlutterMod->load() > 0.5f;
    p.pingPong   = pPingPong->load() > 0.5f;
    engine.setParams (p);
    effectiveTimeMs.store (p.timeMs);

    const int chans = juce::jmin (2, buffer.getNumChannels());
    float inMax = 0.0f;
    for (int ch = 0; ch < chans; ++ch)
    {
        const float m = buffer.getMagnitude (ch, 0, n);
        inMax = juce::jmax (inMax, m);
        if (m > inPeak[ch].load()) inPeak[ch].store (m);
    }
    if (chans == 1 && inMax > inPeak[1].load()) inPeak[1].store (inMax);
    if (inMax > inputPeak.load()) inputPeak.store (inMax);

    engine.process (buffer.getWritePointer (0), chans > 1 ? buffer.getWritePointer (1) : nullptr, n);

    for (int ch = 0; ch < chans; ++ch)
    {
        const float m = buffer.getMagnitude (ch, 0, n);
        if (m > outPeak[ch].load()) outPeak[ch].store (m);
    }
    if (chans == 1)
    {
        const float m = buffer.getMagnitude (0, 0, n);
        if (m > outPeak[1].load()) outPeak[1].store (m);
    }
    const float e = engine.getEchoPeak();
    if (e > echoPeak.load()) echoPeak.store (e);
}

//==============================================================================
int AvelTapeProcessor::getNumPrograms() { return (int) factoryPresets().size(); }

const juce::String AvelTapeProcessor::getProgramName (int index)
{
    const auto& f = factoryPresets();
    return juce::isPositiveAndBelow (index, (int) f.size()) ? juce::String (f[(size_t) index].name) : juce::String();
}

void AvelTapeProcessor::setCurrentProgram (int index)
{
    if (juce::isPositiveAndBelow (index, (int) factoryPresets().size()))
        applyFactory (index);
}

void AvelTapeProcessor::setParamPlain (const juce::String& id, float value)
{
    if (auto* p = apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (value));
        p->endChangeGesture();
    }
}

void AvelTapeProcessor::applyFactory (int index)
{
    const auto& preset = factoryPresets()[(size_t) index];
    for (auto* param : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param))
        {
            float plain = rp->convertFrom0to1 (rp->getDefaultValue());
            for (auto& kv : preset.values)
                if (rp->paramID == kv.first)
                    plain = kv.second;
            rp->beginChangeGesture();
            rp->setValueNotifyingHost (rp->convertTo0to1 (plain));
            rp->endChangeGesture();
        }
    currentFactory = index;
    apvts.state.setProperty ("preset", preset.name, nullptr);
    sendChangeMessage();
}

juce::File AvelTapeProcessor::getUserPresetFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
               .getChildFile ("Avel").getChildFile ("Avel Tape").getChildFile ("Presets");
}

std::vector<AvelTapeProcessor::PresetEntry> AvelTapeProcessor::getPresetList() const
{
    std::vector<PresetEntry> list;
    const auto& f = factoryPresets();
    for (int i = 0; i < (int) f.size(); ++i)
        list.push_back ({ f[(size_t) i].name, true, i, {} });

    auto folder = getUserPresetFolder();
    if (folder.isDirectory())
    {
        auto files = folder.findChildFiles (juce::File::findFiles, false, "*.xml");
        files.sort();
        for (auto& file : files)
            list.push_back ({ file.getFileNameWithoutExtension(), false, -1, file });
    }
    return list;
}

void AvelTapeProcessor::loadPreset (const PresetEntry& p)
{
    if (p.factory)
    {
        applyFactory (p.index);
        return;
    }

    if (auto xml = juce::XmlDocument::parse (p.file))
    {
        auto tree = juce::ValueTree::fromXml (*xml);
        if (tree.hasType (apvts.state.getType()))
        {
            for (auto* param : getParameters())
                if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param))
                {
                    auto child = tree.getChildWithProperty ("id", rp->paramID);
                    const float plain = child.isValid() ? (float) child.getProperty ("value") : rp->convertFrom0to1 (rp->getDefaultValue());
                    rp->beginChangeGesture();
                    rp->setValueNotifyingHost (rp->convertTo0to1 (plain));
                    rp->endChangeGesture();
                }
            currentFactory = -1;
            apvts.state.setProperty ("preset", p.name, nullptr);
            sendChangeMessage();
        }
    }
}

void AvelTapeProcessor::loadPresetByIndex (int indexInList)
{
    auto list = getPresetList();
    if (juce::isPositiveAndBelow (indexInList, (int) list.size()))
        loadPreset (list[(size_t) indexInList]);
}

void AvelTapeProcessor::stepPreset (int delta)
{
    auto list = getPresetList();
    if (list.empty()) return;
    const auto name = getPresetName();
    int cur = -1;
    for (int i = 0; i < (int) list.size(); ++i)
        if (list[(size_t) i].name == name) { cur = i; break; }
    const int n = (int) list.size();
    const int next = cur < 0 ? (delta > 0 ? 0 : n - 1) : ((cur + delta) % n + n) % n;
    loadPreset (list[(size_t) next]);
}

bool AvelTapeProcessor::saveUserPreset (const juce::String& name)
{
    const auto clean = juce::File::createLegalFileName (name.trim());
    if (clean.isEmpty()) return false;
    auto folder = getUserPresetFolder();
    if (! folder.createDirectory()) return false;

    auto state = apvts.copyState();
    state.setProperty ("preset", clean, nullptr);
    if (auto xml = state.createXml())
        if (xml->writeTo (folder.getChildFile (clean + ".xml")))
        {
            apvts.state.setProperty ("preset", clean, nullptr);
            currentFactory = -1;
            sendChangeMessage();
            return true;
        }
    return false;
}

void AvelTapeProcessor::initPreset()
{
    applyFactory (0);
    apvts.state.setProperty ("preset", "Init", nullptr);
    sendChangeMessage();
}

juce::String AvelTapeProcessor::getPresetName() const
{
    return apvts.state.getProperty ("preset", "Init").toString();
}

//==============================================================================
void AvelTapeProcessor::selectAB (int slot)
{
    slot = juce::jlimit (0, 1, slot);
    if (slot == abSlot) return;
    abSlots[abSlot] = apvts.copyState();
    if (abSlots[slot].isValid())
        apvts.replaceState (abSlots[slot].createCopy());
    abSlot = slot;
    sendChangeMessage();
}

void AvelTapeProcessor::copyToOtherSlot()
{
    abSlots[1 - abSlot] = apvts.copyState();
    sendChangeMessage();
}

//==============================================================================
void AvelTapeProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("uiScale", uiScalePercent.load(), nullptr);
    state.setProperty ("abSlot", abSlot, nullptr);
    juce::ValueTree ab ("AB");
    for (int i = 0; i < 2; ++i)
        if (abSlots[i].isValid())
        {
            juce::ValueTree slot ("Slot");
            slot.setProperty ("index", i, nullptr);
            slot.appendChild (abSlots[i].createCopy(), nullptr);
            ab.appendChild (slot, nullptr);
        }
    state.appendChild (ab, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void AvelTapeProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr) return;
    auto tree = juce::ValueTree::fromXml (*xml);
    if (! tree.hasType (apvts.state.getType())) return;

    uiScalePercent.store (juce::jlimit (50, 200, (int) tree.getProperty ("uiScale", 100)));
    abSlot = juce::jlimit (0, 1, (int) tree.getProperty ("abSlot", 0));
    abSlots[0] = abSlots[1] = {};
    auto ab = tree.getChildWithName ("AB");
    if (ab.isValid())
    {
        for (auto slot : ab)
        {
            const int i = juce::jlimit (0, 1, (int) slot.getProperty ("index", 0));
            if (slot.getNumChildren() > 0)
                abSlots[i] = slot.getChild (0).createCopy();
        }
        tree.removeChild (ab, nullptr);
    }
    apvts.replaceState (tree);

    currentFactory = -1;
    const auto name = getPresetName();
    const auto& f = factoryPresets();
    for (int i = 0; i < (int) f.size(); ++i)
        if (name == f[(size_t) i].name) currentFactory = i;
    sendChangeMessage();
}

juce::AudioProcessorEditor* AvelTapeProcessor::createEditor()
{
    return new AvelTapeEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AvelTapeProcessor();
}
