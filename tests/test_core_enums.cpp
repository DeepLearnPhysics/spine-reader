// Enums, bools, strings and variable-length columns of a V1 file built to hold the awkward
// cases (edge_v1_columns.h5, see make_edge_cases.py). The enum behaviour documented in
// src/core/columns.cpp is pinned down here, and is run on HDF5 1.12.2 and 1.14.6.

#include <doctest.h>

#include <cmath>
#include <map>

#include "core_test_util.hpp"

using namespace spine;
using namespace core_test;
using spine::core::ProductKind;

namespace {

std::vector<double> values(const ColumnBuffer& b) {
  std::vector<double> out;
  const std::size_t total =
      b.variable ? static_cast<std::size_t>(b.offsets.back()) : b.rows * b.count;
  for (std::size_t i = 0; i < total; ++i) out.push_back(element(b, i));
  return out;
}

using Doubles = std::vector<double>;

struct EdgeFile {
  Hdf5Lock lock;
  std::unique_ptr<FileSource> file = open("edge_v1_columns.h5");
  ProductSource& objs = file->product("objs");
  ColumnBuffer read(const std::string& column, ScalarType type) {
    return read_column(objs, column, all_rows(objs), type);
  }
};

}  // namespace

TEST_CASE("enum, bool and string columns are described as the file has them") {
  EdgeFile f;
  const core::ProductInfo& info = f.objs.info();
  CHECK(info.kind == ProductKind::Objects);
  CHECK(info.class_name == "Edge");
  CHECK(info.columns.size() == 13);

  auto labels = [&](const char* name) {
    std::map<std::string, std::int64_t> out;
    for (const auto& member : info.find(name)->enum_members) out[member.name] = member.value;
    return out;
  };
  using Labels = std::map<std::string, std::int64_t>;

  CHECK(info.find("shape")->type == ScalarType::Int64);
  CHECK(labels("shape") ==
        Labels{{"UNKNOWN", -1}, {"SHOWER", 0}, {"TRACK", 1}, {"LARCV_UNKNOWN", 6}});
  CHECK(info.find("flag")->type == ScalarType::Bool);
  CHECK(labels("flag") == Labels{{"FALSE", 0}, {"TRUE", 1}});
  CHECK(info.find("plain_flag")->type == ScalarType::UInt8);  // a bool written before SPINE v0.11.0
  CHECK(info.find("plain_flag")->enum_members.empty());
  CHECK(info.find("unsigned_enum")->type == ScalarType::UInt8);
  CHECK(labels("unsigned_enum") == Labels{{"A", 0}, {"B", 200}});
  CHECK(info.find("big_endian")->type == ScalarType::Int32);  // the base type, not the file's order
  CHECK(labels("big_endian") == Labels{{"NEG", -2}, {"POS", 5}});

  CHECK(info.find("narrow")->type == ScalarType::Int32);
  CHECK(info.find("wide")->type == ScalarType::Float64);
  CHECK(info.find("triple")->count == 3);
  CHECK(info.find("single")->count == 1);
  CHECK_FALSE(info.find("single")->variable);
  CHECK(info.find("name")->type == ScalarType::String);
  CHECK(info.find("name")->variable);
  CHECK(info.find("ascii_name")->type == ScalarType::String);
  CHECK(info.find("ids")->type == ScalarType::Int32);
  CHECK(info.find("ids")->variable);
  CHECK(info.find("weights")->type == ScalarType::Float64);
}

