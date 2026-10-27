#pragma once
#include <cstddef>
#include <cstdint>

// Kestrel fleet link layer: one interface, several physical transports.
enum class LinkState : uint32_t { Closed = 0, Opening = 1, Open = 2, Failed = 3 };

class Transport {
public:
    virtual ~Transport();
    virtual const char* name() const = 0;
    virtual bool open() = 0;
    virtual long send(const uint8_t* data, size_t len) = 0;
    virtual long recv(uint8_t* buf, size_t cap) = 0;
    virtual void close() = 0;
    virtual int mtu() const { return 244; }

    LinkState state() const { return state_; }
    uint32_t tx_bytes() const { return tx_bytes_; }
    uint32_t rx_bytes() const { return rx_bytes_; }

protected:
    LinkState state_ = LinkState::Closed;
    uint32_t tx_bytes_ = 0;
    uint32_t rx_bytes_ = 0;
    uint32_t errors_ = 0;
};

class TcpTransport final : public Transport {
public:
    TcpTransport(const char* host, uint16_t port);
    ~TcpTransport() override;
    const char* name() const override;
    bool open() override;
    long send(const uint8_t* data, size_t len) override;
    long recv(uint8_t* buf, size_t cap) override;
    void close() override;
    int mtu() const override { return 1400; }

private:
    char host_[64];
    uint16_t port_;
    int fd_ = -1;
    bool nodelay_ = true;
};

class BleTransport final : public Transport {
public:
    explicit BleTransport(const char* mac);
    const char* name() const override;
    bool open() override;
    long send(const uint8_t* data, size_t len) override;
    long recv(uint8_t* buf, size_t cap) override;
    void close() override;
    int mtu() const override { return att_mtu_ - 3; }

private:
    uint8_t mac_[6];
    uint16_t att_mtu_ = 23;
    uint16_t conn_handle_ = 0xFFFF;
    uint32_t seq_ = 0;
};

class SerialTransport final : public Transport {
public:
    SerialTransport(const char* device, uint32_t baud);
    const char* name() const override;
    bool open() override;
    long send(const uint8_t* data, size_t len) override;
    long recv(uint8_t* buf, size_t cap) override;
    void close() override;

private:
    char device_[32];
    uint32_t baud_;
    uint8_t parity_ = 'N';
};

// Decorator: logs every operation of the wrapped transport.
class LoggingTransport final : public Transport {
public:
    explicit LoggingTransport(Transport* inner);
    ~LoggingTransport() override;
    const char* name() const override;
    bool open() override;
    long send(const uint8_t* data, size_t len) override;
    long recv(uint8_t* buf, size_t cap) override;
    void close() override;
    int mtu() const override;

private:
    Transport* inner_;
    uint32_t calls_ = 0;
};

Transport* make_transport(const char* uri, bool verbose);
