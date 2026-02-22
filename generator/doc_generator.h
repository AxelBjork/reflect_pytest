#pragma once
// doc_generator.h — C++26 reflection helpers for reading doc:: annotations and
// emitting GitHub-renderable Markdown documentation.
//
// Companion to python_code_generator.h; included only by generate_docs.cpp.

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <meta>
#include <set>
#include <string>
#include <string_view>
#include <type_traits>

#include "common.h"
#include "component.h"
#include "messages.h"
#include "traits.h"

using namespace std::string_view_literals;

// -------------------------------------------------------------------------------------------------
// Forward declarations
// -------------------------------------------------------------------------------------------------
namespace ipc {
class UdpBridge;
}

template <typename T>
std::string cpp_type_name_str();

// -------------------------------------------------------------------------------------------------
// Component Sub/Pub Helpers
// -------------------------------------------------------------------------------------------------

template <ipc::MsgId Id>
consteval bool contains_msg(ipc::MsgList<>) {
  return false;
}

template <ipc::MsgId Id, ipc::MsgId First, ipc::MsgId... Rest>
consteval bool contains_msg(ipc::MsgList<First, Rest...>) {
  if constexpr (Id == First)
    return true;
  else
    return contains_msg<Id>(ipc::MsgList<Rest...>{});
}

template <typename Component, ipc::MsgId Id>
consteval bool component_subscribes() {
  return contains_msg<Id>(typename Component::Subscribes{});
}

template <typename Component, ipc::MsgId Id>
consteval bool component_publishes() {
  return contains_msg<Id>(typename Component::Publishes{});
}

template <typename Tuple, ipc::MsgId Id, std::size_t... Is>
consteval bool internal_cxx_subscribes_impl(std::index_sequence<Is...>) {
  return ((component_subscribes<std::tuple_element_t<Is, Tuple>, Id>() &&
           !std::is_same_v<std::tuple_element_t<Is, Tuple>, ipc::UdpBridge>) ||
          ...);
}

template <typename Tuple, ipc::MsgId Id>
consteval bool internal_cxx_subscribes() {
  return internal_cxx_subscribes_impl<Tuple, Id>(
      std::make_index_sequence<std::tuple_size_v<Tuple>>{});
}

template <typename Tuple, ipc::MsgId Id, std::size_t... Is>
consteval bool internal_cxx_publishes_impl(std::index_sequence<Is...>) {
  return ((component_publishes<std::tuple_element_t<Is, Tuple>, Id>() &&
           !std::is_same_v<std::tuple_element_t<Is, Tuple>, ipc::UdpBridge>) ||
          ...);
}

template <typename Tuple, ipc::MsgId Id>
consteval bool internal_cxx_publishes() {
  return internal_cxx_publishes_impl<Tuple, Id>(
      std::make_index_sequence<std::tuple_size_v<Tuple>>{});
}

template <typename Tuple, ipc::MsgId Id, std::size_t... Is>
consteval bool py_subscribes_impl(std::index_sequence<Is...>) {
  return ((component_subscribes<std::tuple_element_t<Is, Tuple>, Id>() &&
           std::is_same_v<std::tuple_element_t<Is, Tuple>, ipc::UdpBridge>) ||
          ...);
}

template <typename Tuple, ipc::MsgId Id>
consteval bool py_subscribes() {
  return py_subscribes_impl<Tuple, Id>(std::make_index_sequence<std::tuple_size_v<Tuple>>{});
}

template <typename Tuple, ipc::MsgId Id, std::size_t... Is>
consteval bool py_publishes_impl(std::index_sequence<Is...>) {
  return ((component_publishes<std::tuple_element_t<Is, Tuple>, Id>() &&
           std::is_same_v<std::tuple_element_t<Is, Tuple>, ipc::UdpBridge>) ||
          ...);
}

template <typename Tuple, ipc::MsgId Id>
consteval bool py_publishes() {
  return py_publishes_impl<Tuple, Id>(std::make_index_sequence<std::tuple_size_v<Tuple>>{});
}

