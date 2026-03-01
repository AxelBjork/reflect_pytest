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

#include "autonomous_msgs.h"
#include "common.h"
#include "core_msgs.h"
#include "message_bus.h"
#include "simulation_msgs.h"

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

template <MsgId Id, MsgId... Ids>
consteval bool contains_msg(ipc::MsgList<Ids...>) {
  return ((Id == Ids) || ...);
}

template <typename Component, MsgId Id>
consteval bool component_subscribes() {
  return contains_msg<Id>(typename Component::Subscribes{});
}

template <typename Component, MsgId Id>
consteval bool component_publishes() {
  return contains_msg<Id>(typename Component::Publishes{});
}

template <typename Tuple, MsgId Id, std::size_t... Is>
consteval bool internal_cxx_subscribes_impl(std::index_sequence<Is...>) {
  return ((component_subscribes<std::tuple_element_t<Is, Tuple>, Id>() &&
           !std::is_same_v<std::tuple_element_t<Is, Tuple>, ipc::UdpBridge>) ||
          ...);
}

template <typename Tuple, MsgId Id>
consteval bool internal_cxx_subscribes() {
  return internal_cxx_subscribes_impl<Tuple, Id>(
      std::make_index_sequence<std::tuple_size_v<Tuple>>{});
}

template <typename Tuple, MsgId Id, std::size_t... Is>
consteval bool internal_cxx_publishes_impl(std::index_sequence<Is...>) {
  return ((component_publishes<std::tuple_element_t<Is, Tuple>, Id>() &&
           !std::is_same_v<std::tuple_element_t<Is, Tuple>, ipc::UdpBridge>) ||
          ...);
}

template <typename Tuple, MsgId Id>
consteval bool internal_cxx_publishes() {
  return internal_cxx_publishes_impl<Tuple, Id>(
      std::make_index_sequence<std::tuple_size_v<Tuple>>{});
}

template <typename Tuple, MsgId Id, std::size_t... Is>
consteval bool py_subscribes_impl(std::index_sequence<Is...>) {
  return ((component_subscribes<std::tuple_element_t<Is, Tuple>, Id>() &&
           std::is_same_v<std::tuple_element_t<Is, Tuple>, ipc::UdpBridge>) ||
          ...);
}

template <typename Tuple, MsgId Id>
consteval bool py_subscribes() {
  return py_subscribes_impl<Tuple, Id>(std::make_index_sequence<std::tuple_size_v<Tuple>>{});
}

template <typename Tuple, MsgId Id, std::size_t... Is>
consteval bool py_publishes_impl(std::index_sequence<Is...>) {
  return ((component_publishes<std::tuple_element_t<Is, Tuple>, Id>() &&
           std::is_same_v<std::tuple_element_t<Is, Tuple>, ipc::UdpBridge>) ||
          ...);
}

template <typename Tuple, MsgId Id>
consteval bool py_publishes() {
  return py_publishes_impl<Tuple, Id>(std::make_index_sequence<std::tuple_size_v<Tuple>>{});
}

template <typename Tuple, MsgId Id, std::size_t... Is>
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

template <typename Tuple, MsgId Id>
bool print_subscribers() {
  return print_subscribers_impl<Tuple, Id>(std::make_index_sequence<std::tuple_size_v<Tuple>>{});
}

template <typename Tuple, MsgId Id, std::size_t... Is>
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

template <typename Tuple, MsgId Id>
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
struct doc_payload_or_void<Id,
                           std::void_t<typename MessageTraits<static_cast<MsgId>(Id)>::Payload>> {
  using type = typename MessageTraits<static_cast<MsgId>(Id)>::Payload;
};

