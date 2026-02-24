#pragma once
// doc_generator.h — C++26 reflection helpers for reading doc:: annotations and
// emitting GitHub-renderable Markdown documentation.
//
// Companion to python_code_generator.h; included only by generate_docs.cpp.

#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <fstream>
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
  else if constexpr (std::is_enum_v<T> || std::is_class_v<T>) {
    if constexpr (std::meta::has_identifier(^^T)) {
      return std::string(std::meta::identifier_of(^^T));
    } else {
      return get_cxx_type_name<T>();
    }
  } else if constexpr (std::is_array_v<T>) {
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
  std::string name;
  if constexpr (std::meta::has_identifier(R)) {
    name = std::meta::identifier_of(R);
  } else {
    name = get_cxx_type_name<T>();
  }
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
  std::string sname;
  if constexpr (std::meta::has_identifier(R)) {
    sname = std::meta::identifier_of(R);
  } else {
    sname = get_cxx_type_name<Payload>();
  }
  std::cout << "### `MsgId::" << mname << "` (`" << sname << "`)\n\n";
  std::cout << "<details>\n<summary>View details and field table</summary>\n\n";
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
  std::cout << "</details>\n\n";
}

// Emit a full payload section (heading + description + field table) for one MsgId.
template <typename Components, uint32_t Id>
void emit_md_payload_section_for_msg_id() {
  using T = typename doc_payload_or_void<Id>::type;
  if constexpr (!std::is_void_v<T>) {
    emit_md_payload_section<Components, T, Id>();
  }
}

#include <map>

// -------------------------------------------------------------------------------------------------
// Graphviz DOT message-flow diagram
// -------------------------------------------------------------------------------------------------

inline std::string dot_escape_label(std::string_view s) {
  std::string out;
  out.reserve(s.size() + 16);
  for (char ch : s) {
    switch (ch) {
      case '\\':
        out += "\\\\";
        break;
      case '"':
        out += "\\\"";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        break;
      default:
        out.push_back(ch);
        break;
    }
  }
  return out;
}

inline std::string html_escape(std::string_view s) {
  std::string out;
  for (char c : s) {
    switch (c) {
      case '&':
        out += "&amp;";
        break;
      case '<':
        out += "&lt;";
        break;
      case '>':
        out += "&gt;";
        break;
      case '"':
        out += "&quot;";
        break;
      case '\'':
        out += "&apos;";
        break;
      case '\n':
        out += "<BR/>";
        break;
      default:
        out += c;
        break;
    }
  }
  return out;
}

inline std::string dot_id(std::string_view s) {
  std::string out;
  out.reserve(s.size() + 1);
  for (char ch : s) {
    unsigned char uc = static_cast<unsigned char>(ch);
    if (std::isalnum(uc))
      out.push_back(ch);
    else
      out.push_back('_');
  }
  if (out.empty() || std::isdigit(static_cast<unsigned char>(out[0]))) out.insert(out.begin(), '_');
  return out;
}

inline std::string first_sentence(std::string_view s) {
  auto dot = s.find('.');
  if (dot == std::string_view::npos) return std::string(s);
  return std::string(s.substr(0, dot + 1));
}

inline std::string wrap_words(std::string_view s, std::size_t width = 45) {
  std::string out;
  std::string word;
  std::size_t col = 0;
  auto flush_word = [&] {
    if (word.empty()) return;
    if (col == 0) {
      out += word;
      col = word.size();
    } else if (col + 1 + word.size() <= width) {
      out.push_back(' ');
      out += word;
      col += 1 + word.size();
    } else {
      out.push_back('\n');
      out += word;
      col = word.size();
    }
    word.clear();
  };
  for (char ch : s) {
    if (ch == ' ' || ch == '\n' || ch == '\r' || ch == '\t') {
      flush_word();
    } else {
      word.push_back(ch);
    }
  }
  flush_word();
  return out;
}

// Edge aggregation: maps (src_id, dst_id) -> set of message names.
using EdgeMap = std::map<std::pair<std::string, std::string>, std::set<std::string>>;

template <typename Tuple, ipc::MsgId Id, std::size_t... Is>
void collect_inbound_edges_impl(EdgeMap& edges, std::index_sequence<Is...>,
                                std::string_view mname) {
  const std::string ub = dot_id("UdpBridge");
  (..., [&]() {
    using Comp = std::tuple_element_t<Is, Tuple>;
    if constexpr (component_subscribes<Comp, Id>() && !std::is_same_v<Comp, ipc::UdpBridge>) {
      edges[{ub, dot_id(cpp_type_name_str<Comp>())}].insert(std::string(mname));
    }
  }());
}

