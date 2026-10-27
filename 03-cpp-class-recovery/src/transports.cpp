#include <cstdio>
#include <cstring>

#include "transport.hpp"

Transport::~Transport() = default;

// ---- TCP -------------------------------------------------------------------
TcpTransport::TcpTransport(const char* host, uint16_t port) : port_(port)
{
    std::snprintf(host_, sizeof(host_), "%s", host);
}
TcpTransport::~TcpTransport() { close(); }
const char* TcpTransport::name() const { return "tcp"; }
bool TcpTransport::open()
{
    state_ = LinkState::Opening;
    std::printf("[tcp] connect %s:%u (nodelay=%d)\n", host_, port_, nodelay_);
    fd_ = 3;
    state_ = LinkState::Open;
    return true;
}
long TcpTransport::send(const uint8_t* data, size_t len)
{
    if (state_ != LinkState::Open) { errors_++; return -1; }
    (void)data;
    tx_bytes_ += (uint32_t)len;
    std::printf("[tcp] -> %zu bytes on fd %d\n", len, fd_);
    return (long)len;
}
long TcpTransport::recv(uint8_t* buf, size_t cap)
{
    static const char ack[] = "ACK";
    size_t n = cap < sizeof(ack) ? cap : sizeof(ack);
    std::memcpy(buf, ack, n);
    rx_bytes_ += (uint32_t)n;
    return (long)n;
}
void TcpTransport::close()
{
    if (fd_ >= 0) std::printf("[tcp] close fd %d\n", fd_);
    fd_ = -1;
    state_ = LinkState::Closed;
}

// ---- BLE -------------------------------------------------------------------
BleTransport::BleTransport(const char* mac)
{
    unsigned b[6] = {0};
    std::sscanf(mac, "%x:%x:%x:%x:%x:%x", &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]);
    for (int i = 0; i < 6; i++) mac_[i] = (uint8_t)b[i];
}
const char* BleTransport::name() const { return "ble"; }
bool BleTransport::open()
{
    state_ = LinkState::Opening;
    conn_handle_ = (uint16_t)(0x40 | (mac_[5] & 0x0F));
    att_mtu_ = 247;
    std::printf("[ble] connected %02x:%02x:..:%02x handle %#x mtu %u\n", mac_[0], mac_[1], mac_[5], conn_handle_, att_mtu_);
    state_ = LinkState::Open;
    return true;
}
long BleTransport::send(const uint8_t* data, size_t len)
{
    if (state_ != LinkState::Open || len > (size_t)mtu()) { errors_++; return -1; }
    (void)data;
    seq_++;
    tx_bytes_ += (uint32_t)len;
    std::printf("[ble] notify #%u (%zu bytes)\n", seq_, len);
    return (long)len;
}
long BleTransport::recv(uint8_t* buf, size_t cap)
{
    if (cap < 2) return 0;
    buf[0] = 0x1B; buf[1] = (uint8_t)seq_;
    rx_bytes_ += 2;
    return 2;
}
void BleTransport::close()
{
    conn_handle_ = 0xFFFF;
    state_ = LinkState::Closed;
}

// ---- Serial ----------------------------------------------------------------
SerialTransport::SerialTransport(const char* device, uint32_t baud) : baud_(baud)
{
    std::snprintf(device_, sizeof(device_), "%s", device);
}
const char* SerialTransport::name() const { return "serial"; }
bool SerialTransport::open()
{
    std::printf("[serial] %s %u 8%c1\n", device_, baud_, parity_);
    state_ = LinkState::Open;
    return true;
}
long SerialTransport::send(const uint8_t* data, size_t len)
{
    (void)data;
    tx_bytes_ += (uint32_t)len;
    return (long)len;
}
long SerialTransport::recv(uint8_t* buf, size_t cap)
{
    if (!cap) return 0;
    buf[0] = 0x06; /* ACK */
    rx_bytes_++;
    return 1;
}
void SerialTransport::close() { state_ = LinkState::Closed; }

// ---- Logging decorator -----------------------------------------------------
LoggingTransport::LoggingTransport(Transport* inner) : inner_(inner) {}
LoggingTransport::~LoggingTransport() { delete inner_; }
const char* LoggingTransport::name() const { return inner_->name(); }
bool LoggingTransport::open()
{
    calls_++;
    bool ok = inner_->open();
    std::printf("[log] open(%s) = %d\n", inner_->name(), ok);
    state_ = inner_->state();
    return ok;
}
long LoggingTransport::send(const uint8_t* data, size_t len)
{
    calls_++;
    long n = inner_->send(data, len);
    std::printf("[log] send(%zu) = %ld\n", len, n);
    tx_bytes_ = inner_->tx_bytes();
    return n;
}
long LoggingTransport::recv(uint8_t* buf, size_t cap)
{
    calls_++;
    long n = inner_->recv(buf, cap);
    rx_bytes_ = inner_->rx_bytes();
    return n;
}
void LoggingTransport::close()
{
    calls_++;
    inner_->close();
    state_ = inner_->state();
}
int LoggingTransport::mtu() const { return inner_->mtu(); }
