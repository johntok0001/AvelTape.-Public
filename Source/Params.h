#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <vector>

namespace avel::tape
{
    namespace pid
    {
        inline constexpr const char* time       = "time";
        inline constexpr const char* sync       = "sync";
        inline constexpr const char* division   = "division";
        inline constexpr const char* feedback   = "feedback";
        inline constexpr const char* mix        = "mix";
        inline constexpr const char* wetOnly    = "wetOnly";
        inline constexpr const char* heads      = "heads";
        inline constexpr const char* wow        = "wow";
        inline constexpr const char* flutter    = "flutter";
        inline constexpr const char* saturation = "saturation";
        inline constexpr const char* tone       = "tone";
        inline constexpr const char* reverb     = "reverb";
        inline constexpr const char* tapeMode   = "tapeMode";
        inline constexpr const char* tapeType   = "tapeType";
        inline constexpr const char* noise      = "noise";
        inline constexpr const char* age        = "age";
        inline constexpr const char* flutterMod = "flutterMod";
        inline constexpr const char* pingPong   = "pingPong";
    }

    struct Division { const char* name; double beats; };

    inline const std::array<Division, 12>& divisions()
    {
        static const std::array<Division, 12> d { {
            { "1/16", 0.25 }, { "1/8 T", 1.0 / 3.0 }, { "1/16 D", 0.375 }, { "1/8", 0.5 },
            { "1/4 T", 2.0 / 3.0 }, { "1/8 D", 0.75 }, { "1/4", 1.0 }, { "1/2 T", 4.0 / 3.0 },
            { "1/4 D", 1.5 }, { "1/2", 2.0 }, { "1/2 D", 3.0 }, { "1/1", 4.0 } } };
        return d;
    }

    inline juce::StringArray divisionNames()
    {
        juce::StringArray s;
        for (auto& d : divisions()) s.add (d.name);
        return s;
    }

    inline juce::StringArray headPatternNames() { return { "I", "II", "III", "I+II", "II+III", "ALL" }; }
    inline juce::StringArray tapeModeNames()    { return { "CLEAN", "VINTAGE", "WORN" }; }
    inline juce::StringArray tapeTypeNames()    { return { "NORMAL", "HIGH", "LOW" }; }

    inline juce::String toneName (float v01)
    {
        if (v01 < 0.20f) return "DARK";
        if (v01 < 0.45f) return "WARM";
        if (v01 < 0.62f) return "NEUTRAL";
        if (v01 < 0.82f) return "BRIGHT";
        return "AIRY";
    }

    inline juce::String timeText (float ms)
    {
        if (ms >= 1000.0f) return juce::String (ms / 1000.0f, 2) + " s";
        return juce::String (juce::roundToInt (ms)) + " ms";
    }

    inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
    {
        using namespace juce;
        AudioProcessorValueTreeState::ParameterLayout l;
        int v = 1;

        auto pct = [] (float val, int) { return String (roundToInt (val)) + " %"; };
        auto pctParse = [] (const String& t) { return t.retainCharacters ("0123456789.-").getFloatValue(); };

        auto pctParam = [&] (const char* id, const char* name, float max, float def)
        {
            l.add (std::make_unique<AudioParameterFloat> (ParameterID { id, v }, name, NormalisableRange<float> (0.0f, max, 0.1f), def,
                                                          AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction (pct)
                                                                                         .withValueFromStringFunction (pctParse)));
        };

        NormalisableRange<float> timeRange (20.0f, 2000.0f, 0.1f);
        timeRange.setSkewForCentre (400.0f);
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::time, v }, "Delay Time", timeRange, 375.0f,
            AudioParameterFloatAttributes().withLabel ("ms")
                .withStringFromValueFunction ([] (float val, int) { return timeText (val); })
                .withValueFromStringFunction ([] (const String& t)
                {
                    const float n = t.retainCharacters ("0123456789.").getFloatValue();
                    return t.containsIgnoreCase ("ms") || ! t.containsIgnoreCase ("s") ? n : n * 1000.0f;
                })));

        l.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::sync, v }, "Sync", StringArray { "Off", "On" }, 0));
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::division, v }, "Sync Division", divisionNames(), 5));
        pctParam (pid::feedback, "Feedback", 120.0f, 38.0f);
        pctParam (pid::mix, "Mix", 100.0f, 28.0f);
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::wetOnly, v }, "Wet Only", StringArray { "Off", "On" }, 0));
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::heads, v }, "Head Pattern", headPatternNames(), 3));
        pctParam (pid::wow, "Wow", 100.0f, 18.0f);
        pctParam (pid::flutter, "Flutter", 100.0f, 12.0f);
        pctParam (pid::saturation, "Saturation", 100.0f, 42.0f);

        l.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::tone, v }, "Tone", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 36.0f,
            AudioParameterFloatAttributes().withStringFromValueFunction ([] (float val, int) { return toneName (val / 100.0f); })
                                           .withValueFromStringFunction ([] (const String& t)
                                           {
                                               const auto u = t.trim().toUpperCase();
                                               if (u == "DARK") return 10.0f;
                                               if (u == "WARM") return 35.0f;
                                               if (u == "NEUTRAL") return 53.0f;
                                               if (u == "BRIGHT") return 72.0f;
                                               if (u == "AIRY") return 90.0f;
                                               return t.retainCharacters ("0123456789.").getFloatValue();
                                           })));

        pctParam (pid::reverb, "Spring Reverb", 100.0f, 16.0f);
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::tapeMode, v }, "Tape Mode", tapeModeNames(), 1));
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::tapeType, v }, "Tape Type", tapeTypeNames(), 0));

        l.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::noise, v }, "Noise", NormalisableRange<float> (-90.0f, -30.0f, 0.1f), -42.0f,
            AudioParameterFloatAttributes().withLabel ("dB")
                .withStringFromValueFunction ([] (float val, int) { return val <= -89.9f ? String ("OFF") : String (roundToInt (val)).replace ("-", String::fromUTF8 ("\xe2\x88\x92")) + " dB"; })
                .withValueFromStringFunction ([] (const String& t)
                {
                    if (t.containsIgnoreCase ("off")) return -90.0f;
                    auto s = t.replace (String::fromUTF8 ("\xe2\x88\x92"), "-");
                    const float n = s.retainCharacters ("0123456789.-").getFloatValue();
                    return n > 0 ? -n : n;
                })));

        pctParam (pid::age, "Tape Age", 100.0f, 38.0f);
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::flutterMod, v }, "Flutter Mod", StringArray { "Off", "On" }, 1));
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::pingPong, 2 }, "Ping Pong", StringArray { "Off", "On" }, 0));
        return l;
    }

    //==============================================================================
    struct FactoryPreset
    {
        const char* name;
        std::vector<std::pair<const char*, float>> values;   // plain values; anything not listed = default
    };

    inline const std::vector<FactoryPreset>& factoryPresets()
    {
        static const std::vector<FactoryPreset> p {
            { "Warm Echoes", {} },
            { "Dub Chamber", { { pid::sync, 1 }, { pid::division, 8 }, { pid::heads, 3 }, { pid::feedback, 72 }, { pid::mix, 35 },
                               { pid::wow, 22 }, { pid::flutter, 14 }, { pid::saturation, 55 }, { pid::tone, 24 }, { pid::reverb, 32 },
                               { pid::tapeType, 2 }, { pid::noise, -48 }, { pid::age, 40 } } },
            { "Slapback", { { pid::time, 95 }, { pid::heads, 0 }, { pid::feedback, 8 }, { pid::mix, 30 }, { pid::wow, 8 }, { pid::flutter, 10 },
                            { pid::saturation, 35 }, { pid::tone, 55 }, { pid::reverb, 0 }, { pid::noise, -60 }, { pid::age, 20 } } },
            { "Space Station", { { pid::time, 220 }, { pid::heads, 5 }, { pid::feedback, 62 }, { pid::mix, 40 }, { pid::wow, 15 },
                                 { pid::flutter, 10 }, { pid::saturation, 40 }, { pid::tone, 45 }, { pid::reverb, 45 } } },
            { "Worn Cassette", { { pid::time, 420 }, { pid::heads, 1 }, { pid::feedback, 45 }, { pid::mix, 35 }, { pid::wow, 55 },
                                 { pid::flutter, 40 }, { pid::saturation, 50 }, { pid::tone, 18 }, { pid::reverb, 10 }, { pid::tapeMode, 2 },
                                 { pid::tapeType, 2 }, { pid::noise, -38 }, { pid::age, 75 } } },
            { "Triplet Bounce", { { pid::sync, 1 }, { pid::division, 1 }, { pid::heads, 3 }, { pid::feedback, 40 }, { pid::mix, 30 },
                                  { pid::tone, 50 }, { pid::reverb, 12 } } },
            { "Clean Tape Delay", { { pid::sync, 1 }, { pid::division, 6 }, { pid::heads, 0 }, { pid::feedback, 35 }, { pid::mix, 25 },
                                    { pid::wow, 0 }, { pid::flutter, 0 }, { pid::saturation, 10 }, { pid::tone, 70 }, { pid::reverb, 0 },
                                    { pid::tapeMode, 0 }, { pid::noise, -90 }, { pid::age, 0 } } },
            { "Runaway Oscillation", { { pid::time, 300 }, { pid::heads, 4 }, { pid::feedback, 106 }, { pid::mix, 45 }, { pid::saturation, 60 },
                                       { pid::tone, 32 }, { pid::reverb, 20 }, { pid::wow, 25 } } },
            { "Vocal Throw", { { pid::sync, 1 }, { pid::division, 6 }, { pid::heads, 0 }, { pid::feedback, 30 }, { pid::mix, 22 },
                               { pid::tone, 40 }, { pid::reverb, 18 }, { pid::saturation, 30 } } },
            { "Ping Pong Dub", { { pid::sync, 1 }, { pid::division, 5 }, { pid::heads, 0 }, { pid::feedback, 58 }, { pid::mix, 32 },
                                 { pid::pingPong, 1 }, { pid::wow, 16 }, { pid::saturation, 48 }, { pid::tone, 30 }, { pid::reverb, 18 },
                                 { pid::tapeType, 2 }, { pid::age, 35 } } },
            { "Ambient Wash", { { pid::time, 600 }, { pid::heads, 5 }, { pid::feedback, 70 }, { pid::mix, 45 }, { pid::reverb, 60 },
                                { pid::wow, 30 }, { pid::tone, 30 }, { pid::age, 50 } } },
        };
        return p;
    }
}
