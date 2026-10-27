#pragma once
#include "core/Model.h"
#include "vendor/muff/MuffDSP.h"
#include <vector>
namespace multifx {
struct Analog {
    double sr = 48000;
    std::array<float, 2> dc{}, tone{}, last{}, aa{};
    muff::Engine fuzz;
    float env = 0, gain = 1, gr = 0, inPeak = 0, outPeak = 0;
    void prepare(double rate) {
        sr = rate;
        dc = {};
        tone = {};
        last = {};
        aa = {};
        fuzz.prepare(sr * 4);
        env = 0;
        gain = 1;
    }
    void preamp(float &l, float &r, const State &s) {
        const int p = layout().preamp;
        int model = int(plain(p, s.values[p]));
        float g = db(plain(p + 1, s.values[p + 1])), out = db(plain(p + 2, s.values[p + 2]));
        float *x[]{&l, &r};
        for (int c = 0; c < 2; ++c) {
            float v = *x[c] * g;
            if (model == 1)
                v = std::tanh(v * 1.3f) / 1.3f;
            else if (model == 2) {
                float y = (std::tanh(v + .25f) - std::tanh(.25f));
                dc[c] += .001f * (y - dc[c]);
                v = y - dc[c];
            }
            *x[c] = v * out;
        }
    }
    void drive(float &l, float &r, const State &s) {
        const int p = layout().drive;
        if (s.values[p + 5] >= .5)
            return;
        int mode = int(plain(p, s.values[p]));
        float amount = s.values[p + 1], t = s.values[p + 2], level = db(plain(p + 3, s.values[p + 3]));
        float *x[]{&l, &r};
        for (int c = 0; c < 2; ++c) {
            float target = *x[c], y = 0;
            for (int k = 1; k <= 4; ++k) {
                float v = last[c] + (target - last[c]) * k * .25f;
                if (mode == 0)
                    y = fuzz.process(v, c, 4, amount, t);
                else {
                    float cut = mode == 1 ? 720.f : 120.f;
                    tone[c] += float(1 - std::exp(-6.2831853 * cut / (sr * 4))) * (v - tone[c]);
                    float high = v - tone[c];
                    float g = 1 + amount * (mode == 1 ? 40 : 100);
                    float nonlinear = mode == 1 ? std::tanh(high * g) : std::clamp(high * g, -.65f, .65f);
                    if (mode == 2 && s.values[p + 4] >= .5)
                        nonlinear = std::tanh(nonlinear * 2 + high * 4);
                    y = nonlinear * (.4f + .6f * t) + tone[c] * (mode == 1 ? .35f : .1f);
                }
                aa[c] += float(1 - std::exp(-6.2831853 * sr * .4 / (sr * 4))) * (y - aa[c]);
            }
            last[c] = target;
            *x[c] = aa[c] * level * .6f;
        }
    }
    void comp(float &l, float &r, const State &s) {
        const int p = layout().compressor;
        float input = db(plain(p + 4, s.values[p + 4]));
        inPeak = std::max(std::abs(l), std::abs(r));
        if (s.values[p + 7] < .5) {
            l *= input;
            r *= input;
            float peak = std::max(std::abs(l), std::abs(r));
            float a = std::exp(-1.f / (float(sr) * plain(p, s.values[p]) * .001f));
            float rel = std::exp(-1.f / (float(sr) * plain(p + 1, s.values[p + 1]) * .001f));
            env = (peak > env ? a : rel) * env + (1 - (peak > env ? a : rel)) * peak;
            float knee = plain(p + 3, s.values[p + 3]),
                  over = 20 * std::log10(std::max(env, 1.e-9f)) + s.values[p + 2] * 42;
            float ratio = 1 + 9 * s.values[p + 2];
            gr = over > knee * .5f
                     ? (1 - 1 / ratio) * over
                     : (knee > 0 && over > -knee * .5f
                            ? (1 - 1 / ratio) * (over + knee * .5f) * (over + knee * .5f) / (2 * knee)
                            : 0);
            float target = db(-gr + plain(p + 5, s.values[p + 5]));
            gain += .003f * (target - gain);
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
    size_t pos = 0;
    double sr = 48000, time = 240;
    std::array<float, 2> low{};
    void prepare(double rate) {
        sr = rate;
        for (auto &a : ring)
            a.assign(size_t(sr * 2.1) + 8, 0);
        pos = 0;
        low = {};
        time = sr * .24;
    }
    std::array<float, 2> tick(float l, float r, const State &s, int base, double bpm) {
        double ms = plain(base, s.values[base]);
        if (s.values[base + 4] >= .5) {
            constexpr double beats[]{.25, .5, 1, 2, 4, 8};
            ms = 60000 / std::max(20., bpm) * beats[int(plain(base + 5, s.values[base + 5]))];
        }
        double goal = std::clamp(ms * sr * .001, 1., double(ring[0].size() - 4));
        time += (goal - time) * .0005;
        float in[]{l, r};
        std::array<float, 2> out{};
        float a = float(1 - std::exp(-6.2831853 * plain(base + 6, s.values[base + 6]) / sr));
        for (int c = 0; c < 2; ++c) {
            double p = double(pos) - time;
            if (p < 0)
                p += ring[c].size();
            size_t k = size_t(p);
            float d = ring[c][k] + (ring[c][(k + 1) % ring[c].size()] - ring[c][k]) * float(p - k);
            low[c] += a * (d - low[c]);
            ring[c][pos] = std::clamp(in[c] + low[c] * plain(base + 1, s.values[base + 1]), -8.f, 8.f);
            out[c] = low[c];
        }
        pos = (pos + 1) % ring[0].size();
        return out;
    }
};
struct Reverb {
    double sr = 48000, phase = 0, pitchPhase = 0;
    std::array<std::vector<float>, 8> lines;
    std::array<size_t, 8> pos{};
    std::array<float, 8> low{}, high{};
    std::array<std::vector<float>, 2> predelay;
    size_t pp = 0;
    std::array<float, 4096> pitch{};
    size_t pitchPos = 0;
    void prepare(double rate) {
        sr = rate;
        constexpr double times[]{.0297, .0371, .0411, .0437, .0531, .0617, .0719, .0893};
        for (int i = 0; i < 8; ++i)
            lines[i].assign(size_t(sr * times[i] * 1.8) + 8, 0);
        for (auto &p : predelay)
            p.assign(size_t(sr * .21) + 8, 0);
        pos = {};
        low = {};
        high = {};
        pp = pitchPos = 0;
        phase = pitchPhase = 0;
        pitch.fill(0);
    }
    std::array<float, 2> tick(float l, float r, const State &s) {
        int p = layout().reverb, mode = int(plain(p, s.values[p]));
        float mix = s.values[p + 9];
        std::array<float, 2> out{l, r};
        float in[]{l, r};
        int pred =
            int(sr * .001 * plain(p + 1 + 1, s.values[p + 2])); // corrected below: parameter +2 is pre-delay
        for (int c = 0; c < 2; ++c) {
            predelay[c][pp] = in[c];
            in[c] = predelay[c][(pp + predelay[c].size() - size_t(std::max(0, pred))) % predelay[c].size()];
        }
        pp = (pp + 1) % predelay[0].size();
        phase += .17 / sr;
        if (phase >= 1)
            phase -= 1;
        float cutoff = plain(p + 6, s.values[p + 6]) * (1 - .75f * s.values[p + 4]);
        float a = float(1 - std::exp(-6.2831853 * cutoff / sr)),
              hp = float(1 - std::exp(-6.2831853 * plain(p + 5, s.values[p + 5]) / sr));
        std::array<float, 8> v{};
        float sum = 0;
        for (int i = 0; i < 8; ++i) {
            double factor = (.35 + 1.3 * s.values[p + 3]) * (mode == 1 ? .55 : mode == 2 ? 1.2 : 1.);
            double d = std::clamp((lines[i].size() - 8) * factor * .65 +
                                      std::sin(6.2831853 * (phase + i * .13)) * s.values[p + 8] * 8,
                                  1., double(lines[i].size() - 4));
            double at = double(pos[i]) - d;
            while (at < 0)
                at += lines[i].size();
            size_t k = size_t(at);
            float x = lines[i][k] + (lines[i][(k + 1) % lines[i].size()] - lines[i][k]) * float(at - k);
            low[i] += a * (x - low[i]);
            high[i] += hp * (low[i] - high[i]);
            v[i] = low[i] - high[i];
            sum += v[i];
        }
        float shifted = 0;
        if (mode == 4 || mode == 3) {
            pitch[pitchPos] = sum * .125f;
            double ratio = std::pow(2., plain(p + 11, s.values[p + 11]) / 12);
            if (mode == 3)
                ratio = 1.004;
            pitchPhase += (1 - ratio) / 2048;
            pitchPhase -= std::floor(pitchPhase);
            for (int w = 0; w < 2; ++w) {
                double ph = pitchPhase + w * .5;
                ph -= std::floor(ph);
                double at = double(pitchPos) - 1 - ph * 2048;
                while (at < 0)
                    at += pitch.size();
                size_t k = size_t(at);
                float x = pitch[k] + (pitch[(k + 1) % pitch.size()] - pitch[k]) * float(at - k);
                shifted += x * float(.5 - .5 * std::cos(6.2831853 * ph));
            }
            pitchPos = (pitchPos + 1) % pitch.size();
        }
        for (int i = 0; i < 8; ++i) {
            float orth = v[i] - sum * .25f;
            float decay = plain(p + 1, s.values[p + 1]);
            float feedback =
                std::min(.985f, float(std::exp(-6.907755 * lines[i].size() * .65 / (sr * decay))));
            float diffuse = s.values[p + 7];
            float feed = (orth * diffuse + v[(i + 3) % 8] * (1 - diffuse)) * feedback;
            if (mode >= 3)
                feed += shifted * plain(p + 12, s.values[p + 12]) * .15f;
            lines[i][pos[i]] = std::clamp((in[i % 2]) * .25f + feed, -8.f, 8.f);
            pos[i] = (pos[i] + 1) % lines[i].size();
        }
        float wetL = (v[0] + v[2] + v[4] + v[6]) * .5f, wetR = (v[1] + v[3] + v[5] + v[7]) * .5f;
        if (mode == 0) {
            wetL += v[1] * .2f;
            wetR += v[0] * .2f;
        }
        float gain = db(plain(p + 10, s.values[p + 10]));
        out[0] = (l * (1 - mix) + wetL * mix) * gain;
        out[1] = (r * (1 - mix) + wetR * mix) * gain;
        return out;
    }
};
} // namespace multifx
