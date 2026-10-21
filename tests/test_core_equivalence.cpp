// The key test of the core: a V1 file and the V2 file with identical logical content give
// identical buffers, column by column and event by event.
//
// Legitimate differences between the layouts, none of which shows in a ColumnBuffer:
//  - the order of ProductInfo::columns (V1: the file's compound; V2: fixed columns, then pool
//    fields) and of FileSource::product_names (V1: members of /events; V2: name order);
//  - the position of an empty event: V1 has no position (rows(e).begin is 0), V2 knows it.
// Everything else, in particular type, count, variable, enum labels, values and offsets, is
// the same.

#include <doctest.h>

#include <map>

#include "core_test_util.hpp"

using namespace spine;
using namespace core_test;
using spine::core::ProductKind;

namespace {

struct Pair {
  const char* v1;
  const char* v2;
};

const Pair kPairs[] = {
    {"spine_v1_events.h5", "spine_v2_events.h5"},
    {"spine_v1_events_lite.h5", "spine_v2_events_lite.h5"},
    {"spine_v1_exotic.h5", "spine_v2_exotic.h5"},
};

std::map<std::string, std::int64_t> labels(const ColumnInfo& column) {
  std::map<std::string, std::int64_t> out;
  for (const auto& member : column.enum_members) out[member.name] = member.value;
  return out;
}

bool readable(const core::ProductInfo& info) {
  return info.kind != ProductKind::List && info.kind != ProductKind::MultiList;
}

}  // namespace

TEST_CASE("V1 and V2 files list the same products with the same properties") {
  Hdf5Lock lock;
  for (const Pair& pair : kPairs) {
    CAPTURE(pair.v1);
    auto v1 = open(pair.v1);
    auto v2 = open(pair.v2);
    CHECK(v1->info().num_events == v2->info().num_events);
    CHECK(v1->info().spine_version == v2->info().spine_version);
    std::vector<std::string> a = v1->product_names();
    std::vector<std::string> b = v2->product_names();
    std::sort(a.begin(), a.end());
    std::sort(b.begin(), b.end());
    REQUIRE(a == b);
    for (const std::string& name : a) {
      CAPTURE(name);
      const core::ProductInfo& x = v1->product(name).info();
      const core::ProductInfo& y = v2->product(name).info();
      CHECK(x.kind == y.kind);
      CHECK(x.class_name == y.class_name);
      CHECK(x.scalar == y.scalar);
      REQUIRE(x.columns.size() == y.columns.size());
      for (const ColumnInfo& column : x.columns) {
        const ColumnInfo* other = y.find(column.name);
        REQUIRE_MESSAGE(other != nullptr, column.name);
        CHECK_MESSAGE(column.type == other->type, column.name);
        CHECK_MESSAGE(column.count == other->count, column.name);
        CHECK_MESSAGE(column.variable == other->variable, column.name);
        CHECK_MESSAGE(labels(column) == labels(*other), column.name);
      }
    }
  }
}

TEST_CASE("every column of every product, every event: V1 buffers equal V2 buffers") {
  Hdf5Lock lock;
  std::size_t compared = 0;
  for (const Pair& pair : kPairs) {
    CAPTURE(pair.v1);
    auto v1 = open(pair.v1);
    auto v2 = open(pair.v2);
    for (const std::string& name : v1->product_names()) {
      CAPTURE(name);
      ProductSource& a = v1->product(name);
      ProductSource& b = v2->product(name);
      if (!readable(a.info())) continue;
      CHECK(a.num_rows() == b.num_rows());

      // Three readings of every column: as the file has it, as float64, as int32. One read per
      // event and reading takes all columns (the V2 request is in V1's column order, by name).
      for (int variant = 0; variant < 3; ++variant) {
        CAPTURE(variant);
        std::vector<ColumnRequest> requests_a;
        std::vector<ColumnRequest> requests_b;
        for (const ColumnInfo& column : a.info().columns) {
          ScalarType type = column.type;
          if (type != ScalarType::String) {
            if (variant == 1) type = ScalarType::Float64;
            if (variant == 2 && type != ScalarType::Bool) type = ScalarType::Int32;
          }
          requests_a.push_back({&column, type});
          requests_b.push_back({b.info().find(column.name), type});
          REQUIRE(requests_b.back().column != nullptr);
        }
        for (std::uint64_t e = 0; e < v1->info().num_events; ++e) {
          CAPTURE(e);
          const RowRange ra = a.rows(e);
          const RowRange rb = b.rows(e);
          REQUIRE(ra.size() == rb.size());
          if (ra.size() > 0) CHECK(ra.begin == rb.begin);
          std::vector<ColumnBuffer> out_a;
          std::vector<ColumnBuffer> out_b;
          a.read(ra, requests_a, out_a);
          b.read(rb, requests_b, out_b);
          REQUIRE(out_a.size() == out_b.size());
          for (std::size_t i = 0; i < out_a.size(); ++i) {
            CHECK_MESSAGE(same_buffer(out_a[i], out_b[i]),
                          requests_a[i].column->name << " as " << to_string(requests_a[i].type));
            ++compared;
          }
        }
      }
    }
  }
  MESSAGE("compared " << compared << " (column, event, type) buffers");
  CHECK(compared > 3000);  // the loops above are not empty
}

