#pragma once
// ipc/udp_bridge.hpp — UDP <-> MessageBus relay.
//
// pytest "subscribes" by sending any packet to listen_port.
// The bridge records the sender's address and forwards all
// subsequent bus messages to it over UDP (same wire format).

#include <netinet/in.h>

#include <mutex>
#include <thread>

#include "component.h"
#include "message_bus.h"

namespace ipc {

class DOC_DESC(
    "Stateful bridge that relays IPC messages between the internal MessageBus and external UDP "
    "clients.\n\n"
    "It remembers the IP address and port of the last connected test harness and bidirectionally "
    "routes "
    "all subscribed C++ events out through the UDP socket while safely injecting incoming UDP "
    "datagrams "
    "onto the internal MessageBus.")
UdpBridge {
 public:
  using Subscribes =
      MsgList<MsgId::Log, MsgId::QueryState, MsgId::KinematicsData, MsgId::PowerData>;
  using Publishes = MsgList<MsgId::QueryState, MsgId::MotorSequence, MsgId::KinematicsRequest,
                            MsgId::PowerRequest>;

  static constexpr uint16_t kDefaultPort = 9000;

  UdpBridge(MessageBus& bus);
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
