#pragma once
// ipc/socket_bus.hpp — AF_UNIX SOCK_DGRAM send/recv helpers.
//
// All messages share the wire format:
//   [ uint16_t msgId ][ payload bytes (sizeof(T)) ]
//
// Invalid-size datagrams are silently dropped with an error to stderr.

#include <cstddef>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include "traits.h"  // brings in messages.hpp + MessageTraits<>

namespace ipc {

// Raw datagram: msgId + opaque payload bytes.
struct RawMessage {
  MsgId msgId;
  std::vector<uint8_t> payload;
};

// SocketBus wraps a single AF_UNIX SOCK_DGRAM file descriptor.
//
// Typical server setup:
//   SocketBus bus("/tmp/sil.sock", /*create=*/true);
//
// Typical client setup:
//   SocketBus bus("/tmp/sil.sock", /*create=*/false);
class SocketBus {
 public:
  explicit SocketBus(std::string path, bool create);
  ~SocketBus();

  // Non-copyable, movable.
  SocketBus(const SocketBus&) = delete;
  SocketBus& operator=(const SocketBus&) = delete;
  SocketBus(SocketBus&&) noexcept;
  SocketBus& operator=(SocketBus&&) noexcept;

  // Typed send: serialises msgId + sizeof(T) payload bytes.
  template <MsgId Id>
  void send(const typename MessageTraits<Id>::Payload& payload);

  // Raw send (used internally and for testing).
  void send_raw(MsgId id, const void* data, size_t size);

  // Blocking recv.  Returns std::nullopt if the fd was closed/interrupted.
  // Datagrams with wrong payload size are discarded (returns the next valid
  // one).
  std::optional<RawMessage> recv();

  int fd() const {
    return fd_;
  }

 private:
  int fd_;
  std::string path_;
  bool owner_;  // true → unlink path_ on destruction
};

// ── Template implementation
// ───────────────────────────────────────────────────

template <MsgId Id>
void SocketBus::send(const typename MessageTraits<Id>::Payload& payload) {
  send_raw(Id, &payload, sizeof(payload));
}

}  // namespace ipc
