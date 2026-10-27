// kestrel-transport: fleet link test tool.  usage: kestrel-transport <uri> [-v]
#include <cstdio>
#include <cstring>

#include "transport.hpp"

namespace {

// Frames: [len][cmd][payload...][xor]
__attribute__((noinline)) long send_frame(Transport* t, uint8_t cmd, const char* payload)
{
    uint8_t frame[256];
    size_t n = std::strlen(payload);
    int room = t->mtu() - 3;
    if ((int)n > room) n = (size_t)room;
    frame[0] = (uint8_t)(n + 1);
    frame[1] = cmd;
    std::memcpy(frame + 2, payload, n);
    uint8_t x = 0;
    for (size_t i = 0; i < n + 2; i++) x ^= frame[i];
    frame[n + 2] = x;
    return t->send(frame, n + 3);
}

__attribute__((noinline)) bool handshake(Transport* t)
{
    uint8_t reply[16];
    if (!t->open()) return false;
    if (send_frame(t, 0x01, "KESTREL-HELLO v2") < 0) return false;
    long n = t->recv(reply, sizeof(reply));
    return n > 0;
}

__attribute__((noinline)) void report(const Transport* t)
{
    std::printf("%s: state=%u mtu=%d tx=%u rx=%u\n", t->name(), (unsigned)t->state(), t->mtu(), t->tx_bytes(), t->rx_bytes());
}

}  // namespace

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <tcp://host:port|ble://mac|serial://dev@baud> [-v]\n", argv[0]);
        return 2;
    }
    bool verbose = argc > 2 && !std::strcmp(argv[2], "-v");
    Transport* t = make_transport(argv[1], verbose);
    if (!t) {
        std::fprintf(stderr, "unsupported transport: %s\n", argv[1]);
        return 1;
    }
    if (handshake(t)) {
        send_frame(t, 0x10, "DOOR 7 UNLOCK");
        send_frame(t, 0x11, "AUDIT EXPORT");
    }
    report(t);
    t->close();
    delete t;
    return 0;
}
