// Comparisons of model members, shared by test_golden.cpp, test_model.cpp, test_reader.cpp and
// test_contracts.cpp.
//
// * equal_member(value, json): a member against a decoded fixture value or a JSON default, as the
//   fixture tooling and the contract table write them ("nan", "inf" and "-inf" for floats).
// * nan_equal(a, b): two members of the same type, NaN equal to NaN. Tests of other behaviour
//   compare with a default-constructed object through it, never with a literal default.
// * member_json(value): the JSON form equal_member() reads, for messages.
// * all_nan(values): every element is NaN.

#pragma once

#include <json.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

namespace spine_test {

template <class T>
struct is_std_array : std::false_type {};
template <class T, std::size_t N>
struct is_std_array<std::array<T, N>> : std::true_type {};
template <class T>
struct is_std_vector : std::false_type {};
template <class T, class A>
struct is_std_vector<std::vector<T, A>> : std::true_type {};

/// True if `value` is what `want` holds. Floats compare as the member's own type: the fixture
/// stores the exact double of a float32, and a float64 column read into a float member is rounded
/// to float.
template <class M>
bool equal_member(const M& value, const nlohmann::json& want) {
  if constexpr (std::is_same_v<M, std::string>) {
    return want.is_string() && want.get<std::string>() == value;
  } else if constexpr (std::is_same_v<M, bool>) {
    if (want.is_boolean()) return want.get<bool>() == value;
    return want.is_number() && (want.get<double>() != 0) == value;
  } else if constexpr (std::is_enum_v<M>) {
    return want.is_number_integer() && want.get<std::int64_t>() == static_cast<std::int64_t>(value);
  } else if constexpr (std::is_floating_point_v<M>) {
    if (want.is_string()) {
      const std::string text = want.get<std::string>();
      if (text == "nan") return std::isnan(value);
      if (text == "inf") return std::isinf(value) && value > 0;
      if (text == "-inf") return std::isinf(value) && value < 0;
      return false;
    }
    return want.is_number() && static_cast<M>(want.get<double>()) == value;
  } else if constexpr (std::is_arithmetic_v<M>) {
    return want.is_number_integer() && static_cast<M>(want.get<std::int64_t>()) == value;
  } else {
    static_assert(is_std_array<M>::value || is_std_vector<M>::value, "unsupported member");
    if (!want.is_array() || want.size() != value.size()) return false;
    for (std::size_t i = 0; i < value.size(); ++i) {
      if (!equal_member(value[i], want[i])) return false;
    }
    return true;
  }
}

/// a == b, element by element for arrays and vectors, with NaN equal to NaN.
template <class M>
bool nan_equal(const M& a, const M& b) {
  if constexpr (std::is_floating_point_v<M>) {
    return (std::isnan(a) && std::isnan(b)) || a == b;
  } else if constexpr (is_std_array<M>::value || is_std_vector<M>::value) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
      if (!nan_equal(a[i], b[i])) return false;
    }
    return true;
  } else {
    return a == b;
  }
}

/// The JSON form of a member that equal_member() accepts, for failure messages.
template <class M>
nlohmann::json member_json(const M& value) {
  if constexpr (std::is_same_v<M, std::string> || std::is_same_v<M, bool>) {
    return value;
  } else if constexpr (std::is_enum_v<M>) {
    return static_cast<std::int64_t>(value);
  } else if constexpr (std::is_floating_point_v<M>) {
    if (std::isnan(value)) return "nan";
    if (std::isinf(value)) return value > 0 ? "inf" : "-inf";
    return static_cast<double>(value);
  } else if constexpr (std::is_arithmetic_v<M>) {
    return static_cast<std::int64_t>(value);
  } else {
    nlohmann::json values = nlohmann::json::array();
    for (const auto& v : value) values.push_back(member_json(v));
    return values;
  }
}

template <class A>
bool all_nan(const A& values) {
  return std::all_of(values.begin(), values.end(), [](auto v) { return std::isnan(v); });
}

}  // namespace spine_test
