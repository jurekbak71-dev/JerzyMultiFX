// Bounded BLE-to-JACK bridge, based on JerzyAudioPlatform's MIDI bridge architecture.
// Unlike the old bridge, routing is owned by MultiFX; no reconnection to an old DSP client.
#include <array>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstring>
#include <dlfcn.h>
#include <fstream>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
struct Event {
    unsigned size;
    std::array<unsigned char, 3> bytes;
};
struct Bridge {
    void *library = nullptr, *client = nullptr, *port = nullptr;
    std::array<Event, 1024> queue{};
    std::atomic<unsigned> head{0}, tail{0};
    std::atomic<bool> panic{false}, running{true}, lost{false};
    std::atomic<unsigned long long> sent{0}, dropped{0};
    void *(*open)(const char *, unsigned, unsigned *, ...) = nullptr;
    int (*close)(void *) = nullptr;
    int (*activate)(void *) = nullptr;
    int (*deactivate)(void *) = nullptr;
    void *(*registerPort)(void *, const char *, const char *, unsigned, unsigned) = nullptr;
    void *(*buffer)(void *, unsigned) = nullptr;
    void (*clear)(void *) = nullptr;
    int (*write)(void *, unsigned, const unsigned char *, size_t) = nullptr;
    int (*callback)(void *, int (*)(unsigned, void *), void *) = nullptr;
    void (*shutdown)(void *, void (*)(void *), void *) = nullptr;
    int (*connected)(const void *) = nullptr;
    template <class T> bool symbol(T &out, const char *name) {
        out = reinterpret_cast<T>(dlsym(library, name));
        return out != nullptr;
    }
    bool prepare() {
        library = dlopen("libjack.so.0", RTLD_NOW | RTLD_LOCAL);
        if (!library)
            return false;
#define LOAD(field, name)                                                                                    \
    if (!symbol(field, "jack_" name))                                                                        \
    return false
        LOAD(open, "client_open");
        LOAD(close, "client_close");
        LOAD(activate, "activate");
        LOAD(deactivate, "deactivate");
        LOAD(registerPort, "port_register");
        LOAD(buffer, "port_get_buffer");
        LOAD(clear, "midi_clear_buffer");
        LOAD(write, "midi_event_write");
        LOAD(callback, "set_process_callback");
        LOAD(shutdown, "on_shutdown");
        LOAD(connected, "port_connected");
#undef LOAD
        unsigned status = 0;
        client = open("JerzyBluetoothMIDI", 3, &status);
        if (!client)
            return false;
        port = registerPort(client, "out", "8 bit raw midi", 2, 0);
        if (!port)
            return false;
        if (callback(client, process, this))
            return false;
        shutdown(
            client,
            [](void *p) {
                auto &b = *static_cast<Bridge *>(p);
                b.lost.store(true);
                b.running.store(false);
            },
            this);
        return activate(client) == 0;
    }
    static int process(unsigned frames, void *opaque) {
        auto &b = *static_cast<Bridge *>(opaque);
        auto *out = b.buffer(b.port, frames);
        b.clear(out);
        if (b.panic.exchange(false)) {
            b.tail.store(b.head.load(std::memory_order_acquire), std::memory_order_release);
            for (unsigned ch = 0; ch < 16; ++ch) {
                unsigned char off[]{static_cast<unsigned char>(176 + ch), 123, 0};
                b.write(out, 0, off, 3);
            }
            return 0;
        }
        unsigned t = b.tail.load(std::memory_order_relaxed);
        const auto h = b.head.load(std::memory_order_acquire);
        for (unsigned count = 0; t != h && count < 128; ++count, ++t) {
            const auto &e = b.queue[t % b.queue.size()];
            if (b.write(out, 0, e.bytes.data(), e.size)) {
                b.dropped.fetch_add(1);
                b.panic.store(true);
            } else
                b.sent.fetch_add(1);
        }
        b.tail.store(t, std::memory_order_release);
        return 0;
    }
    bool push(const unsigned char *bytes, unsigned n) {
        if (!n || n > 3 || bytes[0] < 128)
            return false;
        for (unsigned i = 1; i < n; ++i)
            if (bytes[i] > 127)
                return false;
        const unsigned h = head.load(std::memory_order_relaxed);
        if (h - tail.load(std::memory_order_acquire) >= queue.size()) {
            dropped.fetch_add(1);
            panic.store(true);
            return false;
        }
        auto &e = queue[h % queue.size()];
        e.size = n;
        for (unsigned i = 0; i < n; ++i)
            e.bytes[i] = bytes[i];
        head.store(h + 1, std::memory_order_release);
        return true;
    }
    ~Bridge() {
        if (client) {
            deactivate(client);
            close(client);
        }
        if (library)
            dlclose(library);
    }
};
static Bridge *active = nullptr;
static void stop(int) {
    if (active)
        active->running.store(false);
}
int main(int argc, char **argv) {
    if (argc != 3)
        return 2;
    const std::string path = argv[1], status = argv[2];
    if (path.size() >= sizeof(sockaddr_un::sun_path))
        return 2;
    Bridge b;
    active = &b;
    signal(SIGINT, stop);
    signal(SIGTERM, stop);
    if (!b.prepare())
        return 3;
    int fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strcpy(addr.sun_path, path.c_str());
    if (fd < 0 || bind(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)))
        return 4;
    chmod(path.c_str(), 0600);
    auto updated = std::chrono::steady_clock::now() - std::chrono::seconds(2);
    while (b.running.load()) {
        pollfd p{fd, POLLIN, 0};
        if (poll(&p, 1, 200) > 0) {
            unsigned char bytes[16];
            const auto n = recv(fd, bytes, sizeof(bytes), 0);
            if (n == 1 && bytes[0] == '!')
                b.panic.store(true);
            else if (n > 0)
                b.push(bytes, unsigned(n));
        }
        if (std::chrono::steady_clock::now() - updated >= std::chrono::seconds(1)) {
            std::ofstream file(status + ".tmp");
            file << "{\"pid\":" << getpid()
                 << ",\"running\":true,\"connected\":" << (b.connected(b.port) ? "true" : "false")
                 << ",\"sent\":" << b.sent.load() << ",\"dropped\":" << b.dropped.load() << "}\n";
            file.close();
            rename((status + ".tmp").c_str(), status.c_str());
            updated = std::chrono::steady_clock::now();
        }
    }
    b.panic.store(true);
    usleep(30000);
    ::close(fd);
    unlink(path.c_str());
    std::ofstream(status) << "{\"running\":false,\"connected\":false}\n";
    return b.lost.load() ? 75 : 0;
}
