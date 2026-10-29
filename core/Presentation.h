#pragma once
#include "Model.h"
#include "ControlUI.h"
#include <cstdio>
#include <string_view>
namespace multifx {
inline int parameterId(std::string_view key) {
    const auto &ps = layout().parameters;
    for (size_t i = 0; i < ps.size(); ++i)
        if (ps[i].key == key)
            return int(i);
    return -1;
}
struct PanelSelection {
    int block = 0, section = 0, mxSlot = 0;
    bool advanced = false;
};
inline constexpr const char *mdEngineChoices =
    "Digital|Volante / MultiHead Reel|El Capistan / Tape Echo|Olivera / Oil Can|EC-1 / Tube Echo|Brig / "
    "BBD|Deco / Doubletrack|DIG / Dual Digital|Space Echo 202|DM-101|Memory Delay|Reel Echo|Oil Drum Echo";
inline const char *blockName(int block) {
    const char *names[]{"ANALOG", "CHORUS / ATTACK / ATOMIC", "MD MULTIDELAY", "REVERB", "IR LOADER"};
    return names[std::clamp(block, 0, 4)];
}
inline std::vector<std::string> sectionNames(int block) {
    if (block == 0)
        return {"PREAMP", "TAPE DRIVE", "DRIVE", "COMPRESSOR"};
    if (block == 1)
        return {"CHORUS", "ATTACK DECAY", "ATOMIC CLUSTER"};
    if (block == 2)
        return {"DELAY A", "DELAY B"};
    return {};
}
inline std::vector<int> panelParameters(PanelSelection view, const State &s) {
    const auto &lay = layout();
    std::vector<int> result;
    auto add = [&](const std::string &key) {
        int id = parameterId(key);
        if (id >= 0)
            result.push_back(id);
    };
    auto group = [&](const std::string &prefix) {
        for (size_t i = 0; i < lay.parameters.size(); ++i)
            if (lay.parameters[i].key.starts_with(prefix))
                result.push_back(int(i));
    };
    auto ordered = [&](const std::string &prefix, std::initializer_list<const char *> keys) {
        for (auto key : keys)
            add(prefix + key);
    };
    if (view.block == 0) {
        if (view.section == 2) {
            ordered("drive.", {"type", "gain", "tone", "level", "bypass", "gate.on", "gate.threshold", "gate.release"});
            const int mode = int(plain(lay.drive, s.values[lay.drive]));
            if (mode == 0) {
                add("fuzz.model");
                add("fuzz.volume");
            } else if (mode == 2)
                add("drive.turbo");
        } else
            group(
                std::array<std::string, 4>{"cpd.", "td.", "drive.", "comp."}[std::clamp(view.section, 0, 3)]);
    } else if (view.block == 1) {
        if (view.section == 0)
            ordered("clone.", {"model", "rate", "depth", "color", "mix", "level", "stereo", "filter", "sync",
                               "division", "bypass"});
        else if (view.section == 1) {
            ordered("attack.", {"poly", "sens", "hvol", "tone", "harmonix", "volume", "blend", "attack",
                                "decay", "bypass"});
            if (view.advanced)
                ordered("attack.",
                        {"fuzz", "compMono", "compPoly", "makeup", "continuousMono", "continuousPoly", "attackShape",
                         "decayShape", "triggerMode", "hblend", "useReturn", "returnBypass", "fuzzOnReturn",
                         "rblend", "expressionEnabled", "expression"});
            if (view.advanced)
                for (int j = 0; j < 8; ++j)
                    group("attack.expr" + std::to_string(j) + ".");
        } else
            ordered("atomic.", {"volume", "blend", "speed", "atoms", "smooth", "flat", "motion", "stereo",
                                "sync", "division", "tapEnabled", "bypass"});
    } else if (view.block == 2) {
        add("md.routing");
        if (s.values[lay.delayMode] > .75f)
            add("md.crossfade");
        const int bank = std::clamp(view.section, 0, 1);
        const std::string base = bank ? "delayB." : "delayA.";
        add(base + "type");
        add(base + "bypass");
        // Bank MIX is the common wet/dry control and MIDI macro for every engine.
        // Keep it above engine-specific controls in both the basic and advanced views.
        ordered(base, {"mix", "level"});
        const int type = int(plain(lay.delayType[bank], s.values[lay.delayType[bank]]));
        if (type == 0)
            ordered(base, {"time", "feedback", "sync", "division", "filter"});
        else if (type == 1) {
            const std::string mx = base + "mx.";
            ordered(mx, {"mix", "inputTrim", "outputTrim"});
            const std::string pre =
                mx + "slot" + std::to_string(view.advanced ? std::clamp(view.mxSlot, 0, 1) : 0) + "_";
            ordered(pre, {"sync", "division", "time", "feedback", "level"});
            const int algorithm = parameterId(pre + "algorithm");
            const int chosen = algorithm >= 0 ? int(std::round(clean(s.values[algorithm]) * 8)) : 0;
            group(pre + "algo" + std::to_string(chosen) + "_");
            if (chosen == 0)
                ordered(pre, {"headPan0", "headPan1", "headPan2", "headPan3"});
            if (view.advanced) {
                ordered(mx, {"routing", "spill", "machineCondition"});
                ordered(pre, {"algorithm", "enable", "pan", "duck"});
                for (int fx = 0; fx < 4; ++fx)
                    group(mx + "fx" + std::to_string(fx) + "_");
            }
        } else if (type == 2)
            group(base + "memory.");
        else if (type == 3)
            group(base + "reel.");
        else
            group(base + "oil.");
    } else if (view.block == 3)
        group("reverb.");
    else
        ordered("ir.", {"mix", "output"});
    // Hide clock controls which the selected sync mode makes inaudible.
    auto remove=[&](const std::string &key){const int id=parameterId(key);result.erase(std::remove(result.begin(),result.end(),id),result.end());};
    if(view.block==1&&view.section==0){if(s.values[parameterId("clone.sync")]>=.5f)remove("clone.rate");else remove("clone.division");}
    if(view.block==1&&view.section==2){if(s.values[parameterId("atomic.sync")]>=.5f)remove("atomic.speed");else remove("atomic.division");}
    if(view.block==2){const std::string base=view.section?"delayB.":"delayA.";const int type=int(plain(lay.delayType[view.section?1:0],s.values[lay.delayType[view.section?1:0]]));
        if(type==0){if(s.values[parameterId(base+"sync")]>=.5f)remove(base+"time");else remove(base+"division");}
        if(type==1){const std::string pre=base+"mx.slot"+std::to_string(view.advanced?view.mxSlot:0)+"_";if(s.values[parameterId(pre+"sync")]>=.5f)remove(pre+"time");else remove(pre+"division");}
        if(type==2){if(s.values[parameterId(base+"memory.tempoSync")]>=.5f)remove(base+"memory.delay");else remove(base+"memory.division");}
    }
    return result;
}
inline std::string controlTitle(int id) {
    const auto &p = controlUI()[id].definition;
    const std::string &k = p.key;
    // MX has two internal engines in each delay bank. "Silnik A" in DELAY B
    // was easily mistaken for the separate DELAY A bank.
    if (k.starts_with("delayA.mx.slot0_") || k.starts_with("delayA.mx.slot1_") ||
        k.starts_with("delayB.mx.slot0_") || k.starts_with("delayB.mx.slot1_")) {
        const auto separator = p.name.find(" / ");
        std::string detail = separator == std::string::npos ? p.name : p.name.substr(separator + 3);
        if (detail.starts_with("Silnik A: ") || detail.starts_with("Silnik B: "))
            detail = detail.substr(std::string("Silnik A: ").size());
        const std::string bank = k.starts_with("delayB.") ? "DELAY B" : "DELAY A";
        const std::string engine = k.find(".slot1_") != std::string::npos ? "MX 2" : "MX 1";
        return bank + " / " + engine + ": " + detail;
    }
    if (k.ends_with(".bypass"))
        return "EFEKT";
    if (k == "atomic.speed")
        return "SPEED";
    if (k == "atomic.atoms")
        return "ATOMS";
    if (k == "atomic.smooth")
        return "SMOOTH";
    if (k == "atomic.flat")
        return "FLAT";
    if (k == "atomic.motion")
        return "PATTERN";
    if (k == "atomic.stereo")
        return "STEREO";
    if (k == "attack.sens")
        return "SENS";
    if (k == "attack.hvol")
        return "H.VOL";
    if (k == "attack.tone")
        return "TONE";
    if (k == "attack.harmonix")
        return "HARMONIX";
    if (k == "attack.attack")
        return "ATTACK";
    if (k == "attack.decay")
        return "DECAY";
    if (k == "clone.rate")
        return "RATE";
    if (k == "clone.depth")
        return "DEPTH";
    if (k == "clone.color")
        return "COLOR / FEEDBACK";
    if (k == "clone.mix")
        return "CHORUS / VIBRATO";
    if (k == "clone.level")
        return "OUTPUT";
    if (k == "clone.stereo")
        return "STEREO WIDTH";
    if (k == "clone.model")
        return "MODEL";
    if (k == "atomic.volume" || k == "attack.volume")
        return "VOL";
    if (k == "atomic.blend" || k == "attack.blend")
        return "BLEND";
    if (k == "delayA.type" || k == "delayB.type")
        return "DELAY ENGINE";
    if (k == "delayA.mix" || k == "delayB.mix")
        return "BANK MIX";
    auto detail = p.name.find(" — ");
    if (detail != std::string::npos)
        return p.name.substr(detail + std::string(" — ").size());
    auto pos = p.name.find(" / ");
    return pos == std::string::npos ? p.name : p.name.substr(pos + 3);
}
inline std::string parameterText(int id, float value, bool poly = false) {
    const auto &p = controlUI()[id].definition;
    const auto &k = p.key;
    const double v = clean(value);
    char b[96];
    if (!p.choices.empty()) {
        const int choice = int(std::round(v * std::count(p.choices.begin(), p.choices.end(), '|')));
        size_t start = 0;
        for (int i = 0; i < choice; ++i) {
            const auto at = p.choices.find('|', start);
            if (at == std::string::npos)
                break;
            start = at + 1;
        }
        return p.choices.substr(start, p.choices.find('|', start) - start);
    }
    if (k == "clone.rate")
        snprintf(b, sizeof(b), "%.2f Hz", .05 * std::pow(160., v));
    else if (k == "clone.level")
        snprintf(b, sizeof(b), "%+.1f dB", 24 * v - 12);
    else if (k == "atomic.volume")
        snprintf(b, sizeof(b), "%.1f dB", 20 * std::log10(std::max(1.e-6, 2 * v)));
    else if (k == "atomic.atoms")
        snprintf(b, sizeof(b), "%d atoms", int(std::round(std::pow(64., v))));
    else if (k == "atomic.speed")
        snprintf(b, sizeof(b), "%.1f ms", 2000 * std::pow(1. / 750., v));
    else if (k == "attack.volume")
        snprintf(b, sizeof(b), "%+.1f dB", 39 * v - 26);
    else if (k == "attack.hvol")
        snprintf(b, sizeof(b), "%+.1f dB", 36 * v - 24);
    else if (k == "td.optoMakeup")
        snprintf(b, sizeof(b), "%+.1f dB", 24 * v - 12);
    else if (k == "attack.makeup")
        snprintf(b, sizeof(b), "%+.1f dB", plain(id, float(v)));
    else if (k == "comp.makeup" || k.ends_with(".oil.icmakeup") || k.ends_with(".oil.ocmakeup"))
        snprintf(b, sizeof(b), "%+.1f dB", plain(id, float(v)));
    else if (k == "attack.attack" || k == "attack.decay") {
        if ((k == "attack.attack" && v <= .00001) || (k == "attack.decay" && v >= .99999))
            return "OFF";
        snprintf(b, sizeof(b), "%.1f ms", 4 * std::pow(poly ? 1000. : 2000., v));
    } else if (k.find(".mx.") != std::string::npos && k.ends_with("_time"))
        snprintf(b, sizeof(b), "%.1f ms", std::pow(2500., v));
    else if (k.find(".mx.") != std::string::npos && k.ends_with(".inputTrim"))
        snprintf(b, sizeof(b), "%+.1f dB", 36 * v - 18);
    else if (k.find(".mx.") != std::string::npos && k.ends_with(".outputTrim"))
        snprintf(b, sizeof(b), "%+.1f dB", 30 * v - 18);
    else if (k.find(".memory.") != std::string::npos && k.ends_with(".delay"))
        snprintf(b, sizeof(b), "%.1f ms", 20 + 530 * v);
    else if (p.minimum == 0 && p.maximum == 1)
        snprintf(b, sizeof(b), "%.1f %%", 100 * v);
    else
        snprintf(b, sizeof(b), "%.2f %s", plain(id, float(v)), p.unit.c_str());
    return b;
}

} // namespace multifx
