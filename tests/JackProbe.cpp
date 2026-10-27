#include <atomic>
#include <chrono>
#include <cmath>
#include <iostream>
#include <jack/jack.h>
#include <jack/midiport.h>
#include <thread>
struct Probe {
    jack_client_t *c;
    jack_port_t *source;
    jack_port_t *midi;
    bool sendCC = false;
    jack_port_t *capture[2];
    double phase = 0;
    std::atomic<unsigned long> samples{0};
    std::atomic<double> sum{0}, peak{0};
    static int callback(jack_nframes_t n, void *ptr) {
        auto &p = *static_cast<Probe *>(ptr);
        void *midi = jack_port_get_buffer(p.midi, n);
        jack_midi_clear_buffer(midi);
        if (p.sendCC) {
            const unsigned char cc[]{0xb0, 74, 127};
            jack_midi_event_write(midi, 0, cc, 3);
        }
        auto *out = static_cast<float *>(jack_port_get_buffer(p.source, n));
        const auto *l = static_cast<float *>(jack_port_get_buffer(p.capture[0], n));
        const auto *r = static_cast<float *>(jack_port_get_buffer(p.capture[1], n));
        double sum = 0, peak = 0;
        for (unsigned i = 0; i < n; ++i) {
            out[i] = float(.1 * std::sin(p.phase));
            p.phase += 6.28318530718 * 220 / jack_get_sample_rate(p.c);
            if (p.phase > 6.28318530718)
                p.phase -= 6.28318530718;
            sum += double(l[i]) * l[i] + double(r[i]) * r[i];
            peak = std::max(peak, double(std::max(std::abs(l[i]), std::abs(r[i]))));
        }
        p.samples.fetch_add(n * 2);
        p.sum.store(p.sum.load() + sum);
        p.peak.store(std::max(p.peak.load(), peak));
        return 0;
    }
};
int main(int argc, char **argv) {
    if (argc < 2)
        return 2;
    Probe p;
    jack_status_t status{};
    p.c = jack_client_open("JerzyProbe", JackNoStartServer, &status);
    if (!p.c)
        return 3;
    p.sendCC = argc > 3;
    p.midi = jack_port_register(p.c, "midi", JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0);
    p.source = jack_port_register(p.c, "sygnal", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
    for (int c = 0; c < 2; ++c)
        p.capture[c] = jack_port_register(p.c, ("pomiar" + std::to_string(c)).c_str(),
                                          JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
    jack_set_process_callback(p.c, Probe::callback, &p);
    if (jack_activate(p.c))
        return 4;
    for (int c = 0; c < 2; ++c) {
        if (jack_connect(p.c, jack_port_name(p.source),
                         (std::string(argv[1]) + ":input_" + std::string(c ? "R" : "L")).c_str()))
            return 5;
        if (jack_connect(p.c, (std::string(argv[1]) + ":output_" + std::string(c ? "R" : "L")).c_str(),
                         jack_port_name(p.capture[c])))
            return 6;
    }
    if (p.sendCC &&
        jack_connect(p.c, jack_port_name(p.midi), (std::string(argv[1]) + ":midi_control").c_str()))
        return 8;
    std::this_thread::sleep_for(std::chrono::milliseconds(argc > 2 ? std::stoi(argv[2]) : 1000));
    jack_deactivate(p.c);
    jack_client_close(p.c);
    std::cout << "{\"samples\":" << p.samples << ",\"rms\":" << std::sqrt(p.sum / p.samples)
              << ",\"peak\":" << p.peak << "}\n";
    return p.samples > 0 ? 0 : 7;
}