TEST_CASE("an enum converts like its base integer, whatever the label table says") {
  EdgeFile f;
  // -1 (UNKNOWN), labelled values, and 99, which the enum does not list
  const Doubles shape = {-1, 0, 1, 6, 99};
  CHECK(values(f.read("shape", ScalarType::Int64)) == shape);
  CHECK(values(f.read("shape", ScalarType::Int32)) == shape);
  CHECK(values(f.read("shape", ScalarType::Int16)) == shape);
  CHECK(values(f.read("shape", ScalarType::Int8)) == shape);
  CHECK(values(f.read("shape", ScalarType::Float64)) == shape);
  CHECK(values(f.read("shape", ScalarType::Float32)) == shape);
  // narrowing to an unsigned type saturates, as it does for integers: -1 becomes 0
  CHECK(values(f.read("shape", ScalarType::UInt8)) == Doubles{0, 0, 1, 6, 99});
  CHECK(values(f.read("shape", ScalarType::UInt64)) == Doubles{0, 0, 1, 6, 99});

  // an unsigned enum base; 200 does not fit a signed byte
  CHECK(values(f.read("unsigned_enum", ScalarType::UInt8)) == Doubles{0, 200, 200, 0, 7});
  CHECK(values(f.read("unsigned_enum", ScalarType::Int32)) == Doubles{0, 200, 200, 0, 7});
  CHECK(values(f.read("unsigned_enum", ScalarType::Int8)) == Doubles{0, 127, 127, 0, 7});

  // a base type that is not in native byte order, and a value (3) its enum has no label for:
  // reading through a native copy of the enum type would turn that 3 into -1
  const Doubles big_endian = {-2, 5, 5, -2, 3};
  CHECK(values(f.read("big_endian", ScalarType::Int32)) == big_endian);
  CHECK(values(f.read("big_endian", ScalarType::Int64)) == big_endian);
  CHECK(values(f.read("big_endian", ScalarType::Float64)) == big_endian);
}

TEST_CASE("bools: Bool is 0 or 1, the integer reading shows what the file holds") {
  EdgeFile f;
  const ColumnBuffer as_bool = f.read("flag", ScalarType::Bool);
  CHECK(as_bool.type == ScalarType::Bool);
  CHECK(values(as_bool) == Doubles{0, 1, 1, 0, 1});  // the 2 in the file counts as true
  CHECK(values(f.read("flag", ScalarType::UInt8)) == Doubles{0, 1, 1, 0, 2});
  CHECK(values(f.read("flag", ScalarType::Int64)) == Doubles{0, 1, 1, 0, 2});

  // a plain uint8 bool is an integer; only enum bools can be read as Bool
  CHECK(values(f.read("plain_flag", ScalarType::UInt8)) == Doubles{0, 1, 0, 1, 1});
  CHECK(values(f.read("plain_flag", ScalarType::Int32)) == Doubles{0, 1, 0, 1, 1});
  CHECK_THROWS_AS(f.read("plain_flag", ScalarType::Bool), Error);
}

TEST_CASE("plain numbers, fixed arrays and arrays of length one") {
  EdgeFile f;
  CHECK(values(f.read("narrow", ScalarType::Int32)) == Doubles{-3, 4, 70000, 0, 2147483647});
  CHECK(values(f.read("narrow", ScalarType::Int64)) == Doubles{-3, 4, 70000, 0, 2147483647});
  // narrowing saturates
  CHECK(values(f.read("narrow", ScalarType::Int16)) == Doubles{-3, 4, 32767, 0, 32767});
  CHECK(values(f.read("wide", ScalarType::Float64)) == Doubles{0.5, -1.25, 1e300, 0.0, M_PI});
  CHECK(values(f.read("wide", ScalarType::Float32))[0] == 0.5);

  const ColumnBuffer triple = f.read("triple", ScalarType::Float64);
  CHECK(triple.count == 3);
  CHECK(triple.rows == 5);
  CHECK(values(triple) == Doubles{1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0, -1, -2, -3});

  const ColumnBuffer single = f.read("single", ScalarType::Int32);
  CHECK(single.count == 1);
  CHECK(values(single) == Doubles{1, 2, 3, 4, 5});
}

