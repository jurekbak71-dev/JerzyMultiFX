#include "core/Engine.h"
#include "core/Presets.h"
#include "core/Project.h"
#include <chrono>
#include <csignal>
#include <dlfcn.h>
#include <iostream>
#include <netinet/in.h>
#include <sstream>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
namespace {
std::atomic<bool> running{true};
void stop(int) {
    running.store(false);
}
std::string quote(const std::string &x) {
    std::string s = "\"";
    for (unsigned char c : x) {
        if (c == '"' || c == '\\')
            s += '\\';
        if (c >= 32)
            s += char(c);
    }
    return s + '"';
}
struct Jack {
    void *library = nullptr;
    void *client = nullptr;
    void *ports[5]{};
    using Frames = uint32_t;
    void *(*client_open)(const char *, unsigned, unsigned *, ...) = nullptr;
    int (*client_close)(void *) = nullptr;
    int (*activate)(void *) = nullptr;
    int (*deactivate)(void *) = nullptr;
    Frames (*get_sample_rate)(void *) = nullptr;
    Frames (*get_buffer_size)(void *) = nullptr;
    int (*set_process_callback)(void *, int (*)(Frames, void *), void *) = nullptr;
    int (*set_xrun_callback)(void *, int (*)(void *), void *) = nullptr;
    void *(*port_register)(void *, const char *, const char *, unsigned, unsigned) = nullptr;
    void *(*port_get_buffer)(void *, Frames) = nullptr;
    const char *(*port_name)(const void *) = nullptr;
    const char **(*get_ports)(void *, const char *, const char *, unsigned) = nullptr;
    int (*connect)(void *, const char *, const char *) = nullptr;
    int (*port_connected_to)(const void *, const char *) = nullptr;
    void (*free_)(void *) = nullptr;
    float (*cpu_load)(void *) = nullptr;
    uint32_t (*midi_get_event_count)(void *) = nullptr;
    struct MidiEvent {
        uint32_t time;
        size_t size;
        unsigned char *buffer;
    };
    int (*midi_event_get)(MidiEvent *, void *, uint32_t) = nullptr;
    template <class T> void symbol(T &ptr, const char *name) {
        ptr = reinterpret_cast<T>(dlsym(library, name));
        if (!ptr)
            throw std::runtime_error(name);
    }
    void open() {
        library = dlopen("libjack.so.0", RTLD_NOW | RTLD_LOCAL);
        if (!library)
            throw std::runtime_error("Install JACK2 (libjack.so.0) and configure an audio server first");
#define LOAD(n) symbol(n, "jack_" #n)
        LOAD(client_open);
        LOAD(client_close);
        LOAD(activate);
        LOAD(deactivate);
        LOAD(get_sample_rate);
        LOAD(get_buffer_size);
        LOAD(set_process_callback);
        LOAD(set_xrun_callback);
        LOAD(port_register);
        LOAD(port_get_buffer);
        LOAD(port_name);
        LOAD(get_ports);
        LOAD(connect);
        LOAD(port_connected_to);
        symbol(free_, "jack_free");
        LOAD(cpu_load);
        LOAD(midi_get_event_count);
        LOAD(midi_event_get);
#undef LOAD
        unsigned status = 0;
        client = client_open("JerzyMultiFX", 1u, &status);
        if (!client)
            throw std::runtime_error(
                "JACK unavailable: select/start a device in the audio configuration first");
        ports[0] = port_register(client, "input_L", "32 bit float mono audio", 1, 0);
        ports[1] = port_register(client, "input_R", "32 bit float mono audio", 1, 0);
        ports[2] = port_register(client, "output_L", "32 bit float mono audio", 2, 0);
        ports[3] = port_register(client, "output_R", "32 bit float mono audio", 2, 0);
        ports[4] = port_register(client, "midi_control", "8 bit raw midi", 1, 0);
        for (void *p : ports)
            if (!p)
                throw std::runtime_error("JACK port registration");
    }
    void connectPorts() {
        for (unsigned direction : {1u, 2u}) {
            auto names = get_ports(client, nullptr, "32 bit float mono audio", direction | 4u);
            if (names) {
                for (int c = 0; c < 2 && names[c]; ++c) {
                    if (direction == 2) {
                        if (!port_connected_to(ports[c], names[c]))
                            connect(client, names[c], port_name(ports[c]));
                    } else if (!port_connected_to(ports[c + 2], names[c]))
                        connect(client, port_name(ports[c + 2]), names[c]);
                }
                free_(const_cast<char **>(names));
            }
        }
        auto midi = get_ports(client, nullptr, "8 bit raw midi", 2);
        if (midi) {
            for (int i = 0; midi[i]; ++i)
                if (!port_connected_to(ports[4], midi[i]))
                    connect(client, midi[i], port_name(ports[4]));
            free_(const_cast<char **>(midi));
        }
    }
    ~Jack() {
        if (client) {
            deactivate(client);
            client_close(client);
        }
        if (library)
            dlclose(library);
    }
};
struct Host {
    multifx::Engine engine;
    std::array<std::atomic<float>, multifx::maxParameters> values;
    multifx::Project project;
    Jack jack;
    double sr = 48000;
    std::atomic<uint32_t> xruns{0};
    std::atomic<float> bpm{120};
    std::atomic<int> program{-1};
    std::atomic<uint32_t> pendingMidi{0};
    std::atomic<bool> dirty{false};
    Host() {
        (void)multifx::factoryPresets();
        for (int i = 0; i < multifx::maxParameters; ++i)
            values[i].store(project.state.values[i]);
    }
    multifx::State state() {
        multifx::State s;
        for (size_t i = 0; i < multifx::layout().parameters.size(); ++i)
            s.values[i] = values[i].load(std::memory_order_relaxed);
        return s;
    }
    void setState(const multifx::State &s) {
        for (size_t i = 0; i < multifx::layout().parameters.size(); ++i)
            values[i].store(s.values[i], std::memory_order_relaxed);
    }
    static int audio(uint32_t n, void *opaque) {
        auto &h = *static_cast<Host *>(opaque);
        auto s = h.state();
        auto *buffer = h.jack.port_get_buffer(h.jack.ports[4], n);
        std::array<JAMidi, 128> midi{};
        unsigned nm = 0;
        uint32_t pending = h.pendingMidi.exchange(0);
        if (pending)
            midi[nm++] = {0, 3, {uint8_t(pending >> 16), uint8_t(pending >> 8), uint8_t(pending)}};
        auto &lay = multifx::layout();
        for (uint32_t i = 0; i < h.jack.midi_get_event_count(buffer) && nm < 128; ++i) {
            Jack::MidiEvent e{};
            if (!h.jack.midi_event_get(&e, buffer, i) && e.size >= 2 && e.size <= 3) {
                midi[nm++] = {e.time, 3, {e.buffer[0], e.buffer[1], uint8_t(e.size == 3 ? e.buffer[2] : 0)}};
                if ((e.buffer[0] & 240) == 192)
                    h.program.store(e.buffer[1]);
                if ((e.buffer[0] & 240) == 176) {
                    int cc = e.buffer[1],
                        target = int(multifx::plain(lay.midiLearn, s.values[lay.midiLearn]));
                    if (target >= 0) {
                        s.values[lay.midiMap + cc] = float(target + 1) / lay.controlCount;
                        s.values[lay.midiLearn] = 0;
                        h.values[lay.midiMap + cc].store(s.values[lay.midiMap + cc]);
                        h.values[lay.midiLearn].store(0);
                    }
                    int id = int(multifx::plain(lay.midiMap + cc, s.values[lay.midiMap + cc]));
                    if (id >= 0) {
                        s.values[id] = float(e.buffer[2]) / 127;
                        h.values[id].store(s.values[id]);
                        h.dirty.store(true);
                    }
                }
            }
        }
        auto l = static_cast<float *>(h.jack.port_get_buffer(h.jack.ports[0], n)),
             r = static_cast<float *>(h.jack.port_get_buffer(h.jack.ports[1], n));
        auto ol = static_cast<float *>(h.jack.port_get_buffer(h.jack.ports[2], n)),
             orr = static_cast<float *>(h.jack.port_get_buffer(h.jack.ports[3], n));
        h.engine.process(l, r, ol, orr, int(n), s, h.bpm.load(), 0, false, midi.data(), nm);
        return 0;
    }
    static int xrun(void *p) {
        static_cast<Host *>(p)->xruns.fetch_add(1);
        return 0;
    }
    std::string command(const std::string &line) {
        std::istringstream in(line);
        std::string cmd;
        in >> cmd;
        if (cmd == "STATE") {
            auto s = state();
            std::ostringstream out;
            out << "{\"ok\":true,\"sampleRate\":" << sr
                << ",\"latencySamples\":128,\"xruns\":" << xruns.load()
                << ",\"cpu\":" << (jack.client ? jack.cpu_load(jack.client) : 0)
                << ",\"in\":" << engine.inputPeak.load() << ",\"out\":" << engine.outputPeak.load()
                << ",\"gr\":" << engine.gainReduction.load() << ",\"ir\":" << quote(project.irName)
                << ",\"values\":[";
            for (size_t i = 0; i < multifx::layout().parameters.size(); ++i) {
                if (i)
                    out << ',';
                out << s.values[i];
            }
            out << "]}";
            return out.str();
        }
        if (cmd == "METADATA") {
            std::ostringstream out;
            out << "{\"ok\":true,\"controlCount\":" << multifx::layout().controlCount << ",\"parameters\":[";
            auto &p = multifx::layout().parameters;
            for (size_t i = 0; i < p.size(); ++i) {
                if (i)
                    out << ',';
                out << "{\"id\":" << i << ",\"key\":" << quote(p[i].key) << ",\"name\":" << quote(p[i].name)
                    << ",\"min\":" << p[i].minimum << ",\"max\":" << p[i].maximum
                    << ",\"default\":" << p[i].initial << ",\"steps\":" << p[i].steps
                    << ",\"choices\":" << quote(p[i].choices) << "}";
            }
            out << "]}";
            return out.str();
        }
        if (cmd == "SET") {
            int id;
            float value;
            if (!(in >> id >> value) || id < 0 || id >= int(multifx::layout().parameters.size()) ||
                !std::isfinite(value) || value < 0 || value > 1)
                return "{\"ok\":false,\"error\":\"Invalid parameter\"}";
            if (id == multifx::layout().factoryProgram) {
                auto s = state();
                multifx::selectFactory(s, int(std::round(value * 7)));
                setState(s);
            }
            values[id].store(value);
            dirty.store(true);
            return "{\"ok\":true}";
        }
        if (cmd == "MIDI") {
            int status, a, b = 0;
            if (!(in >> status >> a) || status < 128 || status > 239 || a < 0 || a > 127)
                return "{\"ok\":false}";
            in >> b;
            if (b < 0 || b > 127)
                return "{\"ok\":false}";
            auto &lay = multifx::layout();
            if ((status & 240) == 176) {
                int target = int(multifx::plain(lay.midiLearn, values[lay.midiLearn].load()));
                if (target >= 0) {
                    values[lay.midiMap + a].store(float(target + 1) / lay.controlCount);
                    values[lay.midiLearn].store(0);
                }
                int id = int(multifx::plain(lay.midiMap + a, values[lay.midiMap + a].load()));
                if (id >= 0) {
                    values[id].store(float(b) / 127);
                    dirty.store(true);
                }
            } else if ((status & 240) == 192)
                program.store(a);
            else
                pendingMidi.store(uint32_t(status << 16 | a << 8 | b));
            return "{\"ok\":true}";
        }
        if (cmd == "BPM") {
            float v;
            if (!(in >> v) || !std::isfinite(v) || v < 20 || v > 400)
                return "{\"ok\":false}";
            bpm.store(v);
            return "{\"ok\":true}";
        }
        if (cmd == "SAVE" || cmd == "LOAD" || cmd == "IR") {
            std::string path;
            std::getline(in, path);
            if (path.size() < 2)
                return "{\"ok\":false}";
            path.erase(0, 1);
            project.state = state();
            bool ok = false;
            if (cmd == "SAVE")
                ok = multifx::saveProject(path, project);
            else {
                auto p = project;
                ok = cmd == "LOAD" ? multifx::loadProject(path, p) : multifx::loadWave(path, sr, p);
                if (ok && !p.irL.empty()) {
                    if (std::abs(p.irRate - sr) > .1)
                        ok = false;
                    else
                        ok = engine.loadIR(p.irL.data(), p.irR.data(), int(p.irL.size()));
                }
                if (ok) {
                    if (p.irL.empty()) {
                        float identity = 1;
                        ok = engine.loadIR(&identity, nullptr, 1);
                    }
                    if (ok) {
                        project = std::move(p);
                        setState(project.state);
                    }
                }
            }
            return ok ? "{\"ok\":true}"
                      : "{\"ok\":false,\"error\":\"Invalid file or incompatible IR sample rate/length\"}";
        }
        return "{\"ok\":false,\"error\":\"Unknown command\"}";
    }
    void serve(int port) {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0)
            throw std::runtime_error("socket");
        int yes = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = htons(uint16_t(port));
        if (bind(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) || listen(fd, 4)) {
            close(fd);
            throw std::runtime_error("Control port unavailable");
        }
        while (running.load()) {
            fd_set set;
            FD_ZERO(&set);
            FD_SET(fd, &set);
            timeval timeout{1, 0};
            if (select(fd + 1, &set, nullptr, nullptr, &timeout) > 0) {
                int client = accept(fd, nullptr, nullptr);
                if (client < 0)
                    continue;
                timeval readTimeout{2, 0};
                setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &readTimeout, sizeof(readTimeout));
                std::string line;
                char c;
                while (line.size() < 4096 && recv(client, &c, 1, 0) == 1 && c != '\n')
                    line += c;
                std::string response = command(line) + '\n';
                size_t sent = 0;
                while (sent < response.size()) {
                    auto n = send(client, response.data() + sent, response.size() - sent, MSG_NOSIGNAL);
                    if (n <= 0)
                        break;
                    sent += size_t(n);
                }
                close(client);
            }
            if (dirty.exchange(false)) {
                const char *data = std::getenv("MULTIFX_DATA");
                if (data) {
                    std::string path = std::string(data) + "/last-state.jmfx";
                    project.state = state();
                    if (multifx::saveProject(path + ".tmp", project))
                        std::rename((path + ".tmp").c_str(), path.c_str());
                }
            }
            int selected = program.exchange(-1);
            if (selected >= 0) {
                const char *data = std::getenv("MULTIFX_DATA");
                bool loaded = false;
                if (data)
                    loaded = command("LOAD " + std::string(data) + "/presets/program" +
                                     std::to_string(selected) + ".jmfx")
                                 .find("true") != std::string::npos;
                if (!loaded && selected < 8) {
                    auto s = state();
                    multifx::selectFactory(s, selected);
                    setState(s);
                    dirty.store(true);
                }
            }
            if (jack.client)
                jack.connectPorts();
        }
        close(fd);
    }
};
} // namespace
int main(int argc, char **argv) {
    try {
        signal(SIGINT, stop);
        signal(SIGTERM, stop);
        auto host = std::make_unique<Host>();
        if (argc > 1 && !strcmp(argv[1], "--metadata")) {
            std::cout << host->command("METADATA") << '\n';
            return 0;
        }
        bool test = argc > 1 && !strcmp(argv[1], "--test-server");
        int port = 9817;
        if (argc > 2)
            port = std::stoi(argv[2]);
        if (!test) {
            host->jack.open();
            host->sr = host->jack.get_sample_rate(host->jack.client);
        }
        host->engine.prepare(host->sr);
        if (const char *data = std::getenv("MULTIFX_DATA"))
            host->command("LOAD " + std::string(data) + "/last-state.jmfx");
        if (!test) {
            host->jack.set_process_callback(host->jack.client, Host::audio, host.get());
            host->jack.set_xrun_callback(host->jack.client, Host::xrun, host.get());
            if (host->jack.activate(host->jack.client))
                throw std::runtime_error("JACK activation");
            host->jack.connectPorts();
        }
        std::cout << "Jerzy MultiFX control ready (loopback), " << host->sr << " Hz; "
                  << (test ? "TEST SERVER - no audio" : "JACK audio active") << std::endl;
        host->serve(port);
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
