#include <doctest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#include <spine/descriptor.hpp>
#include <spine/detail/assign.hpp>
#include <spine/error.hpp>
#include <spine/object.hpp>

#include "bind/binder.hpp"

namespace {

using spine::ScalarType;
using spine::core::ColumnInfo;
using spine::core::RowRange;

constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();

enum class Shape : std::int64_t { UNKNOWN = -1, SHOWER = 0, TRACK = 1 };
const spine::EnumMember kShapeMembers[] = {{"UNKNOWN", -1}, {"SHOWER", 0}, {"TRACK", 1}};
const spine::EnumDescriptor kShape{"Shape", kShapeMembers, 3};

// Hand-written stand-in for a generated class.
struct Toy : spine::Object<6> {
  enum class Field : std::uint16_t { id, shape, is_contained, start_point, match_ids, units };
  std::int64_t id = -1;
  Shape shape = Shape::UNKNOWN;
  bool is_contained = false;
  std::array<float, 3> start_point = {kNaN, kNaN, kNaN};
  std::vector<std::int32_t> match_ids;
  std::string units = "cm";
  bool has(Field f) const { return has_field(static_cast<std::size_t>(f)); }
  static const spine::ClassDescriptor& descriptor();
};

const spine::FieldDescriptor kToyFields[] = {
    {"id", ScalarType::Int64, 1, false, spine::kIndex, "", nullptr, "", "", "",
     &spine::detail::assign<&Toy::id>},
    {"shape", ScalarType::Int64, 1, false, spine::kCategorical, "", &kShape, "", "", "",
     &spine::detail::assign<&Toy::shape>},
    {"is_contained", ScalarType::Bool, 1, false, 0, "", nullptr, "", "", "",
     &spine::detail::assign<&Toy::is_contained>},
    {"start_point", ScalarType::Float32, 3, false, spine::kPosition, "instance", nullptr, "", "",
     "", &spine::detail::assign<&Toy::start_point>},
    {"match_ids", ScalarType::Int32, 0, true, spine::kIndex, "", nullptr, "particle", "opposite",
     "", &spine::detail::assign<&Toy::match_ids>},
    {"units", ScalarType::String, 0, true, 0, "", nullptr, "", "", "",
     &spine::detail::assign<&Toy::units>},
};

const spine::ClassDescriptor& Toy::descriptor() {
  static const spine::ClassDescriptor d{"Toy", kToyFields, std::size(kToyFields), "test"};
  return d;
}

// In-memory product: each column holds its rows as doubles (or strings) and converts them
// to whatever type is requested, like the real core does through HDF5.
class FakeSource : public spine::core::ProductSource {
 public:
  struct Column {
    ColumnInfo info;
    std::vector<std::vector<double>> rows;
    std::vector<std::string> strings;
  };

  explicit FakeSource(std::vector<Column> columns) : columns_(std::move(columns)) {
    info_.name = "toys";
    info_.class_name = "Toy";
    for (const Column& c : columns_) info_.columns.push_back(c.info);
  }

  const spine::core::ProductInfo& info() const override { return info_; }
  RowRange rows(std::uint64_t) override { return {0, num_rows()}; }
  std::uint64_t num_rows() override {
    const Column& c = columns_.front();
    return c.info.type == ScalarType::String ? c.strings.size() : c.rows.size();
  }

  void read(RowRange rows, const std::vector<spine::core::ColumnRequest>& reqs,
            std::vector<spine::detail::ColumnBuffer>& out) override {
    out.assign(reqs.size(), {});
    for (std::size_t k = 0; k < reqs.size(); ++k) {
      const Column& c = columns_[static_cast<std::size_t>(reqs[k].column - info_.columns.data())];
      // What the real core refuses (core::check_request): a binder that asks for it is wrong.
      if ((c.info.type == ScalarType::String) != (reqs[k].type == ScalarType::String)) {
        throw spine::Error("FakeSource: column '" + c.info.name + "' cannot be read as that type");
      }
      if (reqs[k].type == ScalarType::Bool && c.info.type != ScalarType::Bool) {
        throw spine::Error("FakeSource: column '" + c.info.name + "' is not a bool");
      }
      spine::detail::ColumnBuffer& b = out[k];
      b.type = reqs[k].type;
      b.variable = c.info.variable;
      b.count = c.info.variable ? 1 : c.info.count;
      b.rows = rows.size();
      if (b.type == ScalarType::String) {
        b.strings.assign(c.strings.begin() + rows.begin, c.strings.begin() + rows.end);
        continue;
      }
      if (b.variable) b.offsets.push_back(0);
      for (auto r = rows.begin; r < rows.end; ++r) {
        for (double v : c.rows[r]) append(b, v);
        if (b.variable) b.offsets.push_back(b.offsets.back() + c.rows[r].size());
      }
    }
  }