template <typename Tuple, ipc::MsgId Id, std::size_t... Is>
bool print_subscribers_impl(std::index_sequence<Is...>) {
  bool first = true;
  (..., [&]() {
    using Comp = std::tuple_element_t<Is, Tuple>;
    if constexpr (component_subscribes<Comp, Id>()) {
      std::string cname = cpp_type_name_str<Comp>();
      if (cname != "UdpBridge") {
        if (!first) std::cout << ", ";
        std::cout << "`" << cname << "`";
        first = false;
      }
    }
  }());
  if (first) return false;
  return true;
}

template <typename Tuple, ipc::MsgId Id>
bool print_subscribers() {
  return print_subscribers_impl<Tuple, Id>(std::make_index_sequence<std::tuple_size_v<Tuple>>{});
}

template <typename Tuple, ipc::MsgId Id, std::size_t... Is>
bool print_publishers_impl(std::index_sequence<Is...>) {
  bool first = true;
  (..., [&]() {
    using Comp = std::tuple_element_t<Is, Tuple>;
    if constexpr (component_publishes<Comp, Id>()) {
      std::string cname = cpp_type_name_str<Comp>();
      if (cname != "UdpBridge") {
        if (!first) std::cout << ", ";
        std::cout << "`" << cname << "`";
        first = false;
      }
    }
  }());
  if (first) return false;
  return true;
}

template <typename Tuple, ipc::MsgId Id>
bool print_publishers() {
  return print_publishers_impl<Tuple, Id>(std::make_index_sequence<std::tuple_size_v<Tuple>>{});
}


// -------------------------------------------------------------------------------------------------
// Human-readable C++ type name
// Returns a runtime std::string so arrays can be formatted as "ElemType[N]".
// -------------------------------------------------------------------------------------------------

template <typename T>
std::string cpp_type_name_str() {
  if constexpr (std::is_same_v<T, bool>)
    return "bool";
  else if constexpr (std::is_same_v<T, char>)
    return "char";
  else if constexpr (std::is_same_v<T, uint8_t>)
    return "uint8_t";
  else if constexpr (std::is_same_v<T, int8_t>)
    return "int8_t";
  else if constexpr (std::is_same_v<T, uint16_t>)
    return "uint16_t";
  else if constexpr (std::is_same_v<T, int16_t>)
    return "int16_t";
  else if constexpr (std::is_same_v<T, uint32_t>)
    return "uint32_t";
  else if constexpr (std::is_same_v<T, int32_t>)
    return "int32_t";
  else if constexpr (std::is_same_v<T, uint64_t>)
    return "uint64_t";
  else if constexpr (std::is_same_v<T, int64_t>)
    return "int64_t";
  else if constexpr (std::is_same_v<T, float>)
    return "float";
  else if constexpr (std::is_same_v<T, double>)
    return "double";
  else if constexpr (std::is_enum_v<T> || std::is_class_v<T>)
    return std::string(std::meta::identifier_of(^^T));
  else if constexpr (std::is_array_v<T>) {
    using Elem = std::remove_extent_t<T>;
    constexpr std::size_t n = std::extent_v<T>;
    return cpp_type_name_str<Elem>() + "[" + std::to_string(n) + "]";
  } else
    return "?";
}

// Python type hint for the table "Py Type" column.
template <std::meta::info Type>
consteval std::string_view py_type_hint() {
  using T = typename[:Type:];
  if constexpr (std::is_same_v<T, bool>)
    return "bool"sv;
  else if constexpr (std::is_same_v<T, float>)
    return "float"sv;
  else if constexpr (std::is_same_v<T, double>)
    return "float"sv;
  else if constexpr (std::is_integral_v<T>)
    return "int"sv;
  else if constexpr (std::is_enum_v<T>)
    return std::meta::identifier_of(Type);
  else if constexpr (std::is_array_v<T>)
    return "bytes"sv;
  else
    return "Any"sv;
}

