// ipc/socket_bus.cpp — AF_UNIX SOCK_DGRAM implementation.

#include "ipc/socket_bus.hpp"

#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace ipc {

// Maximum datagram we will ever read (must be >= largest payload + 2).
static constexpr size_t kMaxDgram = 4096;

// ── Helpers ───────────────────────────────────────────────────────────────────

static int make_socket(const std::string& path, bool create)
{
    int fd = ::socket(AF_UNIX, SOCK_DGRAM, 0);
    if (fd < 0)
        throw std::runtime_error(std::string("socket(): ") + std::strerror(errno));

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    if (path.size() >= sizeof(addr.sun_path))
        throw std::runtime_error("Socket path too long: " + path);
    std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);

    if (create) {
        ::unlink(path.c_str()); // remove stale socket
        if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            ::close(fd);
            throw std::runtime_error(std::string("bind(): ") + std::strerror(errno));
        }
    } else {
        if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            ::close(fd);
            throw std::runtime_error(std::string("connect(): ") + std::strerror(errno));
        }
    }
    return fd;
}

// ── SocketBus ─────────────────────────────────────────────────────────────────

SocketBus::SocketBus(std::string path, bool create)
    : fd_(make_socket(path, create))
    , path_(std::move(path))
    , owner_(create)
{}

SocketBus::~SocketBus()
{
    if (fd_ >= 0) {
        ::close(fd_);
        if (owner_)
            ::unlink(path_.c_str());
    }
}

SocketBus::SocketBus(SocketBus&& o) noexcept
    : fd_(o.fd_), path_(std::move(o.path_)), owner_(o.owner_)
{
    o.fd_    = -1;
    o.owner_ = false;
}

SocketBus& SocketBus::operator=(SocketBus&& o) noexcept
{
    if (this != &o) {
        if (fd_ >= 0) {
            ::close(fd_);
            if (owner_) ::unlink(path_.c_str());
        }
        fd_    = o.fd_;    o.fd_    = -1;
        path_  = std::move(o.path_);
        owner_ = o.owner_; o.owner_ = false;
    }
    return *this;
}

void SocketBus::send_raw(MsgId id, const void* data, size_t size)
{
    // Wire: [uint16_t msgId][payload]
    std::vector<uint8_t> buf(sizeof(uint16_t) + size);
    uint16_t id_le = static_cast<uint16_t>(id);
    std::memcpy(buf.data(),                   &id_le, sizeof(id_le));
    std::memcpy(buf.data() + sizeof(uint16_t), data,  size);

    ssize_t sent = ::send(fd_, buf.data(), buf.size(), 0);
    if (sent < 0)
        throw std::runtime_error(std::string("send(): ") + std::strerror(errno));
}

std::optional<RawMessage> SocketBus::recv()
{
    uint8_t buf[kMaxDgram];

    while (true) {
        ssize_t n = ::recv(fd_, buf, sizeof(buf), 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return std::nullopt; // closed / error
        }
        if (n < static_cast<ssize_t>(sizeof(uint16_t))) {
            std::fprintf(stderr, "[ipc] datagram too short (%zd bytes), discarded\n", n);
            continue;
        }

        uint16_t id_raw{};
        std::memcpy(&id_raw, buf, sizeof(id_raw));
        auto id      = static_cast<MsgId>(id_raw);
        size_t plen  = static_cast<size_t>(n) - sizeof(uint16_t);

        RawMessage msg;
        msg.msgId   = id;
        msg.payload = std::vector<uint8_t>(buf + sizeof(uint16_t),
                                           buf + sizeof(uint16_t) + plen);
        return msg;
    }
}

} // namespace ipc
