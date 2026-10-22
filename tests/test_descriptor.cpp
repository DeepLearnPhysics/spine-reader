#include <doctest.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <spine/descriptor.hpp>
#include <spine/detail/assign.hpp>
#include <spine/error.hpp>

namespace {

enum class Colour : std::int64_t { UNKNOWN = -1, RED = 0, GREEN = 1 };

struct Wide {  // one numeric member of each kind that assign converts to
  double d = 0;
  float f = 0;
  std::int64_t i = 0;
  std::int32_t i32 = 0;
  std::uint8_t u8 = 0;
};

struct Plain {
  std::int64_t id = -1;
  Colour colour = Colour::UNKNOWN;
  bool flag = false;
  std::array<float, 3> point = {0, 0, 0};
  std::vector<std::int32_t> ids;
  std::string label;
};

template <class T>
spine::detail::ColumnBuffer fixed_column(spine::ScalarType type, std::size_t count,
                                         const std::vector<T>& values) {
  spine::detail::ColumnBuffer c;
  c.type = type;
  c.count = count;
  c.rows = values.size() / count;
  c.data.resize(values.size() * sizeof(T));
  std::memcpy(c.data.data(), values.data(), c.data.size());
  return c;
}

}  // namespace

TEST_CASE("assign converts scalars, enums, bools and fixed arrays") {
  Plain toy;
  auto ids = fixed_column<std::int32_t>(spine::ScalarType::Int32, 1, {7, 8});
  spine::detail::assign<&Plain::id>(&toy, ids, 1);
  CHECK(toy.id == 8);

  auto colours = fixed_column<std::int64_t>(spine::ScalarType::Int64, 1, {1});
  spine::detail::assign<&Plain::colour>(&toy, colours, 0);
  CHECK(toy.colour == Colour::GREEN);

  auto flags = fixed_column<std::uint8_t>(spine::ScalarType::Bool, 1, {0, 1});
  spine::detail::assign<&Plain::flag>(&toy, flags, 1);
  CHECK(toy.flag);

  auto points = fixed_column<double>(spine::ScalarType::Float64, 3, {1, 2, 3, 4, 5, 6});
  spine::detail::assign<&Plain::point>(&toy, points, 1);
  CHECK(toy.point == std::array<float, 3>{4, 5, 6});
}

TEST_CASE("assign slices variable columns and strings") {
  Plain toy;
  auto c = fixed_column<std::int64_t>(spine::ScalarType::Int64, 1, {1, 2, 3});
  c.variable = true;
  c.rows = 2;
  c.offsets = {0, 0, 3};
  spine::detail::assign<&Plain::ids>(&toy, c, 0);
  CHECK(toy.ids.empty());
  spine::detail::assign<&Plain::ids>(&toy, c, 1);
  CHECK(toy.ids == std::vector<std::int32_t>{1, 2, 3});

  spine::detail::ColumnBuffer s;
  s.type = spine::ScalarType::String;
  s.rows = 1;
  s.strings = {"cm"};
  spine::detail::assign<&Plain::label>(&toy, s, 0);
  CHECK(toy.label == "cm");
}

TEST_CASE("assign loads a column of every ScalarType, to any numeric member") {
  // One row per type, holding a value that exposes a wrong width or signedness: negative for
  // the signed types, above the signed range for the unsigned ones.
  struct Case {
    spine::ScalarType type;
    double value;
    spine::detail::ColumnBuffer column;
  };
  const std::vector<Case> cases = {
      {spine::ScalarType::Int8, -2, fixed_column<std::int8_t>(spine::ScalarType::Int8, 1, {-2})},
      {spine::ScalarType::Int16, -300,
       fixed_column<std::int16_t>(spine::ScalarType::Int16, 1, {-300})},
      {spine::ScalarType::Int32, -70000,
       fixed_column<std::int32_t>(spine::ScalarType::Int32, 1, {-70000})},
      {spine::ScalarType::Int64, -5e9,
       fixed_column<std::int64_t>(spine::ScalarType::Int64, 1, {-5000000000})},
      {spine::ScalarType::UInt8, 250,
       fixed_column<std::uint8_t>(spine::ScalarType::UInt8, 1, {250})},
      {spine::ScalarType::UInt16, 65000,
       fixed_column<std::uint16_t>(spine::ScalarType::UInt16, 1, {65000})},
      {spine::ScalarType::UInt32, 4e9,
       fixed_column<std::uint32_t>(spine::ScalarType::UInt32, 1, {4000000000u})},
      {spine::ScalarType::UInt64, 1e10,
       fixed_column<std::uint64_t>(spine::ScalarType::UInt64, 1, {10000000000ull})},
      {spine::ScalarType::Float32, -2.5,
       fixed_column<float>(spine::ScalarType::Float32, 1, {-2.5f})},
      {spine::ScalarType::Float64, 1e-3,
       fixed_column<double>(spine::ScalarType::Float64, 1, {1e-3})},
      {spine::ScalarType::Bool, 1, fixed_column<std::uint8_t>(spine::ScalarType::Bool, 1, {1})},
  };
  for (const Case& c : cases) {
    CAPTURE(spine::to_string(c.type));
    Wide w;
    spine::detail::assign<&Wide::d>(&w, c.column, 0);
    CHECK(w.d == c.value);
    spine::detail::assign<&Wide::f>(&w, c.column, 0);
    CHECK(w.f == static_cast<float>(c.value));
    spine::detail::assign<&Wide::i>(&w, c.column, 0);
    CHECK(w.i == static_cast<std::int64_t>(c.value));
    spine::detail::assign<&Wide::i32>(&w, c.column, 0);
    CHECK(w.i32 == static_cast<std::int32_t>(static_cast<std::int64_t>(c.value)));
  }
}

