#pragma once
// ipc/udp_bridge.h — UDP <-> MessageBus relay.
//
// pytest "subscribes" by sending any packet to listen_port.
// The bridge records the sender's address and forwards all
// subsequent bus messages to it over UDP (same wire format).

#include <netinet/in.h>

#include <cstring>
#include <thread>

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
                             MsgId::EnvironmentRequest, MsgId::EnvironmentData, MsgId::SensorAck,
                             MsgId::RevisionResponse>;
  using Publishes = MsgList<MsgId::StateRequest, MsgId::MotorSequence, MsgId::KinematicsRequest,
                            MsgId::PowerRequest, MsgId::ThermalRequest, MsgId::AutoDriveCommand,
                            MsgId::EnvironmentData, MsgId::SensorRequest, MsgId::RevisionRequest>;

  static constexpr uint16_t kDefaultPort = 9000;

  static bool is_connected();

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

  struct sockaddr_in peer_{};

  void rx_loop();

  // Serialize a typed payload to wire format and send via UDP.
  // Performs zero dynamic allocations and is lock-free.
  template <MsgId Id, typename Payload>
  void forward_to_udp(const Payload& payload) {
    if (!is_connected()) return;

    constexpr size_t total_size = sizeof(uint16_t) + sizeof(Payload);
    uint8_t buf[total_size];

    uint16_t id_raw = static_cast<uint16_t>(Id);
    std::memcpy(buf, &id_raw, sizeof(uint16_t));
    std::memcpy(buf + sizeof(uint16_t), &payload, sizeof(Payload));

    ::sendto(udp_fd_, buf, total_size, 0, reinterpret_cast<sockaddr*>(&peer_), sizeof(peer_));
  }
};

}  // namespace ipc
