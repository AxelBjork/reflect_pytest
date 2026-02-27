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
#include <utility>
#include <vector>

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

// Make MsgId labels narrower by breaking at '_' and wrapping.
inline std::string msg_label_text(std::string_view s, std::size_t width = 18) {
  std::string t{s};
  for (char& c : t)
    if (c == '_') c = ' ';
  return wrap_words(t, width);
}

// Per-message flow: create a node per MsgId and connect publishers -> MsgId -> subscribers.
struct MsgNodeInfo {
  std::string id;         // DOT node id (sanitized)
  std::string name;       // MsgId enumerator identifier (human label)
  std::string direction;  // "Inbound" | "Outbound" | "Bidirectional" | "Internal"
  std::string fillcolor;  // node color by direction
  std::string anchor_id;  // primary service to "own" this message for clustering
};

template <typename Tuple, ipc::MsgId Id, std::size_t... Is>
void collect_publishers_for_msg_impl(std::set<std::string>& pubs, std::index_sequence<Is...>) {
  (..., [&]() {
    using Comp = std::tuple_element_t<Is, Tuple>;
    if constexpr (component_publishes<Comp, Id>()) {
      pubs.insert(dot_id(cpp_type_name_str<Comp>()));
    }
  }());
}

template <typename Tuple, ipc::MsgId Id>
void collect_publishers_for_msg(std::set<std::string>& pubs) {
  collect_publishers_for_msg_impl<Tuple, Id>(pubs,
                                             std::make_index_sequence<std::tuple_size_v<Tuple>>{});
}

template <typename Tuple, ipc::MsgId Id, std::size_t... Is>
void collect_subscribers_for_msg_impl(std::set<std::string>& subs, std::index_sequence<Is...>) {
  (..., [&]() {
    using Comp = std::tuple_element_t<Is, Tuple>;
    if constexpr (component_subscribes<Comp, Id>()) {
      subs.insert(dot_id(cpp_type_name_str<Comp>()));
    }
  }());
}

template <typename Tuple, ipc::MsgId Id>
void collect_subscribers_for_msg(std::set<std::string>& subs) {
  collect_subscribers_for_msg_impl<Tuple, Id>(subs,
                                              std::make_index_sequence<std::tuple_size_v<Tuple>>{});
}

using MsgEdge = std::pair<std::string, std::string>;
using MsgEdgeSet = std::set<MsgEdge>;

template <typename Components, uint32_t Id>
void collect_msg_nodes_and_edges(std::vector<MsgNodeInfo>& nodes, MsgEdgeSet& edges) {
  using T = typename doc_payload_or_void<Id>::type;
  if constexpr (!std::is_void_v<T>) {
    constexpr auto mid = static_cast<ipc::MsgId>(Id);
    constexpr std::string_view mname = ipc::MessageTraits<mid>::name;
    constexpr bool cxx_sub = internal_cxx_subscribes<Components, mid>();
    constexpr bool cxx_pub = internal_cxx_publishes<Components, mid>();
    constexpr bool py_sub = py_subscribes<Components, mid>();
    constexpr bool py_pub = py_publishes<Components, mid>();

    const std::string ub = dot_id("UdpBridge");

    // Determine direction (for styling only).
    std::string direction = "Internal";
    std::string fillcolor = "#64748B";
    if (py_pub && cxx_sub && !py_sub && !cxx_pub) {
      direction = "Inbound";
      fillcolor = "#2563EB";
    } else if (cxx_pub && py_sub && !cxx_sub && !py_pub) {
      direction = "Outbound";
      fillcolor = "#F97316";
    } else if ((py_pub || py_sub) && (cxx_pub || cxx_sub)) {
      direction = "Bidirectional";
      fillcolor = "#A855F7";
    }

    // Gather publishers/subscribers (includes UdpBridge when applicable).
    std::set<std::string> pubs;
    std::set<std::string> subs;
    collect_publishers_for_msg<Components, mid>(pubs);
    collect_subscribers_for_msg<Components, mid>(subs);

    if (pubs.empty() && subs.empty()) return;

    // Pick an "anchor" service so messages cluster near the service that primarily owns them.
    // Heuristic:
    //  - Inbound (UdpBridge -> internal): anchor first internal subscriber
    //  - Outbound (internal -> UdpBridge): anchor first internal publisher
    //  - Internal: anchor first internal publisher (else first internal subscriber)
    //  - Bidirectional: prefer internal publisher else internal subscriber
    std::set<std::string> internal_pubs = pubs;
    std::set<std::string> internal_subs = subs;
    internal_pubs.erase(ub);
    internal_subs.erase(ub);

    std::string anchor;
    if (py_pub && cxx_sub && !py_sub && !cxx_pub) {
      if (!internal_subs.empty()) anchor = *internal_subs.begin();
    } else if (cxx_pub && py_sub && !cxx_sub && !py_pub) {
      if (!internal_pubs.empty()) anchor = *internal_pubs.begin();
    } else {
      if (!internal_pubs.empty())
        anchor = *internal_pubs.begin();
      else if (!internal_subs.empty())
        anchor = *internal_subs.begin();
    }

    std::string msg_id = dot_id(std::string("msg_") + std::string(mname));
    nodes.push_back(MsgNodeInfo{msg_id, std::string(mname), direction, fillcolor, anchor});

    for (auto const& p : pubs) edges.insert({p, msg_id});
    for (auto const& s : subs) edges.insert({msg_id, s});
  }
}

