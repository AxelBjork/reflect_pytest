// generate_docs.cpp — Standalone executable that reflects over ipc:: messages
// and emits a GitHub-renderable Markdown documentation file.
//
// Build:  cmake --build build --target generate_docs
// Run:    ./build/generate_docs > build/doc/ipc_protocol.md
//         (CMake custom command does this automatically)

#include "doc_generator.h"

int main() {
  // ── Front-matter & module overview ─────────────────────────────────────────

  std::cout <<
      R"(# IPC Protocol Reference

> **Auto-generated** by `generate_docs` using C++26 static reflection (P2996 + P3394).
> Do not edit by hand — re-run `cmake --build build --target generate_docs` to refresh.

This document describes the full wire protocol used between the Python test harness
and the C++ SIL (Software-in-the-Loop) application.

## System Architecture

```
┌─────────────────────┐          UDP (port 9000)         ┌─────────────────────┐
│   Python / pytest   │ ──────────────────────────────── │     sil_app (C++)   │
│                     │   [uint16_t msgId][payload bytes] │                     │
│  udp_client.py      │                                   │  UdpBridge          │
│  generated.py       │                                   │  ├─ MessageBus      │
└─────────────────────┘                                   │  ├─ Simulator       │
                                                          │  └─ Logger          │
                                                          └─────────────────────┘
```

**Wire format:** Every datagram starts with a `uint16_t` message ID (host-byte
order) followed immediately by the fixed-size payload struct (packed, no padding).
If `sizeof(received payload) != sizeof(Payload)` the message is silently discarded.

**Threads (C++ side):**

| Thread | Purpose |
|---|---|
| `main` | Waits on shutdown signal; futex sleep |
| `heartbeat` | Publishes `LogPayload` "Hello World #N" every 500 ms |
| `bus-listener` | AF\_UNIX recv loop → dispatch to subscribers |
| `sim-exec` | Steps through `MotorSequencePayload` in real time |
| `sim-log` | Publishes kinematics status log every 1 000 ms |
| `bridge-rx` | UDP recv → injects into MessageBus |

---

## Message Flow

)";

  emit_mermaid_flow();

  // ── Enums ───────────────────────────────────────────────────────────────────
  std::cout << "---\n\n## Enums\n\n";
  emit_md_enum_section<ipc::MsgId>();
  emit_md_enum_section<ipc::Severity>();
  emit_md_enum_section<ipc::ComponentId>();
  emit_md_enum_section<ipc::SystemState>();

  // ── Message Payloads ────────────────────────────────────────────────────────
  std::cout << "---\n\n## Message Payloads\n\n";
  std::cout << "Each section corresponds to one `MsgId` enumerator. "
               "The **direction badge** shows which side initiates the message.\n\n";

  constexpr std::size_t num_msgs = doc_get_enum_size<ipc::MsgId>();
  [&]<std::size_t... Is>(std::index_sequence<Is...>) {
    (..., [] {
      constexpr auto e = DocEnumArrHolder<ipc::MsgId, num_msgs>::arr[Is];
      constexpr uint32_t val = static_cast<uint32_t>([:e:]);
      emit_md_payload_section_for_msg_id<val>();
    }());
  }(std::make_index_sequence<num_msgs>{});

  // ── Footer ─────────────────────────────────────────────────────────────────
  std::cout <<
      R"(---

## Regenerating This File

```bash
# From the repo root:
cmake -B build -G Ninja
cmake --build build --target generate_docs
# Output: build/doc/ipc_protocol.md
```

_Generated with GCC trunk `-std=c++26 -freflection` (P2996R13 + P3394R4)._
)";

  return 0;
}
