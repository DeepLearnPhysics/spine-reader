// Reading columns: values against what h5py decodes (expected.json), type conversion, enums,
// strings, variable-length columns, and what must be refused.

#include <doctest.h>

#include <cmath>
#include <functional>
#include <map>

#include "core_test_util.hpp"

using namespace spine;
using namespace core_test;
using spine::core::ProductKind;

namespace {

bool contains(const std::string& text, const std::string& part) {
  return text.find(part) != std::string::npos;
}

ProductKind kind_from_name(const std::string& name) {
  for (ProductKind kind : {ProductKind::Objects, ProductKind::Array, ProductKind::String,
                           ProductKind::List, ProductKind::MultiList}) {
    if (core::to_string(kind) == name) return kind;
  }
  throw std::runtime_error("unknown kind " + name);
}

/// Row `row` of a fixed-width numeric buffer, as a buffer of its own.
ColumnBuffer one_row(const ColumnBuffer& buffer, std::size_t row) {
  const std::size_t bytes = buffer.count * scalar_size(buffer.type);
  ColumnBuffer out = buffer;
  out.rows = 1;
  out.data.assign(buffer.data.begin() + row * bytes, buffer.data.begin() + (row + 1) * bytes);
  return out;
}

/// Reads every column of `product`, event by event, in one read each, and compares with `want`
/// (one entry of expected.json). With `subset` the file may lack some of the expected columns
/// (lite and litified files); it may never have others.
void check_product(ProductSource& product, const json& want, std::uint64_t events, bool subset) {
  const core::ProductInfo& info = product.info();
  CHECK(info.kind == kind_from_name(want["kind"]));
  CHECK(info.scalar == want["scalar"].get<bool>());
  if (want.contains("class_name")) CHECK(info.class_name == want["class_name"].get<std::string>());

  const json& columns = want["columns"];
  if (!subset) CHECK(info.columns.size() == columns.size());
  for (const ColumnInfo& column : info.columns) {
    REQUIRE_MESSAGE(columns.contains(column.name), "unexpected column '" << column.name << "'");
    const json& entry = columns[column.name];
    CHECK_MESSAGE(to_string(column.type) == entry["type"].get<std::string>(), column.name);
    CHECK_MESSAGE(column.count == entry["count"].get<std::size_t>(), column.name);
    CHECK_MESSAGE(column.variable == entry["variable"].get<bool>(), column.name);
    if (entry.contains("enum")) {
      std::map<std::string, std::int64_t> labels;
      for (const auto& member : column.enum_members) labels[member.name] = member.value;
      const auto want_labels = entry["enum"].get<std::map<std::string, std::int64_t>>();
      CHECK_MESSAGE(labels == want_labels, column.name);
    }
  }

  const std::vector<ColumnRequest> requests = natural_requests(info);
  for (std::uint64_t e = 0; e < events; ++e) {
    const RowRange rows = product.rows(e);
    std::vector<ColumnBuffer> out;
    product.read(rows, requests, out);
    REQUIRE(out.size() == requests.size());
    for (std::size_t i = 0; i < out.size(); ++i) {
      const std::string& name = info.columns[i].name;
      const json& entry = columns[name];
      CAPTURE(name);
      CAPTURE(e);
      CHECK(out[i].type == info.columns[i].type);
      if (entry.contains("data")) {
        const std::string why = difference(out[i], entry["data"][e]);
        CHECK_MESSAGE(why.empty(), "column '" << name << "', event " << e << ": " << why);
      } else {  // a big array: shape, sum, first and last row
        const json& shape = entry["shapes"][e];
        CHECK(out[i].rows == shape[0].get<std::size_t>());
        CHECK(out[i].count == entry["count"].get<std::size_t>());
        double sum = 0;
        for (std::size_t k = 0; k < out[i].rows * out[i].count; ++k) sum += element(out[i], k);
        const double want_sum = entry["sums"][e].get<double>();
        CHECK(std::fabs(sum - want_sum) <= 1e-9 * std::max(1.0, std::fabs(want_sum)));
        if (out[i].rows > 0) {
          const json first = json::array({entry["first"][e]});
          const json last = json::array({entry["last"][e]});
          const std::string why_first = difference(one_row(out[i], 0), first);
          const std::string why_last = difference(one_row(out[i], out[i].rows - 1), last);
          CHECK_MESSAGE(why_first.empty(), why_first);
          CHECK_MESSAGE(why_last.empty(), why_last);
        }
      }
    }
  }
}

void check_file(const std::string& file_name, const std::string& key, bool subset = false) {
  CAPTURE(file_name);
  auto file = open(file_name);
  for (const auto& [name, want] : expected()[key].items()) {
    CAPTURE(name);
    if (subset && !file->has_product(name)) continue;
    if (want.contains("unsupported")) continue;
    REQUIRE(file->has_product(name));
    check_product(file->product(name), want, file->info().num_events, subset);
  }
}

}  // namespace