 private:
  template <class T>
  static void put(spine::detail::ColumnBuffer& b, T v) {
    const std::size_t at = b.data.size();
    b.data.resize(at + sizeof(T));
    std::memcpy(b.data.data() + at, &v, sizeof(T));
  }

  static void append(spine::detail::ColumnBuffer& b, double v) {
    switch (b.type) {
      case ScalarType::Bool:
        return put(b, static_cast<std::uint8_t>(v != 0));
      case ScalarType::UInt8:
        return put(b, static_cast<std::uint8_t>(v));
      case ScalarType::Int32:
        return put(b, static_cast<std::int32_t>(v));
      case ScalarType::Int64:
        return put(b, static_cast<std::int64_t>(v));
      case ScalarType::Float32:
        return put(b, static_cast<float>(v));
      case ScalarType::Float64:
        return put(b, v);
      default:
        FAIL("type not supported by FakeSource");
    }
  }

  std::vector<Column> columns_;
  spine::core::ProductInfo info_;
};

ColumnInfo column(std::string name, ScalarType type, std::size_t count = 1, bool variable = false) {
  ColumnInfo c;
  c.name = std::move(name);
  c.type = type;
  c.count = variable ? 0 : count;
  c.variable = variable;
  return c;
}

}  // namespace

TEST_CASE("a file with every model field binds completely") {
  FakeSource source({
      {column("id", ScalarType::Int64), {{0}, {1}, {2}}, {}},
      {column("shape", ScalarType::Int64), {{1}, {0}, {-1}}, {}},
      {column("is_contained", ScalarType::Bool), {{1}, {0}, {1}}, {}},
      {column("start_point", ScalarType::Float64, 3), {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}}, {}},
      {column("match_ids", ScalarType::Int64, 0, true), {{}, {3, 4}, {5}}, {}},
      {column("units", ScalarType::String, 1, true), {}, {"cm", "px", "cm"}},
  });
  spine::bind::ProductBinding binding(Toy::descriptor(), source);
  CHECK(binding.report().complete());
  CHECK(binding.report().extra.empty());

  std::vector<Toy> toys;
  binding.read(RowRange{1, 3}, toys);
  REQUIRE(toys.size() == 2);
  CHECK(toys[0].id == 1);
  CHECK(toys[0].shape == Shape::SHOWER);
  CHECK_FALSE(toys[0].is_contained);
  CHECK(toys[0].start_point == std::array<float, 3>{4, 5, 6});
  CHECK(toys[0].match_ids == std::vector<std::int32_t>{3, 4});
  CHECK(toys[0].units == "px");
  CHECK(toys[1].shape == Shape::UNKNOWN);
  CHECK(toys[1].match_ids == std::vector<std::int32_t>{5});
  CHECK(toys[1].has(Toy::Field::units));
}