// -------------------------------------------------------------------------------------------------
// Field table helper — shared by emit_md_struct_section and emit_md_payload_section_for_msg_id
// -------------------------------------------------------------------------------------------------

template <typename T>
void emit_md_struct_section(std::set<std::string>& visited);

template <typename T>
void emit_field_table(std::set<std::string>& visited) {
  constexpr std::size_t N = get_fields_size<T>();
  if constexpr (N == 0) {
    std::cout << "_No fields._\n\n";
    return;
  }

  std::cout << "<table>\n";
  std::cout << "  <thead>\n";
  std::cout << "    <tr><th>Field</th><th>C++ Type</th><th>Py "
               "Type</th><th>Bytes</th><th>Offset</th></tr>\n";
  std::cout << "  </thead>\n";
  std::cout << "  <tbody>\n";

  [&]<std::size_t... Is>(std::index_sequence<Is...>) {
    (..., [&] {
      constexpr auto field = StructArrHolder<T, N>::arr[Is];
      constexpr auto type = std::meta::type_of(field);

      const std::string fname{std::meta::identifier_of(field)};
      using FT = typename[:type:];
      const std::string ctype = cpp_type_name_str<FT>();
      constexpr std::string_view ptype = py_type_hint<type>();

      constexpr std::size_t fsize = sizeof(FT);
      constexpr std::size_t foffset = std::meta::offset_of(field).bytes;

      std::cout << "    <tr>\n"
                << "      <td>" << fname << "</td>\n"
                << "      <td>" << ctype << "</td>\n"
                << "      <td>" << ptype << "</td>\n"
                << "      <td>" << fsize << "</td>\n"
                << "      <td>" << foffset << "</td>\n"
                << "    </tr>\n";
    }());
  }(std::make_index_sequence<N>{});

  std::cout << "  </tbody>\n";
  std::cout << "</table>\n\n";

  [&]<std::size_t... Is>(std::index_sequence<Is...>) {
    (..., [&] {
      constexpr auto field = StructArrHolder<T, N>::arr[Is];
      constexpr auto type = std::meta::type_of(field);
      using FT = typename[:type:];
      using BaseT = std::remove_all_extents_t<FT>;
      if constexpr (std::is_class_v<BaseT>) {
        emit_md_struct_section<BaseT>(visited);
      }
    }());
  }(std::make_index_sequence<N>{});
}

// -------------------------------------------------------------------------------------------------
// Enum table emitter
// -------------------------------------------------------------------------------------------------

template <typename E>
void emit_md_enum_section() {
  constexpr auto R = ^^E;
  const std::string name{std::meta::identifier_of(R)};

  constexpr doc::Desc desc = get_desc<R>();

  using U = std::underlying_type_t<E>;
  const char* underlying = []() -> const char* {
    if constexpr (std::is_same_v<U, uint8_t>)
      return "uint8_t";
    else if constexpr (std::is_same_v<U, uint16_t>)
      return "uint16_t";
    else if constexpr (std::is_same_v<U, uint32_t>)
      return "uint32_t";
    else if constexpr (std::is_same_v<U, int8_t>)
      return "int8_t";
    else if constexpr (std::is_same_v<U, int16_t>)
      return "int16_t";
    else if constexpr (std::is_same_v<U, int32_t>)
      return "int32_t";
    else
      return "int";
  }();

  std::cout << "### `" << name << "` (`" << underlying << "`)\n\n";
  if (desc.text[0] != '\0') std::cout << "> " << desc.text << "\n\n";

  std::cout << "| Enumerator | Value |\n";
  std::cout << "|---|---|\n";

  constexpr std::size_t N = get_enum_size<E>();
  [&]<std::size_t... Is>(std::index_sequence<Is...>) {
    (..., [] {
      constexpr auto e = EnumArrHolder<E, N>::arr[Is];
      std::cout << "| `" << std::meta::identifier_of(e) << "` | `" << static_cast<uint32_t>([:e:])
                << "` |\n";
    }());
  }(std::make_index_sequence<N>{});

  std::cout << "\n";
}

