#pragma once

#include <array>
#include <cstdint>
#include <iostream>
#include <meta>
#include <string_view>
#include <type_traits>

#include "common.h"
#include "messages.h"
#include "traits.h"

using namespace std::string_view_literals;

// -------------------------------------------------------------------------------------------------
// Enum Reflection
// -------------------------------------------------------------------------------------------------

template <typename E>
void generate_enum() {
  std::cout << "class " << std::meta::identifier_of(^^E) << "(IntEnum):\n";
  constexpr auto desc = get_desc<^^E>();
  if (desc.text[0] != '\0') {
    std::cout << "    \"\"\"" << desc.text << "\"\"\"\n";
  }
  constexpr std::size_t N = get_enum_size<E>();
  [&]<std::size_t... Is>(std::index_sequence<Is...>) {
    (..., [] {
      constexpr auto e = EnumArrHolder<E, N>::arr[Is];
      std::cout << "    " << std::meta::identifier_of(e) << " = " << static_cast<uint16_t>([:e:])
                << "\n";
    }());
  }(std::make_index_sequence<N>{});
  std::cout << "\n";
}

// -------------------------------------------------------------------------------------------------
// Struct Reflection
// -------------------------------------------------------------------------------------------------

// Python type mapping
template <std::meta::info Type>
consteval std::string_view get_python_type_hint() {
  using T = typename[:Type:];
  if constexpr (std::is_same_v<T, bool>) {
    return "bool"sv;
  } else if constexpr (std::is_integral_v<T>) {
    return "int"sv;
  } else if constexpr (std::is_floating_point_v<T>) {
    return "float"sv;
  } else if constexpr (std::is_enum_v<T>) {
    return std::meta::identifier_of(Type);
  } else if constexpr (std::is_array_v<T>) {
    return "bytes"sv;  // our arrays are char text[...], mapped to bytes
  }
  return "Any"sv;
}

template <std::meta::info Type>
consteval std::string_view get_struct_format_char() {
  using T = typename[:Type:];
  if constexpr (std::is_same_v<T, bool>)
    return "?"sv;
  else if constexpr (std::is_integral_v<T>) {
    if constexpr (sizeof(T) == 1) return std::is_signed_v<T> ? "b"sv : "B"sv;
    if constexpr (sizeof(T) == 2) return std::is_signed_v<T> ? "h"sv : "H"sv;
    if constexpr (sizeof(T) == 4) return std::is_signed_v<T> ? "i"sv : "I"sv;
    if constexpr (sizeof(T) == 8) return std::is_signed_v<T> ? "q"sv : "Q"sv;
  } else if constexpr (std::is_floating_point_v<T>) {
    if constexpr (sizeof(T) == 4) return "f"sv;
    if constexpr (sizeof(T) == 8) return "d"sv;
  } else if constexpr (std::is_enum_v<T>) {
    if constexpr (sizeof(T) == 1) return "B"sv;
    if constexpr (sizeof(T) == 2) return "H"sv;
    if constexpr (sizeof(T) == 4) return "I"sv;
    if constexpr (sizeof(T) == 8) return "Q"sv;
  } else if constexpr (std::is_array_v<T>) {
    return "s"sv;  // Handled specially
  }
  return "?"sv;
}

template <typename T>
void generate_struct() {
  std::string class_name{std::meta::identifier_of(^^T)};
  std::cout << "@dataclass\nclass " << class_name << ":\n";

  constexpr auto desc = get_desc<^^T>();
  if (desc.text[0] != '\0') {
    std::cout << "    \"\"\"" << desc.text << "\"\"\"\n";
  }

  constexpr std::size_t N = get_fields_size<T>();

  std::string pack_args;
  std::string pack_fmt = "<";

  if constexpr (N > 0) {
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
      (..., [&] {
        constexpr auto field = StructArrHolder<T, N>::arr[Is];
        constexpr auto type = std::meta::type_of(field);
        std::string field_name{std::meta::identifier_of(field)};

        constexpr auto field_desc = get_desc<field>();

        std::cout << "    " << field_name << ": " << get_python_type_hint<type>() << "\n";
        if (field_desc.text[0] != '\0') {
          std::cout << "    \"\"\"" << field_desc.text << "\"\"\"\n";
        }

        if (!pack_args.empty()) pack_args += ", ";
        pack_args += "self." + field_name;

        if constexpr (std::is_array_v<typename[:type:]>) {
          using ElemT = std::remove_extent_t<typename[:type:]>;
          constexpr auto N_elems = std::extent_v<typename[:type:]>;
          constexpr auto byte_count = N_elems * sizeof(ElemT);
          pack_fmt += std::to_string(byte_count) + "s";
        } else {
          pack_fmt += get_struct_format_char<type>();
        }
      }());
    }(std::make_index_sequence<N>{});
  } else {
    std::cout << "    pass\n";
  }

  std::cout << "\n";
  std::cout << "    def pack_wire(self) -> bytes:\n";
  if (N > 0) {
    std::cout << "        return struct.pack(\"" << pack_fmt << "\", " << pack_args << ")\n\n";
  } else {
    std::cout << "        return b\"\"\n\n";
  }

  std::cout << "    @classmethod\n";
  std::cout << "    def unpack_wire(cls, data: bytes) -> \"" << class_name << "\":\n";
  if (N > 0) {
    std::cout << "        unpacked = struct.unpack(\"" << pack_fmt << "\", data)\n";
    std::cout << "        return cls(*unpacked)\n\n";
  } else {
    std::cout << "        return cls()\n\n";
  }
}

template <uint32_t Id, typename = void>
struct payload_or_void {
  using type = void;
};

template <uint32_t Id>
struct payload_or_void<
    Id, std::void_t<typename ipc::MessageTraits<static_cast<ipc::MsgId>(Id)>::Payload>> {
  using type = typename ipc::MessageTraits<static_cast<ipc::MsgId>(Id)>::Payload;
};

template <uint32_t Id>
void generate_struct_for_msg_id() {
  using T = typename payload_or_void<Id>::type;
  if constexpr (!std::is_void_v<T>) {
    generate_struct<T>();
  }
}

template <typename ActualT, uint32_t Id>
void emit_metadata_for_actual_type() {
  std::string class_name{std::meta::identifier_of(^^ActualT)};
  std::cout << "    MsgId." << ipc::MessageTraits<static_cast<ipc::MsgId>(Id)>::name << ": "
            << class_name << ",\n";
}

template <uint32_t Id>
void emit_metadata_for_msg_id() {
  using T = typename payload_or_void<Id>::type;
  if constexpr (!std::is_void_v<T>) {
    emit_metadata_for_actual_type<T, Id>();
  }
}

template <uint32_t Id>
void emit_size_for_msg_id() {
  using T = typename payload_or_void<Id>::type;
  if constexpr (!std::is_void_v<T>) {
    std::cout << "    MsgId." << ipc::MessageTraits<static_cast<ipc::MsgId>(Id)>::name << ": "
              << sizeof(T) << ",\n";
  }
}
