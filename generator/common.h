#pragma once
// common.h — Shared C++26 reflection utilities for doc generation and python bindings.

#include <array>
#include <cstddef>
#include <cstdint>
#include <meta>
#include <string_view>

#include "messages.h"

// -------------------------------------------------------------------------------------------------
// Annotation readers
// -------------------------------------------------------------------------------------------------

template <std::meta::info R>
consteval doc::Desc get_desc() {
  for (std::meta::info a : std::meta::annotations_of(R)) {
    if (std::meta::type_of(a) == ^^doc::Desc) {
      return std::meta::extract<doc::Desc>(a);
    }
    if (std::meta::type_of(a) == ^^const doc::Desc) {
      return std::meta::extract<const doc::Desc>(a);
    }
  }
  return doc::Desc("");
}

// -------------------------------------------------------------------------------------------------
// Enum Reflection
// -------------------------------------------------------------------------------------------------

template <typename E>
consteval std::size_t get_enum_size() {
  return std::meta::enumerators_of(^^E).size();
}

template <typename E, std::size_t N>
consteval std::array<std::meta::info, N> get_enum_array() {
  auto vec = std::meta::enumerators_of(^^E);
  std::array<std::meta::info, N> arr{};
  for (std::size_t i = 0; i < N; ++i) {
    arr[i] = vec[i];
  }
  return arr;
}

template <typename E, std::size_t N>
struct EnumArrHolder {
  static constexpr auto arr = get_enum_array<E, N>();
};

// -------------------------------------------------------------------------------------------------
// Struct Reflection
// -------------------------------------------------------------------------------------------------

template <typename T>
consteval std::size_t get_fields_size() {
  auto ctx = std::meta::access_context::current();
  return std::meta::nonstatic_data_members_of(^^T, ctx).size();
}

template <typename T, std::size_t N>
consteval std::array<std::meta::info, N> get_fields_array() {
  auto ctx = std::meta::access_context::current();
  auto vec = std::meta::nonstatic_data_members_of(^^T, ctx);
  std::array<std::meta::info, N> arr{};
  for (std::size_t i = 0; i < N; ++i) {
    arr[i] = vec[i];
  }
  return arr;
}

template <typename T, std::size_t N>
struct StructArrHolder {
  static constexpr auto arr = get_fields_array<T, N>();
};

// -------------------------------------------------------------------------------------------------
// Stringification Helpers for C++26 templates
// -------------------------------------------------------------------------------------------------

template <typename T>
std::string get_cxx_type_name() {
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
  else {
    std::string s(std::meta::display_string_of(^^T));
    size_t pos = s.find("ipc::");
    if (pos != std::string::npos) s.replace(pos, 5, "");
    pos = s.find("sil::");
    if (pos != std::string::npos) s.replace(pos, 5, "");
    return s;
  }
}

template <typename T>
std::string get_python_type_name() {
  std::string s = get_cxx_type_name<T>();
  for (char& c : s) {
    if (c == '<' || c == '>' || c == ',' || c == ' ' || c == ':') c = '_';
  }
  while (!s.empty() && s.back() == '_') s.pop_back();
  return s;
}