TEST_CASE("every column of every product, event by event, agrees with h5py: V1") {
  Hdf5Lock lock;
  check_file("spine_v1_events.h5", "events");
}

TEST_CASE("every column of every product, event by event, agrees with h5py: V2") {
  Hdf5Lock lock;
  check_file("spine_v2_events.h5", "events");
}

TEST_CASE("exotic products agree with h5py: scalars, strings, 2-D arrays, in both layouts") {
  Hdf5Lock lock;
  check_file("spine_v1_exotic.h5", "exotic");
  check_file("spine_v2_exotic.h5", "exotic");
}

TEST_CASE("lite and litified files agree with the full ones on the columns they have") {
  Hdf5Lock lock;
  check_file("spine_v1_events_lite.h5", "events", true);
  check_file("spine_v2_events_lite.h5", "events", true);
  check_file("spine_v2_events_litified.h5", "events", true);
}

TEST_CASE("the two V2 layouts of SPINE v0.16.3 and v1.4.0 read the same way") {
  Hdf5Lock lock;
  check_file("mini_v2_v0.16.3.h5", "mini_v0.16.3");
  check_file("mini_v2_v1.4.0.h5", "mini_v1.4.0");
}

TEST_CASE("a read of several events at once is the reads of each event, joined") {
  Hdf5Lock lock;
  for (const char* file_name : {"spine_v1_events.h5", "spine_v2_events.h5"}) {
    CAPTURE(file_name);
    auto file = open(file_name);
    for (const char* name : {"reco_particles", "truth_interactions", "flashes"}) {
      CAPTURE(name);
      ProductSource& product = file->product(name);
      const auto requests = natural_requests(product.info());
      std::vector<ColumnBuffer> whole;
      product.read(all_rows(product), requests, whole);
      // rows 1..end, to start in the middle of the pools
      std::vector<ColumnBuffer> tail;
      product.read({1, product.num_rows()}, requests, tail);
      for (std::size_t i = 0; i < requests.size(); ++i) {
        const ColumnInfo& column = product.info().columns[i];
        CAPTURE(column.name);
        // the tail is the whole without its first row
        ColumnBuffer cut = whole[i];
        cut.rows -= 1;
        if (column.type == ScalarType::String) {
          cut.strings.erase(cut.strings.begin());
        } else if (column.variable) {
          const std::size_t size = scalar_size(cut.type);
          const auto skipped = static_cast<std::ptrdiff_t>(cut.offsets[1] * size);
          cut.data.erase(cut.data.begin(), cut.data.begin() + skipped);
          const std::uint64_t shift = cut.offsets[1];
          cut.offsets.erase(cut.offsets.begin());
          for (auto& offset : cut.offsets) offset -= shift;
        } else {
          cut.data.erase(cut.data.begin(), cut.data.begin() + cut.count * scalar_size(cut.type));
        }
        CHECK(same_buffer(cut, tail[i]));
      }
    }
  }
}

