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

#include "messages.h"
#include "traits.h"

using namespace std::string_view_literals;

// -------------------------------------------------------------------------------------------------
// Enum helpers (independent of python_code_generator.h)
// -------------------------------------------------------------------------------------------------

template <typename E>
consteval std::size_t doc_get_enum_size() {
  return std::meta::enumerators_of(^^E).size();
}

template <typename E, std::size_t N>
consteval std::array<std::meta::info, N> doc_get_enum_array() {
  auto vec = std::meta::enumerators_of(^^E);
  std::array<std::meta::info, N> arr{};
  for (std::size_t i = 0; i < N; ++i) arr[i] = vec[i];
  return arr;
}

template <typename E, std::size_t N>
struct DocEnumArrHolder {
  static constexpr auto arr = doc_get_enum_array<E, N>();
};

// -------------------------------------------------------------------------------------------------
// Struct field helpers
// -------------------------------------------------------------------------------------------------

template <typename T>
consteval std::size_t doc_get_fields_size() {
  auto ctx = std::meta::access_context::current();
  return std::meta::nonstatic_data_members_of(^^T, ctx).size();
}

template <typename T, std::size_t N>
consteval std::array<std::meta::info, N> doc_get_fields_array() {
  auto ctx = std::meta::access_context::current();
  auto vec = std::meta::nonstatic_data_members_of(^^T, ctx);
  std::array<std::meta::info, N> arr{};
  for (std::size_t i = 0; i < N; ++i) arr[i] = vec[i];
  return arr;
}

template <typename T, std::size_t N>
struct DocStructArrHolder {
  static constexpr auto arr = doc_get_fields_array<T, N>();
};

// -------------------------------------------------------------------------------------------------
// Annotation readers
// -------------------------------------------------------------------------------------------------

// Returns doc::Desc::text if the entity has one, otherwise "".
template <std::meta::info R>
consteval const char* get_desc() {
  for (std::meta::info a : std::meta::annotations_of(R)) {
    if (std::meta::type_of(a) == ^^doc::Desc) {
      return std::meta::extract<doc::Desc>(a).text;
    }
  }
  return "";
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
  constexpr std::size_t N = doc_get_fields_size<T>();
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
      constexpr auto field = DocStructArrHolder<T, N>::arr[Is];
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
      constexpr auto field = DocStructArrHolder<T, N>::arr[Is];
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

  const char* desc = get_desc<R>();

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
  if (*desc) std::cout << "> " << desc << "\n\n";

  std::cout << "| Enumerator | Value |\n";
  std::cout << "|---|---|\n";

  constexpr std::size_t N = doc_get_enum_size<E>();
  [&]<std::size_t... Is>(std::index_sequence<Is...>) {
    (..., [] {
      constexpr auto e = DocEnumArrHolder<E, N>::arr[Is];
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

  const char* desc = get_desc<R>();

  std::cout << "#### Sub-struct: `" << name << "`\n\n";
  if (*desc) std::cout << "> " << desc << "\n\n";
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

template <typename Payload, uint32_t Id>
void emit_md_payload_section() {
  constexpr auto R = ^^Payload;
  constexpr auto mid = static_cast<ipc::MsgId>(Id);
  constexpr std::string_view mname = ipc::MessageTraits<mid>::name;
  const char* desc = get_desc<R>();
  const std::string sname{std::meta::identifier_of(R)};
  std::cout << "### `MsgId::" << mname << "` (`" << sname << "`)\n\n";
  if (*desc) std::cout << "> " << desc << "\n\n";
  std::cout << "**Wire size:** " << sizeof(Payload) << " bytes\n\n";

  std::set<std::string> visited;
  emit_field_table<Payload>(visited);
}

// Emit a full payload section (heading + description + field table) for one MsgId.
template <uint32_t Id>
void emit_md_payload_section_for_msg_id() {
  using T = typename doc_payload_or_void<Id>::type;
  if constexpr (!std::is_void_v<T>) {
    emit_md_payload_section<T, Id>();
  }
}

// -------------------------------------------------------------------------------------------------
// Mermaid message-flow diagram
// -------------------------------------------------------------------------------------------------

template <uint32_t Id>
void emit_mermaid_edge_for_msg_id() {
  using T = typename doc_payload_or_void<Id>::type;
  if constexpr (!std::is_void_v<T>) {
    constexpr auto mid = static_cast<ipc::MsgId>(Id);
    constexpr std::string_view mname = ipc::MessageTraits<mid>::name;
    // We lost direction but we can point it as double ended
    std::cout << "    PY <-->|" << mname << "| CXX\n";
  }
}

void emit_mermaid_flow() {
  constexpr std::size_t num_msgs = doc_get_enum_size<ipc::MsgId>();

  std::cout << "```mermaid\nflowchart LR\n";
  std::cout << "    PY([\"Python / pytest\"])\n";
  std::cout << "    CXX([\"C++ SIL\"])\n";

  [&]<std::size_t... Is>(std::index_sequence<Is...>) {
    (..., [] {
      constexpr auto e = DocEnumArrHolder<ipc::MsgId, num_msgs>::arr[Is];
      constexpr uint32_t val = static_cast<uint32_t>([:e:]);
      emit_mermaid_edge_for_msg_id<val>();
    }());
  }(std::make_index_sequence<num_msgs>{});

  std::cout << "```\n\n";
}