template <typename Components>
void emit_graphviz_flow_dot(std::ostream& os) {
  constexpr std::size_t num_msgs = get_enum_size<ipc::MsgId>();

  os << "digraph IPC {\n";
  os << "  rankdir=LR;\n";
  os << "  bgcolor=\"#0F172A\";\n";
  os << "  pad=0.35;\n";
  os << "  nodesep=0.95;\n";
  os << "  ranksep=0.95;\n";
  os << "  splines=polyline;\n";
  os << "  remincross=true;\n\n";
  os << "  compound=true;\n";

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

      if constexpr (!std::is_same_v<Comp, ipc::UdpBridge>) {
        std::string desc_text = wrap_words(first_sentence(get_desc<R>().text), 40);
        if (desc_text.empty()) desc_text = cname;
        os << "    " << dot_id(cname) << " [\n"
           << "      fillcolor=\"#0369A1\",\n"
           << "      label=<<B><FONT POINT-SIZE=\"30\">" << html_escape(cname)
           << "</FONT></B><BR/><FONT POINT-SIZE=\"24\">" << html_escape(desc_text) << "</FONT>>\n"
           << "    ];\n";
      }
    }());
  }(std::make_index_sequence<std::tuple_size_v<Components>>{});
  os << "  }\n\n";

  // UdpBridge (positioned to the right of the services)
  os << "  " << ub_id << " [\n"
     << "    fillcolor=\"#0F766E\",\n"
     << "    label=<<B><FONT POINT-SIZE=\"38\">UdpBridge</FONT></B><BR/><FONT "
        "POINT-SIZE=\"26\">Message Distribution Hub</FONT>>\n"
     << "  ];\n\n";

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

  // Collect per-message nodes + edges (publish -> MsgId -> subscribe).
  std::vector<MsgNodeInfo> msg_nodes;
  MsgEdgeSet msg_edges;
  [&]<std::size_t... Is>(std::index_sequence<Is...>) {
    (..., [&]() {
      constexpr auto e = EnumArrHolder<ipc::MsgId, num_msgs>::arr[Is];
      constexpr uint32_t val = static_cast<uint32_t>([:e:]);
      collect_msg_nodes_and_edges<Components, val>(msg_nodes, msg_edges);
    }());
  }(std::make_index_sequence<num_msgs>{});

  // Group messages by anchor service for clustering.
  std::map<std::string, std::vector<const MsgNodeInfo*>> msgs_by_anchor;
  std::vector<const MsgNodeInfo*> msgs_misc;
  for (auto const& n : msg_nodes) {
    if (!n.anchor_id.empty())
      msgs_by_anchor[n.anchor_id].push_back(&n);
    else
      msgs_misc.push_back(&n);
  }

  // Tier 2.5: Message Types (one node per MsgId), clustered by owning service.
  os << "  // --- IPC Messages ---\n";
  os << "  subgraph cluster_msgs {\n";
  os << "    label=<<B><FONT POINT-SIZE=\"40\">Messages</FONT></B>>;\n";
  os << "    fontcolor=\"#F1F5F9\";\n";
  os << "    style=\"rounded,filled\";\n";
  os << "    color=\"#8B5CF6\";\n";
  os << "    penwidth=6;\n";
  os << "    fillcolor=\"#1F1636\";\n";
  os << "    margin=24;\n\n";

  os << "    node [\n"
     << "      shape=oval,\n"
     << "      style=\"filled\",\n"
     << "      fontname=\"Helvetica,Arial,sans-serif\",\n"
     << "      fontsize=20,\n"
     << "      fontcolor=\"#FFFFFF\",\n"
     << "      penwidth=3,\n"
     << "      color=\"#E2E8F0\",\n"
     << "      margin=\"0.12,0.08\"\n"
     << "    ];\n\n";

  // One sub-cluster per anchor service (messages "owned" by that service).
  // This restores a more natural hierarchy and keeps related MsgIds near the service.
  for (auto const& [anchor, vec] : msgs_by_anchor) {
    if (vec.empty()) continue;
    os << "    subgraph cluster_msgs_" << anchor << " {\n"
       << "      label=<<B><FONT POINT-SIZE=\"22\">" << html_escape(anchor) << "</FONT></B>>;\n"
       << "      style=\"rounded\";\n"
       << "      color=\"#334155\";\n"
       << "      penwidth=2;\n";
    for (auto const* n : vec) {
      if (!n) {
        continue;
      }
      os << "      " << n->id << " [\n"
         << "        fillcolor=\"" << n->fillcolor << "\",\n"
         << "        label=<<B><FONT POINT-SIZE=\"24\">" << html_escape(msg_label_text(n->name))
         << "</FONT></B><BR/><FONT POINT-SIZE=\"16\">" << html_escape(n->direction) << "</FONT>>\n"
         << "      ];\n";
    }
    os << "    }\n\n";
  }

  // Any remaining messages without an anchor.
  if (!msgs_misc.empty()) {
    os << "    subgraph cluster_msgs_misc {\n"
       << "      label=<<B><FONT POINT-SIZE=\"22\">Misc</FONT></B>>;\n"
       << "      style=\"rounded\";\n"
       << "      color=\"#334155\";\n"
       << "      penwidth=2;\n";
    for (auto const* n : msgs_misc) {
      if (!n) {
        continue;
      }
      os << "      " << n->id << " [\n"
         << "        fillcolor=\"" << n->fillcolor << "\",\n"
         << "        label=<<B><FONT POINT-SIZE=\"24\">" << html_escape(msg_label_text(n->name))
         << "</FONT></B><BR/><FONT POINT-SIZE=\"16\">" << html_escape(n->direction) << "</FONT>>\n"
         << "      ];\n";
    }
    os << "    }\n\n";
  }
  os << "  }\n\n";

  // Invisible affinity edges: gently pull each MsgId node toward its owning service.
  // (Does not affect ranks; only reduces crossings and improves grouping.)
  os << "  // Affinity edges (invisible)\n";
  for (auto const& n : msg_nodes) {
    if (!n.anchor_id.empty()) {
      os << "  " << n.anchor_id << " -> " << n.id
         << " [style=invis, weight=60, constraint=false];\n";
    }
  }
  os << "\n";

  // Flow: Test Harness -> Network -> Simulator
  os << "  TestCase -> TX [label=\"send_msg\", color=\"#F1F5F9\", penwidth=5];\n";
  os << "  TX -> " << ub_id
     << " [label=\"udp/9000\", color=\"#F1F5F9\", penwidth=5, fontsize=25];\n\n";

  // Message Flow (publish -> MsgId -> subscribe)
  os << "  // Message Flow (publish -> MsgId -> subscribe)\n";
  for (auto const& e : msg_edges) {
    os << "  " << e.first << " -> " << e.second << " [style=dashed];\n";
  }

  os << "\n  // Flow: Simulator -> Network -> Test Harness\n";
  os << "  RX -> " << ub_id
     << " [label=\"udp/9001\", color=\"#F1F5F9\", penwidth=5, dir=back, fontsize=25, "
        "arrowtail=normal];\n";
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
