#include "core/Engine.h"
#include "core/Presentation.h"
#include "core/Presets.h"
#include "core/Project.h"
#include "ParameterUI.h"
#include "StateWriter.h"
#include <chrono>
#include <csignal>
#include <dlfcn.h>
#include <iostream>
#include <netinet/in.h>
#include <set>
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
    void *ports[7]{};
    std::atomic<bool> returnConnected{false};
    std::array<std::string, 6> routing{}, previousRouting{};
    std::set<std::string> disabledMidi;
    std::atomic<bool> serverLost{false};
    using Frames = uint32_t;
    void *(*client_open)(const char *, unsigned, unsigned *, ...) = nullptr;
    int (*client_close)(void *) = nullptr;
    int (*is_realtime)(void *) = nullptr;
    int (*activate)(void *) = nullptr;
    int (*deactivate)(void *) = nullptr;
    Frames (*get_sample_rate)(void *) = nullptr;
    int (*transport_query)(const void *, void *) = nullptr;
    Frames (*transport_frame)(const void *) = nullptr;
    Frames (*get_buffer_size)(void *) = nullptr;
    int (*set_process_callback)(void *, int (*)(Frames, void *), void *) = nullptr;
    int (*set_xrun_callback)(void *, int (*)(void *), void *) = nullptr;
    void *(*port_register)(void *, const char *, const char *, unsigned, unsigned) = nullptr;
    void *(*port_get_buffer)(void *, Frames) = nullptr;
    const char *(*port_name)(const void *) = nullptr;
    const char **(*get_ports)(void *, const char *, const char *, unsigned) = nullptr;
    int (*connect)(void *, const char *, const char *) = nullptr;
    int (*disconnect)(void *, const char *, const char *) = nullptr;
    int (*set_buffer_size)(void *, Frames) = nullptr;
    void (*on_shutdown)(void *, void (*)(void *), void *) = nullptr;
    int (*port_connected_to)(const void *, const char *) = nullptr;
    int (*port_connected)(const void *) = nullptr;
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
        LOAD(is_realtime);
        LOAD(deactivate);
        LOAD(get_sample_rate);
        transport_query=reinterpret_cast<decltype(transport_query)>(dlsym(library,"jack_transport_query"));
        transport_frame=reinterpret_cast<decltype(transport_frame)>(dlsym(library,"jack_get_current_transport_frame"));
        LOAD(get_buffer_size);
        LOAD(set_process_callback);
        LOAD(set_xrun_callback);
        LOAD(port_register);
        LOAD(port_get_buffer);
        LOAD(port_name);
        LOAD(get_ports);
        LOAD(connect);
        LOAD(disconnect);
        LOAD(set_buffer_size);
        LOAD(on_shutdown);
        LOAD(port_connected_to);
        LOAD(port_connected);
        symbol(free_, "jack_free");
        LOAD(cpu_load);
        LOAD(midi_get_event_count);
        LOAD(midi_event_get);