TEST_CASE("the whole product read at once is the same in V1 and V2, and as single columns") {
  Hdf5Lock lock;
  std::size_t alone = 0;  // columns also read alone
  for (const Pair& pair : kPairs) {
    CAPTURE(pair.v1);
    auto v1 = open(pair.v1);
    auto v2 = open(pair.v2);
    for (const std::string& name : v1->product_names()) {
      CAPTURE(name);
      ProductSource& a = v1->product(name);
      ProductSource& b = v2->product(name);
      if (!readable(a.info())) continue;
      const bool check_alone = name == "reco_particles" || name == "truth_interactions" ||
                               name == "flashes" || name == "label";

      // the requests in V2's column order differ from V1's: match them by name
      std::vector<ColumnRequest> requests_a;
      std::vector<ColumnRequest> requests_b;
      for (const ColumnInfo& column : a.info().columns) {
        requests_a.push_back({&column, column.type});
        requests_b.push_back({b.info().find(column.name), column.type});
      }
      std::vector<ColumnBuffer> out_a;
      std::vector<ColumnBuffer> out_b;
      a.read(all_rows(a), requests_a, out_a);
      b.read(all_rows(b), requests_b, out_b);
      REQUIRE(out_a.size() == out_b.size());
      for (std::size_t i = 0; i < out_a.size(); ++i) {
        CAPTURE(requests_a[i].column->name);
        CHECK(same_buffer(out_a[i], out_b[i]));
        // one column alone is the same as that column within the full request (checked for the
        // products that have fixed, variable and string columns, which is where it can differ)
        if (!check_alone) continue;
        const ColumnBuffer alone_a =
            read_column(a, *requests_a[i].column, all_rows(a), requests_a[i].type);
        const ColumnBuffer alone_b =
            read_column(b, *requests_b[i].column, all_rows(b), requests_b[i].type);
        CHECK(same_buffer(alone_a, out_a[i]));
        CHECK(same_buffer(alone_b, out_b[i]));
        ++alone;
      }
    }
  }
  CHECK(alone > 100);  // the loops above are not empty
}

TEST_CASE("reading events one by one equals reading the whole product, V1 and V2") {
  Hdf5Lock lock;
  std::size_t compared = 0;
  for (const char* file_name : {"spine_v1_events.h5", "spine_v2_events.h5"}) {
    CAPTURE(file_name);
    auto file = open(file_name);
    for (const char* name :
         {"reco_particles", "truth_particles", "reco_interactions", "flashes", "crthits"}) {
      CAPTURE(name);
      ProductSource& product = file->product(name);
      const std::vector<ColumnRequest> requests = natural_requests(product.info());
      std::vector<ColumnBuffer> whole;  // every column, all events at once
      product.read(all_rows(product), requests, whole);
      REQUIRE(whole.size() == requests.size());

      std::vector<std::size_t> row(requests.size(), 0);  // rows of `whole` consumed so far
      std::vector<ColumnBuffer> part;                    // every column, one event
      for (std::uint64_t e = 0; e < file->info().num_events; ++e) {
        CAPTURE(e);
        const RowRange rows = product.rows(e);
        product.read(rows, requests, part);
        REQUIRE(part.size() == requests.size());
        for (std::size_t i = 0; i < requests.size(); ++i) {
          const ColumnInfo& column = *requests[i].column;
          CAPTURE(column.name);
          CHECK(part[i].rows == rows.size());
          if (column.type == ScalarType::String) {
            for (std::size_t r = 0; r < part[i].rows; ++r) {
              CHECK(part[i].strings[r] == whole[i].strings[row[i] + r]);
            }
          } else if (column.variable) {
            for (std::size_t r = 0; r < part[i].rows; ++r) {
              const auto n = part[i].offsets[r + 1] - part[i].offsets[r];
              REQUIRE(whole[i].offsets[row[i] + r + 1] - whole[i].offsets[row[i] + r] == n);
              const std::size_t size = scalar_size(column.type);
              CHECK(same_bytes(part[i].data.data() + part[i].offsets[r] * size,
                               whole[i].data.data() + whole[i].offsets[row[i] + r] * size,
                               static_cast<std::size_t>(n) * size));
            }
          } else {
            const std::size_t row_bytes = part[i].count * scalar_size(column.type);
            CHECK(same_bytes(part[i].data.data(), whole[i].data.data() + row[i] * row_bytes,
                             part[i].rows * row_bytes));
          }
          row[i] += part[i].rows;
          ++compared;
        }
      }
      for (std::size_t i = 0; i < requests.size(); ++i) CHECK(row[i] == whole[i].rows);
    }
  }
  CHECK(compared > 1000);  // the loops above are not empty
}
