// generate_docs.cpp — Standalone executable that reflects over ipc:: messages
// and emits a GitHub-renderable Markdown documentation file.
//
// Build:  cmake --build build --target generate_docs
// Run:    ./build/generate_docs > build/doc/ipc_protocol.md
//         (CMake custom command does this automatically)

#include "app_components.h"
#include "doc_generator.h"

struct MainPublisher {
  using Subscribes = ipc::MsgList<>;
  using Publishes = ipc::MsgList<ipc::MsgId::Log>;
};

// Combine all sil_app services plus the virtual main publisher thread
using AllComponents = decltype(std::tuple_cat(std::declval<sil::AppServices>(),
                                              std::declval<std::tuple<MainPublisher>>()));

void emit_toc() {
  [&]<std::size_t... Is>(std::index_sequence<Is...>) {
    (..., [] {
      constexpr auto e = EnumArrHolder<ipc::MsgId, get_enum_size<ipc::MsgId>()>::arr[Is];
      constexpr uint32_t val = static_cast<uint32_t>([:e:]);
      using T = typename doc_payload_or_void<val>::type;
      if constexpr (!std::is_void_v<T>) {
        constexpr auto mid = static_cast<ipc::MsgId>(val);
        constexpr std::string_view mname = ipc::MessageTraits<mid>::name;
        const std::string sname = cpp_type_name_str<T>();
        std::string link = "msgid" + std::string(mname) + "-" + sname;
        for (auto& c : link) c = std::tolower(c);
        std::cout << "- [`" << mname << "`](#" << link << ")\n";
      }
    }());
  }(std::make_index_sequence<get_enum_size<ipc::MsgId>()>{});
}

template <typename Components>
void emit_payloads() {
  [&]<std::size_t... Is>(std::index_sequence<Is...>) {
    (..., [] {
      constexpr auto e = EnumArrHolder<ipc::MsgId, get_enum_size<ipc::MsgId>()>::arr[Is];
      constexpr uint32_t val = static_cast<uint32_t>([:e:]);
      emit_md_payload_section_for_msg_id<Components, val>();
    }());
  }(std::make_index_sequence<get_enum_size<ipc::MsgId>()>{});
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

This diagram gives three distinct columns: `Pytest` uses the `UdpClient` module to orchestrate test cases, `Network` maps the transport layer across two explicit sockets (`Client -> App` and `App -> Client`), and `Simulator` processes the messages internally.

)";

  emit_mermaid_flow<AllComponents>();

  // ── Services & Components ───────────────────────────────────────────────────
  std::cout << "---\n\n## Component Services\n\n";
  std::cout << "The application is composed of the following services:\n\n";
  emit_components<AllComponents>();

  // ── Message Payloads & TOC ──────────────────────────────────────────────────
  std::cout << "---\n\n## Message Payloads\n\n";

  emit_toc();

  std::cout << "\nEach section corresponds to one `MsgId` enumerator. "
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
# Output: build/doc/ipc_protocol.md
```

_Generated with GCC trunk `-std=c++26 -freflection` (P2996R13 + P3394R4)._
)";

  return 0;
}
