// Every numeric dtype and bool, in V1 and in V2 (edge_dtypes_v1.h5, edge_dtypes_v2.h5, written
// by make_audit_cases.py with identical content): how a column is described, and how it
// converts to each of the ten memory types. The files hold 4 rows over 3 events, the middle
// one empty.
//
// The expected values are written out here, from the script's literals, and never computed by
// the library. The conversions use one oracle: HDF5 saturates when it narrows, so a value that
// is out of range reads as the nearest limit of the memory type. The probe column holds values
// that saturate differently in each of the ten types, so a wrong width or signedness anywhere
// in the type mapping shows.

#include <doctest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <map>

#include "core_test_util.hpp"

using namespace spine;
using namespace core_test;

namespace {

const char* const kFiles[] = {"edge_dtypes_v1.h5", "edge_dtypes_v2.h5"};
constexpr RowRange kEvents[] = {{0, 2}, {2, 2}, {2, 4}};

template <class T>
struct TypeOf;
#define TYPE_OF(T, S)                                   \
  template <>                                           \
  struct TypeOf<T> {                                    \
    static constexpr ScalarType value = ScalarType::S; \
  }
TYPE_OF(std::int8_t, Int8);
TYPE_OF(std::int16_t, Int16);
TYPE_OF(std::int32_t, Int32);
TYPE_OF(std::int64_t, Int64);
TYPE_OF(std::uint8_t, UInt8);
TYPE_OF(std::uint16_t, UInt16);
TYPE_OF(std::uint32_t, UInt32);
TYPE_OF(std::uint64_t, UInt64);
TYPE_OF(float, Float32);
TYPE_OF(double, Float64);
#undef TYPE_OF

/// The elements of a fixed or variable numeric buffer, as T.
template <class T>
std::vector<T> typed(const ColumnBuffer& b) {
  REQUIRE(b.type == TypeOf<T>::value);
  const std::size_t n = b.variable ? static_cast<std::size_t>(b.offsets.back()) : b.rows * b.count;
  REQUIRE(b.data.size() == n * sizeof(T));
  std::vector<T> out(n);
  if (n > 0) std::memcpy(out.data(), b.data.data(), b.data.size());
  return out;
}

std::vector<double> doubles(const ColumnBuffer& b) {
  const std::size_t n = b.variable ? static_cast<std::size_t>(b.offsets.back()) : b.rows * b.count;
  std::vector<double> out;
  for (std::size_t i = 0; i < n; ++i) out.push_back(element(b, i));
  return out;
}

struct Limits {
  ScalarType type;
  double low;
  double high;
};

/// The range of each memory type; floats are not clamped.
const Limits kTypes[] = {
    {ScalarType::Int8, -128.0, 127.0},
    {ScalarType::UInt8, 0.0, 255.0},
    {ScalarType::Int16, -32768.0, 32767.0},
    {ScalarType::UInt16, 0.0, 65535.0},
    {ScalarType::Int32, -2147483648.0, 2147483647.0},
    {ScalarType::UInt32, 0.0, 4294967295.0},
    {ScalarType::Int64, -9.2e18, 9.2e18},
    {ScalarType::UInt64, 0.0, 1.8e19},
    {ScalarType::Float32, -1e300, 1e300},
    {ScalarType::Float64, -1e300, 1e300},
};

std::vector<double> saturated(const std::vector<double>& values, const Limits& limits) {
  std::vector<double> out;
  for (double v : values) out.push_back(std::min(limits.high, std::max(limits.low, v)));
  return out;
}

struct Zoo {
  Hdf5Lock lock;
  std::unique_ptr<FileSource> file;
  ProductSource& zoo;
  explicit Zoo(const char* name) : file(open(name)), zoo(file->product("zoo")) {}
  ColumnBuffer read(const std::string& column, ScalarType type, RowRange rows) {
    return read_column(zoo, column, rows, type);
  }
  ColumnBuffer read(const std::string& column, ScalarType type) {
    return read(column, type, all_rows(zoo));
  }
};

template <class T>
void check_column(Zoo& z, const char* name, const std::vector<T>& want) {
  CAPTURE(name);
  const ColumnInfo* column = z.zoo.info().find(name);
  REQUIRE(column != nullptr);
  CHECK(column->type == TypeOf<T>::value);
  CHECK(column->count == 1);
  CHECK_FALSE(column->variable);
  CHECK(typed<T>(z.read(name, TypeOf<T>::value)) == want);
  // event by event: rows [0, 2), none, [2, 4)
  for (std::size_t e = 0; e < 3; ++e) {
    const RowRange rows = z.zoo.rows(e);
    CHECK(rows.size() == kEvents[e].size());
    if (rows.size() > 0) CHECK(rows.begin == kEvents[e].begin);  // an empty event has no position
    const std::vector<T> part(want.begin() + static_cast<std::ptrdiff_t>(kEvents[e].begin),
                              want.begin() + static_cast<std::ptrdiff_t>(kEvents[e].end));
    CHECK(typed<T>(z.read(name, TypeOf<T>::value, rows)) == part);
  }
}

}  // namespace

