#pragma once
// ipc/udp_bridge.h — UDP <-> MessageBus relay.
//
// pytest "subscribes" by sending any packet to listen_port.
// The bridge records the sender's address and forwards all
// subsequent bus messages to it over UDP (same wire format).

#include <netinet/in.h>

#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

#include "autonomous_msgs.h"
#include "component.h"
#include "core_msgs.h"
#include "simulation_msgs.h"

namespace ipc {

class DOC_DESC(
    "Stateful bridge that relays IPC messages between the internal MessageBus and external UDP "
    "clients.\n\n"
    "It remembers the IP address and port of the last connected test harness and bidirectionally "
    "routes "
    "all subscribed C++ events out through the UDP socket while safely injecting incoming UDP "
    "datagrams "
    "onto the internal MessageBus.") UdpBridge {
 public:
  using Subscribes = MsgList<MsgId::Log, MsgId::StateData, MsgId::KinematicsData, MsgId::PowerData,
                             MsgId::ThermalData, MsgId::EnvironmentAck, MsgId::AutoDriveStatus,
                             MsgId::EnvironmentRequest>;
  using Publishes = MsgList<MsgId::StateRequest, MsgId::MotorSequence, MsgId::KinematicsRequest,
                            MsgId::PowerRequest, MsgId::ThermalRequest, MsgId::AutoDriveCommand,
                            MsgId::EnvironmentData>;

  static constexpr uint16_t kDefaultPort = 9000;

  UdpBridge(MessageBus& bus);
  ~UdpBridge();

  void start();
  UdpBridge(const UdpBridge&) = delete;
  UdpBridge& operator=(const UdpBridge&) = delete;

 private:
  TypedPublisher<UdpBridge> bus_;
  int udp_fd_;
  int wake_[2];
  std::thread rx_thread_;

  std::mutex peer_mu_;
  struct sockaddr_in peer_{};
  bool peer_valid_{false};

  void rx_loop();

  // Serialize a typed payload to wire format and send via UDP.
  template <MsgId Id>
  void forward_to_udp(const typename MessageTraits<Id>::Payload& payload) {
    std::lock_guard lk(peer_mu_);
    if (!peer_valid_) return;

    constexpr size_t psize = sizeof(typename MessageTraits<Id>::Payload);
    std::vector<uint8_t> buf(sizeof(uint16_t) + psize);
    uint16_t id = static_cast<uint16_t>(Id);
    std::memcpy(buf.data(), &id, sizeof(id));
    std::memcpy(buf.data() + sizeof(uint16_t), &payload, psize);

    ::sendto(udp_fd_, buf.data(), buf.size(), 0, reinterpret_cast<const sockaddr*>(&peer_),
             sizeof(peer_));
  }
};

}  // namespace ipc