#undef LOAD
        unsigned status = 0;
        client = client_open("JerzyMultiFX", 3u, &status);
        if (!client)
            throw std::runtime_error(
                "JACK unavailable: select/start a device in the audio configuration first");
        on_shutdown(
            client,
            [](void *opaque) {
                static_cast<Jack *>(opaque)->serverLost.store(true);
                running.store(false);
            },
            this);
        ports[0] = port_register(client, "input_L", "32 bit float mono audio", 1, 0);
        ports[1] = port_register(client, "input_R", "32 bit float mono audio", 1, 0);
        ports[2] = port_register(client, "output_L", "32 bit float mono audio", 2, 0);
        ports[3] = port_register(client, "output_R", "32 bit float mono audio", 2, 0);
        ports[4] = port_register(client, "midi_control", "8 bit raw midi", 1, 0);
        ports[5] = port_register(client, "return_L", "32 bit float mono audio", 1, 0);
        ports[6] = port_register(client, "return_R", "32 bit float mono audio", 1, 0);
        for (void *p : ports)
            if (!p)
                throw std::runtime_error("JACK port registration");
    }
    void connectPorts() {
        for (unsigned direction : {1u, 2u}) {
            auto names = get_ports(client, nullptr, "32 bit float mono audio", direction | 4u);
            if (names) {
                int count = 0;
                while (names[count])
                    ++count;
                for (int channel = 0; channel < 2; ++channel) {
                    const int setting = direction == 2 ? channel : channel + 2;
                    const int port = setting;
                    std::string target =
                        routing[setting].empty() ? (channel < count ? names[channel] : "") : routing[setting];
                    if (previousRouting[setting] != target && !previousRouting[setting].empty() &&
                        port_connected_to(ports[port], previousRouting[setting].c_str())) {
                        if (direction == 2)
                            disconnect(client, previousRouting[setting].c_str(), port_name(ports[port]));
                        else
                            disconnect(client, port_name(ports[port]), previousRouting[setting].c_str());
                    }
                    previousRouting[setting] = target;
                    if (!target.empty() && !port_connected_to(ports[port], target.c_str())) {
                        if (direction == 2)
                            connect(client, target.c_str(), port_name(ports[port]));
                        else
                            connect(client, port_name(ports[port]), target.c_str());
                    }
                }
                free_(const_cast<char **>(names));
            }
        }
        for (int channel = 0; channel < 2; ++channel) {
            const int setting = 4 + channel, port = 5 + channel;
            const auto &target = routing[setting];
            if (previousRouting[setting] != target && !previousRouting[setting].empty() &&
                port_connected_to(ports[port], previousRouting[setting].c_str()))
                disconnect(client, previousRouting[setting].c_str(), port_name(ports[port]));
            previousRouting[setting] = target;
            if (!target.empty() && !port_connected_to(ports[port], target.c_str()))
                connect(client, target.c_str(), port_name(ports[port]));
        }
        returnConnected.store(port_connected(ports[5]) > 0 || port_connected(ports[6]) > 0);
        auto midi = get_ports(client, nullptr, "8 bit raw midi", 2);
        if (midi) {
            for (int i = 0; midi[i]; ++i)
                if (!disabledMidi.contains(midi[i]) && !port_connected_to(ports[4], midi[i]))
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
    std::atomic<int> inputMode{0};
    std::atomic<int> program{-1};
    std::atomic<uint32_t> pendingMidi{0};
    std::atomic<bool> dirty{false};
    Host() {
        (void)multifx::factoryPresets();
        (void)multifx::rpi::parameterUI();
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
            if (!h.jack.midi_event_get(&e, buffer, i) && e.size >= 1 && e.size <= 3) {
                midi[nm++] = {e.time,
                              uint32_t(e.size),
                              {e.buffer[0], uint8_t(e.size > 1 ? e.buffer[1] : 0),
                               uint8_t(e.size == 3 ? e.buffer[2] : 0)}};
                if ((e.buffer[0] & 240) == 192 && e.size > 1) {
                    if (e.buffer[1] < 8) {
                        multifx::selectFactory(s, e.buffer[1]);
                        h.setState(s);
                        h.dirty.store(true);
                    } else
                        h.program.store(e.buffer[1]);
                }
                if ((e.buffer[0] & 240) == 176 && e.size == 3) {
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
        const int inputMode=h.inputMode.load();
        const float *inputLeft=inputMode==2?r:l,*inputRight=inputMode==1?l:r;
        const double tempo=h.bpm.load();
        const bool playing=h.jack.transport_query && h.jack.transport_query(h.jack.client,nullptr)==1;
        const double beat=playing && h.jack.transport_frame ? double(h.jack.transport_frame(h.jack.client))*tempo/(60*h.sr) : 0;
        h.engine.process(
            inputLeft, inputRight, ol, orr, int(n), s, tempo, beat, playing, midi.data(), nm,
            h.jack.returnConnected.load() ? static_cast<float *>(h.jack.port_get_buffer(h.jack.ports[5], n))
                                          : nullptr,
            h.jack.returnConnected.load() ? static_cast<float *>(h.jack.port_get_buffer(h.jack.ports[6], n))
                                          : nullptr);
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
        if (cmd == "VIEW") {
            multifx::PanelSelection selection;
            int advanced = 0;
            if (!(in >> selection.block >> selection.section >> selection.mxSlot >> advanced) ||
                selection.block < 0 || selection.block > 4 || selection.section < 0 ||
                selection.section > 3 || selection.mxSlot < 0 || selection.mxSlot > 1)
                return "{\"ok\":false}";
            selection.advanced = advanced != 0;
            auto ids = multifx::panelParameters(selection, state());
            std::ostringstream out;
            out << "{\"ok\":true,\"ids\":[";
            for (size_t k = 0; k < ids.size(); ++k) {
                if (k)
                    out << ',';
                out << ids[k];
            }
            out << "]}";
            return out.str();
        }
        if (cmd == "AUDIOPORTS") {
            std::ostringstream out;
            out << "{\"ok\":true";
            for (unsigned direction : {2u, 1u}) {
                out << (direction == 2 ? ",\"inputs\":[" : ",\"outputs\":[");
                if (jack.client) {
                    auto names =
                        jack.get_ports(jack.client, nullptr, "32 bit float mono audio", direction | 4u);
                    if (names) {
                        for (int i = 0; names[i]; ++i) {
                            if (i)
                                out << ',';
                            out << quote(names[i]);
                        }
                        jack.free_(const_cast<char **>(names));
                    }
                }
                out << ']';
            }
            out << ",\"routing\":[";
            for (int i = 0; i < 6; ++i) {
                if (i)
                    out << ',';
                out << quote(jack.routing[i]);
            }
            out << "]}";
            return out.str();
        }
        if (cmd == "AUDIOSET") {
            int index = -1;
            if (!(in >> index) || index < 0 || index > 5 || !jack.client)
                return "{\"ok\":false}";
            std::string name;
            std::getline(in, name);
            if (!name.empty())
                name.erase(0, 1);
            bool found = name.empty();
            auto names = jack.get_ports(jack.client, nullptr, "32 bit float mono audio",
                                        (index < 2 || index > 3) ? 6u : 5u);
            if (names) {
                for (int i = 0; names[i]; ++i)
                    if (name == names[i])
                        found = true;
                jack.free_(const_cast<char **>(names));
            }
            if (!found)
                return "{\"ok\":false}";
            jack.routing[index] = name;
            jack.connectPorts();
            if (const char *data = std::getenv("MULTIFX_DATA")) {
                std::ofstream file(std::string(data) + "/audio-routing.txt");
                for (const auto &port : jack.routing)
                    file << port << '\n';
            }
            return "{\"ok\":true}";
        }
        if (cmd == "PORTS") {
            std::ostringstream out;
            out << "{\"ok\":true,\"ports\":[";
            if (jack.client) {
                auto names = jack.get_ports(jack.client, nullptr, "8 bit raw midi", 2);
                if (names) {
                    for (int i = 0; names[i]; ++i) {
                        if (i)
                            out << ',';
                        out << "{\"name\":" << quote(names[i]) << ",\"connected\":"
                            << (jack.port_connected_to(jack.ports[4], names[i]) ? "true" : "false") << "}";
                    }
                    jack.free_(const_cast<char **>(names));
                }
            }
            out << "]}";
            return out.str();
        }
        if (cmd == "BUFFER") {
            int frames = 0;
            if (!(in >> frames) ||
                (frames != 64 && frames != 128 && frames != 256 && frames != 512 && frames != 1024) ||
                !jack.client)
                return "{\"ok\":false,\"error\":\"Invalid buffer or no audio server\"}";
            if (jack.set_buffer_size(jack.client, uint32_t(frames)) ||
                jack.get_buffer_size(jack.client) != uint32_t(frames))
                return "{\"ok\":false,\"error\":\"Audio device rejected buffer\"}";
            return "{\"ok\":true}";
        }
        if (cmd == "MIDIINPUT") {
            int enable = 0;
            if (!(in >> enable) || (enable != 0 && enable != 1) || !jack.client)
                return "{\"ok\":false}";
            std::string name;
            std::getline(in, name);
            if (name.size() < 2)
                return "{\"ok\":false}";
            name.erase(0, 1);
            bool found = false;
            auto names = jack.get_ports(jack.client, nullptr, "8 bit raw midi", 2);
            if (names) {
                for (int i = 0; names[i]; ++i)
                    if (name == names[i])
                        found = true;
                jack.free_(const_cast<char **>(names));
            }
            if (!found)
                return "{\"ok\":false}";
            if (enable) {
                jack.disabledMidi.erase(name);
                if (!jack.port_connected_to(jack.ports[4], name.c_str()))
                    jack.connect(jack.client, name.c_str(), jack.port_name(jack.ports[4]));
            } else {
                jack.disabledMidi.insert(name);
                if (jack.port_connected_to(jack.ports[4], name.c_str()))
                    jack.disconnect(jack.client, name.c_str(), jack.port_name(jack.ports[4]));
            }
            if (const char *data = std::getenv("MULTIFX_DATA")) {
                std::ofstream file(std::string(data) + "/midi-inputs-disabled.txt");
                for (const auto &port : jack.disabledMidi)
                    file << port << '\n';
            }
            return "{\"ok\":true}";
        }
        if (cmd == "STATE") {
            auto s = state();
            std::ostringstream out;
            out << "{\"ok\":true,\"version\":" << quote(MULTIFX_VERSION) << ",\"sampleRate\":" << sr
                << ",\"latencySamples\":" << engine.currentLatency() << ",\"bpm\":" << bpm.load()
                << ",\"inputMode\":" << inputMode.load()
                << ",\"realtime\":" << (jack.client && jack.is_realtime(jack.client) ? "true" : "false")
                << ",\"xruns\":" << xruns.load()
                << ",\"cpu\":" << (jack.client ? jack.cpu_load(jack.client) : 0)
                << ",\"in\":" << engine.inputPeak.load() << ",\"out\":" << engine.outputPeak.load()
                << ",\"bufferSamples\":" << (jack.client ? jack.get_buffer_size(jack.client) : 0)
                << ",\"clips\":" << engine.clips.load()
                << ",\"delaysReady\":" << (engine.delaysReady() ? "true" : "false")
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
            out << "{\"ok\":true,\"version\":" << quote(MULTIFX_VERSION)
                << ",\"controlCount\":" << multifx::layout().controlCount << ",\"parameters\":[";
            const auto &presentation = multifx::rpi::parameterUI();
            std::vector<multifx::Parameter> p;
            for (const auto &item : presentation) p.push_back(item.definition);
            for (size_t i = 0; i < p.size(); ++i) {
                if (i)
                    out << ',';
                out << "{\"id\":" << i << ",\"key\":" << quote(p[i].key) << ",\"name\":" << quote(p[i].name)
                    << ",\"min\":" << p[i].minimum << ",\"max\":" << p[i].maximum
                    << ",\"default\":" << p[i].initial << ",\"steps\":" << p[i].steps
                    << ",\"choices\":" << quote(p[i].choices)
                    << ",\"label\":" << quote(multifx::controlTitle(int(i)))
                    << ",\"unit\":" << quote(p[i].unit)
                    << ",\"control\":" << quote(presentation[i].control)
                    << ",\"bypass\":" << (presentation[i].bypass ? "true" : "false") << "}";
            }
            out << "]}";
            return out.str();
        }
        if (cmd == "INPUTMODE") {
            int mode;
            if (!(in >> mode) || mode<0 || mode>2)return "{\"ok\":false}";
            if (const char *data=std::getenv("MULTIFX_DATA")) {
                const auto path=std::string(data)+"/input-mode.txt";
                std::ofstream file(path+".tmp");file<<mode;file.close();
                if (!file || std::rename((path+".tmp").c_str(),path.c_str()))return "{\"ok\":false}";
            }
            inputMode.store(mode);return "{\"ok\":true}";
        }
        if (cmd == "TEMPO") {
            float tempo;
            if (!(in >> tempo) || !std::isfinite(tempo) || tempo<20 || tempo>400)
                return "{\"ok\":false,\"error\":\"BPM must be 20..400\"}";
            if (const char *data=std::getenv("MULTIFX_DATA")) {
                const auto path=std::string(data)+"/tempo.txt";
                std::ofstream file(path+".tmp");file<<tempo;file.close();
                if (!file || std::rename((path+".tmp").c_str(),path.c_str()))
                    return "{\"ok\":false,\"error\":\"Tempo could not be saved\"}";
            }
            bpm.store(tempo);
            return "{\"ok\":true}";
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
            values[id].store(multifx::rpi::discreteValue(id, value));
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
                pendingMidi.store(uint32_t(status << 16 | a << 8 | b));
            } else if ((status & 240) == 192) {
                if (a < 8) {
                    auto s = state();
                    multifx::selectFactory(s, a);
                    setState(s);
                    dirty.store(true);
                } else
                    program.store(a);
            } else
                pendingMidi.store(uint32_t(status << 16 | a << 8 | b));
            return "{\"ok\":true}";
        }
        if (cmd == "IRNAME") {
            std::string name;
            std::getline(in, name);
            if (name.size() < 2 || name.size() > 256)
                return "{\"ok\":false}";
            name.erase(0, 1);
            project.irName = name;
            dirty.store(true);
            return "{\"ok\":true}";
        }
        if (cmd == "BPM") {
            float v;
            if (!(in >> v) || !std::isfinite(v) || v < 20 || v > 400)
                return "{\"ok\":false}";
            bpm.store(v);
            return "{\"ok\":true}";
        }
        if (cmd == "KEEP_MIDI") {
            std::string path;
            std::getline(in, path);
            if (path.size() < 2)
                return "{\"ok\":false}";
            path.erase(0, 1);
            multifx::Project old;
            if (!multifx::loadProject(path, old))
                return "{\"ok\":false}";
            auto clean = multifx::factoryPresets()[0];
            const auto &l = multifx::layout();
            for (int cc = 0; cc < 128; ++cc)
                clean.values[l.midiMap + cc] = old.state.values[l.midiMap + cc];
            setState(clean);
            project.state = clean;
            dirty.store(true);
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
                        if (cmd == "IR") {
                            p.state.values[multifx::layout().ir] = 1;
                            p.state.values[multifx::layout().blockBypass[4]] = 0;
                            dirty.store(true);
                        }
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
        auto lastRouting = std::chrono::steady_clock::now() - std::chrono::seconds(1);
        const char *dataDirectory = std::getenv("MULTIFX_DATA");
        std::unique_ptr<multifx::rpi::StateWriter> stateWriter;
        if (dataDirectory) stateWriter=std::make_unique<multifx::rpi::StateWriter>(std::string(dataDirectory)+"/last-state.jmfx");
        auto lastSave = std::chrono::steady_clock::now(), lastChange = lastSave;
        bool savePending = false;
        auto save = [&] {
            if (stateWriter) {
                project.state = state();
                stateWriter->submit(project);
            }
        };
        while (running.load()) {
            fd_set set;
            FD_ZERO(&set);
            FD_SET(fd, &set);
            timeval timeout{0, 10000};
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
            const auto now = std::chrono::steady_clock::now();
            if (dirty.exchange(false)) {
                savePending = true;
                lastChange = now;
            }
            if (savePending && (now - lastChange >= std::chrono::milliseconds(300) ||
                                now - lastSave >= std::chrono::seconds(2))) {
                save();
                savePending = false;
                lastSave = now;
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
            if (jack.client && now - lastRouting >= std::chrono::milliseconds(500)) {
                jack.connectPorts();
                lastRouting = now;
            }
        }
        if (savePending || dirty.exchange(false))
            save();
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
        if (const char *data=std::getenv("MULTIFX_DATA")) {
            std::ifstream channelFile(std::string(data)+"/input-mode.txt");int mode;
            if ((channelFile>>mode) && mode>=0 && mode<=2)host->inputMode.store(mode);
            std::ifstream file(std::string(data)+"/tempo.txt");float tempo;
            if ((file>>tempo) && std::isfinite(tempo) && tempo>=20 && tempo<=400)host->bpm.store(tempo);
        }
        host->engine.prepare(host->sr);
        if (const char *data = std::getenv("MULTIFX_DATA"))
            host->command("LOAD " + std::string(data) + "/last-state.jmfx");
        if (const char *data = std::getenv("MULTIFX_DATA")) {
            std::ifstream file(std::string(data) + "/midi-inputs-disabled.txt");
            std::string port;
            while (std::getline(file, port))
                if (!port.empty())
                    host->jack.disabledMidi.insert(port);
        }
        if (const char *data = std::getenv("MULTIFX_DATA")) {
            std::ifstream file(std::string(data) + "/audio-routing.txt");
            for (auto &port : host->jack.routing)
                std::getline(file, port);
        }
        if (!host->engine.prepareState(host->state()))
            throw std::runtime_error("Active DSP could not be prepared");
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
        return host->jack.serverLost.load() ? 75 : 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