TEST_CASE("each numeric dtype is described and read as itself, V1 and V2") {
  for (const char* file : kFiles) {
    CAPTURE(file);
    Zoo z(file);
    constexpr auto i64 = std::numeric_limits<std::int64_t>::min();
    check_column<std::int8_t>(z, "c_i8", {-128, 127, 0, -1});
    check_column<std::int16_t>(z, "c_i16", {-32768, 32767, 0, -1});
    check_column<std::int32_t>(z, "c_i32", {-2147483647 - 1, 2147483647, 0, -1});
    check_column<std::int64_t>(z, "c_i64", {i64, std::numeric_limits<std::int64_t>::max(), 0, -1});
    check_column<std::uint8_t>(z, "c_u8", {0, 255, 1, 128});
    check_column<std::uint16_t>(z, "c_u16", {0, 65535, 1, 32768});
    check_column<std::uint32_t>(z, "c_u32", {0, 4294967295u, 1, 2147483648u});
    check_column<std::uint64_t>(z, "c_u64",
                                {0, std::numeric_limits<std::uint64_t>::max(), 1, 1ull << 63});
    check_column<float>(z, "c_f32", {-0.5f, 16777216.0f, 0.0f, 1.25f});
    check_column<double>(z, "c_f64", {-0.1, 1e300, 0.0, 2.5});
  }
}

TEST_CASE("a column converts to each of the ten memory types, saturating like HDF5") {
  const std::vector<double> probe = {-300, 200, 70000, 5000000000.0};
  // vprobe is [[-300, 200], [70000], [], [5e9]], vprobe2 is [[1], [2, 3], [], [4]]
  const std::vector<double> vprobe = {-300, 200, 70000, 5000000000.0};
  const std::vector<double> vprobe2 = {1, 2, 3, 4};
  const std::vector<std::uint64_t> vprobe_offsets = {0, 2, 3, 3, 4};
  const std::vector<std::uint64_t> vprobe2_offsets = {0, 1, 3, 3, 4};

  for (const char* file : kFiles) {
    CAPTURE(file);
    Zoo z(file);
    ProductSource& array = z.file->product("arr_probe");
    for (const Limits& limits : kTypes) {
      CAPTURE(to_string(limits.type));
      // a fixed column of the compound (V1) or of `fixed` (V2)
      const ColumnBuffer fixed = z.read("probe", limits.type);
      CHECK(fixed.type == limits.type);
      CHECK(doubles(fixed) == saturated(probe, limits));
      // a variable-length column: a VLEN member (V1) or a pool field (V2)
      const ColumnBuffer variable = z.read("vprobe", limits.type);
      CHECK(variable.offsets == vprobe_offsets);
      CHECK(doubles(variable) == saturated(vprobe, limits));
      // an array product
      const ColumnBuffer plain = read_column(array, "", all_rows(array), limits.type);
      CHECK(doubles(plain) == saturated(probe, limits));
    }
  }

  // Two columns in one read, each in a type of its own. In V2 they are two fields of the same
  // pool, which is read once per element type and cut up twice.
  for (const char* file : kFiles) {
    CAPTURE(file);
    Zoo z(file);
    const core::ProductInfo& info = z.zoo.info();
    for (const Limits& first : kTypes) {
      for (const Limits& second : {kTypes[2], kTypes[5], kTypes[9]}) {
        CAPTURE(to_string(first.type));
        CAPTURE(to_string(second.type));
        std::vector<ColumnBuffer> out;
        z.zoo.read(all_rows(z.zoo),
                   {{info.find("vprobe"), first.type}, {info.find("vprobe2"), second.type},
                    {info.find("probe"), first.type}},
                   out);
        REQUIRE(out.size() == 3);
        CHECK(out[0].type == first.type);
        CHECK(out[1].type == second.type);
        CHECK(out[0].offsets == vprobe_offsets);
        CHECK(out[1].offsets == vprobe2_offsets);
        CHECK(doubles(out[0]) == saturated(vprobe, first));
        CHECK(doubles(out[1]) == saturated(vprobe2, second));
        CHECK(doubles(out[2]) == saturated(probe, first));
      }
    }
  }
}