TEST_CASE("numbers convert to the requested type, enums to integers") {
  Hdf5Lock lock;
  for (const char* file_name : {"spine_v1_events.h5", "spine_v2_events.h5"}) {
    CAPTURE(file_name);
    auto file = open(file_name);
    ProductSource& product = file->product("reco_particles");
    const RowRange rows = all_rows(product);

    const ColumnBuffer id64 = read_column(product, "id", rows, ScalarType::Int64);
    const ColumnBuffer id32 = read_column(product, "id", rows, ScalarType::Int32);
    const ColumnBuffer id_f = read_column(product, "id", rows, ScalarType::Float64);
    const ColumnBuffer id_u8 = read_column(product, "id", rows, ScalarType::UInt8);
    REQUIRE(id64.rows == 14);
    CHECK(id32.type == ScalarType::Int32);
    CHECK(id32.data.size() == 14 * 4);
    for (std::size_t i = 0; i < 14; ++i) {
      CHECK(element(id32, i) == element(id64, i));
      CHECK(element(id_f, i) == element(id64, i));
      CHECK(element(id_u8, i) == element(id64, i));
    }

    // float32 columns widen exactly; variable ones too
    const ColumnBuffer point32 = read_column(product, "start_point", rows, ScalarType::Float32);
    const ColumnBuffer point64 = read_column(product, "start_point", rows, ScalarType::Float64);
    CHECK(point64.count == 3);
    for (std::size_t i = 0; i < 14 * 3; ++i) CHECK(element(point64, i) == element(point32, i));
    const ColumnBuffer ids32 = read_column(product, "match_ids", rows, ScalarType::Int32);
    const ColumnBuffer ids64 = read_column(product, "match_ids", rows, ScalarType::Int64);
    CHECK(ids64.offsets == ids32.offsets);
    for (std::size_t i = 0; i < ids32.offsets.back(); ++i) {
      CHECK(element(ids64, i) == element(ids32, i));
    }

    // an enum reads as an integer of any width; Bool as Bool or as an integer
    const ColumnBuffer pid64 = read_column(product, "pid", rows, ScalarType::Int64);
    const ColumnBuffer pid16 = read_column(product, "pid", rows, ScalarType::Int16);
    const ColumnBuffer pid_f = read_column(product, "pid", rows, ScalarType::Float32);
    for (std::size_t i = 0; i < 14; ++i) {
      CHECK(element(pid16, i) == element(pid64, i));
      CHECK(element(pid_f, i) == element(pid64, i));
    }
    const ColumnBuffer flag_bool = read_column(product, "is_contained", rows, ScalarType::Bool);
    const ColumnBuffer flag_i32 = read_column(product, "is_contained", rows, ScalarType::Int32);
    CHECK(flag_bool.type == ScalarType::Bool);
    CHECK(flag_bool.data.size() == 14);
    for (std::size_t i = 0; i < 14; ++i) {
      CHECK(flag_bool.data[i] <= 1);
      CHECK(element(flag_i32, i) == flag_bool.data[i]);
    }
  }
}

TEST_CASE("requests the column cannot answer are refused") {
  Hdf5Lock lock;
  for (const char* file_name : {"spine_v1_events.h5", "spine_v2_events.h5"}) {
    CAPTURE(file_name);
    auto file = open(file_name);
    ProductSource& product = file->product("reco_particles");
    const RowRange rows = all_rows(product);
    std::vector<ColumnBuffer> out;

    // strings are not numbers, and numbers are not strings
    CHECK_THROWS_AS(read_column(product, "units", rows, ScalarType::Int32), Error);
    CHECK_THROWS_AS(read_column(product, "id", rows, ScalarType::String), Error);
    CHECK_THROWS_AS(read_column(product, "match_ids", rows, ScalarType::String), Error);
    // Bool only for bools
    CHECK_THROWS_AS(read_column(product, "id", rows, ScalarType::Bool), Error);
    // a column of another product
    ProductSource& other = file->product("flashes");
    const ColumnInfo& foreign = other.info().columns.at(1);
    CHECK_THROWS_AS(product.read(rows, {{&foreign, foreign.type}}, out), Error);
    CHECK_THROWS_AS(product.read(rows, {{nullptr, ScalarType::Int32}}, out), Error);
    // the same column twice in one read
    const ColumnInfo& id = *product.info().find("id");
    const std::vector<ColumnRequest> twice = {{&id, ScalarType::Int64}, {&id, ScalarType::Int64}};
    CHECK_THROWS_AS(product.read(rows, twice, out), Error);
    try {
      read_column(product, "units", rows, ScalarType::Int32);
    } catch (const Error& error) {
      CHECK(contains(error.what(), data_path(file_name)));
      CHECK(contains(error.what(), "reco_particles"));
      CHECK(contains(error.what(), "units"));
    }
    // after failures, the product still reads
    CHECK(read_column(product, "id", rows, ScalarType::Int64).rows == 14);
  }
}

