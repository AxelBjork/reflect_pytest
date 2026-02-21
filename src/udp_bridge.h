#pragma once
// ipc/udp_bridge.hpp — UDP <-> MessageBus relay.
//
// pytest "subscribes" by sending any packet to listen_port.
// The bridge records the sender's address and forwards all
// subsequent bus messages to it over UDP (same wire format).

#include <netinet/in.h>

#include <mutex>
#include <thread>

#include "message_bus.hpp"

namespace ipc {

class UdpBridge {
 public:
  // listen_port: UDP port the bridge binds to (bridge listens here).
  UdpBridge(MessageBus& bus, uint16_t listen_port);
  ~UdpBridge();

  UdpBridge(const UdpBridge&) = delete;
  UdpBridge& operator=(const UdpBridge&) = delete;

 private:
  MessageBus& bus_;
  int udp_fd_;
  int wake_[2];
  std::thread rx_thread_;

  std::mutex peer_mu_;
  struct sockaddr_in peer_{};
  bool peer_valid_{false};

  void rx_loop();
  void forward_to_udp(RawMessage msg);
};

}  // namespace ipc
