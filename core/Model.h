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
    int cpdBypass, slotBypass[2], masterGain, outputLimiter, fuzzModel, fuzzVolume, attackTrigger,
        attackExpression, attackExpressionEnabled, gateEnabled, gateThreshold, gateRelease, attackMakeup;
    std::array<std::array<int, 3>, 8> attackExpressionMap{};
    std::array<int, 4> processorBypass{};
    std::array<std::array<int, 2>, 2> mxAlgorithms{};
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
        add("comp.makeup", "COMP Makeup", .5, -24, 24, 0, "", "dB");
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
            for (size_t id = 0; id < parameters.size(); ++id)
                for (int slot = 0; slot < 2; ++slot)
                    if (parameters[id].key == prefix + ".mx.slot" + std::to_string(slot) + "_algorithm")
                        mxAlgorithms[s][slot] = int(id);
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
        add("reverb.pitch", "Shimmer pitch", 1, -12, 12);
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
        // Append only: existing automation/state IDs and CC normalization remain stable.
        cpdBypass = add("cpd.bypass", "PREAMP Bypass", 0, 0, 1, 1);
        slotBypass[0] = add("delayA.bypass", "DELAY A Bypass", 0, 0, 1, 1);
        slotBypass[1] = add("delayB.bypass", "DELAY B Bypass", 0, 0, 1, 1);
        masterGain = add("master.output", "Output", 60.f / 66.f, -60, 6, 0, "", "dB");
        outputLimiter = add("master.limiter", "Output protection", 1, 0, 1, 1);
        fuzzModel = add(
            "fuzz.model", "FUZZ Circuit", 4.f / 19, 0, 19, 19,
            "Legacy Triangle|Legacy Ram's Head|Legacy Op-Amp|Legacy Civil War|Legacy Green Russian|Legacy "
            "NYC|V1 66#5|Triangle 66#8|Triangle 66#6|Triangle 66#11|Jordan Creator 6000|V1 67#1|Ace Tone "
            "FM-3 Distortion|Ace Tone FM-3 Boost|Triangle 72#1|Triangle 72#2 PNP|Elk / Electro Sound "
            "Sustainar|Sekova Model 2015|Ram's Head 73#18|Colorsound Supa ToneBender");
        fuzzVolume = add("fuzz.volume", "FUZZ Volume", .5);
        attackTrigger = add("attack.trigger", "Manual trigger", 0, 0, 1, 1);
        attackExpression = add("attack.expression", "Expression / CC11", 1);
        attackExpressionEnabled = add("attack.expressionEnabled", "Enable expression", 0, 0, 1, 1);
        const char *targets[]{"Volume",      "Blend",    "Attack", "Decay",
                              "Sensitivity", "Harmonix", "Tone",   "Harmonix Volume"};
        for (int j = 0; j < 8; ++j) {
            const std::string key = "attack.expr" + std::to_string(j) + ".";
            attackExpressionMap[j][0] =
                add(key + "on", std::string(targets[j]) + " expression", j == 0 ? 1 : 0, 0, 1, 1);
            attackExpressionMap[j][1] = add(key + "heel", std::string(targets[j]) + " heel", 0);
            attackExpressionMap[j][2] = add(key + "toe", std::string(targets[j]) + " toe", 1);
        }
        gateEnabled = add("drive.gate.on", "Noise Gate ON", 0, 0, 1, 1);
        gateThreshold = add("drive.gate.threshold", "Gate Threshold", .28f, -80, -20, 0, "", "dB");
        gateRelease = add("drive.gate.release", "Gate Release", .2f, 20, 800, 0, "", "ms");
        attackMakeup = add("attack.makeup", "Attack Compressor Makeup", .5f, -12, 12, 0, "", "dB");
        // Version-2 states use a fixed MIDI address span; appended controls keep their IDs.
        controlCount = maxParameters;
        parameters[midiLearn].maximum = maxParameters - 1;
        parameters[midiLearn].steps = maxParameters;
        for (int cc = 0; cc < 128; ++cc) {
            parameters[midiMap + cc].maximum = maxParameters - 1;
            parameters[midiMap + cc].steps = maxParameters;
        }

        for (int j = 0; j < 4; ++j) {
            processorBypass[j] = -1;
            for (unsigned k = 0; k < processors[j].api->parameter_count; ++k)
                if (std::string(processors[j].api->parameters[k].id) == "bypass")
                    processorBypass[j] = processors[j].start + int(k);
        }
        if (processorBypass[0] >= 0)
            parameters[processorBypass[0]].initial = 1;
        auto annotate = [&](const std::string &key, const std::string &choices) {
            for (auto &d : parameters)
                if (d.key == key) {
                    d.choices = choices;
                    d.steps = int(std::count(choices.begin(), choices.end(), '|'));
                }
        };
        annotate(
            "clone.model",
            "Small Clone|Clone Theory SAD|Clone Theory MN|Electric Mistress V2|Electric Mistress V3|Electric "
            "Mistress V4|Electric Mistress V5|Electric Mistress V6|Deluxe Mistress V1|Deluxe Mistress "
            "V2|Deluxe Mistress V3|Deluxe Mistress V4|Deluxe Mistress V5|Stereo Mistress Inspired");
        annotate("clone.division", "4 bars|2 bars|1 bar|1/2 D|1/2|1/2 T|1/4 D|1/4|1/4 T|1/8 D|1/8|1/8 T|1/16 "
                                   "D|1/16|1/16 T|1/32|1/32 T|1/64");
        annotate("atomic.motion", "Cluster|Arpeggio|Random harmonic");
        annotate("atomic.division", "1/16|1/8|1/4|1/2|1 bar");
        annotate("attack.attackShape", "Linear|Parabolic|S-Curve");
        annotate("attack.decayShape", "Linear|Parabolic|S-Curve");
        annotate("attack.triggerMode", "Auto|Manual|MIDI");
        for (auto &d : parameters) {
            if (d.key.ends_with(".bypass") || d.key == "clone.filter" || d.key == "clone.sync" ||
                d.key == "atomic.sync" || d.key == "atomic.tapEnabled" || d.key == "attack.poly" ||
                d.key == "attack.fuzz" || d.key == "attack.compMono" || d.key == "attack.compPoly" ||
                d.key == "attack.continuousMono" || d.key == "attack.continuousPoly" ||
                d.key == "attack.useReturn" || d.key == "attack.returnBypass" ||
                d.key == "attack.fuzzOnReturn")
                d.steps = 1;
        }
        for (const std::string slot : {"delayA", "delayB"}) {
            annotate(slot + ".memory.model",
                     "EH7500 / SAD|Issue A / SAD|Squelch / SAD|Deluxe / MN|Boy / BL|Toy / BL");
            annotate(slot + ".memory.division",
                     "1/1|1/1 D|1/1 T|1/2|1/2 D|1/2 T|1/4|1/4 D|1/4 T|1/8|1/8 D|1/8 T|1/16|1/16 D|1/16 "
                     "T|1/32|1/32 D|1/32 T|1/64|1/64 D|1/64 T");
            annotate(slot + ".mx.routing", "Series|Parallel|Split L/R");
            annotate(slot + ".mx.machineCondition", "Serviced|Used|Worn|Damaged");
            for (int fx = 0; fx < 4; ++fx) {
                annotate(slot + ".mx.fx" + std::to_string(fx) + "_type",
                         "Off|Chorus|Flanger|LoFi|Overdrive|Granular");
                annotate(slot + ".mx.fx" + std::to_string(fx) + "_route", "Input / Pre|Wet only");
            }
            for (int j = 0; j < 2; ++j) {
                const auto pre = slot + ".mx.slot" + std::to_string(j) + "_";
                annotate(pre + "algorithm",
                         "Volante|El Capistan|Olivera|EC-1|Brig|Deco|DIG|Space Echo 202|DM-101");
                annotate(pre + "division", "1/1|1/2 D|1/2|1/4 D|1/4|1/8 D|1/8|1/4 T|1/16|1/8 T");
                annotate(pre + "algo4_0", "3205|3005|MULTI");
                annotate(pre + "algo1_7", "FIXED|MULTI|SINGLE");
                annotate(pre + "algo5_3", "SUM|INVERT|BOUNCE");
                annotate(pre + "algo6_0", "24/96|ADM|12 BIT");
                annotate(pre + "algo8_0", "CLASSIC|VINTAGE|MODERN|MULTI-HEAD|NON-LINEAR|AMBIENCE|REFLECT|"
                                          "DOUBLING+DELAY|WIDE|DUAL MOD|PAN|PATTERN");
                annotate(pre + "enable", "Off|On");
                if (j == 1)
                    for (auto &d : parameters)
                        if (d.key == pre + "enable")
                            d.initial = 0;
                annotate(pre + "sync", "Free|Sync");
            }
        }
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