template <typename Tuple, ipc::MsgId Id>
void collect_inbound_edges(EdgeMap& edges, std::string_view mname) {
  collect_inbound_edges_impl<Tuple, Id>(edges, std::make_index_sequence<std::tuple_size_v<Tuple>>{},
                                        mname);
}

template <typename Tuple, ipc::MsgId Id, std::size_t... Is>
void collect_outbound_edges_impl(EdgeMap& edges, std::index_sequence<Is...>,
                                 std::string_view mname) {
  const std::string ub = dot_id("UdpBridge");
  (..., [&]() {
    using Comp = std::tuple_element_t<Is, Tuple>;
    if constexpr (component_publishes<Comp, Id>() && !std::is_same_v<Comp, ipc::UdpBridge>) {
      edges[{dot_id(cpp_type_name_str<Comp>()), ub}].insert(std::string(mname));
    }
  }());
}

template <typename Tuple, ipc::MsgId Id>
void collect_outbound_edges(EdgeMap& edges, std::string_view mname) {
  collect_outbound_edges_impl<Tuple, Id>(
      edges, std::make_index_sequence<std::tuple_size_v<Tuple>>{}, mname);
}

template <typename Tuple, ipc::MsgId Id, std::size_t... Is>
void collect_internal_edges_impl(EdgeMap& edges, std::index_sequence<Is...>, std::string_view mname,
                                 std::string_view pub_name) {
  (..., [&]() {
    using Comp = std::tuple_element_t<Is, Tuple>;
    if constexpr (component_subscribes<Comp, Id>() && !std::is_same_v<Comp, ipc::UdpBridge>) {
      edges[{dot_id(pub_name), dot_id(cpp_type_name_str<Comp>())}].insert(std::string(mname));
    }
  }());
}

template <typename Tuple, ipc::MsgId Id, std::size_t... Is>
void collect_internal_edges_pub_impl(EdgeMap& edges, std::index_sequence<Is...>,
                                     std::string_view mname) {
  (..., [&]() {
    using Comp = std::tuple_element_t<Is, Tuple>;
    if constexpr (component_publishes<Comp, Id>() && !std::is_same_v<Comp, ipc::UdpBridge>) {
      collect_internal_edges_impl<Tuple, Id>(edges,
                                             std::make_index_sequence<std::tuple_size_v<Tuple>>{},
                                             mname, cpp_type_name_str<Comp>());
    }
  }());
}

template <typename Tuple, ipc::MsgId Id>
void collect_internal_edges(EdgeMap& edges, std::string_view mname) {
  collect_internal_edges_pub_impl<Tuple, Id>(
      edges, std::make_index_sequence<std::tuple_size_v<Tuple>>{}, mname);
}

template <typename Components, uint32_t Id>
void collect_msg_edges(EdgeMap& edges, std::set<std::string>& inbound_msg_names,
                       std::set<std::string>& outbound_msg_names) {
  using T = typename doc_payload_or_void<Id>::type;
  if constexpr (!std::is_void_v<T>) {
    constexpr auto mid = static_cast<ipc::MsgId>(Id);
    constexpr std::string_view mname = ipc::MessageTraits<mid>::name;
    constexpr bool cxx_sub = internal_cxx_subscribes<Components, mid>();
    constexpr bool cxx_pub = internal_cxx_publishes<Components, mid>();
    constexpr bool py_sub = py_subscribes<Components, mid>();
    constexpr bool py_pub = py_publishes<Components, mid>();

    if (py_pub && cxx_sub && !cxx_pub && !py_sub) {
      inbound_msg_names.insert(std::string(mname));
      collect_inbound_edges<Components, mid>(edges, mname);
    } else if (cxx_pub && py_sub && !py_pub && !cxx_sub) {
      outbound_msg_names.insert(std::string(mname));
      collect_outbound_edges<Components, mid>(edges, mname);
    } else if ((py_pub || py_sub) && (cxx_pub || cxx_sub)) {
      inbound_msg_names.insert(std::string(mname));
      collect_inbound_edges<Components, mid>(edges, mname);
      outbound_msg_names.insert(std::string(mname));
      collect_outbound_edges<Components, mid>(edges, mname);
    } else if (!py_pub && !py_sub && cxx_pub && cxx_sub) {
      collect_internal_edges<Components, mid>(edges, mname);
    }
  }
}

