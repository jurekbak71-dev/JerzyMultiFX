#pragma once
#include "core/Model.h"
#include "vendor/muff/Circuit/AnalogCircuit.h"
#include "vendor/muff/MuffDSP.h"
#include <vector>
namespace multifx {
struct Analog {
    double sr = 48000;
    std::array<float, 2> dc{}, tone{}, last{}, aa{};
    muff::Engine fuzz;
    std::array<std::array<muff::circuit::AnalogCircuit, 2>, 14> circuits;
    std::array<float, 65> oversampleCoefficients{};
    std::array<std::array<float, 65>, 2> upHistory{}, downHistory{};
    std::array<int, 2> oversamplePosition{};
    int fuzzModel = 4;
    float fuzzVolume = .5f;
    bool channelsAligned = true;
    float filtered(std::array<float, 65> &history, int position, float input) {
        history[position] = input;
        float output = oversampleCoefficients[32] * history[(position - 32 + 65) % 65];
        for (int k = 0; k < 32; ++k)
            output += oversampleCoefficients[k] *
                      (history[(position - k + 65) % 65] + history[(position - (64 - k) + 65) % 65]);
        return output;
    }
    float env = 0, gain = 1, gr = 0, inPeak = 0, outPeak = 0;
    float gainSmoothing = .003f;
    float preGain = 1, preOut = 1, slew = .002f;
    float driveCut[2]{}, driveAA = 0;
    int driveMode = 0, preampModel = 0;
    float driveAmount = 0, driveTone = .5f, driveLevel = 1;
    float wantedPreGain = 1, wantedPreOut = 1, compInput = 1, compAttack = 0, compRelease = 0, compKnee = 0,
          compThreshold = 0, compRatio = 1, compMakeup = 0;
    void prepare(double rate) {
        sr = rate;
        gainSmoothing=float(1-std::exp(-1./(.006934*sr)));
        dc = {};
        tone = {};
        last = {};
        aa = {};
        fuzz.prepare(sr * 4);
        for (size_t model = 0; model < circuits.size(); ++model)
            for (auto &c : circuits[model])
                if (!c.prepare(sr * 4, model))
                    throw std::runtime_error("Circuit preparation failed");
        double sum = 0;
        for (int k = 0; k < 65; ++k) {
            const int t = k - 32;
            const double x =
                t == 0 ? .225 : std::sin(6.283185307179586 * .1125 * t) / (3.141592653589793 * t);
            const double window =
                .42 - .5 * std::cos(6.283185307179586 * k / 64) + .08 * std::cos(12.566370614359172 * k / 64);
            oversampleCoefficients[k] = float(x * window);
            sum += x * window;
        }
        for (auto &c : oversampleCoefficients)
            c /= float(sum);
        upHistory = {};
        downHistory = {};
        oversamplePosition = {};
        channelsAligned = true;
        preGain = preOut = 1;
        slew = float(1 - std::exp(-1 / (sr * .01)));
        env = 0;
        gain = 1;
        driveCut[0] = float(1 - std::exp(-6.2831853 * 720 / (sr * 4)));
        driveCut[1] = float(1 - std::exp(-6.2831853 * 120 / (sr * 4)));
        driveAA = float(1 - std::exp(-6.2831853 * .1));
    }
    void configure(const State &s) {
        const int p = layout().drive;
        driveMode = int(plain(p, s.values[p]));
        driveAmount = s.values[p + 1];
        driveTone = s.values[p + 2];
        driveLevel = db(plain(p + 3, s.values[p + 3]));
        fuzzModel = int(plain(layout().fuzzModel, s.values[layout().fuzzModel]));
        fuzzVolume = s.values[layout().fuzzVolume];
        const int pre = layout().preamp;
        preampModel = int(plain(pre, s.values[pre]));
        wantedPreGain = db(plain(pre + 1, s.values[pre + 1]));
        wantedPreOut = db(plain(pre + 2, s.values[pre + 2]));
        const int comp = layout().compressor;
        compInput = db(plain(comp + 4, s.values[comp + 4]));
        compAttack = std::exp(-1.f / (float(sr) * plain(comp, s.values[comp]) * .001f));
        compRelease = std::exp(-1.f / (float(sr) * plain(comp + 1, s.values[comp + 1]) * .001f));
        compKnee = plain(comp + 3, s.values[comp + 3]);
        compThreshold = s.values[comp + 2] * 42;
        compRatio = 1 + 9 * s.values[comp + 2];
        compMakeup = plain(comp + 5, s.values[comp + 5]);
    }
    void preamp(float &l, float &r, const State &s) {
        const int p = layout().preamp;
        (void)p;
        (void)s;
        const int model = preampModel;
        const float g = wantedPreGain, out = wantedPreOut;
        preGain += slew * (g - preGain);
        preOut += slew * (out - preOut);
        float *x[]{&l, &r};
        for (int c = 0; c < 2; ++c) {
            float v = *x[c] * preGain;
            if (model == 1)
                v = std::tanh(v * 1.3f) / 1.3f;
            else if (model == 2) {
                float y = (std::tanh(v + .25f) - std::tanh(.25f));
                dc[c] += .001f * (y - dc[c]);
                v = y - dc[c];
            }
            *x[c] = v * preOut;
        }
    }
    void drive(float &l, float &r, const State &s) {
        const int p = layout().drive;
        if (s.values[p + 5] >= .5)
            return;
        int mode = driveMode;
        float amount = driveAmount, t = driveTone, level = driveLevel;
        if (channelsAligned && l != r) {
            for (auto &model : circuits)
                model[1] = model[0];
            fuzz.mirrorChannel();
            upHistory[1] = upHistory[0];
            downHistory[1] = downHistory[0];
            oversamplePosition[1] = oversamplePosition[0];
            tone[1] = tone[0];
            aa[1] = aa[0];
            last[1] = last[0];
            channelsAligned = false;
        }
        float *x[]{&l, &r};
        for (int c = 0; c < (channelsAligned ? 1 : 2); ++c) {
            float target = *x[c], y = 0;
            for (int k = 0; k < 4; ++k) {
                const int position = oversamplePosition[c];
                upHistory[c][position] = k == 0 ? target * 4 : 0;
                float v = 0;
                for (int tap = k; tap < 65; tap += 4)
                    v += oversampleCoefficients[tap] * upHistory[c][(position - tap + 65) % 65];
                if (mode == 0)
                    y = fuzzModel < 6 ? fuzz.process(v, c, fuzzModel, amount, t) * 2 * fuzzVolume
                                      : circuits[size_t(fuzzModel - 6)][c].process(v, amount, t, fuzzVolume);
                else {
                    tone[c] += driveCut[mode == 1 ? 0 : 1] * (v - tone[c]);
                    float high = v - tone[c];
                    float g = 1 + amount * (mode == 1 ? 40 : 100);
                    float nonlinear = mode == 1 ? std::tanh(high * g) : std::clamp(high * g, -.65f, .65f);
                    if (mode == 2 && s.values[p + 4] >= .5)
                        nonlinear = std::tanh(nonlinear * 2 + high * 4);
                    y = nonlinear * (.4f + .6f * t) + tone[c] * (mode == 1 ? .35f : .1f);
                }
                downHistory[c][position] = y;
                if (k == 0)
                    aa[c] = filtered(downHistory[c], position, y);
                oversamplePosition[c] = (position + 1) % 65;
            }
            last[c] = target;
            *x[c] = aa[c] * level;
        }
        if (channelsAligned)
            r = l;
    }
    void comp(float &l, float &r, const State &s) {
        const int p = layout().compressor;
        float input = compInput;
        inPeak = std::max(std::abs(l), std::abs(r));
        if (s.values[p + 7] >= .5f) {
            gr = 0;
            outPeak = inPeak;
            return;
        }
        if (s.values[p + 7] < .5) {
            l *= input;
            r *= input;
            float peak = std::max(std::abs(l), std::abs(r));
            const float a = compAttack, rel = compRelease;
            env = (peak > env ? a : rel) * env + (1 - (peak > env ? a : rel)) * peak;
            float knee = compKnee, over = 20 * std::log10(std::max(env, 1.e-9f)) + compThreshold;
            float ratio = compRatio;
            gr = over > knee * .5f
                     ? (1 - 1 / ratio) * over
                     : (knee > 0 && over > -knee * .5f
                            ? (1 - 1 / ratio) * (over + knee * .5f) * (over + knee * .5f) / (2 * knee)
                            : 0);
            float target = db(-gr + compMakeup);
            gain += gainSmoothing * (target - gain);
            l *= gain;
            r *= gain;
        } else
            gr = 0;
        if (s.values[p + 6] >= .5) {
            constexpr float ceiling = .891250938f;
            float peak = std::max(std::abs(l), std::abs(r));
            if (peak > ceiling) {
                float g = ceiling / peak;
                l *= g;
                r *= g;
            }
        }
        outPeak = std::max(std::abs(l), std::abs(r));
    }
};
struct Digital {
    std::array<std::vector<float>, 2> ring;
    std::vector<uint32_t> epochs;
    uint32_t epoch = 1;
    void reset() {
        ++epoch;
        pos = 0;
        low = {};
        time = sr * .24;
    }

