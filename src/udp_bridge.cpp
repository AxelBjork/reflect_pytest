// ipc/udp_bridge.cpp

#include "udp_bridge.h"

#include <arpa/inet.h>
#include <sys/select.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace ipc {

static constexpr size_t kMaxDgram = 4096;

UdpBridge::UdpBridge(MessageBus& bus, uint16_t listen_port) : bus_(bus) {
  udp_fd_ = ::socket(AF_INET, SOCK_DGRAM, 0);
  if (udp_fd_ < 0) throw std::runtime_error("UdpBridge: socket() failed");

  int opt = 1;
  ::setsockopt(udp_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(listen_port);

  if (::bind(udp_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    ::close(udp_fd_);
    throw std::runtime_error("UdpBridge: bind() failed");
  }

  if (::pipe(wake_) < 0) {
    ::close(udp_fd_);
    throw std::runtime_error("UdpBridge: pipe() failed");
  }

  // Subscribe to every bus message → forward to the UDP peer.
  bus_.subscribe_all([this](RawMessage msg) { forward_to_udp(std::move(msg)); });

  rx_thread_ = std::thread(&UdpBridge::rx_loop, this);
}

UdpBridge::~UdpBridge() {
  char b = 0;
  (void)::write(wake_[1], &b, 1);
  if (rx_thread_.joinable()) rx_thread_.join();
  ::close(udp_fd_);
  ::close(wake_[0]);
  ::close(wake_[1]);
}

void UdpBridge::rx_loop() {
  uint8_t buf[kMaxDgram];

  while (true) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(udp_fd_, &fds);
    FD_SET(wake_[0], &fds);
    int maxfd = std::max(udp_fd_, wake_[0]) + 1;

    if (::select(maxfd, &fds, nullptr, nullptr, nullptr) < 0) {
      if (errno == EINTR) continue;
      break;
    }
    if (FD_ISSET(wake_[0], &fds)) break;

    if (FD_ISSET(udp_fd_, &fds)) {
      sockaddr_in sender{};
      socklen_t slen = sizeof(sender);
      ssize_t n =
          ::recvfrom(udp_fd_, buf, sizeof(buf), 0, reinterpret_cast<sockaddr*>(&sender), &slen);
      if (n < 0) continue;

      // Always record the sender so we can forward messages back.
      {
        std::lock_guard lk(peer_mu_);
        peer_ = sender;
        peer_valid_ = true;
      }

      // Inject valid datagrams (>= 2 bytes for msgId) into the bus.
      if (n < static_cast<ssize_t>(sizeof(uint16_t))) continue;

      uint16_t id_raw{};
      std::memcpy(&id_raw, buf, sizeof(id_raw));
      bus_.publish_raw(static_cast<MsgId>(id_raw), buf + sizeof(uint16_t),
                       static_cast<size_t>(n) - sizeof(uint16_t));
    }
  }
}

void UdpBridge::forward_to_udp(RawMessage msg) {
  std::lock_guard lk(peer_mu_);
  if (!peer_valid_) return;

  // Re-assemble wire format: [uint16_t msgId][payload]
  std::vector<uint8_t> buf(sizeof(uint16_t) + msg.payload.size());
  uint16_t id = static_cast<uint16_t>(msg.msgId);
  std::memcpy(buf.data(), &id, sizeof(id));
  std::memcpy(buf.data() + sizeof(uint16_t), msg.payload.data(), msg.payload.size());

  ::sendto(udp_fd_, buf.data(), buf.size(), 0, reinterpret_cast<const sockaddr*>(&peer_),
           sizeof(peer_));
}

}  // namespace ipc
