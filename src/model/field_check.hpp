// GENERATED from SPINE 1.4.0 by tools/gen/generate.py. Do not edit.
#pragma once

// Compile-time agreement between a generated class member and its FieldDescriptor.

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <spine/descriptor.hpp>
#include <spine/detail/assign.hpp>

namespace spine::detail {

template <class T>
constexpr ScalarType element_type() {
  if constexpr (std::is_same_v<T, bool>) {
    return ScalarType::Bool;
  } else if constexpr (std::is_enum_v<T>) {
    static_assert(std::is_same_v<std::underlying_type_t<T>, std::int64_t>,
                  "SPINE enums are stored as int64");
    return ScalarType::Int64;
  } else if constexpr (std::is_same_v<T, std::int8_t>) {
    return ScalarType::Int8;
  } else if constexpr (std::is_same_v<T, std::int16_t>) {
    return ScalarType::Int16;
  } else if constexpr (std::is_same_v<T, std::int32_t>) {
    return ScalarType::Int32;
  } else if constexpr (std::is_same_v<T, std::int64_t>) {
    return ScalarType::Int64;
  } else if constexpr (std::is_same_v<T, std::uint8_t>) {
    return ScalarType::UInt8;
  } else if constexpr (std::is_same_v<T, std::uint16_t>) {
    return ScalarType::UInt16;
  } else if constexpr (std::is_same_v<T, std::uint32_t>) {
    return ScalarType::UInt32;
  } else if constexpr (std::is_same_v<T, std::uint64_t>) {
    return ScalarType::UInt64;
  } else if constexpr (std::is_same_v<T, float>) {
    return ScalarType::Float32;
  } else if constexpr (std::is_same_v<T, double>) {
    return ScalarType::Float64;
  } else if constexpr (std::is_same_v<T, std::string>) {
    return ScalarType::String;
  } else {
    static_assert(dependent_false<T>, "spine-reader: unsupported member type");
  }
}

/// How a member of type M must be described: element type, count, variable flag.
template <class M>
struct MemberShape {
  static constexpr ScalarType type = element_type<M>();
  static constexpr std::uint32_t count = std::is_same_v<M, std::string> ? 0 : 1;
  static constexpr bool variable = std::is_same_v<M, std::string>;
  static constexpr bool is_enum = std::is_enum_v<M>;
};
template <class E, std::size_t N>
struct MemberShape<std::array<E, N>> {
  static constexpr ScalarType type = element_type<E>();
  static constexpr std::uint32_t count = static_cast<std::uint32_t>(N);
  static constexpr bool variable = false;
  static constexpr bool is_enum = std::is_enum_v<E>;
};
template <class E, class A>
struct MemberShape<std::vector<E, A>> {
  static constexpr ScalarType type = element_type<E>();
  static constexpr std::uint32_t count = 0;
  static constexpr bool variable = true;
  static constexpr bool is_enum = std::is_enum_v<E>;
};

template <class M>
constexpr bool field_matches(const FieldDescriptor& d, std::string_view name) {
  using S = MemberShape<M>;
  return d.name == name && d.type == S::type && d.count == S::count && d.variable == S::variable &&
         (d.enumeration != nullptr) == S::is_enum && d.assign != nullptr;
}

}  // namespace spine::detail

// Inside the anonymous namespace of a generated .cpp that defines `kFields`.
#define SPINE_MODEL_CHECK_FIELD(Class, name)                                              \
  static_assert(::spine::detail::field_matches<decltype(Class::name)>(                    \
                    kFields[static_cast<std::size_t>(Class::Field::name)], #name),        \
                #Class "::" #name " does not match its FieldDescriptor")