template <typename Components>
void emit_graphviz_flow_dot(std::ostream& os) {
  constexpr std::size_t num_msgs = get_enum_size<ipc::MsgId>();

  os << "digraph IPC {\n";
  os << "  rankdir=TB;\n";
  os << "  newrank=true;\n";
  os << "  nodesep=1.2;\n";
  os << "  ranksep=1.5;\n";
  os << "  splines=curved;\n";
  os << "  bgcolor=transparent;\n";
  os << "  edge [fontsize=16, fontcolor=\"#475569\", color=\"#94A3B8\", penwidth=2.0];\n\n";

  // Tier 1: Simulator Services
  os << "  subgraph cluster_sim {\n";
  os << "    label=\"Simulator Services\";\n";
  os << "    fontsize=36; fontcolor=\"#1E293B\"; style=dotted; color=\"#CBD5E1\"; labeljust=l;\n";
  [&]<std::size_t... Is>(std::index_sequence<Is...>) {
    (..., [&] {
      using Comp = std::tuple_element_t<Is, Components>;
      if constexpr (std::is_same_v<Comp, ipc::UdpBridge>) return;
      constexpr auto R = ^^Comp;
      const std::string cname = cpp_type_name_str<Comp>();
      std::string desc_text = wrap_words(first_sentence(get_desc<R>().text), 50);
      os << "    " << dot_id(cname)
         << " [shape=none, label=<<TABLE BORDER=\"0\" CELLBORDER=\"2\" CELLSPACING=\"0\" "
            "CELLPADDING=\"10\" BGCOLOR=\"white\" COLOR=\"#0284C7\">\n"
         << "      <TR><TD BGCOLOR=\"#E0F2FE\" ALIGN=\"LEFT\"><B><FONT COLOR=\"#0369A1\" "
            "POINT-SIZE=\"24\">"
         << html_escape(cname) << "</FONT></B></TD></TR>\n"
         << "      <TR><TD ALIGN=\"LEFT\" VALIGN=\"TOP\"><FONT POINT-SIZE=\"16\" COLOR=\"#334155\">"
         << html_escape(desc_text) << "</FONT></TD></TR>\n"
         << "    </TABLE>>];\n";
    }());
  }(std::make_index_sequence<std::tuple_size_v<Components>>{});
  os << "  }\n\n";

  // Tier 2: UdpBridge (Central Message Hub)
  const std::string ub_id = dot_id("UdpBridge");
  os << "  " << ub_id
     << " [shape=none, label=<<TABLE BORDER=\"0\" CELLBORDER=\"2\" CELLSPACING=\"0\" "
        "CELLPADDING=\"12\" BGCOLOR=\"white\" COLOR=\"#0D9488\">\n"
     << "    <TR><TD BGCOLOR=\"#CCFBF1\" ALIGN=\"CENTER\"><B><FONT COLOR=\"#0F766E\" "
        "POINT-SIZE=\"28\">UdpBridge</FONT></B></TD></TR>\n"
     << "    <TR><TD ALIGN=\"CENTER\"><FONT POINT-SIZE=\"18\" COLOR=\"#475569\">Message "
        "Distribution Hub</FONT></TD></TR>\n"
     << "  </TABLE>>];\n\n";

  // Tier 3: Network Layer
  os << "  subgraph cluster_net {\n";
  os << "    label=\"Network Layer\";\n";
  os << "    fontsize=36; fontcolor=\"#1E293B\"; style=dotted; color=\"#CBD5E1\"; labeljust=l;\n";
  os << "    TX [shape=none, label=<<TABLE BORDER=\"0\" CELLBORDER=\"2\" CELLSPACING=\"0\" "
        "CELLPADDING=\"10\" BGCOLOR=\"white\" COLOR=\"#D97706\">\n"
     << "      <TR><TD BGCOLOR=\"#FEF3C7\" ALIGN=\"CENTER\"><B><FONT COLOR=\"#92400E\" "
        "POINT-SIZE=\"24\">TX Socket</FONT></B></TD></TR>\n"
     << "      <TR><TD ALIGN=\"CENTER\"><FONT POINT-SIZE=\"16\" COLOR=\"#475569\">Port 9000 (SIL "
        "Inbound)</FONT></TD></TR>\n"
     << "    </TABLE>>];\n";
  os << "    RX [shape=none, label=<<TABLE BORDER=\"0\" CELLBORDER=\"2\" CELLSPACING=\"0\" "
        "CELLPADDING=\"10\" BGCOLOR=\"white\" COLOR=\"#D97706\">\n"
     << "      <TR><TD BGCOLOR=\"#FEF3C7\" ALIGN=\"CENTER\"><B><FONT COLOR=\"#92400E\" "
        "POINT-SIZE=\"24\">RX Socket</FONT></B></TD></TR>\n"
     << "      <TR><TD ALIGN=\"CENTER\"><FONT POINT-SIZE=\"16\" COLOR=\"#475569\">Port 9001 (SIL "
        "Outbound)</FONT></TD></TR>\n"
     << "    </TABLE>>];\n";
  os << "  }\n\n";

  // Tier 4: Test Harness
  os << "  subgraph cluster_py {\n";
  os << "    label=\"Test Harness\";\n";
  os << "    fontsize=36; fontcolor=\"#1E293B\"; style=dotted; color=\"#CBD5E1\"; labeljust=l;\n";
  os << "    TestCase [shape=none, label=<<TABLE BORDER=\"0\" CELLBORDER=\"2\" CELLSPACING=\"0\" "
        "CELLPADDING=\"14\" BGCOLOR=\"white\" COLOR=\"#059669\">\n"
     << "      <TR><TD BGCOLOR=\"#D1FAE5\" ALIGN=\"CENTER\" WIDTH=\"250\"><B><FONT "
        "COLOR=\"#065F46\" POINT-SIZE=\"26\">Test Case / Fixtures</FONT></B></TD></TR>\n"
     << "    </TABLE>>];\n";
  os << "  }\n\n";

  // Rank constraints
  os << "  { rank=same; TX; RX; }\n\n";

  // Collect edges
  EdgeMap edge_map;
  std::set<std::string> inbound_msg_names;
  std::set<std::string> outbound_msg_names;
  [&]<std::size_t... Is>(std::index_sequence<Is...>) {
    (..., [&]() {
      constexpr auto e = EnumArrHolder<ipc::MsgId, num_msgs>::arr[Is];
      constexpr uint32_t val = static_cast<uint32_t>([:e:]);
      collect_msg_edges<Components, val>(edge_map, inbound_msg_names, outbound_msg_names);
    }());
  }(std::make_index_sequence<num_msgs>{});

  // Emit aggregated edges
  for (auto const& [pair, names] : edge_map) {
    std::string label;
    bool first = true;
    for (auto const& name : names) {
      if (!first) label += "\n";
      label += name;
      first = false;
    }
    os << "  " << pair.first << " -> " << pair.second << " [label=\"" << dot_escape_label(label)
       << "\"];\n";
  }

  // Bridging labels (aggregated)
  auto build_label = [](const std::set<std::string>& names) {
    std::string out;
    bool first = true;
    for (auto const& n : names) {
      if (!first) out += "\n";
      out += n;
      first = false;
    }
    return out;
  };

  std::string in_l = build_label(inbound_msg_names);
  std::string out_l = build_label(outbound_msg_names);

  os << "  TX -> TestCase [label=\"send_msg\", dir=back, color=\"#64748B\", penwidth=2.5];\n";
  if (!in_l.empty())
    os << "  " << ub_id << " -> TX [label=\"" << dot_escape_label(in_l)
       << "\", dir=back, color=\"#64748B\", penwidth=2.5];\n";
  if (!out_l.empty())
    os << "  " << ub_id << " -> RX [label=\"" << dot_escape_label(out_l)
       << "\", color=\"#64748B\", penwidth=2.5];\n";
  os << "  RX -> TestCase [label=\"recv_msg\", color=\"#64748B\", penwidth=2.5];\n";

  os << "}\n";
}

template <typename Components>
void write_graphviz_flow_dot_file(std::string_view path) {
  std::ofstream out{std::string(path)};
  if (!out) {
    std::cerr << "generate_docs: failed to open DOT output: " << path << "\n";
    return;
  }
  emit_graphviz_flow_dot<Components>(out);
}

template <typename Components>
void emit_graphviz_flow_markdown(std::string_view dot_path) {
  write_graphviz_flow_dot_file<Components>(dot_path);
  // We no longer print the DOT source block into the Markdown to keep it clean.
}