// -------------------------------------------------------------------------------------------------
// Struct section emitter (for helper structs not tied to a MsgId)
// -------------------------------------------------------------------------------------------------

template <typename T>
void emit_md_struct_section(std::set<std::string>& visited) {
  constexpr auto R = ^^T;
  const std::string name{std::meta::identifier_of(R)};
  if (!visited.insert(name).second) {
    return;
  }

  constexpr doc::Desc desc = get_desc<R>();

  std::cout << "#### Sub-struct: `" << name << "`\n\n";
  if (desc.text[0] != '\0') std::cout << "> " << desc.text << "\n\n";
  std::cout << "**Wire size:** " << sizeof(T) << " bytes\n\n";

  emit_field_table<T>(visited);
}

// -------------------------------------------------------------------------------------------------
// payload_or_void — same pattern as in python_code_generator.h
// -------------------------------------------------------------------------------------------------

template <uint32_t Id, typename = void>
struct doc_payload_or_void {
  using type = void;
};

template <uint32_t Id>
struct doc_payload_or_void<
    Id, std::void_t<typename ipc::MessageTraits<static_cast<ipc::MsgId>(Id)>::Payload>> {
  using type = typename ipc::MessageTraits<static_cast<ipc::MsgId>(Id)>::Payload;
};

template <typename Components, typename Payload, uint32_t Id>
void emit_md_payload_section() {
  constexpr auto R = ^^Payload;
  constexpr auto mid = static_cast<ipc::MsgId>(Id);
  constexpr std::string_view mname = ipc::MessageTraits<mid>::name;
  constexpr doc::Desc desc = get_desc<R>();
  const std::string sname{std::meta::identifier_of(R)};
  std::cout << "### `MsgId::" << mname << "` (`" << sname << "`)\n\n";
  if (desc.text[0] != '\0') std::cout << "> " << desc.text << "\n\n";

  constexpr bool cxx_sub = internal_cxx_subscribes<Components, mid>();
  constexpr bool cxx_pub = internal_cxx_publishes<Components, mid>();
  constexpr bool py_sub = py_subscribes<Components, mid>();
  constexpr bool py_pub = py_publishes<Components, mid>();

  const char* direction = "Internal";
  if (py_pub && cxx_sub && !py_sub && !cxx_pub)
    direction = "Inbound";
  else if (cxx_pub && py_sub && !cxx_sub && !py_pub)
    direction = "Outbound";
  else if ((py_pub || py_sub) && (cxx_pub || cxx_sub))
    direction = "Bidirectional";

  std::cout << "**Direction:** `" << direction << "`<br>\n";

  if constexpr (cxx_pub) {
    std::cout << "**Publishes:** ";
    if (!print_publishers<Components, mid>()) {
      std::cout << "_None_";
    }
    std::cout << "<br>\n";
  }

  if constexpr (cxx_sub) {
    std::cout << "**Subscribes:** ";
    if (!print_subscribers<Components, mid>()) {
      std::cout << "_None_";
    }
    std::cout << "<br>\n";
  }

  std::cout << "**Wire size:** " << sizeof(Payload) << " bytes\n\n";

  std::set<std::string> visited;
  emit_field_table<Payload>(visited);
}

// Emit a full payload section (heading + description + field table) for one MsgId.
template <typename Components, uint32_t Id>
void emit_md_payload_section_for_msg_id() {
  using T = typename doc_payload_or_void<Id>::type;
  if constexpr (!std::is_void_v<T>) {
    emit_md_payload_section<Components, T, Id>();
  }
}

// -------------------------------------------------------------------------------------------------
// Mermaid message-flow diagram
// -------------------------------------------------------------------------------------------------

