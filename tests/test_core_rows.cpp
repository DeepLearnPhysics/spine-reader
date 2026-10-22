// Row ranges per event, in both layouts: empty events, scalar products, range checks.

#include <doctest.h>

#include "core_test_util.hpp"

using namespace spine;
using namespace core_test;

namespace {

bool contains(const std::string& text, const std::string& part) {
  return text.find(part) != std::string::npos;
}

std::vector<std::uint64_t> sizes(ProductSource& product, std::uint64_t events) {
  std::vector<std::uint64_t> out;
  for (std::uint64_t e = 0; e < events; ++e) out.push_back(product.rows(e).size());
  return out;
}

}  // namespace

TEST_CASE("rows per event match what h5py decodes, V1 and V2") {
  Hdf5Lock lock;
  for (const char* file_name : {"spine_v1_events.h5", "spine_v2_events.h5"}) {
    CAPTURE(file_name);
    auto file = open(file_name);
    for (const auto& [name, product] : expected()["events"].items()) {
      CAPTURE(name);
      const auto want = product["rows_per_event"].get<std::vector<std::uint64_t>>();
      CHECK(sizes(file->product(name), file->info().num_events) == want);
    }
  }
  for (const char* file_name : {"spine_v1_exotic.h5", "spine_v2_exotic.h5"}) {
    CAPTURE(file_name);
    auto file = open(file_name);
    for (const auto& [name, product] : expected()["exotic"].items()) {
      CAPTURE(name);
      if (product.contains("unsupported")) continue;  // lists have no rows to speak of
      const auto want = product["rows_per_event"].get<std::vector<std::uint64_t>>();
      CHECK(sizes(file->product(name), file->info().num_events) == want);
    }
  }
}

TEST_CASE("the object products of the sample have the documented counts, event 2 empty") {
  Hdf5Lock lock;
  for (const char* file_name : {"spine_v1_events.h5", "spine_v2_events.h5"}) {
    CAPTURE(file_name);
    auto file = open(file_name);
    using Counts = std::vector<std::uint64_t>;
    CHECK(sizes(file->product("reco_particles"), 4) == Counts{5, 4, 0, 5});
    CHECK(sizes(file->product("reco_interactions"), 4) == Counts{2, 1, 0, 3});
    CHECK(sizes(file->product("truth_particles"), 4) == Counts{6, 4, 0, 6});
    CHECK(sizes(file->product("truth_interactions"), 4) == Counts{3, 1, 0, 3});
    CHECK(sizes(file->product("flashes"), 4) == Counts{0, 2, 2, 1});
    CHECK(sizes(file->product("crthits"), 4) == Counts{0, 2, 1, 0});
    CHECK(sizes(file->product("run_info"), 4) == Counts{1, 1, 1, 1});
    CHECK(file->product("reco_particles").num_rows() == 14);
    CHECK(file->product("truth_particles").num_rows() == 16);
  }
}

TEST_CASE("the ranges of the events tile the product, empty events included") {
  Hdf5Lock lock;
  for (const char* file_name : {"spine_v1_events.h5", "spine_v2_events.h5"}) {
    CAPTURE(file_name);
    auto file = open(file_name);
    for (const char* name :
         {"reco_particles", "truth_interactions", "flashes", "points", "run_info"}) {
      CAPTURE(name);
      ProductSource& product = file->product(name);
      std::uint64_t next = 0;
      for (std::uint64_t e = 0; e < 4; ++e) {
        const RowRange rows = product.rows(e);
        CHECK(rows.end >= rows.begin);
        // An empty event has no position to speak of: V1 says 0, V2 says where it would be.
        if (rows.size() > 0) CHECK(rows.begin == next);
        next += rows.size();
      }
      CHECK(next == product.num_rows());
    }
  }
}

TEST_CASE("an empty event is an empty range that reads as zero rows") {
  Hdf5Lock lock;
  for (const char* file_name : {"spine_v1_events.h5", "spine_v2_events.h5"}) {
    CAPTURE(file_name);
    auto file = open(file_name);
    ProductSource& product = file->product("reco_particles");
    const RowRange rows = product.rows(2);
    CHECK(rows.size() == 0);
    std::vector<ColumnBuffer> out;
    product.read(rows, natural_requests(product.info()), out);
    REQUIRE(out.size() == 55);
    for (std::size_t i = 0; i < out.size(); ++i) {
      const ColumnInfo& column = product.info().columns[i];
      CAPTURE(column.name);
      CHECK(out[i].rows == 0);
      CHECK(out[i].type == column.type);
      CHECK(out[i].data.empty());
      CHECK(out[i].strings.empty());
      if (column.variable && column.type != ScalarType::String) {
        CHECK(out[i].offsets == std::vector<std::uint64_t>{0});
      }
    }
    // and an empty read of the first rows of a non-empty event
    product.read({3, 3}, natural_requests(product.info()), out);
    CHECK(out[0].rows == 0);
  }
}

TEST_CASE("scalar products have exactly one row per event, row e for event e") {
  Hdf5Lock lock;
  for (const char* file_name : {"spine_v1_events.h5", "spine_v2_events.h5"}) {
    CAPTURE(file_name);
    auto file = open(file_name);
    for (const char* name :
         {"run_info", "meta", "trigger", "index", "file_index", "source_file_entry_index"}) {
      CAPTURE(name);
      ProductSource& product = file->product(name);
      REQUIRE(product.info().scalar);
      for (std::uint64_t e = 0; e < 4; ++e) {
        const RowRange rows = product.rows(e);
        CHECK(rows.begin == e);
        CHECK(rows.end == e + 1);
      }
    }
  }
}

