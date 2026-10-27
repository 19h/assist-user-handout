#include <cstdlib>
#include <cstring>

#include "transport.hpp"

// tcp://host:port | ble://aa:bb:cc:dd:ee:ff | serial:///dev/ttyUSB0@115200
__attribute__((noinline)) Transport* make_transport(const char* uri, bool verbose)
{
    Transport* t = nullptr;
    if (!std::strncmp(uri, "tcp://", 6)) {
        char host[64];
        const char* colon = std::strrchr(uri + 6, ':');
        size_t n = colon ? (size_t)(colon - (uri + 6)) : std::strlen(uri + 6);
        if (n >= sizeof(host)) n = sizeof(host) - 1;
        std::memcpy(host, uri + 6, n);
        host[n] = '\0';
        t = new TcpTransport(host, (uint16_t)(colon ? std::atoi(colon + 1) : 7443));
    } else if (!std::strncmp(uri, "ble://", 6)) {
        t = new BleTransport(uri + 6);
    } else if (!std::strncmp(uri, "serial://", 9)) {
        const char* at = std::strchr(uri + 9, '@');
        char dev[32] = "/dev/ttyUSB0";
        if (at && (size_t)(at - (uri + 9)) < sizeof(dev)) {
            std::memcpy(dev, uri + 9, (size_t)(at - (uri + 9)));
            dev[at - (uri + 9)] = '\0';
        }
        t = new SerialTransport(dev, at ? (uint32_t)std::atoi(at + 1) : 115200u);
    }
    if (t && verbose)
        t = new LoggingTransport(t);
    return t;
}