template <typename Tuple, ipc::MsgId Id, std::size_t... Is>
void mermaid_inbound_edges_impl(std::index_sequence<Is...>, std::string_view mname) {
  bool first = true;
  (..., [&]() {
    using Comp = std::tuple_element_t<Is, Tuple>;
    if constexpr (component_subscribes<Comp, Id>()) {
      std::string cname = cpp_type_name_str<Comp>();
      if (cname != "UdpBridge") {
        if (!first) {
          std::cout << "\n        UdpBridge -->|" << mname << "| ";
        }
        std::cout << cname << "\n";
        first = false;
      }
    }
  }());
}

template <typename Tuple, ipc::MsgId Id>
void mermaid_inbound_edges(std::string_view mname) {
  mermaid_inbound_edges_impl<Tuple, Id>(std::make_index_sequence<std::tuple_size_v<Tuple>>{},
                                        mname);
}

template <typename Tuple, ipc::MsgId Id, std::size_t... Is>
void mermaid_outbound_edges_impl(std::index_sequence<Is...>, std::string_view mname) {
  (..., [&]() {
    using Comp = std::tuple_element_t<Is, Tuple>;
    if constexpr (component_publishes<Comp, Id>()) {
      std::string cname = cpp_type_name_str<Comp>();
      if (cname != "UdpBridge") {
        std::cout << "        " << cname << " -->|" << mname << "| UdpBridge\n";
      }
    }
  }());
}

template <typename Tuple, ipc::MsgId Id>
void mermaid_outbound_edges(std::string_view mname) {
  mermaid_outbound_edges_impl<Tuple, Id>(std::make_index_sequence<std::tuple_size_v<Tuple>>{},
                                         mname);
}

template <typename Components, uint32_t Id>
void emit_mermaid_edge_for_msg_id(std::string& inbound, std::string& outbound, bool& first_in,
                                  bool& first_out) {
  using T = typename doc_payload_or_void<Id>::type;
  if constexpr (!std::is_void_v<T>) {
    constexpr auto mid = static_cast<ipc::MsgId>(Id);
    constexpr std::string_view mname = ipc::MessageTraits<mid>::name;

    constexpr bool cxx_sub = internal_cxx_subscribes<Components, mid>();
    constexpr bool cxx_pub = internal_cxx_publishes<Components, mid>();
    constexpr bool py_sub = py_subscribes<Components, mid>();
    constexpr bool py_pub = py_publishes<Components, mid>();

    if (py_pub && cxx_sub && !cxx_pub && !py_sub) {
      if (!first_in) inbound += "<br/>";
      inbound += mname;
      first_in = false;
      std::cout << "        UdpBridge -->|" << mname << "| ";
      mermaid_inbound_edges<Components, mid>(mname);
    } else if (cxx_pub && py_sub && !py_pub && !cxx_sub) {
      if (!first_out) outbound += "<br/>";
      outbound += mname;
      first_out = false;
      mermaid_outbound_edges<Components, mid>(mname);
    } else if ((py_pub || py_sub) && (cxx_pub || cxx_sub)) {
      if (!first_in) inbound += "<br/>";
      inbound += mname;
      first_in = false;
      std::cout << "        UdpBridge -->|" << mname << "| ";
      mermaid_inbound_edges<Components, mid>(mname);

      if (!first_out) outbound += "<br/>";
      outbound += mname;
      first_out = false;
      mermaid_outbound_edges<Components, mid>(mname);
    }
  }
}