TEST_CASE("missing and mismatched fields keep defaults and are reported") {
  FakeSource source({
      {column("id", ScalarType::Int32), {{7}}, {}},
      {column("start_point", ScalarType::Float64, 4), {{1, 2, 3, 4}}, {}},
      {column("is_contained", ScalarType::UInt8), {{2}}, {}},
      {column("shape", ScalarType::Int64), {{1}}, {}},
      {column("brand_new", ScalarType::Float32), {{0.5}}, {}},
  });
  spine::bind::ProductBinding binding(Toy::descriptor(), source);
  const spine::ProductReport& report = binding.report();
  CHECK_FALSE(report.complete());
  CHECK(report.missing == std::vector<std::string>{"match_ids", "units"});
  REQUIRE(report.mismatched.size() == 1);
  CHECK(report.mismatched[0] == "start_point: file float64[4] vs model float32[3]");
  CHECK(report.extra == std::vector<std::string>{"brand_new"});

  std::vector<Toy> toys;
  binding.read(RowRange{0, 1}, toys);
  REQUIRE(toys.size() == 1);
  CHECK(toys[0].id == 7);
  CHECK(toys[0].is_contained);  // integer-stored bool: non-zero is true
  CHECK(std::isnan(toys[0].start_point[0]));
  CHECK(toys[0].units == "cm");
  // exactly the fields that were bound are present
  const std::vector<std::pair<Toy::Field, bool>> present = {
      {Toy::Field::id, true},          {Toy::Field::shape, true},
      {Toy::Field::is_contained, true}, {Toy::Field::start_point, false},
      {Toy::Field::match_ids, false},  {Toy::Field::units, false}};
  for (const auto& [field, expected] : present) {
    CHECK_MESSAGE(toys[0].has(field) == expected, "field " << static_cast<int>(field));
    CHECK(binding.present(static_cast<std::size_t>(field)) == expected);
  }
}

TEST_CASE("a bool stored as a plain integer is read as that integer, then made a bool") {
  // The core refuses to read a non-bool column as Bool, and so does FakeSource: this test fails
  // if the binder asks for Bool, and passes only if it reads the integer and lets assign convert.
  FakeSource source({{column("is_contained", ScalarType::UInt8), {{0}, {1}, {2}, {255}}, {}}});
  spine::bind::ProductBinding binding(Toy::descriptor(), source);
  std::vector<Toy> toys;
  binding.read(RowRange{0, 4}, toys);
  REQUIRE(toys.size() == 4);
  CHECK_FALSE(toys[0].is_contained);
  CHECK(toys[1].is_contained);
  CHECK(toys[2].is_contained);
  CHECK(toys[3].is_contained);
  CHECK(toys[0].has(Toy::Field::is_contained));

  // an enum-bool column is read as Bool
  FakeSource enum_bool({{column("is_contained", ScalarType::Bool), {{0}, {1}}, {}}});
  spine::bind::ProductBinding enum_binding(Toy::descriptor(), enum_bool);
  enum_binding.read(RowRange{0, 2}, toys);
  CHECK_FALSE(toys[0].is_contained);
  CHECK(toys[1].is_contained);
}

TEST_CASE("fields are bound by name, not by position") {
  // every column of the model, but in reverse order and with a foreign column in front
  FakeSource source({
      {column("units", ScalarType::String, 1, true), {}, {"px"}},
      {column("match_ids", ScalarType::Int32, 0, true), {{4, 5}}, {}},
      {column("start_point", ScalarType::Float32, 3), {{1, 2, 3}}, {}},
      {column("is_contained", ScalarType::Bool), {{1}}, {}},
      {column("shape", ScalarType::Int64), {{1}}, {}},
      {column("id", ScalarType::Int64), {{42}}, {}},
      {column("zzz", ScalarType::Int64), {{0}}, {}},
  });
  spine::bind::ProductBinding binding(Toy::descriptor(), source);
  CHECK(binding.report().complete());  // extra columns do not count
  CHECK(binding.report().extra == std::vector<std::string>{"zzz"});
  CHECK(binding.report().missing.empty());
  std::vector<Toy> toys;
  binding.read(RowRange{0, 1}, toys);
  REQUIRE(toys.size() == 1);
  CHECK(toys[0].id == 42);
  CHECK(toys[0].shape == Shape::TRACK);
  CHECK(toys[0].is_contained);
  CHECK(toys[0].start_point == std::array<float, 3>{1, 2, 3});
  CHECK(toys[0].match_ids == std::vector<std::int32_t>{4, 5});
  CHECK(toys[0].units == "px");
}

