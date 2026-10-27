#pragma once
#include "Modules.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
namespace multifx {
constexpr int maxParameters = 2048;
struct Parameter {
    std::string key, name, unit, choices;
    float initial = 0, minimum = 0, maximum = 1;
    int steps = 0;
};
struct Group {
    const JAModule *api = nullptr;
    int start = 0;
    std::string name;
};
struct Layout {
    std::vector<Parameter> parameters;
    std::array<Group, 4> processors;
    std::array<std::array<Group, 4>, 2> delays;
    int order, bypass, blockBypass[5], preamp, drive, compressor, delayMode, delayType[2], digital[2], reverb,
        ir, midiLearn, midiMap, controlCount, factoryProgram;
    int add(std::string key, std::string name, float value = 0, float lo = 0, float hi = 1, int steps = 0,
            std::string choices = "", std::string unit = "") {
        int i = int(parameters.size());
        parameters.push_back({key, name, unit, choices, value, lo, hi, steps});
        return i;
    }
    Group module(std::string prefix, const JAModule *m) {
        Group g{m, int(parameters.size()), prefix};
        for (unsigned i = 0; i < m->parameter_count; ++i) {
            auto &p = m->parameters[i];
            add(prefix + "." + p.id, prefix + " / " + p.label,
                (p.initial - p.minimum) / std::max(.00001f, p.maximum - p.minimum), p.minimum, p.maximum,
                p.integer ? int(p.maximum - p.minimum) : 0, p.choices, p.unit);
        }
        return g;
    }
    Layout() {
        order = add("chain.order", "Kolejnosc 5 blokow", 0, 0, 119, 119, "");
        bypass = add("master.bypass", "Master Bypass", 0, 0, 1, 1);
        for (int i = 0; i < 5; ++i)
            blockBypass[i] = add("block." + std::to_string(i) + ".bypass",
                                 "Block " + std::to_string(i + 1) + " Bypass", 0, 0, 1, 1);
        preamp = int(parameters.size());
        add("cpd.model", "CPD Model", 0, 0, 2, 2, "Clean|Solid State|Tube");
        add("cpd.gain", "CPD Gain", 0, 0, 24, 0, "", "dB");
        add("cpd.out", "CPD Out", .5, -24, 24, 0, "", "dB");
        processors[0] = module("td", module_tape());
        drive = int(parameters.size());
        add("drive.type", "DRIVE Type", 0, 0, 2, 2, "Fuzz|TS808 Overdrive|DS Distortion");
        add("drive.gain", "DRIVE Gain", .3);
        add("drive.tone", "DRIVE Tone", .5);
        add("drive.level", "DRIVE Level", .5, -24, 12);
        add("drive.turbo", "DS Turbo", 0, 0, 1, 1);
        add("drive.bypass", "DRIVE Bypass", 1, 0, 1, 1);
        compressor = int(parameters.size());
        add("comp.attack", "COMP Attack", .3, .2, 100);
        add("comp.release", "COMP Release", .35, 20, 1200);
        add("comp.amount", "COMP Compression", .3);
        add("comp.knee", "COMP Knee", .5, 0, 18);
        add("comp.input", "COMP Input", .5, -24, 24);
        add("comp.makeup", "COMP Makeup", 0, 0, 24);
        add("comp.limiter", "COMP Limiter -1 dBFS", 1, 0, 1, 1);
        add("comp.bypass", "COMP Bypass", 1, 0, 1, 1);
        processors[1] = module("clone", module_clone());
        processors[2] = module("attack", module_attack());
        processors[3] = module("atomic", module_atomic());
        // Musical neutral initial graph. Engines remain independently switchable.
        for (int j = 1; j < 4; ++j)
            for (unsigned p = 0; p < processors[j].api->parameter_count; ++p)
                if (std::string(processors[j].api->parameters[p].id) == "bypass")
                    parameters[processors[j].start + p].initial = 1;
        delayMode = add("md.routing", "MD Routing", 0, 0, 2, 2,
                        "Serial A > B|Parallel independent levels|Mix A/B crossfade");
        for (int s = 0; s < 2; ++s) {
            std::string prefix = s ? "delayB" : "delayA";
            delayType[s] = add(prefix + ".type", 0 == s ? "DELAY A Type" : "DELAY B Type", 0, 0, 4, 4,
                               "Digital|MX (all engines)|Memory|Reel|Oil Drum");
            digital[s] = int(parameters.size());
            add(prefix + ".time", "Digital Time", .2, 1, 2000, 0, "", "ms");
            add(prefix + ".feedback", "Digital Feedback", .3, 0, .95);
            add(prefix + ".mix", "Slot Dry/Wet", 0);
            add(prefix + ".level", "Slot Output", .5, -24, 24);
            add(prefix + ".sync", "Digital Sync", 0, 0, 1, 1);
            add(prefix + ".division", "Digital Division", .5, 0, 5, 5, "1/16|1/8|1/4|1/2|1bar|2bars");
            add(prefix + ".filter", "Digital Low Pass", .8, 200, 18000);
            delays[s][0] = module(prefix + ".mx", module_mx());
            delays[s][1] = module(prefix + ".memory", module_memory());
            delays[s][2] = module(prefix + ".reel", module_reel());
            delays[s][3] = module(prefix + ".oil", module_oil());
        }
        add("md.crossfade", "MD A/B Crossfade", .5);
        reverb = int(parameters.size());
        add("reverb.engine", "REVERB Engine", 0, 0, 4, 4, "Plate|Room|Cathedral|Cloud|Shimmer");
        add("reverb.decay", "Reverb Decay", .4, .2, 20);
        add("reverb.predelay", "Pre-delay", 0, 0, 200);
        add("reverb.size", "Size", .5);
        add("reverb.damping", "Damping", .5);
        add("reverb.lowcut", "Low Cut", 0, 20, 1000);
        add("reverb.highcut", "High Cut", .8, 1000, 20000);
        add("reverb.diffusion", "Diffusion", .7);
        add("reverb.modulation", "Modulation", .2);
        add("reverb.mix", "Reverb Mix", 0);
        add("reverb.output", "Reverb Output", .5, -24, 24);
        add("reverb.pitch", "Shimmer pitch", .5, -12, 12);
        add("reverb.feedback", "Shimmer Feedback", .3, 0, .5);
        ir = int(parameters.size());
        add("ir.mix", "IR Dry/Wet", 0);
        add("ir.output", "IR Output", .5, -24, 24);
        controlCount = int(parameters.size());
        midiLearn =
            add("midi.learn", "MIDI Learn target (-1 off)", 0, -1, float(controlCount - 1), controlCount);
        midiMap = int(parameters.size());
        for (int cc = 0; cc < 128; ++cc)
            add("midi.cc." + std::to_string(cc), "MIDI CC " + std::to_string(cc) + " target", 0, -1,
                float(controlCount - 1), controlCount);
        factoryProgram = add("factory.program", "Factory Program", 0, 0, 7, 7,
                             "Clean|Warm Tape|TS808|Green Russian Fuzz|Chorus|Digital Echo|Plate|Shimmer");
    }
};
inline const Layout &layout() {
    static const Layout l;
    return l;
}
struct State {
    std::array<float, maxParameters> values{};
    std::array<int32_t, 128> cc{};
    State() {
        auto &l = layout();
        for (size_t i = 0; i < l.parameters.size(); ++i)
            values[i] = l.parameters[i].initial;
        cc.fill(-1);
    }
};
inline float clean(float v) {
    return std::isfinite(v) ? std::clamp(v, 0.f, 1.f) : 0.f;
}
inline float plain(int id, float normalized) {
    const auto &p = layout().parameters.at(id);
    float v = p.minimum + (p.maximum - p.minimum) * clean(normalized);
    return p.steps ? std::round(v) : v;
}
inline float db(float v) {
    return std::pow(10.f, v * .05f);
}
inline std::array<int, 5> permutation(int rank) {
    std::array<int, 5> a{0, 1, 2, 3, 4};
    for (int i = 0; i < std::clamp(rank, 0, 119); ++i)
        std::next_permutation(a.begin(), a.end());
    return a;
}
} // namespace multifx