template <typename Components>
void emit_mermaid_flow() {
  constexpr std::size_t num_msgs = get_enum_size<ipc::MsgId>();

  std::cout << "```mermaid\nflowchart LR\n";
  std::cout << "    %% ─── COLUMN 1: Pytest Test Harness ───\n";
  std::cout << "    subgraph py[\"Pytest SIL Environment\"]\n";
  std::cout << "        TestCase([\"Test Case / Fixtures\"])\n";
  std::cout << "    end\n\n";

  std::cout << "    %% ─── COLUMN 2: Python UDP Client & Kernel Routing ───\n";
  std::cout << "    subgraph net[\"UdpClient (127.0.0.1)\"]\n";
  std::cout << "        direction TB\n";
  std::cout << "        TX([\"TX Socket<br/>Port 9000<br/>==============<br/>(sendto)\"])\n";
  std::cout << "        RX([\"RX Socket<br/>Port 9001<br/>==============<br/>(recvfrom)\"])\n";
  std::cout << "    end\n\n";

  std::cout << "    %% ─── COLUMN 3: C++ SUT ───\n";
  std::cout << "    subgraph sim[\"Simulator\"]\n";

  [&]<std::size_t... Is>(std::index_sequence<Is...>) {
    (..., [&] {
      using Comp = std::tuple_element_t<Is, Components>;
      constexpr auto R = ^^Comp;
      std::string cname = cpp_type_name_str<Comp>();

      std::cout << "        " << cname << "([\"" << cname;
      constexpr auto desc = get_desc<R>();
      if (desc.text[0] != '\0') {
        std::cout << "<br/><br/>";
        std::string desc_str{desc.text};

        // Find the first period to extract just the first sentence
        size_t dot_pos = desc_str.find('.');
        if (dot_pos != std::string::npos) {
          desc_str = desc_str.substr(0, dot_pos + 1);
        }

        // Word wrap using <br/> tags
        std::string wrapped = "";
        size_t current_line_len = 0;
        size_t last_space = 0;

        for (size_t i = 0; i < desc_str.length(); ++i) {
          if (desc_str[i] == ' ') {
            last_space = i;
          }
          wrapped += desc_str[i];
          current_line_len++;

          if (current_line_len > 35 && last_space != 0) {
            // Replace the last space with <br/>
            wrapped.replace(wrapped.length() - 1 - (i - last_space), 1, "<br/>");
            current_line_len = i - last_space;
            last_space = 0;
          }
        }
        std::cout << wrapped;
      }
      std::cout << "\"])\n";
    }());
  }(std::make_index_sequence<std::tuple_size_v<Components>>{});

  std::cout << "\n        %% Pure Internal Messages\n";
  // Hardcoded for now based on the architecture, in a real system this could use further reflection
  std::cout << "        MotorService -.->|PhysicsTick,<br/>StateChange| KinematicsService\n";
  std::cout << "        MotorService -.->|PhysicsTick,<br/>StateChange| PowerService\n";
  std::cout << "        MotorService -.->|StateChange| StateService\n";
  std::cout << "        MotorService -.->|PhysicsTick,<br/>StateChange| LogService\n\n";
  std::cout << "        %% Bridge Distribution\n";

  std::string inbound_msgs = "";
  std::string outbound_msgs = "";
  bool first_in = true;
  bool first_out = true;

  [&]<std::size_t... Is>(std::index_sequence<Is...>) {
    (..., [](std::string& in_msgs, std::string& out_msgs, bool& f_in, bool& f_out) {
      constexpr auto e = EnumArrHolder<ipc::MsgId, num_msgs>::arr[Is];
      constexpr uint32_t val = static_cast<uint32_t>([:e:]);
      emit_mermaid_edge_for_msg_id<Components, val>(in_msgs, out_msgs, f_in, f_out);
    }(inbound_msgs, outbound_msgs, first_in, first_out));
  }(std::make_index_sequence<num_msgs>{});

  std::cout << "    end\n\n";

  std::cout << "    %% ─── INBOUND PATH ───\n";
  std::cout << "    TestCase -->|send_msg| TX\n";
  if (!inbound_msgs.empty()) {
    std::cout << "    TX -->|\"" << inbound_msgs << "\"| UdpBridge\n\n";
  }

  std::cout << "    %% ─── OUTBOUND PATH ───\n";
  if (!outbound_msgs.empty()) {
    std::cout << "    UdpBridge -->|\"" << outbound_msgs << "\"| RX\n";
  }
  std::cout << "    RX -->|recv_msg| TestCase\n";

  std::cout << "```\n\n";
}