TEST_CASE("reading into a vector that holds objects replaces them, defaults and all") {
  FakeSource source({{column("id", ScalarType::Int64), {{7}, {8}, {9}}, {}}});
  spine::bind::ProductBinding binding(Toy::descriptor(), source);
  std::vector<Toy> toys(5);
  for (Toy& toy : toys) {
    toy.units = "stale";
    toy.shape = Shape::TRACK;
    toy.id = 1000;
  }
  binding.read(RowRange{0, 3}, toys);
  REQUIRE(toys.size() == 3);
  for (const Toy& toy : toys) {
    CHECK(toy.units == "cm");  // the file has no units: the default, not what was there before
    CHECK(toy.shape == Shape::UNKNOWN);
  }
  CHECK(toys[2].id == 9);
  binding.read(RowRange{1, 2}, toys);
  REQUIRE(toys.size() == 1);
  CHECK(toys[0].id == 8);
}

TEST_CASE("enum labels are compared by value, ignoring case") {
  ColumnInfo shape = column("shape", ScalarType::Int64);
  shape.enum_members = {{"Shower", 0}, {"TRACK", 1}, {"Michel", 2}, {"Unknown", -1}};
  FakeSource source({{shape, {{2}}, {}}});
  spine::bind::ProductBinding binding(Toy::descriptor(), source);
  CHECK(binding.report().enum_notes ==
        std::vector<std::string>{"shape: file label Michel=2 is unknown to the model"});

  std::vector<Toy> toys;
  binding.read(RowRange{0, 1}, toys);
  CHECK(static_cast<std::int64_t>(toys[0].shape) == 2);  // kept by value
}

TEST_CASE("string and number columns never bind to each other") {
  CHECK(spine::bind::incompatibility(kToyFields[5], column("units", ScalarType::String, 1, false))
            .empty());
  CHECK(spine::bind::incompatibility(kToyFields[5], column("units", ScalarType::Int64)) ==
        "file int64 vs model string");
  CHECK(spine::bind::incompatibility(kToyFields[4], column("match_ids", ScalarType::Int64, 2)) ==
        "file int64[2] vs model int32[]");
}

TEST_CASE("enum labels that name a known value differently are reported") {
  ColumnInfo shape = column("shape", ScalarType::Int64);
  shape.enum_members = {{"TRACK", 0}, {"SHOWER", 1}, {"UNKNOWN", -1}, {"NOVEL", 9}};
  FakeSource source({{shape, {{0}}, {}}});
  spine::bind::ProductBinding binding(Toy::descriptor(), source);
  std::vector<std::string> notes = binding.report().enum_notes;
  std::sort(notes.begin(), notes.end());
  CHECK(notes == std::vector<std::string>{
                     "shape: file label NOVEL=9 is unknown to the model",
                     "shape: value 0 is TRACK in the file but SHOWER in the model",
                     "shape: value 1 is SHOWER in the file but TRACK in the model"});

  // the value is what is read, whatever the label: 0 is SHOWER in the model
  std::vector<Toy> toys;
  binding.read(RowRange{0, 1}, toys);
  CHECK(toys[0].shape == Shape::SHOWER);

  // no enum labels in the file, nothing to report
  FakeSource plain({{column("shape", ScalarType::Int64), {{1}}, {}}});
  CHECK(spine::bind::ProductBinding(Toy::descriptor(), plain).report().enum_notes.empty());
}

TEST_CASE("shape mismatches name both sides") {
  using spine::bind::incompatibility;
  // fixed length differs, variable vs fixed, fixed vs variable, number vs string, string vs number
  CHECK(incompatibility(kToyFields[3], column("start_point", ScalarType::Float64, 3)).empty());
  CHECK(incompatibility(kToyFields[3], column("start_point", ScalarType::Float64, 4)) ==
        "file float64[4] vs model float32[3]");
  CHECK(incompatibility(kToyFields[3], column("start_point", ScalarType::Float64, 3, true)) ==
        "file float64[] vs model float32[3]");
  CHECK(incompatibility(kToyFields[4], column("match_ids", ScalarType::Int32, 0, true)).empty());
  CHECK(incompatibility(kToyFields[4], column("match_ids", ScalarType::Int32, 1)) ==
        "file int32 vs model int32[]");
  CHECK(incompatibility(kToyFields[0], column("id", ScalarType::String, 1, true)) ==
        "file string vs model int64");
  CHECK(incompatibility(kToyFields[0], column("id", ScalarType::Int64, 2)) ==
        "file int64[2] vs model int64");
  // the element type never matters: the core converts
  CHECK(incompatibility(kToyFields[0], column("id", ScalarType::Float32)).empty());
}
