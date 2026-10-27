#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <vector>
#include <array>
#include <algorithm>

// Tape echo engine: one tape loop, a record head and three playback heads (1x, 2x, 3x
// the delay time), wow & flutter, tape saturation, tone and a spring reverb.
namespace avel::tape
{
    enum class TapeMode { clean = 0, vintage, worn };
    enum class TapeType { normal = 0, high, low };

    // which playback heads are active for each "head pattern" choice
    inline std::array<bool, 3> headsForPattern (int pattern)
    {
        switch (pattern)
        {
            case 0:  return { true,  false, false };   // I
            case 1:  return { false, true,  false };   // II
            case 2:  return { false, false, true  };   // III
            case 3:  return { true,  true,  false };   // I+II
            case 4:  return { false, true,  true  };   // II+III
            default: return { true,  true,  true  };   // ALL
        }
    }

    struct TapeParams
    {
        float timeMs     = 375.0f;  // spacing of head I
        int   pattern    = 3;
        float feedback   = 0.38f;   // 0 .. 1.2
        float mix        = 0.28f;
        bool  wetOnly    = false;
        float wow        = 0.18f;
        float flutter    = 0.12f;
        float saturation = 0.42f;
        float tone       = 0.36f;
        float reverb     = 0.16f;
        int   mode       = 1;
        int   type       = 0;
        float noiseDb    = -42.0f;  // <= -90 means off
        float age        = 0.38f;
        bool  modulation = true;    // "Flutter Mod" switch
        bool  pingPong   = false;   // repeats alternate left / right
    };

    //==============================================================================
    // Smooth random signal in -1..1: a new random target every `period` samples, cosine-interpolated.
    class SmoothRandom
    {
    public:
        void prepare (double sampleRate, double periodSeconds, juce::int64 seed)
        {
            rng.setSeed (seed);
            period = juce::jmax (1, (int) (sampleRate * periodSeconds));
            a = rng.nextFloat() * 2.0f - 1.0f;
            b = rng.nextFloat() * 2.0f - 1.0f;
            pos = 0;
        }

        float next() noexcept
        {
            if (++pos >= period)
            {
                pos = 0;
                a = b;
                b = rng.nextFloat() * 2.0f - 1.0f;
            }
            const float t = (float) pos / (float) period;
            const float w = 0.5f - 0.5f * std::cos (t * juce::MathConstants<float>::pi);
            return a + (b - a) * w;
        }

    private:
        juce::Random rng;
        int period = 1, pos = 0;
        float a = 0, b = 0;
    };

    //==============================================================================
    // Topology-preserving state variable filter (Simper), low/high pass outputs.
    struct SVF
    {
        float g = 0, k = 1.414f, a1 = 0, a2 = 0, a3 = 0;
        float ic1 = 0, ic2 = 0;

        void set (double sampleRate, double freq, double q)
        {
            freq = juce::jlimit (10.0, sampleRate * 0.45, freq);
            g = (float) std::tan (juce::MathConstants<double>::pi * freq / sampleRate);
            k = (float) (1.0 / q);
            a1 = 1.0f / (1.0f + g * (g + k));
            a2 = g * a1;
            a3 = g * a2;
        }

        void reset() { ic1 = ic2 = 0; }

        inline void tick (float v0, float& lp, float& hp) noexcept
        {
            const float v3 = v0 - ic2;
            const float v1 = a1 * ic1 + a2 * v3;
            const float v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0f * v1 - ic1;
            ic2 = 2.0f * v2 - ic2;
            lp = v2;
            hp = v0 - k * v1 - v2;
        }

        inline float lowpass (float x) noexcept  { float l, h; tick (x, l, h); return l; }
        inline float highpass (float x) noexcept { float l, h; tick (x, l, h); return h; }
    };

    // RBJ peaking biquad (head bump)
    struct Peak
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

        void set (double sampleRate, double freq, double q, double gainDb)
        {
            const double A = std::pow (10.0, gainDb / 40.0);
            const double w0 = juce::MathConstants<double>::twoPi * freq / sampleRate;
            const double alpha = std::sin (w0) / (2.0 * q);
            const double cw = std::cos (w0);
            const double a0 = 1.0 + alpha / A;
            b0 = (float) ((1.0 + alpha * A) / a0);
            b1 = (float) ((-2.0 * cw) / a0);
            b2 = (float) ((1.0 - alpha * A) / a0);
            a1 = (float) ((-2.0 * cw) / a0);
            a2 = (float) ((1.0 - alpha / A) / a0);
        }