TEST_CASE("bool enums holding 2: Bool is 0 or 1 on every path, integers show the file") {
  const std::vector<double> raw = {2, 0, 1, 3};
  const std::vector<double> as_bool = {1, 0, 1, 1};
  for (const char* file : kFiles) {
    CAPTURE(file);
    Zoo z(file);
    ProductSource& array = z.file->product("arr_flag");
    const core::ProductInfo& info = z.zoo.info();

    for (const char* name : {"flag", "vflag"}) {
      CAPTURE(name);
      const ColumnInfo* column = info.find(name);
      REQUIRE(column != nullptr);
      CHECK(column->type == ScalarType::Bool);
      CHECK(column->variable == (std::string(name) == "vflag"));
    }
    CHECK(array.info().columns.at(0).type == ScalarType::Bool);

    // fixed column of the compound / `fixed`
    const ColumnBuffer flag = z.read("flag", ScalarType::Bool);
    CHECK(flag.type == ScalarType::Bool);
    CHECK(doubles(flag) == as_bool);
    CHECK(doubles(z.read("flag", ScalarType::UInt8)) == raw);
    CHECK(doubles(z.read("flag", ScalarType::Int32)) == raw);

    // variable-length column: [[2, 0], [], [3], [1]]
    const ColumnBuffer vflag = z.read("vflag", ScalarType::Bool);
    CHECK(vflag.offsets == std::vector<std::uint64_t>{0, 2, 2, 3, 4});
    CHECK(doubles(vflag) == std::vector<double>{1, 0, 1, 1});
    CHECK(doubles(z.read("vflag", ScalarType::Int16)) == std::vector<double>{2, 0, 3, 1});

    // array product
    CHECK(doubles(read_column(array, "", all_rows(array), ScalarType::Bool)) == as_bool);
    CHECK(doubles(read_column(array, "", all_rows(array), ScalarType::UInt8)) == raw);

    // an integer column is never read as Bool
    CHECK_THROWS_AS(z.read("c_u8", ScalarType::Bool), Error);
    CHECK_THROWS_AS(z.read("probe", ScalarType::Bool), Error);
  }
}

TEST_CASE("strings: in V2 a pool of bytes cut by offsets, in V1 variable-length strings") {
  for (const char* file : kFiles) {
    CAPTURE(file);
    Zoo z(file);
    const ColumnBuffer s = z.read("s", ScalarType::String);
    CHECK(s.strings == std::vector<std::string>{"alpha", "", "gamma \xc2\xb5", "d"});
    // events: rows [0, 2), none, [2, 4)
    CHECK(z.read("s", ScalarType::String, z.zoo.rows(2)).strings ==
          std::vector<std::string>{"gamma \xc2\xb5", "d"});
    CHECK(z.read("s", ScalarType::String, z.zoo.rows(1)).strings.empty());
  }
}

TEST_CASE("variable-length strings that were never written read as empty strings") {
  Hdf5Lock lock;
  auto file = open("edge_dtypes_v1.h5");
  // a compound dataset: the file holds NULL pointers where nothing was written
  ProductSource& objects = file->product("unset_objects");
  const ColumnBuffer label = read_column(objects, "label", all_rows(objects), ScalarType::String);
  CHECK(label.strings == std::vector<std::string>{"", ""});
  const ColumnBuffer ids = read_column(objects, "ids", all_rows(objects), ScalarType::Int32);
  CHECK(ids.offsets == std::vector<std::uint64_t>{0, 0, 0});
  // a plain dataset of strings
  ProductSource& labels = file->product("unset_labels");
  CHECK(read_column(labels, "", all_rows(labels), ScalarType::String).strings ==
        std::vector<std::string>{"", ""});
}

TEST_CASE("V2 string products: rows are events whether or not the product is flagged scalar") {
  Hdf5Lock lock;
  auto file = open("edge_v2_strings.h5");
  for (const char* name : {"flagged", "unflagged"}) {
    CAPTURE(name);
    ProductSource& product = file->product(name);
    CHECK(product.info().scalar);  // the reader treats it as one value per event either way
    CHECK(product.num_rows() == 3);
    for (std::uint64_t e = 0; e < 3; ++e) {
      CHECK(product.rows(e).begin == e);
      CHECK(product.rows(e).end == e + 1);
    }
    CHECK(read_column(product, "", all_rows(product), ScalarType::String).strings ==
          std::vector<std::string>{"alpha", "", " \xc2\xb5"});
    CHECK(read_column(product, "", product.rows(2), ScalarType::String).strings ==
          std::vector<std::string>{" \xc2\xb5"});
    // an empty string on its own: no bytes to read at all
    CHECK(read_column(product, "", product.rows(1), ScalarType::String).strings ==
          std::vector<std::string>{""});
  }
}