template <typename Components, typename Payload, uint32_t Id>
void emit_md_payload_section() {
  constexpr auto R = ^^Payload;
  constexpr auto mid = static_cast<MsgId>(Id);
  constexpr std::string_view mname = MessageTraits<mid>::name;
  constexpr doc::Desc desc = get_desc<R>();
  std::string sname;
  if constexpr (std::meta::has_identifier(R)) {
    sname = std::meta::identifier_of(R);
  } else {
    sname = get_cxx_type_name<Payload>();
  }

  std::cout << "<details>\n";
  std::cout << "<summary><font size=\"+1\"><b>MsgId::" << mname << " (" << sname
            << ")</b></font></summary>\n\n";
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

template <typename Tuple, MsgId Id, std::size_t... Is>
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

template <typename Tuple, MsgId Id>
void collect_inbound_edges(EdgeMap& edges, std::string_view mname) {
  collect_inbound_edges_impl<Tuple, Id>(edges, std::make_index_sequence<std::tuple_size_v<Tuple>>{},
                                        mname);
}

template <typename Tuple, MsgId Id, std::size_t... Is>
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

template <typename Tuple, MsgId Id>
void collect_outbound_edges(EdgeMap& edges, std::string_view mname) {
  collect_outbound_edges_impl<Tuple, Id>(
      edges, std::make_index_sequence<std::tuple_size_v<Tuple>>{}, mname);
}

template <typename Tuple, MsgId Id, std::size_t... Is>
void collect_internal_edges_impl(EdgeMap& edges, std::index_sequence<Is...>, std::string_view mname,
                                 std::string_view pub_name) {
  (..., [&]() {
    using Comp = std::tuple_element_t<Is, Tuple>;
    if constexpr (component_subscribes<Comp, Id>() && !std::is_same_v<Comp, ipc::UdpBridge>) {
      edges[{dot_id(pub_name), dot_id(cpp_type_name_str<Comp>())}].insert(std::string(mname));
    }
  }());
}

template <typename Tuple, MsgId Id, std::size_t... Is>
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

template <typename Tuple, MsgId Id>
void collect_internal_edges(EdgeMap& edges, std::string_view mname) {
  collect_internal_edges_pub_impl<Tuple, Id>(
      edges, std::make_index_sequence<std::tuple_size_v<Tuple>>{}, mname);
}

template <typename Components, uint32_t Id>
void collect_msg_edges(EdgeMap& edges, std::set<std::string>& inbound_msg_names,
                       std::set<std::string>& outbound_msg_names) {
  using T = typename doc_payload_or_void<Id>::type;
  if constexpr (!std::is_void_v<T>) {
    constexpr auto mid = static_cast<MsgId>(Id);
    constexpr std::string_view mname = MessageTraits<mid>::name;
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
  constexpr std::size_t num_msgs = get_enum_size<MsgId>();

  os << "digraph IPC {\n";
  os << "  rankdir=LR;\n";
  os << "  bgcolor=\"#0F172A\";\n";
  os << "  pad=0.22;\n";
  os << "  nodesep=0.62;\n";
  os << "  ranksep=0.70;\n";
  os << "  splines=spline;\n";
  os << "  remincross=true;\n\n";

  os << "  fontname=\"Helvetica,Arial,sans-serif\";\n";
  os << "  fontsize=52;\n";
  os << "  fontcolor=\"#F1F5F9\";\n";
  os << "  label=<<B>System Architecture and Message Flow</B>>;\n";
  os << "  labelloc=\"t\";\n\n";

  os << "  node [\n"
     << "    fontname=\"Helvetica,Arial,sans-serif\",\n"
     << "    shape=rect,\n"
     << "    style=\"filled,rounded\",\n"
     << "    fixedsize=false,\n"
     << "    margin=\"0.12,0.08\",\n"
     << "    fontsize=28,\n"
     << "    fontcolor=\"#FFFFFF\",\n"
     << "    penwidth=4,\n"
     << "    color=\"#F1F5F9\"\n"
     << "  ];\n\n";

  os << "  edge [\n"
     << "    fontname=\"Helvetica,Arial,sans-serif\",\n"
     << "    fontsize=22,\n"
     << "    fontcolor=\"#CBD5E1\",\n"
     << "    color=\"#64748B\",\n"
     << "    penwidth=3.0,\n"
     << "    arrowsize=1.4,\n"
     << "    labelfloat=false,\n"
     << "    labeldistance=0.8,\n"
     << "    labelangle=10\n"
     << "  ];\n\n";

  // Tier 1: Simulator Services
  os << "  // --- Simulator ---\n";
  os << "  subgraph cluster_sim {\n";
  os << "    label=<<B><FONT POINT-SIZE=\"40\">Simulator</FONT></B>>;\n";
  os << "    fontcolor=\"#F1F5F9\";\n";
  os << "    style=\"rounded,filled\";\n";
  os << "    color=\"#475569\";\n";
  os << "    penwidth=4;\n";
  os << "    fillcolor=\"#1E293B\";\n";
  os << "    margin=34;\n";

  const std::string ub_id = dot_id("UdpBridge");

  [&]<std::size_t... Is>(std::index_sequence<Is...>) {
    (..., [&] {
      using Comp = std::tuple_element_t<Is, Components>;
      constexpr auto R = ^^Comp;
      const std::string cname = cpp_type_name_str<Comp>();

      // Use DOC_DESC for all components
      std::string desc_text = wrap_words(first_sentence(get_desc<R>().text), 40);
      if (desc_text.empty()) desc_text = cname;

      if constexpr (std::is_same_v<Comp, ipc::UdpBridge>) {
        os << "    " << ub_id << " [\n"
           << "      fillcolor=\"#0F766E\",\n"
           << "      label=<<B><FONT POINT-SIZE=\"38\">" << html_escape(cname)
           << "</FONT></B><BR/><FONT POINT-SIZE=\"26\">" << html_escape(desc_text) << "</FONT>>\n"
           << "    ];\n";
      } else {
        os << "    " << dot_id(cname) << " [\n"
           << "      fillcolor=\"#0369A1\",\n"
           << "      label=<<B><FONT POINT-SIZE=\"30\">" << html_escape(cname)
           << "</FONT></B><BR/><FONT POINT-SIZE=\"24\">" << html_escape(desc_text) << "</FONT>>\n"
           << "    ];\n";
      }
    }());
  }(std::make_index_sequence<std::tuple_size_v<Components>>{});
  os << "  }\n\n";

  // Tier 2: Network Layer
  os << "  // --- Network Layer bounding box (TX/RX) ---\n";
  os << "  subgraph cluster_sockets {\n";
  os << "    label=<<B><FONT POINT-SIZE=\"40\">Network Layer</FONT></B>>;\n";
  os << "    fontcolor=\"#F1F5F9\";\n";
  os << "    style=\"rounded,filled\";\n";
  os << "    color=\"#F59E0B\";\n";
  os << "    penwidth=6;\n";
  os << "    fillcolor=\"#2A1B07\";\n";
  os << "    margin=24;\n\n";
  os << "    { rank=same; TX; RX; }\n\n";

  os << "    TX [\n"
     << "      fillcolor=\"#B45309\",\n"
     << "      label=<<B><FONT POINT-SIZE=\"30\">TX Socket</FONT></B><BR/><FONT "
        "POINT-SIZE=\"24\">Port 9000 (SIL Inbound)</FONT>>\n"
     << "    ];\n";
  os << "    RX [\n"
     << "      fillcolor=\"#B45309\",\n"
     << "      label=<<B><FONT POINT-SIZE=\"30\">RX Socket</FONT></B><BR/><FONT "
        "POINT-SIZE=\"24\">Port 9001 (SIL Outbound)</FONT>>\n"
     << "    ];\n\n";
  os << "    TX -> RX [style=invis, weight=50, constraint=false];\n";
  os << "  }\n\n";

  // Tier 3: Test Harness
  os << "  // --- Test Harness bounding box ---\n";
  os << "  subgraph cluster_fixtures {\n";
  os << "    label=<<B><FONT POINT-SIZE=\"40\">Test Harness</FONT></B>>;\n";
  os << "    fontcolor=\"#F1F5F9\";\n";
  os << "    style=\"rounded,filled\";\n";
  os << "    color=\"#22C55E\";\n";
  os << "    penwidth=6;\n";
  os << "    fillcolor=\"#0B2A20\";\n";
  os << "    margin=24;\n\n";

  os << "    TestCase [\n"
     << "      fillcolor=\"#047857\",\n"
     << "      label=<<B><FONT POINT-SIZE=\"34\">Test Case / Fixtures</FONT></B>>\n"
     << "    ];\n";
  os << "  }\n\n";

  // Collect edges
  EdgeMap edge_map;
  std::set<std::string> inbound_msg_names;
  std::set<std::string> outbound_msg_names;
  [&]<std::size_t... Is>(std::index_sequence<Is...>) {
    (..., [&]() {
      constexpr auto e = EnumArrHolder<MsgId, num_msgs>::arr[Is];
      constexpr uint32_t val = static_cast<uint32_t>([:e:]);
      collect_msg_edges<Components, val>(edge_map, inbound_msg_names, outbound_msg_names);
    }());
  }(std::make_index_sequence<num_msgs>{});

  // Bridging labels (aggregated, paired)
  auto build_label = [](const std::set<std::string>& names) {
    std::string out;
    int count = 0;
    for (auto const& n : names) {
      if (count > 0) {
        if (count % 2 == 0)
          out += "\n";
        else
          out += ", ";
      }
      out += n;
      count++;
    }
    return out;
  };

  std::string in_l = build_label(inbound_msg_names);
  std::string out_l = build_label(outbound_msg_names);

  // Flow: Test Harness -> Network -> Simulator
  os << "  TestCase -> TX [label=\"send_msg\", color=\"#F1F5F9\", penwidth=5];\n";
  os << "  TX -> " << ub_id << " [label=\"Inbound Traffic:\\n"
     << dot_escape_label(in_l) << "\", color=\"#F1F5F9\", penwidth=5, fontsize=25];\n\n";

  // Internal Simulator Logic
  os << "  // Internal Simulator Logic\n";
  for (auto const& [pair, names] : edge_map) {
    os << "  " << pair.first << " -> " << pair.second << " [label=\""
       << dot_escape_label(build_label(names)) << "\", style=dashed];\n";
  }

  os << "\n  // Flow: Simulator -> Network -> Test Harness\n";
  os << "  RX -> " << ub_id << " [label=\"Outbound Traffic:\\n"
     << dot_escape_label(out_l)
     << "\", color=\"#F1F5F9\", penwidth=5, dir=back, fontsize=25, arrowtail=normal];\n";
  os << "  TestCase -> RX [label=\"recv_msg\", color=\"#F1F5F9\", penwidth=5, dir=back, "
        "arrowtail=normal];\n";

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