        void reset() { z1 = z2 = 0; }

        inline float process (float x) noexcept
        {
            const float y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
    };

    inline float fastTanh (float x) noexcept
    {
        x = juce::jlimit (-4.5f, 4.5f, x);
        const float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    //==============================================================================
    // Spring reverb: three dispersive "springs" (stretched allpass chains inside a damped
    // feedback delay), giving the characteristic chirp and drip.
    class SpringReverb
    {
    public:
        void prepare (double sr)
        {
            sampleRate = sr;
            const double lengthsMs[kSprings] = { 33.7, 41.3, 47.9 };
            for (int s = 0; s < kSprings; ++s)
            {
                auto& sp = springs[(size_t) s];
                sp.len = (int) (sr * lengthsMs[s] / 1000.0);
                sp.buf.assign ((size_t) (sp.len + 64), 0.0f);
                sp.w = 0;
                sp.modPhase = (float) s * 1.7f;
                const int stretch = juce::jmax (2, (int) std::round (3.0 * sr / 48000.0));
                for (auto& ap : sp.ap)
                {
                    ap.buf.assign ((size_t) stretch, 0.0f);
                    ap.pos = 0;
                }
                sp.damp = 0; sp.dc = 0; sp.dcIn = 0;
            }
            inHP.set (sr, 260.0, 0.7);
            inLP.set (sr, 4800.0, 0.7);
            inHP.reset(); inLP.reset();
            dampCoef = (float) std::exp (-juce::MathConstants<double>::twoPi * 4200.0 / sr);
            dcCoef = (float) std::exp (-juce::MathConstants<double>::twoPi * 120.0 / sr);
        }

        void reset()
        {
            for (auto& sp : springs)
            {
                std::fill (sp.buf.begin(), sp.buf.end(), 0.0f);
                for (auto& ap : sp.ap) std::fill (ap.buf.begin(), ap.buf.end(), 0.0f);
                sp.damp = sp.dc = sp.dcIn = 0;
            }
            inHP.reset(); inLP.reset();
        }

        inline void process (float in, float& outL, float& outR) noexcept
        {
            float x = inLP.lowpass (inHP.highpass (in));
            float o[kSprings];

            for (int s = 0; s < kSprings; ++s)
            {
                auto& sp = springs[(size_t) s];
                const int size = (int) sp.buf.size();

                // read with a slow wobble (the "drip")
                sp.modPhase += (float) (juce::MathConstants<double>::twoPi * (0.9 + 0.37 * s) / sampleRate);
                if (sp.modPhase > juce::MathConstants<float>::twoPi) sp.modPhase -= juce::MathConstants<float>::twoPi;
                const float d = (float) sp.len - 8.0f - 6.0f * std::sin (sp.modPhase);
                float rp = (float) sp.w - d;
                while (rp < 0) rp += (float) size;
                const int i0 = (int) rp;
                const float fr = rp - (float) i0;
                const float delayed = sp.buf[(size_t) i0] + fr * (sp.buf[(size_t) ((i0 + 1) % size)] - sp.buf[(size_t) i0]);

                // damping and DC/low cut in the loop
                sp.damp = delayed + dampCoef * (sp.damp - delayed);
                const float hp = sp.damp - sp.dcIn + dcCoef * sp.dc;
                sp.dcIn = sp.damp;
                sp.dc = hp;

                float v = x * 0.6f + hp * 0.83f;

                // dispersion: stretched first-order allpasses
                for (auto& ap : sp.ap)
                {
                    const float z = ap.buf[(size_t) ap.pos];
                    const float y = -kApCoef * v + z;
                    ap.buf[(size_t) ap.pos] = v + kApCoef * y;
                    if (++ap.pos >= (int) ap.buf.size()) ap.pos = 0;
                    v = y;
                }

                sp.buf[(size_t) sp.w] = fastTanh (v);
                if (++sp.w >= size) sp.w = 0;
                o[s] = delayed;
            }

            outL = 0.55f * (o[0] + 0.6f * o[2]);
            outR = 0.55f * (o[1] + 0.6f * o[2]);
        }

    private:
        static constexpr int kSprings = 3;
        static constexpr int kStages = 14;
        static constexpr float kApCoef = 0.62f;

        struct AP { std::vector<float> buf; int pos = 0; };
        struct Spring
        {
            std::vector<float> buf;
            int len = 1, w = 0;
            float modPhase = 0, damp = 0, dc = 0, dcIn = 0;
            std::array<AP, kStages> ap;
        };

        std::array<Spring, kSprings> springs;
        SVF inHP, inLP;
        float dampCoef = 0, dcCoef = 0;
        double sampleRate = 48000.0;
    };

    //==============================================================================
    class TapeEngine
    {
    public:
        static constexpr double maxDelayMs = 4000.0;   // per head-I spacing (synced up to 1/1 at 60 BPM)

        void prepare (double sr, int /*maxBlock*/)
        {
            sampleRate = sr;
            const int len = (int) (sr * (maxDelayMs * 3.0 * 1.05 / 1000.0)) + 16;
            for (auto& b : tape) b.assign ((size_t) len, 0.0f);
            bufLen = len;
            writePos = 0;

            wowRand.prepare (sr, 1.3, 11);
            flutRand.prepare (sr, 0.045, 23);
            dropRand.prepare (sr, 0.11, 37);
            ageRand.prepare (sr, 2.7, 41);
            noise.setSeed (77);

            reverb.prepare (sr);
            for (auto& r : rev) r = 0;

            lastSettings = -1.0;
            timeSmooth = -1.0;
            gains.reset (sr, 0.03);
            reset();
        }

        void reset()
        {
            for (auto& b : tape) std::fill (b.begin(), b.end(), 0.0f);
            for (int c = 0; c < 2; ++c)
            {
                toneLP[c].reset(); toneHP[c].reset(); bwLP[c].reset(); bump[c].reset();
                hissPrev[c] = 0;
            }
            reverb.reset();
            wowPhase = flut1 = flut2 = 0;
        }

        void setParams (const TapeParams& p) noexcept { params = p; }

        float getEchoPeak() const noexcept { return echoPeak; }
        float getCurrentTimeMs() const noexcept { return (float) (timeSmooth / sampleRate * 1000.0); }

        // Process stereo (or mono when right == nullptr) in place.
        void process (float* left, float* right, int n) noexcept
        {
            const auto& p = params;
            updateFilters();

            const auto heads = headsForPattern (p.pattern);
            int active = 0;
            for (bool h : heads) active += h ? 1 : 0;
            active = juce::jmax (1, active);
            const float echoGain = 1.0f / std::sqrt ((float) active);
            const float fbNorm = 1.0f / (float) active;

            const double targetSamples = juce::jlimit (1.0, maxDelayMs, (double) p.timeMs) * sampleRate / 1000.0;
            if (timeSmooth < 0) timeSmooth = targetSamples;
            const double timeCoef = 1.0 - std::exp (-1.0 / (0.18 * sampleRate));

            // modulation depths
            const float modeWow[]  = { 0.0f, 0.0006f, 0.0022f };
            const float modeFlut[] = { 0.0f, 0.00025f, 0.0007f };
            const int mode = juce::jlimit (0, 2, p.mode);
            const float wowAmt  = p.modulation ? p.wow * 0.006f + modeWow[mode] + p.age * 0.0012f : 0.0f;
            const float flutAmt = p.modulation ? p.flutter * 0.0019f + modeFlut[mode] : 0.0f;
            const float dropDepth = p.age * (mode == 2 ? 0.75f : mode == 1 ? 0.3f : 0.08f);

            // record drive
            const float typeDrive[] = { 1.0f, 0.7f, 1.4f };
            const int type = juce::jlimit (0, 2, p.type);
            const float driveDb = p.saturation * 18.0f + 20.0f * std::log10 (typeDrive[type]);
            const float drive = juce::Decibels::decibelsToGain (driveDb);
            const float driveComp = 1.0f / std::pow (drive, 0.75f);

            float noiseGain = p.noiseDb <= -89.9f ? 0.0f : juce::Decibels::decibelsToGain (p.noiseDb + (mode == 2 ? 6.0f : 0.0f));

            const float mix = juce::jlimit (0.0f, 1.0f, p.mix);
            gains.setTarget ({ p.wetOnly ? 0.0f : juce::jmin (1.0f, 2.0f * (1.0f - mix)),
                               juce::jmin (1.0f, 2.0f * mix),
                               p.feedback,
                               std::pow (juce::jlimit (0.0f, 1.0f, p.reverb), 1.5f) * 1.3f,
                               noiseGain });

            const float twoPi = juce::MathConstants<float>::twoPi;
            const float wowInc  = (float) (twoPi * 0.55 / sampleRate);
            const float fl1Inc  = (float) (twoPi * 7.3 / sampleRate);
            const float fl2Inc  = (float) (twoPi * 13.1 / sampleRate);
            const int numCh = right != nullptr ? 2 : 1;
            const bool pingPong = p.pingPong && numCh == 2;
            float* chans[2] = { left, right };
            float peak = 0.0f;

            for (int i = 0; i < n; ++i)
            {
                timeSmooth += (targetSamples - timeSmooth) * timeCoef;
                const auto g = gains.next();

                wowPhase += wowInc; if (wowPhase > twoPi) wowPhase -= twoPi;
                flut1 += fl1Inc;    if (flut1 > twoPi) flut1 -= twoPi;
                flut2 += fl2Inc;    if (flut2 > twoPi) flut2 -= twoPi;

                const float w = 0.65f * std::sin (wowPhase) + 0.55f * wowRand.next() + 0.3f * ageRand.next() * p.age;
                const float f = 0.6f * std::sin (flut1) + 0.25f * std::sin (flut2) + 0.35f * flutRand.next();
                const double speed = 1.0 + (double) (wowAmt * w + flutAmt * f);

                const float dv = 0.5f + 0.5f * dropRand.next();
                const float dropT = juce::jlimit (0.0f, 1.0f, (dv - 0.55f) / 0.45f);
                const float drop = 1.0f - dropDepth * dropT * dropT * (3.0f - 2.0f * dropT);

                float inMono = 0.0f;
                float echoOut[2] = { 0, 0 };

                // 1. playback: read the heads of each tape track
                float fbOut[2] = { 0, 0 };
                for (int c = 0; c < numCh; ++c)
                {
                    inMono += chans[c][i];
                    const auto& buf = tape[(size_t) c];

                    float sum = 0.0f;
                    for (int h = 0; h < 3; ++h)
                    {
                        if (! heads[(size_t) h]) continue;
                        const double d = timeSmooth * (double) (h + 1) * speed;
                        sum += readCubic (buf, d);
                    }
                    sum *= drop;

                    // playback EQ: head bump, tone and loop high-pass
                    float s = bump[c].process (sum);
                    s = toneLP[c].lowpass (s);
                    s = toneHP[c].highpass (s);

                    echoOut[c] = s * echoGain;
                    fbOut[c] = fastTanh (s * fbNorm * g.feedback);
                    peak = juce::jmax (peak, std::abs (echoOut[c]));
                }

                // 2. record. Ping pong: the input goes to the left track only and each
                //    track feeds the other one, so the repeats bounce L, R, L, R...
                for (int c = 0; c < numCh; ++c)
                {
                    float recIn;
                    if (pingPong)
                        recIn = c == 0 ? 0.5f * (chans[0][i] + chans[1][i]) + fbOut[1] : fbOut[0];
                    else
                        recIn = chans[c][i] + fbOut[c];

                    // saturation, bandwidth and hiss
                    const float rec = fastTanh (recIn * drive) * driveComp;
                    const float wn = noise.nextFloat() * 2.0f - 1.0f;
                    const float hiss = (wn - 0.65f * hissPrev[c]) * g.noise;
                    hissPrev[c] = wn;

                    tape[(size_t) c][(size_t) writePos] = bwLP[c].lowpass (rec) + hiss;
                }

                if (numCh == 1) echoOut[1] = echoOut[0];
                inMono /= (float) numCh;

                // spring reverb on the input and the echoes, like the classic units
                float rl = 0, rr = 0;
                if (g.reverb > 0.0001f)
                    reverb.process (inMono * 0.6f + 0.5f * (echoOut[0] + echoOut[1]), rl, rr);

                for (int c = 0; c < numCh; ++c)
                {
                    const float x = chans[c][i];
                    const float wet = echoOut[c] + (c == 0 ? rl : (numCh == 1 ? 0.5f * (rl + rr) : rr)) * g.reverb;
                    chans[c][i] = x * g.dry + wet * g.wet;
                }

                if (++writePos >= bufLen) writePos = 0;
            }

            echoPeak = peak;
        }

    private:
        struct Gains { float dry, wet, feedback, reverb, noise; };

        // 5 linearly smoothed gains
        struct GainSmoother
        {
            void reset (double sr, double seconds) { steps = juce::jmax (1, (int) (sr * seconds)); cur = tgt; left = 0; }
            void setTarget (Gains t)
            {
                if (first) { cur = tgt = t; first = false; return; }
                tgt = t; left = steps;
                inc = { (t.dry - cur.dry) / (float) steps, (t.wet - cur.wet) / (float) steps, (t.feedback - cur.feedback) / (float) steps,
                        (t.reverb - cur.reverb) / (float) steps, (t.noise - cur.noise) / (float) steps };
            }
            Gains next()
            {
                if (left > 0)
                {
                    --left;
                    cur.dry += inc.dry; cur.wet += inc.wet; cur.feedback += inc.feedback; cur.reverb += inc.reverb; cur.noise += inc.noise;
                    if (left == 0) cur = tgt;
                }
                return cur;
            }
            Gains cur {}, tgt {}, inc {};
            int steps = 1, left = 0;
            bool first = true;
        };

        inline float readCubic (const std::vector<float>& buf, double delay) const noexcept
        {
            double rp = (double) writePos - delay;
            while (rp < 0) rp += bufLen;
            const int i1 = (int) rp;
            const float t = (float) (rp - i1);
            const int i0 = i1 == 0 ? bufLen - 1 : i1 - 1;
            const int i2 = i1 + 1 >= bufLen ? 0 : i1 + 1;
            const int i3 = i2 + 1 >= bufLen ? 0 : i2 + 1;
            const float y0 = buf[(size_t) i0], y1 = buf[(size_t) i1], y2 = buf[(size_t) i2], y3 = buf[(size_t) i3];
            const float c1 = 0.5f * (y2 - y0);
            const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
            const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
            return ((c3 * t + c2) * t + c1) * t + y1;
        }

        void updateFilters()
        {
            const auto& p = params;
            const int mode = juce::jlimit (0, 2, p.mode);
            const int type = juce::jlimit (0, 2, p.type);
            const double key = p.tone * 1000.0 + mode * 10.0 + type + p.age * 0.001;
            if (std::abs (key - lastSettings) < 1.0e-9) return;
            lastSettings = key;

            const double typeBright[] = { 1.0, 1.35, 0.75 };
            const double toneHz = 1500.0 * std::pow (2.0, p.tone * 3.4) * typeBright[type];
            const double modeBw[] = { 18000.0, 9500.0, 5500.0 };
            const double bwHz = modeBw[mode] * typeBright[type] * (1.0 - 0.45 * p.age);
            const double hpHz[] = { 90.0, 110.0, 65.0 };
            const double bumpDb = (mode == 0 ? 0.5 : mode == 1 ? 1.5 : 2.0) + (type == 2 ? 1.0 : type == 1 ? -0.5 : 0.0);

            for (int c = 0; c < 2; ++c)
            {
                toneLP[c].set (sampleRate, toneHz, 0.6);
                toneHP[c].set (sampleRate, hpHz[type], 0.707);
                bwLP[c].set (sampleRate, bwHz, 0.707);
                bump[c].set (sampleRate, type == 2 ? 70.0 : 90.0, 0.9, bumpDb);
            }
        }

        TapeParams params;
        double sampleRate = 48000.0;
        std::array<std::vector<float>, 2> tape;
        int bufLen = 1, writePos = 0;
        double timeSmooth = -1.0, lastSettings = -1.0;

        SVF toneLP[2], toneHP[2], bwLP[2];
        Peak bump[2];
        float hissPrev[2] = { 0, 0 };

        SmoothRandom wowRand, flutRand, dropRand, ageRand;
        juce::Random noise;
        float wowPhase = 0, flut1 = 0, flut2 = 0;

        SpringReverb reverb;
        float rev[2] = { 0, 0 };
        GainSmoother gains;
        float echoPeak = 0.0f;
    };
}
