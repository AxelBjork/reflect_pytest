// generate_docs.cpp — Standalone executable that reflects over ipc:: messages
// and emits a GitHub-renderable Markdown documentation file.

#include <filesystem>

#include "app_components.h"
#include "doc_generator.h"

// Combine all sil_app services plus the virtual main publisher thread
using AllComponents = decltype(std::tuple_cat(std::declval<sil::AppServices>()));

template <typename Components>
void emit_payloads() {
  [&]<std::size_t... Is>(std::index_sequence<Is...>) {
    (..., [] {
      constexpr auto e = EnumArrHolder<MsgId, get_enum_size<MsgId>()>::arr[Is];
      constexpr uint32_t val = static_cast<uint32_t>([:e:]);
      emit_md_payload_section_for_msg_id<Components, val>();
    }());
  }(std::make_index_sequence<get_enum_size<MsgId>()>{});
}

template <typename Components>
void emit_components() {
  constexpr std::size_t num_components = std::tuple_size_v<Components>;
  [&]<std::size_t... Is>(std::index_sequence<Is...>) {
    (..., [] {
      using C = std::tuple_element_t<Is, Components>;
      constexpr auto R = ^^C;
      const std::string name{cpp_type_name_str<C>()};
      constexpr auto desc = get_desc<R>();
      if (desc.text[0] != '\0') {
        std::cout << "### `" << name << "`\n\n> " << desc.text << "\n\n";
      }
    }());
  }(std::make_index_sequence<num_components>{});
}

int main(int argc, char** argv) {
  namespace fs = std::filesystem;
  fs::path out_dir = ".";
  if (argc > 1) {
    out_dir = argv[1];
  }
  fs::path dot_path = out_dir / "ipc_flow.dot";

  // ── Front-matter & module overview ─────────────────────────────────────────

  std::cout <<
      R"(# IPC Protocol Reference

[Home](../../README.md)

> **Auto-generated** by `generate_docs` using C++26 static reflection (P2996 + P3394).
> Do not edit by hand — re-run `cmake --build build --target generate_docs` to refresh.

## System Architecture

The complete system architecture, wire format, and message flow are detailed below.

![IPC Flow Diagram](ipc_flow.svg)

### How to Read the Diagram

The architecture is divided into three logical vertical layers:

1.  **Test Harness (Left)**: The `pytest` environment. Test cases use the `UdpClient` to orchestrate scenarios, sending commands and receiving telemetry via the auto-generated Python bindings.
2.  **Network Layer (Middle)**: The UDP transport bridging the two processes. It shows the explicit socket mapping (Port 9000 for Inbound SIL traffic, Port 9001 for Outbound) handled by the host OS.
3.  **Simulator (Right)**: The C++ `sil_app`. The `UdpBridge` acts as a gateway, translating UDP packets into internal `MessageBus` events which are then routed to decoupled services.

**Legend**:
- **Solid White Lines**: External UDP socket traffic between the harness and the bridge.
- **Dotted Slate Lines**: Internal C++ `MessageBus` Publish/Subscribe routing.
- **Colored Nodes**: Services and components grouped by their logical domain.

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
)";

  // Emits the DOT file (for tooling) and also prints the DOT source into the Markdown.
  // Typical render: dot -Tsvg <dot_path> -o <svg_path>
  emit_graphviz_flow_markdown<AllComponents>(dot_path.string());

  // ── Services & Components ───────────────────────────────────────────────────
  std::cout << "---\n\n## Component Services\n\n";
  std::cout << "The application is composed of the following services:\n\n";
  emit_components<AllComponents>();

  // ── Message Payloads ──────────────────────────────────────────────────
  std::cout << "---\n\n## Message Payloads\n\n";

  std::cout << "Each section corresponds to one `MsgId` enumerator. "
               "The **direction badge** shows which side initiates the message.\n\n";

  emit_payloads<AllComponents>();

  // ── Footer ─────────────────────────────────────────────────────────────────
  std::cout <<
      R"(---

## Regenerating This File

```bash
# From the repo root:
cmake -B build -G Ninja
cmake --build build --target generate_docs
# Output: doc/ipc/protocol.md
```

_Generated with GCC trunk `-std=c++26 -freflection` (P2996R13 + P3394R4)._
)";

  return 0;
}