TEST_CASE("member arrays of several dimensions are one column; unreadable members are left out") {
  for (const char* file : kFiles) {
    CAPTURE(file);
    Zoo z(file);
    const core::ProductInfo& info = z.zoo.info();

    const ColumnInfo* wide = info.find("c_2d");  // float32[2][3] in every row
    REQUIRE(wide != nullptr);
    CHECK(wide->type == ScalarType::Float32);
    CHECK(wide->count == 6);
    CHECK_FALSE(wide->variable);
    CHECK(typed<float>(z.read("c_2d", ScalarType::Float32)) ==
          std::vector<float>{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 0, 0, 0, 0, 0, 0,
                             -1, -2, -3, -4, -5, -6});
    CHECK(typed<double>(z.read("c_2d", ScalarType::Float64, z.zoo.rows(2))) ==
          std::vector<double>{0, 0, 0, 0, 0, 0, -1, -2, -3, -4, -5, -6});

    // a fixed-length string and a nested compound are not columns; neither is an offsets helper
    CHECK(info.find("fixed_text") == nullptr);
    CHECK(info.find("nested") == nullptr);
    CHECK(info.find("_var_offsets_0") == nullptr);
    // the 11 numeric columns, flag, 3 enums, c_2d, and vprobe, vprobe2, vflag, s
    CHECK(info.columns.size() == 20);
    // and what is left reads
    CHECK(typed<std::int8_t>(z.read("c_i8", ScalarType::Int8)) ==
          std::vector<std::int8_t>{-128, 127, 0, -1});
  }
}

TEST_CASE("a request that does not fit the column says which column and why") {
  for (const char* file : kFiles) {
    CAPTURE(file);
    Zoo z(file);
    CHECK_THROWS_WITH_AS(
        z.read("probe", ScalarType::String),
        doctest::Contains("column 'probe' is numeric and cannot be read as string"), Error);
    CHECK_THROWS_WITH_AS(z.read("s", ScalarType::Int32),
                         doctest::Contains("column 's' is a string and cannot be read as int32"),
                         Error);
    CHECK_THROWS_WITH_AS(z.read("c_u8", ScalarType::Bool),
                         doctest::Contains("column 'c_u8' is not a bool; read it as uint8"), Error);
  }
}

TEST_CASE("enums on 16 and 32 bits report their base type and their labels") {
  using Labels = std::map<std::string, std::int64_t>;
  for (const char* file : kFiles) {
    CAPTURE(file);
    Zoo z(file);
    const core::ProductInfo& info = z.zoo.info();
    const auto labels = [&](const char* name) {
      Labels out;
      for (const auto& member : info.find(name)->enum_members) out[member.name] = member.value;
      return out;
    };
    CHECK(info.find("e_i16")->type == ScalarType::Int16);
    CHECK(labels("e_i16") == Labels{{"NEG", -2}, {"POS", 7}});
    CHECK(info.find("e_u16")->type == ScalarType::UInt16);
    CHECK(labels("e_u16") == Labels{{"LOW", 0}, {"HIGH", 60000}});
    CHECK(info.find("e_u32")->type == ScalarType::UInt32);
    CHECK(labels("e_u32") == Labels{{"ZERO", 0}, {"BIG", 4000000000}});

    // read as themselves and as int64: the value is the number, not the label
    CHECK(typed<std::int16_t>(z.read("e_i16", ScalarType::Int16)) ==
          std::vector<std::int16_t>{-2, 7, 7, -2});
    CHECK(typed<std::uint16_t>(z.read("e_u16", ScalarType::UInt16)) ==
          std::vector<std::uint16_t>{0, 60000, 60000, 0});
    CHECK(typed<std::uint32_t>(z.read("e_u32", ScalarType::UInt32)) ==
          std::vector<std::uint32_t>{0, 4000000000u, 4000000000u, 0});
    CHECK(doubles(z.read("e_i16", ScalarType::Int64)) == std::vector<double>{-2, 7, 7, -2});
    CHECK(doubles(z.read("e_u32", ScalarType::Int64)) ==
          std::vector<double>{0, 4000000000.0, 4000000000.0, 0});
  }
}
