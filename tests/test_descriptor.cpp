#include <doctest.h>

#include <array>
#include <cstring>
#include <string>
#include <vector>

#include <spine/descriptor.hpp>
#include <spine/detail/assign.hpp>

namespace {

enum class Colour : std::int64_t { UNKNOWN = -1, RED = 0, GREEN = 1 };

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
