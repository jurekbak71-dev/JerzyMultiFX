#pragma once
#include "Convolver.h"
#include "Model.h"
#include <bit>
#include <cstring>
#include <fstream>
#include <stdexcept>
namespace multifx {
inline uint32_t keyHash(const std::string &s) {
    uint32_t h = 2166136261;
    for (unsigned char c : s)
        h = (h ^ c) * 16777619;
    return h;
}
struct Project {
    State state;
    std::vector<float> irL, irR;
    std::string irName;
    double irRate = 48000;
};
inline uint32_t crc32(const uint8_t *data, size_t size) {
    uint32_t crc = ~0u;
    for (size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (int j = 0; j < 8; ++j)
            crc = (crc >> 1) ^ (0xedb88320u & uint32_t(-int(crc & 1)));
    }
    return ~crc;
}
inline void u32(std::vector<uint8_t> &b, uint32_t x) {
    for (int i = 0; i < 4; ++i)
        b.push_back(uint8_t(x >> (8 * i)));
}
inline std::vector<uint8_t> encode(const Project &p) {
    std::vector<uint8_t> b;
    u32(b, 0x31464d4a);
    u32(b, 1);
    const auto &l = layout();
    u32(b, uint32_t(l.parameters.size()));
    for (size_t i = 0; i < l.parameters.size(); ++i) {
        u32(b, keyHash(l.parameters[i].key));
        u32(b, std::bit_cast<uint32_t>(clean(p.state.values[i])));
    }
    u32(b, uint32_t(p.irL.size()));
    u32(b, uint32_t(p.irRate));
    u32(b, uint32_t(p.irName.size()));
    b.insert(b.end(), p.irName.begin(), p.irName.end());
    for (auto *channel : {&p.irL, &p.irR})
        for (float f : *channel)
            u32(b, std::bit_cast<uint32_t>(f));
    u32(b, crc32(b.data(), b.size()));
    return b;
}
inline bool decode(const std::vector<uint8_t> &b, Project &result) {
    if (b.size() < 28 || b.size() > 1024 * 1024)
        return false;
    size_t pos = 0;
    bool valid = true;
    auto read = [&]() {
        if (pos + 4 > b.size()) {
            valid = false;
            return uint32_t(0);
        }
        uint32_t x = 0;
        for (int i = 0; i < 4; ++i)
            x |= uint32_t(b[pos++]) << (i * 8);
        return x;
    };
    if (read() != 0x31464d4a || read() != 1)
        return false;
    auto count = read();
    if (count > maxParameters)
        return false;
    Project p;
    const auto &l = layout();
    for (unsigned i = 0; i < count; ++i) {
        auto hash = read();
        float value = std::bit_cast<float>(read());
        if (!std::isfinite(value) || value < 0 || value > 1)
            return false;
        for (size_t k = 0; k < l.parameters.size(); ++k)
            if (keyHash(l.parameters[k].key) == hash)
                p.state.values[k] = value;
    }
    auto frames = read();
    p.irRate = read();
    auto name = read();
    if (frames > Convolver::maxFrames || p.irRate < 8000 || p.irRate > 192000 || name > 4096 ||
        pos + name + frames * 8 + 4 != b.size())
        return false;
    p.irName.assign(reinterpret_cast<const char *>(b.data() + pos), name);
    pos += name;
    p.irL.resize(frames);
    p.irR.resize(frames);
    for (auto *channel : {&p.irL, &p.irR})
        for (float &x : *channel) {
            x = std::bit_cast<float>(read());
            if (!std::isfinite(x) || std::abs(x) > 16)
                return false;
        }
    auto crc = read();
    if (!valid || crc != crc32(b.data(), b.size() - 4))
        return false;
    result = std::move(p);
    return true;
}
inline bool saveProject(const std::string &path, const Project &p) {
    auto bytes = encode(p);
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char *>(bytes.data()), std::streamsize(bytes.size()));
    return bool(f);
}
inline bool loadProject(const std::string &path, Project &p) {
    std::ifstream f(path, std::ios::binary);
    if (!f)
        return false;
    std::vector<uint8_t> b((std::istreambuf_iterator<char>(f)), {});
    return decode(b, p);
}
inline bool loadWave(const std::string &path, double rate, Project &project) {
    std::ifstream f(path, std::ios::binary);
    if (!f)
        return false;
    std::vector<uint8_t> b((std::istreambuf_iterator<char>(f)), {});
    if (b.size() < 44 || b.size() > 32 * 1024 * 1024 || std::memcmp(b.data(), "RIFF", 4) ||
        std::memcmp(b.data() + 8, "WAVE", 4))
        return false;
    auto r16 = [&](size_t i) { return uint16_t(b.at(i) | (uint16_t(b.at(i + 1)) << 8)); };
    auto r32 = [&](size_t i) { return uint32_t(r16(i)) | (uint32_t(r16(i + 2)) << 16); };
    int format = 0, channels = 0, bits = 0;
    uint32_t sr = 0;
    size_t data = 0, length = 0;
    for (size_t i = 12; i + 8 <= b.size();) {
        auto n = r32(i + 4);
        if (i + 8 + n > b.size())
            return false;
        if (!std::memcmp(b.data() + i, "fmt ", 4) && n >= 16) {
            format = r16(i + 8);
            channels = r16(i + 10);
            sr = r32(i + 12);
            bits = r16(i + 22);
        }
        if (!std::memcmp(b.data() + i, "data", 4)) {
            data = i + 8;
            length = n;
        }
        i += 8 + n + (n & 1);
    }
    if (!data || channels < 1 || channels > 2 || sr < 8000 || sr > 192000 ||
        !((format == 1 && (bits == 16 || bits == 24 || bits == 32)) || (format == 3 && bits == 32)))
        return false;
    int sourceFrames = int(length / (channels * (bits / 8)));
    int frames = int(std::ceil(sourceFrames * rate / sr));
    if (frames < 1 || frames > Convolver::maxFrames)
        return false;
    std::vector<float> source[2];
    for (int c = 0; c < 2; ++c) {
        source[c].resize(sourceFrames);
        for (int i = 0; i < sourceFrames; ++i) {
            size_t at = data + size_t((i * channels + std::min(c, channels - 1)) * (bits / 8));
            float x;
            if (format == 3)
                x = std::bit_cast<float>(r32(at));
            else if (bits == 16)
                x = float(int16_t(r16(at))) / 32768.f;
            else if (bits == 24) {
                int32_t v = int32_t(b[at]) | (int32_t(b[at + 1]) << 8) | (int32_t(b[at + 2]) << 16);
                if (v & 0x800000)
                    v |= int32_t(0xff000000);
                x = float(v) / 8388608.f;
            } else
                x = float(int32_t(r32(at))) / 2147483648.f;
            if (!std::isfinite(x) || std::abs(x) > 16)
                return false;
            source[c][i] = x;
        }
    }
    Project next = project;
    next.irL.resize(frames);
    next.irR.resize(frames);
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < frames; ++i) {
            double x = i * double(sr) / rate;
            size_t a = std::min(size_t(x), size_t(sourceFrames - 1)),
                   bb = std::min(a + 1, size_t(sourceFrames - 1));
            (c ? next.irR : next.irL)[i] = source[c][a] + (source[c][bb] - source[c][a]) * float(x - a);
        }
    next.irName = path;
    next.irRate = rate;
    project = std::move(next);
    return true;
}
} // namespace multifx
