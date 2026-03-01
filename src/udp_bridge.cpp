// ipc/udp_bridge.cpp

#include "udp_bridge.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/select.h>
#include <unistd.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <stdexcept>

#include "component_logger.h"

namespace ipc {

namespace {
alignas(64) std::atomic<bool> g_peer_valid{false};
}  // namespace

bool UdpBridge::is_connected() {
  return g_peer_valid.load(std::memory_order_acquire);
}

static constexpr size_t kMaxDgram = 4096;

UdpBridge::UdpBridge(MessageBus& bus) : bus_(bus) {
  udp_fd_ = ::socket(AF_INET, SOCK_DGRAM, 0);
  if (udp_fd_ < 0) throw std::runtime_error("UdpBridge: socket() failed");

  int opt = 1;
  ::setsockopt(udp_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(kDefaultPort);

  if (::bind(udp_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    ::close(udp_fd_);
    throw std::runtime_error("UdpBridge: bind() failed");
  }

  if (::pipe(wake_) < 0) {
    ::close(udp_fd_);
    throw std::runtime_error("UdpBridge: pipe() failed");
  }

  // Subscribe to outgoing messages — typed handlers serialize to UDP.
  auto bind_udp = [this]<MsgId... Ids>(MsgList<Ids...>) {
    (bus_.bus().subscribe<Ids>(
         [this](const typename MessageTraits<Ids>::Payload& p) { forward_to_udp<Ids>(p); }),
     ...);
  };
  bind_udp(Subscribes{});
}

void UdpBridge::start() {
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

      // Update the peer address if it changed. By only doing this on mismatch,
      // we avoid data races and cache line bouncing during steady-state test execution.
      bool new_peer = peer_.sin_addr.s_addr != sender.sin_addr.s_addr || peer_.sin_port != sender.sin_port;

      if (new_peer) {
        g_peer_valid.store(false, std::memory_order_release);
        peer_ = sender;
        g_peer_valid.store(true, std::memory_order_release);
      }

      // Inject valid datagrams (>= 2 bytes for msgId) into the bus.
      if (n < static_cast<ssize_t>(sizeof(uint16_t))) continue;

      uint16_t id_raw{};
      std::memcpy(&id_raw, buf, sizeof(id_raw));
      bus_.publish_if_authorized(static_cast<MsgId>(id_raw), buf + sizeof(uint16_t),
                                 static_cast<size_t>(n) - sizeof(uint16_t));
    }
  }
}

}  // namespace ipc