    size_t pos = 0;
    double sr = 48000, time = 240;
    std::array<float, 2> low{};
    double goal = 240, timeSmoothing = .0005;
    float coefficient = 0, feedback = 0;
    void prepare(double rate) {
        sr = rate;
        timeSmoothing=1-std::exp(-1./(.041656*sr));
        for (auto &a : ring)
            a.assign(size_t(sr * 2.1) + 8, 0);
        epochs.assign(ring[0].size(), 0);
        epoch = 1;
        pos = 0;
        low = {};
        time = sr * .24;
    }
    void configure(const State &s, int base, double bpm) {
        double ms = plain(base, s.values[base]);
        if (s.values[base + 4] >= .5) {
            constexpr double beats[]{.25, .5, 1, 2, 4, 8};
            ms = 60000 / std::max(20., bpm) * beats[int(plain(base + 5, s.values[base + 5]))];
        }
        goal = std::clamp(ms * sr * .001, 1., double(ring[0].size() - 4));
        coefficient = float(1 - std::exp(-6.2831853 * plain(base + 6, s.values[base + 6]) / sr));
        feedback = plain(base + 1, s.values[base + 1]);
    }
    std::array<float, 2> tick(float l, float r) {
        time += (goal - time) * timeSmoothing;
        float in[]{l, r};
        std::array<float, 2> out{};
        const float a = coefficient;
        for (int c = 0; c < 2; ++c) {
            double p = double(pos) - time;
            if (p < 0)
                p += ring[c].size();
            size_t k = size_t(p);
            const size_t next = (k + 1) % ring[c].size();
            const float first = epochs[k] == epoch ? ring[c][k] : 0,
                        second = epochs[next] == epoch ? ring[c][next] : 0;
            float d = first + (second - first) * float(p - k);
            low[c] += a * (d - low[c]);
            ring[c][pos] = std::clamp(in[c] + low[c] * feedback, -8.f, 8.f);
            out[c] = low[c];
        }
        epochs[pos] = epoch;
        pos = (pos + 1) % ring[0].size();
        return out;
    }
};
struct Reverb {
    // Cloud has its own six-stage stereo input diffuser. Allocation and epoch
    // storage live in prepare(); reset invalidates tails without clearing RAM
    // in the audio callback. Allpass gain stays below unity.
    struct CloudDiffuser {
        std::array<std::array<std::vector<float>, 2>, 6> ring;
        std::array<std::array<std::vector<uint32_t>, 2>, 6> stamps;
        std::array<std::array<size_t, 2>, 6> position{};
        void prepare(double rate) {
            constexpr double ms[6][2]{{3.7,4.9},{7.1,8.3},{11.3,12.7},
                                     {16.9,18.1},{21.1,23.3},{27.7,29.9}};
            for(int stage=0;stage<6;++stage)
                for(int ch=0;ch<2;++ch){
                    const size_t n=size_t(rate*ms[stage][ch]*.001)+1;
                    ring[stage][ch].assign(n,0);
                    stamps[stage][ch].assign(n,0);
                }
            position={};
        }
        void tick(float (&signal)[2], float gain, uint32_t epoch) {
            for(int stage=0;stage<6;++stage){
                // Orthogonal stereo rotation between stages preserves energy
                // while preventing identical left/right reflection patterns.
                const float l=signal[0],r=signal[1];
                signal[0]=(l+r)*.70710678f;
                signal[1]=(l-r)*.70710678f;
                for(int ch=0;ch<2;++ch){
                    auto& at=position[stage][ch];
                    auto& b=ring[stage][ch];
                    auto& stamp=stamps[stage][ch];
                    const float delayed=stamp[at]==epoch?b[at]:0.f;
                    const float output=delayed-gain*signal[ch];
                    b[at]=signal[ch]+gain*output;
                    stamp[at]=epoch;
                    signal[ch]=output;
                    at=(at+1)%b.size();
                }
            }
        }
    } cloudDiffuser;
    double sr = 48000, phase = 0, pitchPhase = 0;
    std::array<std::vector<float>, 8> lines;
    std::array<std::vector<uint32_t>, 8> lineEpoch;
    std::vector<uint32_t> preEpoch;
    std::array<uint32_t, 4096> pitchEpoch{};
    uint32_t epoch = 1;
    void reset() {
        ++epoch;
        pos = {};
        cloudDiffuser.position = {};
        low = {};
        high = {};
        pp = pitchPos = 0;
        phase = pitchPhase = 0;
    }

