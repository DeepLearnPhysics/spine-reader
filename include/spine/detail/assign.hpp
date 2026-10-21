#pragma once

// Copies one row of a ColumnBuffer into a member of a generated class. Generated field
// tables point at assign<&Class::member>; nothing else should need these templates.

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

#include <spine/detail/column_buffer.hpp>
#include <spine/error.hpp>

namespace spine::detail {

template <class T>
struct is_std_array : std::false_type {};
template <class T, std::size_t N>
struct is_std_array<std::array<T, N>> : std::true_type {};

template <class T>
struct is_std_vector : std::false_type {};
template <class T, class A>
struct is_std_vector<std::vector<T, A>> : std::true_type {};

template <class T>
inline constexpr bool dependent_false = false;

/// Element `i` of `column`, converted to T (arithmetic or enum).
template <class T>
T load(const ColumnBuffer& column, std::size_t i) {
  if constexpr (std::is_enum_v<T>) {
    return static_cast<T>(load<std::underlying_type_t<T>>(column, i));
  } else {
    switch (column.type) {
      case ScalarType::Bool:
      case ScalarType::UInt8:
        return static_cast<T>(column.elements<std::uint8_t>()[i]);
      case ScalarType::Int8:
        return static_cast<T>(column.elements<std::int8_t>()[i]);
      case ScalarType::Int16:
        return static_cast<T>(column.elements<std::int16_t>()[i]);
      case ScalarType::UInt16:
        return static_cast<T>(column.elements<std::uint16_t>()[i]);
      case ScalarType::Int32:
        return static_cast<T>(column.elements<std::int32_t>()[i]);
      case ScalarType::UInt32:
        return static_cast<T>(column.elements<std::uint32_t>()[i]);
      case ScalarType::Int64:
        return static_cast<T>(column.elements<std::int64_t>()[i]);
      case ScalarType::UInt64:
        return static_cast<T>(column.elements<std::uint64_t>()[i]);
      case ScalarType::Float32:
        return static_cast<T>(column.elements<float>()[i]);
      case ScalarType::Float64:
        return static_cast<T>(column.elements<double>()[i]);
      case ScalarType::String:
        break;
    }
    throw Error("spine-reader: a string column cannot be read as a number");
  }
}

template <class M>
void assign_value(M& out, const ColumnBuffer& column, std::size_t row) {
  if constexpr (std::is_same_v<M, std::string>) {
    out = column.strings[row];
  } else if constexpr (std::is_arithmetic_v<M> || std::is_enum_v<M>) {
    out = load<M>(column, row * column.count);
  } else if constexpr (is_std_array<M>::value) {
    using E = typename M::value_type;
    const std::size_t base = row * column.count;
    for (std::size_t k = 0; k < out.size() && k < column.count; ++k) {
      out[k] = load<E>(column, base + k);
    }
  } else if constexpr (is_std_vector<M>::value) {
    using E = typename M::value_type;
    std::size_t begin = row * column.count;
    std::size_t end = begin + column.count;
    if (column.variable) {
      begin = static_cast<std::size_t>(column.offsets[row]);
      end = static_cast<std::size_t>(column.offsets[row + 1]);
    }
    out.resize(end - begin);
    for (std::size_t k = begin; k < end; ++k) {
      out[k - begin] = load<E>(column, k);
    }
  } else {
    static_assert(dependent_false<M>, "spine-reader: unsupported member type in a generated class");
  }
}

template <class P>
struct member_pointer_traits;
template <class C, class M>
struct member_pointer_traits<M C::*> {
  using object_type = C;
  using member_type = M;
};

template <auto Member>
void assign(void* object, const ColumnBuffer& column, std::size_t row) {
  using Object = typename member_pointer_traits<decltype(Member)>::object_type;
  assign_value(static_cast<Object*>(object)->*Member, column, row);
}

}  // namespace spine::detail