TEST_CASE("buffers are reused between reads without stale content") {
  Hdf5Lock lock;
  for (const char* file_name : {"spine_v1_events.h5", "spine_v2_events.h5"}) {
    CAPTURE(file_name);
    auto file = open(file_name);
    ProductSource& product = file->product("reco_particles");
    const auto requests = natural_requests(product.info());
    std::vector<ColumnBuffer> reused;
    product.read(product.rows(0), requests, reused);  // 5 rows
    product.read(product.rows(1), requests, reused);  // 4 rows, into the same buffers
    std::vector<ColumnBuffer> fresh;
    product.read(product.rows(1), requests, fresh);
    REQUIRE(reused.size() == fresh.size());
    for (std::size_t i = 0; i < fresh.size(); ++i) CHECK(same_buffer(reused[i], fresh[i]));
    // fewer requests than before
    product.read(product.rows(1), {requests[2], requests[8]}, reused);
    CHECK(reused.size() == 2);
    // an empty request list is fine
    product.read(product.rows(1), {}, reused);
    CHECK(reused.empty());
  }
}

TEST_CASE("lists of arrays are described but not read") {
  Hdf5Lock lock;
  for (const char* file_name : {"spine_v1_exotic.h5", "spine_v2_exotic.h5"}) {
    CAPTURE(file_name);
    auto file = open(file_name);
    for (const char* name : {"lists_same_width", "lists_mixed_width"}) {
      CAPTURE(name);
      ProductSource& product = file->product(name);
      std::vector<ColumnBuffer> out;
      const std::vector<std::function<void()>> calls = {
          [&] { product.rows(0); },
          [&] { product.num_rows(); },
          [&] { product.read({0, 1}, natural_requests(product.info()), out); }};
      for (const auto& call : calls) {
        try {
          call();
          FAIL("lists are not supported");
        } catch (const Error& error) {
          CHECK(contains(error.what(), "not supported"));
          CHECK(contains(error.what(), data_path(file_name)));
          CHECK(contains(error.what(), name));
        }
      }
    }
  }
}

TEST_CASE("corrupt V2 offsets are caught, not followed") {
  Hdf5Lock lock;
  auto file = open("edge_v2_corrupt.h5");

  // an event whose rows run past the data
  ProductSource& shorter = file->product("short");
  CHECK(shorter.rows(1).end == 5);
  CHECK_THROWS_AS(read_column(shorter, "", shorter.rows(1), ScalarType::Int64), Error);

  // the offsets column does not fit its pool
  try {
    file->product("wrong_width");
    FAIL("a mismatching _var_offsets_ must throw");
  } catch (const Error& error) {
    CHECK(contains(error.what(), data_path("edge_v2_corrupt.h5")));
    CHECK(contains(error.what(), "wrong_width"));
    CHECK(contains(error.what(), "_var_offsets_0"));
  }

  // offsets that point past the end of their pool
  ProductSource& past = file->product("past_pool");
  CHECK_THROWS_WITH_AS(read_column(past, "a", past.rows(0), ScalarType::Int32),
                       doctest::Contains("corrupt"), Error);
  // ...while the fixed columns of the same product are fine
  CHECK(read_column(past, "id", past.rows(0), ScalarType::Int64).rows == 1);
}