    std::array<size_t, 8> pos{};
    std::array<float, 8> low{}, high{};
    std::array<std::vector<float>, 2> predelay;
    size_t pp = 0;
    std::array<float, 4096> pitch{};
    size_t pitchPos = 0;
    float lowCoefficient = 0, highCoefficient = 0, outputGain = 1, feedbackGain = 0;
    double pitchRatio = 1;
    int preDelaySamples = 0;
    std::array<float, 8> lineFeedback{};
    std::array<double, 8> lineBase{}, phaseSine{}, phaseCosine{};
    void prepare(double rate) {
        sr = rate;
        cloudDiffuser.prepare(rate);
        constexpr double times[]{.0297, .0371, .0411, .0437, .0531, .0617, .0719, .0893};
        for (int i = 0; i < 8; ++i) {
            lines[i].assign(size_t(sr * times[i] * 1.8) + 8, 0);
            lineEpoch[i].assign(lines[i].size(), 0);
            phaseSine[i] = std::sin(6.2831853 * i * .13);
            phaseCosine[i] = std::cos(6.2831853 * i * .13);
        }
        for (auto &p : predelay)
            p.assign(size_t(sr * .21) + 8, 0);
        preEpoch.assign(predelay[0].size(), 0);
        epoch = 1;
        pitchEpoch = {};
        pos = {};
        cloudDiffuser.position = {};
        low = {};
        high = {};
        pp = pitchPos = 0;
        phase = pitchPhase = 0;
        pitch.fill(0);
    }
    void configure(const State &s) {
        const int p = layout().reverb;
        const float cutoff = plain(p + 6, s.values[p + 6]) * (1 - .75f * s.values[p + 4]);
        lowCoefficient = float(1 - std::exp(-6.2831853 * cutoff / sr));
        highCoefficient = float(1 - std::exp(-6.2831853 * plain(p + 5, s.values[p + 5]) / sr));
        outputGain = db(plain(p + 10, s.values[p + 10]));
        pitchRatio = std::pow(2., plain(p + 11, s.values[p + 11]) / 12);
        feedbackGain = float(std::exp(-6.907755 * .65 / (sr * plain(p + 1, s.values[p + 1]))));
        preDelaySamples = int(sr * .001 * plain(p + 2, s.values[p + 2]));
        const int mode = int(plain(p, s.values[p]));
        const double factor = (.35 + 1.3 * s.values[p + 3]) * (mode == 1 ? .55 : mode == 2 ? 1.2 : 1.);
        for (int i = 0; i < 8; ++i) {
            lineBase[i] = (lines[i].size() - 8) * factor * .65;
            if(mode==3)
                lineBase[i]*=1.35;
            lineBase[i]=std::clamp(lineBase[i],1.,double(lines[i].size())-(mode==3?sr*.0016+8.:16.));
            // RT60 follows the actual delay in every engine and at every Size.
            // A ring's allocated capacity is not the acoustic reflection time.
            lineFeedback[i]=float(std::exp(-6.907755*lineBase[i]/
                (sr*plain(p+1,s.values[p+1]))));
        }
    }
    std::array<float, 2> tick(float l, float r, const State &s) {
        int p = layout().reverb, mode = int(plain(p, s.values[p]));
        float mix = s.values[p + 9];
        std::array<float, 2> out{l, r};
        float in[]{l, r};
        const int pred = preDelaySamples;
        preEpoch[pp] = epoch;
        for (int c = 0; c < 2; ++c) {
            predelay[c][pp] = in[c];
            const size_t at = (pp + predelay[c].size() - size_t(std::max(0, pred))) % predelay[c].size();
            in[c] = preEpoch[at] == epoch ? predelay[c][at] : 0;
        }
        pp = (pp + 1) % predelay[0].size();
        if(mode==3)
            cloudDiffuser.tick(in,.55f+.23f*s.values[p+7],epoch);
        phase += .17 / sr;
        if (phase >= 1)
            phase -= 1;
        float a = lowCoefficient, hp = highCoefficient;
        std::array<float, 8> v{};
        const double sine = std::sin(6.2831853 * phase), cosine = std::cos(6.2831853 * phase);
        float sum = 0;
        for (int i = 0; i < 8; ++i) {
            double d = std::clamp(lineBase[i] +
                                      (sine * phaseCosine[i] + cosine * phaseSine[i]) *
                                          (mode==3 ? sr*.0015*s.values[p+8] : s.values[p+8]*8),
                                  1., double(lines[i].size() - 4));
            double at = double(pos[i]) - d;
            while (at < 0)
                at += lines[i].size();
            size_t k = size_t(at);
            const size_t next = (k + 1) % lines[i].size();
            const float first = lineEpoch[i][k] == epoch ? lines[i][k] : 0,
                        second = lineEpoch[i][next] == epoch ? lines[i][next] : 0;
            float x = first + (second - first) * float(at - k);
            low[i] += a * (x - low[i]);
            high[i] += hp * (low[i] - high[i]);
            v[i] = low[i] - high[i];
            sum += v[i];
        }
        float shifted = 0;
        if (mode == 4) {
            pitch[pitchPos] = sum * .125f;
            pitchEpoch[pitchPos] = epoch;
            double ratio = pitchRatio;
            pitchPhase += (1 - ratio) / 2048;
            pitchPhase -= std::floor(pitchPhase);
            for (int w = 0; w < 2; ++w) {
                double ph = pitchPhase + w * .5;
                ph -= std::floor(ph);
                double at = double(pitchPos) - 1 - ph * 2048;
                while (at < 0)
                    at += pitch.size();
                size_t k = size_t(at);
                const size_t next = (k + 1) % pitch.size();
                const float first = pitchEpoch[k] == epoch ? pitch[k] : 0,
                            second = pitchEpoch[next] == epoch ? pitch[next] : 0;
                float x = first + (second - first) * float(at - k);
                shifted += x * float(.5 - .5 * std::cos(6.2831853 * ph));
            }
            pitchPos = (pitchPos + 1) % pitch.size();
        }
        for (int i = 0; i < 8; ++i) {
            float orth = v[i] - sum * .25f;
            const float feedback = lineFeedback[i];
            float diffuse = mode==3 ? 1.f : s.values[p + 7];
            float network = orth * diffuse + v[(i + 3) % 8] * (1 - diffuse);
            if (mode == 4) {
                // Convex injection BEFORE the decay coefficient: pitch feedback
                // must not create an independent, uncontrolled feedback path.
                const float amount=plain(p+12,s.values[p+12])*.75f;
                network=network*(1-amount)+shifted*amount;
            }
            const float feed=network*feedback;
            lines[i][pos[i]] = std::clamp((in[i % 2]) * .25f + feed, -8.f, 8.f);
            lineEpoch[i][pos[i]] = epoch;
            pos[i] = (pos[i] + 1) % lines[i].size();
        }
        float wetL = (v[0] + v[2] + v[4] + v[6]) * .5f, wetR = (v[1] + v[3] + v[5] + v[7]) * .5f;
        if(mode==3){
            // Signed orthogonal output taps give a wide, non-mono late field.
            wetL=(v[0]+v[2]-v[4]-v[6])*.5f;
            wetR=(v[1]-v[3]+v[5]-v[7])*.5f;
        }
        if (mode == 0) {
            wetL += v[1] * .2f;
            wetR += v[0] * .2f;
        }
        float gain = outputGain;
        out[0] = (l * (1 - mix) + wetL * mix) * gain;
        out[1] = (r * (1 - mix) + wetR * mix) * gain;
        return out;
    }
};
} // namespace multifx