TEST_CASE("assign: a bool member is true for any non-zero value, an enum takes the integer") {
  Plain toy;
  auto flags = fixed_column<std::uint8_t>(spine::ScalarType::UInt8, 1, {0, 1, 2, 255});
  const bool expected[] = {false, true, true, true};
  for (std::size_t row = 0; row < 4; ++row) {
    spine::detail::assign<&Plain::flag>(&toy, flags, row);
    CHECK_MESSAGE(toy.flag == expected[row], "row " << row);
  }
  auto colours = fixed_column<std::int32_t>(spine::ScalarType::Int32, 1, {-1, 0, 1});
  for (std::size_t row = 0; row < 3; ++row) {
    spine::detail::assign<&Plain::colour>(&toy, colours, row);
    CHECK(static_cast<std::int64_t>(toy.colour) == static_cast<std::int64_t>(row) - 1);
  }
}

TEST_CASE("assign: fixed arrays read the row's own elements, vectors the row's own span") {
  Plain toy;
  auto points = fixed_column<float>(spine::ScalarType::Float32, 3, {1, 2, 3, 4, 5, 6, 7, 8, 9});
  for (std::size_t row = 0; row < 3; ++row) {
    spine::detail::assign<&Plain::point>(&toy, points, row);
    const float base = 3.0f * static_cast<float>(row);
    CHECK(toy.point == std::array<float, 3>{base + 1, base + 2, base + 3});
  }

  // variable rows: [], [10], [20, 21, 22], [] with a converting element type
  auto c = fixed_column<double>(spine::ScalarType::Float64, 1, {10, 20, 21, 22});
  c.variable = true;
  c.rows = 4;
  c.offsets = {0, 0, 1, 4, 4};
  const std::vector<std::vector<std::int32_t>> expected = {{}, {10}, {20, 21, 22}, {}};
  for (std::size_t row = 0; row < 4; ++row) {
    toy.ids = {99};  // replaced, not appended to
    spine::detail::assign<&Plain::ids>(&toy, c, row);
    CHECK_MESSAGE(toy.ids == expected[row], "row " << row);
  }

  // strings: each row its own
  spine::detail::ColumnBuffer s;
  s.type = spine::ScalarType::String;
  s.rows = 3;
  s.strings = {"a", "", "c\xc2\xb5"};
  for (std::size_t row = 0; row < 3; ++row) {
    spine::detail::assign<&Plain::label>(&toy, s, row);
    CHECK(toy.label == s.strings[row]);
  }
}

TEST_CASE("assign refuses a string column for a number") {
  Plain toy;
  spine::detail::ColumnBuffer s;
  s.type = spine::ScalarType::String;
  s.rows = 1;
  s.strings = {"12"};
  CHECK_THROWS_AS(spine::detail::assign<&Plain::id>(&toy, s, 0), spine::Error);
  CHECK(toy.id == -1);
}

TEST_CASE("ScalarType names and sizes") {
  struct Row {
    spine::ScalarType type;
    const char* name;
    std::size_t size;
  };
  const Row rows[] = {
      {spine::ScalarType::Bool, "bool", 1},     {spine::ScalarType::Int8, "int8", 1},
      {spine::ScalarType::Int16, "int16", 2},   {spine::ScalarType::Int32, "int32", 4},
      {spine::ScalarType::Int64, "int64", 8},   {spine::ScalarType::UInt8, "uint8", 1},
      {spine::ScalarType::UInt16, "uint16", 2}, {spine::ScalarType::UInt32, "uint32", 4},
      {spine::ScalarType::UInt64, "uint64", 8}, {spine::ScalarType::Float32, "float32", 4},
      {spine::ScalarType::Float64, "float64", 8}, {spine::ScalarType::String, "string", 0},
  };
  for (const Row& row : rows) {
    CHECK(spine::to_string(row.type) == row.name);
    CHECK_MESSAGE(spine::scalar_size(row.type) == row.size, row.name);
  }
}