TEST_CASE("an event or a row range outside the file is an error naming file and product") {
  Hdf5Lock lock;
  for (const char* file_name : {"spine_v1_events.h5", "spine_v2_events.h5"}) {
    CAPTURE(file_name);
    auto file = open(file_name);
    for (const char* name : {"reco_particles", "run_info", "points"}) {
      CAPTURE(name);
      ProductSource& product = file->product(name);
      try {
        product.rows(4);
        FAIL("event 4 does not exist");
      } catch (const Error& error) {
        CHECK(contains(error.what(), data_path(file_name)));
        CHECK(contains(error.what(), name));
        CHECK(contains(error.what(), "event 4"));
      }
      CHECK_THROWS_AS(product.rows(1000000), Error);

      std::vector<ColumnBuffer> out;
      const auto requests = natural_requests(product.info());
      const std::uint64_t total = product.num_rows();
      CHECK_THROWS_AS(product.read({0, total + 1}, requests, out), Error);
      CHECK_THROWS_AS(product.read({total + 5, total + 6}, requests, out), Error);
      CHECK_THROWS_AS(product.read({2, 1}, requests, out), Error);
      try {
        product.read({0, total + 1}, requests, out);
        FAIL("rows past the end of the product");
      } catch (const Error& error) {
        CHECK(contains(error.what(), data_path(file_name)));
        CHECK(contains(error.what(), name));
        CHECK(contains(error.what(), "are outside the " + std::to_string(total) + " rows"));
      }
    }
  }
}

TEST_CASE("rows of the old root-level V2 layout") {
  Hdf5Lock lock;
  for (const char* file_name : {"mini_v2_v0.16.3.h5", "mini_v2_v1.4.0.h5"}) {
    CAPTURE(file_name);
    auto file = open(file_name);
    // one particle in event 0, none in event 1; two points in event 0, none in event 1
    ProductSource& particles = file->product("reco_particles");
    CHECK(sizes(particles, 2) == std::vector<std::uint64_t>{1, 0});
    CHECK(particles.rows(0).begin == 0);
    CHECK(sizes(file->product("points"), 2) == std::vector<std::uint64_t>{2, 0});
    CHECK(sizes(file->product("run_info"), 2) == std::vector<std::uint64_t>{1, 1});
  }
}

TEST_CASE("V1 events with a contiguous reference, an empty one, and references that are not rows") {
  Hdf5Lock lock;
  auto file = open("edge_v1_regions.h5");

  ProductSource& ok = file->product("ok");
  CHECK(ok.rows(0).begin == 0);
  CHECK(ok.rows(0).end == 3);
  CHECK(ok.rows(1).size() == 0);
  CHECK(ok.rows(2).begin == 3);
  CHECK(ok.rows(2).end == 10);

  // event 1 selects rows 3, 4 and 8: not one block
  ProductSource& split = file->product("split");
  CHECK(split.rows(0).size() == 3);
  try {
    split.rows(1);
    FAIL("a split selection must throw");
  } catch (const Error& error) {
    CHECK(contains(error.what(), data_path("edge_v1_regions.h5")));
    CHECK(contains(error.what(), "split"));
    CHECK(contains(error.what(), "contiguous"));
  }
  CHECK(split.rows(2).begin == 5);

  // event 1 selects only the first column of two
  ProductSource& partial = file->product("partial_width");
  CHECK(partial.rows(0).size() == 3);
  CHECK_THROWS_WITH_AS(partial.rows(1), doctest::Contains("selects part of the width of the rows"),
                       Error);
  CHECK(partial.rows(2).size() == 5);
}

TEST_CASE("a V1 scalar product has one row per event, and says so when the file disagrees") {
  Hdf5Lock lock;
  auto file = open("edge_v1_scalar.h5");
  ProductSource& product = file->product("scal");
  REQUIRE(product.info().scalar);
  CHECK(product.rows(0).begin == 0);
  CHECK(product.rows(0).end == 1);
  // event 1 references no row at all
  try {
    product.rows(1);
    FAIL("a scalar product needs a row");
  } catch (const Error& error) {
    CHECK(contains(error.what(), data_path("edge_v1_scalar.h5")));
    CHECK(contains(error.what(), "scalar product has no row for event 1"));
  }
  // event 2 references two
  try {
    product.rows(2);
    FAIL("a scalar product has one row");
  } catch (const Error& error) {
    CHECK(contains(error.what(), "scalar product has 2 rows for event 2"));
  }
}

TEST_CASE("a V1 reference past the last event is an error that says which event") {
  Hdf5Lock lock;
  auto file = open("spine_v1_events.h5");
  for (const char* name : {"reco_particles", "run_info"}) {
    CAPTURE(name);
    ProductSource& product = file->product(name);
    CHECK_NOTHROW(product.rows(3));
    try {
      product.rows(4);
      FAIL("there are 4 events");
    } catch (const Error& error) {
      CHECK(contains(error.what(), "event 4 is out of range, the file has 4 events"));
    }
  }
  auto v2 = open("spine_v2_events.h5");
  try {
    v2->product("reco_particles").rows(4);
    FAIL("there are 4 events");
  } catch (const Error& error) {
    CHECK(contains(error.what(), "event 4 is out of range, the file has 4 events"));
  }
}
