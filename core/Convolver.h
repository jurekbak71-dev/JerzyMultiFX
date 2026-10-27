#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <complex>
#include <vector>
namespace multifx {
// Uniform partitioned overlap-save, fixed 128-sample latency, no RT allocation.
class Convolver {
    static constexpr int N = 256, H = 128, P = 128;
    using Spectrum = std::array<std::complex<float>, N>;
    struct Bank {
        std::array<std::array<Spectrum, P>, 2> ir{};
        int parts = 1;
    };
    std::array<Bank, 3> banks{};
    std::atomic<int> published{0}, reading{0}, oldReading{0};
    std::array<std::array<Spectrum, P>, 2> history{};
    std::array<std::array<float, H>, 2> previous{}, input{}, output{}, dry{}, oldOutput{};
    int position = 0, head = 0, active = 0;
    float swapFade = 1;
    static void fft(Spectrum &a, bool inverse = false) {
        for (int i = 1, j = 0; i < N; ++i) {
            int bit = N >> 1;
            for (; j & bit; bit >>= 1)
                j ^= bit;
            j ^= bit;
            if (i < j)
                std::swap(a[i], a[j]);
        }
        for (int len = 2; len <= N; len <<= 1) {
            auto step = std::polar(1.f, float((inverse ? 2 : -2) * 3.14159265358979323846 / len));
            for (int i = 0; i < N; i += len) {
                std::complex<float> w{1, 0};
                for (int j = 0; j < len / 2; ++j) {
                    auto u = a[i + j], v = a[i + j + len / 2] * w;
                    a[i + j] = u + v;
                    a[i + j + len / 2] = u - v;
                    w *= step;
                }
            }
        }
        if (inverse)
            for (auto &x : a)
                x /= float(N);
    }

  public:
    static constexpr int latency = H, maxFrames = H * P;
    Convolver() {
        for (auto &b : banks)
            for (int c = 0; c < 2; ++c) {
                b.ir[c][0][0] = 1;
                fft(b.ir[c][0]);
            }
    }
    // Single control-thread producer; immutable bank publication + audio hazard.
    bool load(const float *l, const float *r, int frames) {
        if (!l || frames < 1 || frames > maxFrames)
            return false;
        int pub = published.load(), read = reading.load(), old = oldReading.load(), slot = 0;
        while (slot == pub || slot == read || slot == old)
            ++slot;
        if (slot >= 3)
            return false;
        auto &b = banks[slot];
        b.parts = (frames + H - 1) / H;
        for (int c = 0; c < 2; ++c)
            for (int p = 0; p < b.parts; ++p) {
                auto &a = b.ir[c][p];
                a = {};
                for (int i = 0; i < H; ++i) {
                    int k = p * H + i;
                    a[i] = k < frames ? (c == 0 || !r ? l[k] : r[k]) : 0;
                }
                fft(a);
            }
        published.store(slot);
        return true;
    }
    void reset() {
        history = {};
        previous = {};
        input = {};
        output = {};
        dry = {};
        position = head = 0;
        active = published.load();
        swapFade = 1;
        oldOutput = {};
    }
    std::array<float, 2> tick(float l, float r, float mix, float gain) {
        std::array<float, 2> result{};
        float x[]{l, r};
        for (int c = 0; c < 2; ++c) {
            result[c] = (dry[c][position] * (1 - mix) +
                         (oldOutput[c][position] * (1 - swapFade) + output[c][position] * swapFade) * mix) *
                        gain;
            dry[c][position] = x[c];
            input[c][position] = x[c];
        }
        swapFade = std::min(1.f, swapFade + 1.f / H);
        if (++position == H) {
            position = 0;
            oldReading.store(active);
            int bank;
            do {
                bank = published.load();
                reading.store(bank);
            } while (bank != published.load());
            const auto &b = banks[bank];
            const auto &old = banks[active];
            bool changed = bank != active;
            if (changed)
                swapFade = 0;
            for (int c = 0; c < 2; ++c) {
                auto &spectrum = history[c][head];
                for (int i = 0; i < H; ++i) {
                    spectrum[i] = previous[c][i];
                    spectrum[i + H] = input[c][i];
                    previous[c][i] = input[c][i];
                }
                fft(spectrum);
                Spectrum sum{};
                for (int p = 0; p < b.parts; ++p) {
                    const auto &s = history[c][(head - p + P) % P];
                    for (int k = 0; k < N; ++k)
                        sum[k] += s[k] * b.ir[c][p][k];
                }
                fft(sum, true);
                for (int i = 0; i < H; ++i)
                    output[c][i] = sum[i + H].real();
                if (changed) {
                    Spectrum prior{};
                    for (int p = 0; p < old.parts; ++p) {
                        const auto &h = history[c][(head - p + P) % P];
                        for (int k = 0; k < N; ++k)
                            prior[k] += h[k] * old.ir[c][p][k];
                    }
                    fft(prior, true);
                    for (int i = 0; i < H; ++i)
                        oldOutput[c][i] = prior[i + H].real();
                }
            }
            active = bank;
            oldReading.store(bank);
            head = (head + 1) % P;
        }
        return result;
    }
};
} // namespace multifx
