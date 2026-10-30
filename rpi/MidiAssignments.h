#pragma once
#include "core/Project.h"
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace multifx::rpi {
inline void clearMidiAssignments(State &state) {
    const auto &l = layout();
    state.values[l.midiLearn] = 0;
    for (int cc = 0; cc < 128; ++cc) state.values[l.midiMap + cc] = 0;
    state.cc.fill(-1);
    // Attack/Decay also has a direct CC11 expression path, outside MIDI Learn.
    state.values[l.attackExpressionEnabled] = 0;
    for (const auto &target : l.attackExpressionMap) state.values[target[0]] = 0;
}

struct MidiResetFile {
    std::filesystem::path original, staged, archived;
    Project project;
};

inline std::optional<int> stageMidiPresetReset(const std::filesystem::path &data,
                                                std::vector<MidiResetFile> &files) {
    namespace fs = std::filesystem;
    const auto presets = data / "presets";
    std::error_code ec;
    const bool exists = fs::exists(presets, ec);
    if (ec) return std::nullopt;
    if (!exists) return 0;
    if (!fs::is_directory(presets, ec) || ec) return std::nullopt;
    for (fs::directory_iterator it(presets, ec), end; !ec && it != end; it.increment(ec)) {
        const auto &entry = *it;
        if (entry.path().extension() != ".jmfx") continue;
        const bool regular = entry.is_regular_file(ec), symlink = entry.is_symlink(ec);
        if (ec) return std::nullopt;
        if (!regular || symlink) continue;
        MidiResetFile file;
        file.original = entry.path();
        file.staged = file.original.string() + ".midi-reset.tmp";
        if (!loadProject(file.original.string(), file.project)) return std::nullopt;
        clearMidiAssignments(file.project.state);
        const std::string stem = file.original.stem().string();
        if (stem.starts_with("program") && stem.size() > 7 && stem.size() <= 10 &&
            stem.find_first_not_of("0123456789", 7) == std::string::npos) {
            const int number = std::stoi(stem.substr(7));
            if (number >= 0 && number <= 127) {
                std::string suffix = stem + "-bez-PC";
                file.archived = presets / (suffix + ".jmfx");
                for (int n = 2; fs::exists(file.archived, ec) && !ec; ++n)
                    file.archived = presets / (suffix + "-" + std::to_string(n) + ".jmfx");
                if (ec) return std::nullopt;
            }
        }
        files.push_back(std::move(file));
    }
    if (ec) return std::nullopt;
    for (auto &file : files) {
        if (!saveProject(file.staged.string(), file.project)) {
            for (auto &cleanup : files) fs::remove(cleanup.staged, ec);
            return std::nullopt;
        }
    }
    return int(files.size());
}

inline bool commitMidiPresetReset(std::vector<MidiResetFile> &files) {
    namespace fs = std::filesystem;
    std::error_code ec;
    for (auto &file : files) {
        fs::rename(file.staged, file.original, ec);
        if (ec) return false;
        if (!file.archived.empty()) {
            fs::rename(file.original, file.archived, ec);
            if (ec) return false;
        }
    }
    return true;
}
} // namespace multifx::rpi