TEST_CASE("strings: UTF-8, ASCII, empty") {
  EdgeFile f;
  const ColumnBuffer name = f.read("name", ScalarType::String);
  CHECK(name.type == ScalarType::String);
  CHECK(name.rows == 5);
  CHECK(name.data.empty());
  CHECK(name.offsets.empty());
  CHECK(name.strings == std::vector<std::string>{"", "pi \xcf\x80", "caf\xc3\xa9", "x",
                                                 "a longer string with spaces"});
  const ColumnBuffer ascii = f.read("ascii_name", ScalarType::String);
  CHECK(ascii.strings == std::vector<std::string>{"a", "", "ccc", "dd", "e"});
}

TEST_CASE("variable-length columns, with empty rows, in any element type") {
  EdgeFile f;
  const ColumnBuffer ids = f.read("ids", ScalarType::Int32);
  CHECK(ids.variable);
  CHECK(ids.rows == 5);
  // row i holds i values: 0, 3, ..., 3 * (i - 1)
  CHECK(ids.offsets == std::vector<std::uint64_t>{0, 0, 1, 3, 6, 10});
  CHECK(values(ids) == Doubles{0, 0, 3, 0, 3, 6, 0, 3, 6, 9});
  CHECK(values(f.read("ids", ScalarType::Int64)) == values(ids));
  CHECK(values(f.read("ids", ScalarType::Float32)) == values(ids));

  const ColumnBuffer weights = f.read("weights", ScalarType::Float64);
  // row i holds 2 * i values
  CHECK(weights.offsets == std::vector<std::uint64_t>{0, 0, 2, 6, 12, 20});
  const std::vector<double> got = values(weights);
  REQUIRE(got.size() == 20);
  CHECK(got[0] == 0.0);
  CHECK(got[1] == 1.0);                        // row 1: linspace(0, 1, 2)
  CHECK(got[3] == doctest::Approx(1.0 / 3));   // row 2: linspace(0, 1, 4)
  CHECK(got[19] == 1.0);                       // row 4: the last of linspace(0, 1, 8)
  const ColumnBuffer narrow = f.read("weights", ScalarType::Float32);
  CHECK(narrow.data.size() == 20 * 4);
  CHECK(element(narrow, 3) == doctest::Approx(1.0 / 3));
}

TEST_CASE("events: [0, 2), empty, [2, 5); a subset of members is one read") {
  EdgeFile f;
  CHECK(f.objs.rows(0).size() == 2);
  CHECK(f.objs.rows(1).size() == 0);
  CHECK(f.objs.rows(2).begin == 2);
  CHECK(f.objs.rows(2).end == 5);

  // all columns in one read, and the same columns one by one
  const auto requests = natural_requests(f.objs.info());
  std::vector<ColumnBuffer> together;
  f.objs.read(f.objs.rows(2), requests, together);
  REQUIRE(together.size() == requests.size());
  for (std::size_t i = 0; i < requests.size(); ++i) {
    CAPTURE(requests[i].column->name);
    std::vector<ColumnBuffer> alone;
    f.objs.read(f.objs.rows(2), {requests[i]}, alone);
    REQUIRE(alone.size() == 1);
    CHECK(same_buffer(alone[0], together[i]));
  }
  CHECK(values(together[0]) == Doubles{1, 6, 99});  // shape of rows 2, 3, 4
}

TEST_CASE("a V1 dataset of strings is a String product") {
  EdgeFile f;
  ProductSource& labels = f.file->product("labels");
  CHECK(labels.info().kind == ProductKind::String);
  CHECK(labels.info().scalar);
  REQUIRE(labels.info().columns.size() == 1);
  CHECK(labels.info().columns[0].type == ScalarType::String);
  CHECK(labels.num_rows() == 3);
  CHECK(read_column(labels, "", all_rows(labels), ScalarType::String).strings ==
        std::vector<std::string>{"zero", "one \xc2\xb5", ""});
  CHECK(read_column(labels, "", labels.rows(1), ScalarType::String).strings ==
        std::vector<std::string>{"one \xc2\xb5"});
  CHECK_THROWS_AS(read_column(labels, "", all_rows(labels), ScalarType::Int32), Error);
}
