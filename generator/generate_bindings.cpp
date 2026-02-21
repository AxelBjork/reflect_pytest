#include "python_code_generator.h"

int main() {
  std::cout << "\"\"\"Auto-generated IPC bindings using C++26 static reflection.\"\"\"\n\n";
  std::cout << "import struct\n";
  std::cout << "from dataclasses import dataclass\n";
  std::cout << "from enum import IntEnum\n\n";

  // ── Enums ──────────────────────────────────────────────────────────────────
  generate_enum<ipc::MsgId>();
  generate_enum<ipc::Severity>();
  generate_enum<ipc::ComponentId>();
  generate_enum<ipc::SystemState>();

  // ── Helper structs (not top-level messages) ────────────────────────────────
  generate_struct<ipc::MotorSubCmd>();

  // ── Message payload structs ────────────────────────────────────────────────
  constexpr std::size_t num_msgs = get_enum_size<ipc::MsgId>();
  [&]<std::size_t... Is>(std::index_sequence<Is...>) {
    (..., [] {
      constexpr auto e = EnumArrHolder<ipc::MsgId, num_msgs>::arr[Is];
      constexpr uint32_t val = static_cast<uint32_t>([:e:]);
      generate_struct_for_msg_id<val>();
    }());
  }(std::make_index_sequence<num_msgs>{});

  std::cout << "MESSAGE_BY_ID = {\n";
  [&]<std::size_t... Is>(std::index_sequence<Is...>) {
    (..., [] {
      constexpr auto e = EnumArrHolder<ipc::MsgId, num_msgs>::arr[Is];
      constexpr uint32_t val = static_cast<uint32_t>([:e:]);
      emit_metadata_for_msg_id<val>();
    }());
  }(std::make_index_sequence<num_msgs>{});
  std::cout << "}\n\n";

  std::cout << "PAYLOAD_SIZE_BY_ID = {\n";
  [&]<std::size_t... Is>(std::index_sequence<Is...>) {
    (..., [] {
      constexpr auto e = EnumArrHolder<ipc::MsgId, num_msgs>::arr[Is];
      constexpr uint32_t val = static_cast<uint32_t>([:e:]);
      emit_size_for_msg_id<val>();
    }());
  }(std::make_index_sequence<num_msgs>{});
  std::cout << "}\n\n";

  return 0;
}
