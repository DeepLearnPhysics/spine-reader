// Golden tests: every fixture in tests/data/fixtures was written by SPINE's own writer and
// decoded independently with h5py (tools/fixtures/decode_raw.py). The core must reproduce the
// decoded file, product by product, column by column, row by row.

#include <doctest.h>
#include <json.hpp>

#include <cmath>
#include <fstream>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include <spine/detail/assign.hpp>
#include <spine/error.hpp>
#include <spine/reader.hpp>

#include "core/source.hpp"

namespace {

using nlohmann::json;
using spine::ScalarType;
using spine::detail::ColumnBuffer;

const std::vector<std::string> kCases = {
    "v1.4.0_v1",      "v1.4.0_v2", "v1.4.0_v1_lite", "v1.4.0_v2_lite",
    "v1.4.0_v2_fixed_only", "v2root", "v0.11.1_v1",
};

std::string fixture(const std::string& name, const std::string& extension) {
  return std::string(SPINE_READER_TEST_DATA) + "/fixtures/" + name + extension;
}

json expected(const std::string& name) {
  std::ifstream in(fixture(name, ".expected.json"));
  REQUIRE(in.good());
  return json::parse(in);
}

bool same_number(const json& want, double got) {
  if (want.is_string()) {
    const std::string s = want.get<std::string>();
    if (s == "nan") return std::isnan(got);
    if (s == "inf") return std::isinf(got) && got > 0;
    if (s == "-inf") return std::isinf(got) && got < 0;
    return false;
  }
  if (want.is_boolean()) return got == (want.get<bool>() ? 1.0 : 0.0);
  return want.get<double>() == got;
}

std::string describe(const json& want, const ColumnBuffer& column, std::size_t row) {
  std::string got;
  if (column.type == ScalarType::String) return want.dump() + " vs \"" + column.strings[row] + "\"";
  std::size_t begin = row * column.count, end = begin + column.count;
  if (column.variable) {
    begin = column.offsets[row];
    end = column.offsets[row + 1];
  }
  for (std::size_t i = begin; i < end; ++i) {
    got += (i > begin ? "," : "") + std::to_string(spine::detail::load<double>(column, i));
  }
  return want.dump() + " vs [" + got + "]";
}

// One row of one column against the decoded value.
bool same_value(const json& want, const ColumnBuffer& column, std::size_t row) {
  if (column.type == ScalarType::String) {
    return want.is_string() && want.get<std::string>() == column.strings[row];
  }
  if (column.variable) {
    const std::size_t begin = column.offsets[row];
    const std::size_t end = column.offsets[row + 1];
    if (!want.is_array() || want.size() != end - begin) return false;
    for (std::size_t i = begin; i < end; ++i) {
      if (!same_number(want[i - begin], spine::detail::load<double>(column, i))) return false;
    }
    return true;
  }
  if (column.count == 1 && !want.is_array()) {
    return same_number(want, spine::detail::load<double>(column, row));
  }
  if (!want.is_array() || want.size() != column.count) return false;
  for (std::size_t k = 0; k < column.count; ++k) {
    if (!same_number(want[k], spine::detail::load<double>(column, row * column.count + k))) {
      return false;
    }
  }
  return true;
}

void check_columns(const spine::core::ProductInfo& info, const json& want) {
  std::set<std::string> got_names, want_names;
  for (const auto& c : info.columns) got_names.insert(c.name);
  for (const auto& [name, unused] : want["columns"].items()) want_names.insert(name);
  CHECK(got_names == want_names);

  for (const auto& c : info.columns) {
    if (!want["columns"].contains(c.name)) continue;
    const json& w = want["columns"][c.name];
    INFO("column " << c.name);
    CHECK(std::string(spine::to_string(c.type)) == w["type"].get<std::string>());
    if (c.type != ScalarType::String) {
      CHECK(c.variable == w["variable"].get<bool>());
      CHECK(c.count == w["count"].get<std::size_t>());
    }
    // The decoder reports SPINE's {FALSE, TRUE} bool enum as a plain bool; the core also
    // lists its two labels.
    std::map<std::string, std::int64_t> labels;
    if (c.type != ScalarType::Bool) {
      for (const auto& m : c.enum_members) labels[m.name] = m.value;
    }
    std::map<std::string, std::int64_t> want_labels;
    if (!w["enum"].is_null()) want_labels = w["enum"].get<std::map<std::string, std::int64_t>>();
    CHECK(labels == want_labels);
  }
}

void check_values(spine::core::ProductSource& product, const json& want, std::uint64_t events) {
  const spine::core::ProductInfo& info = product.info();
  std::vector<spine::core::ColumnRequest> requests;
  for (const auto& c : info.columns) requests.push_back({&c, c.type});

  std::vector<ColumnBuffer> buffers;
  for (std::uint64_t e = 0; e < events; ++e) {
    INFO("event " << e);
    const json& rows = want["events"][e];
    const spine::core::RowRange range = product.rows(e);
    REQUIRE(range.size() == rows.size());
    product.read(range, requests, buffers);
    REQUIRE(buffers.size() == requests.size());
    for (std::size_t r = 0; r < rows.size(); ++r) {
      for (std::size_t k = 0; k < requests.size(); ++k) {
        const std::string& name = info.columns[k].name;
        const json& value = info.kind == spine::core::ProductKind::Objects ? rows[r][name] : rows[r];
        INFO("row " << r << ", column '" << name << "': " << describe(value, buffers[k], r));
        CHECK(same_value(value, buffers[k], r));
      }
    }
  }
}

}  // namespace

TEST_CASE("the core reproduces every golden fixture") {
  std::lock_guard<std::recursive_mutex> lock(spine::core::hdf5_mutex());
  for (const std::string& name : kCases) {
    INFO("fixture " << name);
    const json want = expected(name);
    auto file = spine::core::FileSource::open(fixture(name, ".h5"));
    const spine::core::FileInfo& info = file->info();
    CHECK(std::string(spine::core::to_string(info.layout)) == want["layout"].get<std::string>());
    CHECK(info.num_events == want["num_events"].get<std::uint64_t>());

    std::set<std::string> got_products(file->product_names().begin(),
                                       file->product_names().end());
    std::set<std::string> want_products;
    for (const auto& [key, unused] : want["products"].items()) want_products.insert(key);
    CHECK(got_products == want_products);

    for (const auto& [key, product_want] : want["products"].items()) {
      INFO("product " << key);
      REQUIRE(file->has_product(key));
      spine::core::ProductSource& product = file->product(key);
      const spine::core::ProductInfo& product_info = product.info();
      CHECK(std::string(spine::core::to_string(product_info.kind)) ==
            product_want["kind"].get<std::string>());
      const std::string want_class = product_want["class_name"].is_null()
                                         ? std::string()
                                         : product_want["class_name"].get<std::string>();
      CHECK(product_info.class_name == want_class);
      CHECK(product_info.scalar == product_want["scalar"].get<bool>());
      check_columns(product_info, product_want);
      check_values(product, product_want, info.num_events);
    }
  }
}

TEST_CASE("incomplete files are refused unless allowed") {
  std::lock_guard<std::recursive_mutex> lock(spine::core::hdf5_mutex());
  const std::string path = fixture("incomplete", ".h5");
  CHECK_THROWS_AS(spine::core::FileSource::open(path), spine::Error);
  spine::core::OpenOptions options;
  options.allow_incomplete = true;
  auto file = spine::core::FileSource::open(path, options);
  CHECK_FALSE(file->info().complete);
}

TEST_CASE("the reader fills model objects that agree with the golden fixtures") {
  for (const std::string& name : kCases) {
    INFO("fixture " << name);
    const json want = expected(name);
    spine::Reader reader(fixture(name, ".h5"));
    REQUIRE(reader.num_events() == want["num_events"].get<std::uint64_t>());

    const spine::SchemaReport& report = reader.schema_report();
    for (const spine::ProductReport& product : report.products) {
      INFO("product " << product.product);
      CHECK(product.mismatched.empty());
      // A model field is reported missing exactly when the file lacks that column.
      const json& columns = want["products"][product.product]["columns"];
      for (const std::string& field : product.missing) CHECK_FALSE(columns.contains(field));
    }

    for (std::uint64_t e = 0; e < reader.num_events(); ++e) {
      INFO("event " << e);
      const spine::Event event = reader.event(e);
      const json& particles = want["products"]["reco_particles"]["events"][e];
      REQUIRE(event.reco_particles().size() == particles.size());
      for (std::size_t i = 0; i < particles.size(); ++i) {
        const spine::RecoParticle& p = event.reco_particles()[i];
        const json& w = particles[i];
        CHECK(p.id == w["id"].get<std::int64_t>());
        CHECK(static_cast<std::int64_t>(p.shape) == w["shape"].get<std::int64_t>());
        CHECK(p.is_primary == w["is_primary"].get<bool>());
        for (std::size_t k = 0; k < 3; ++k) CHECK(same_number(w["start_point"][k], p.start_point[k]));
        if (w.contains("match_ids")) {
          CHECK(p.match_ids == w["match_ids"].get<std::vector<std::int32_t>>());
          CHECK(p.units == w["units"].get<std::string>());
        } else {
          CHECK_FALSE(p.has(spine::RecoParticle::Field::match_ids));
        }
        if (const spine::RecoInteraction* interaction = event.interaction_of(p)) {
          CHECK(interaction->id == p.interaction_id);
        }
      }
      if (want["products"].contains("run_info")) {
        const json& run = want["products"]["run_info"]["events"][e][0];
        REQUIRE(event.run_info() != nullptr);
        CHECK(event.run_info()->run == run["run"].get<std::int64_t>());
        CHECK(event.run_info()->event == run["event"].get<std::int64_t>());
        CHECK(reader.find(event.run_info()->run, event.run_info()->subrun,
                          event.run_info()->event) == e);
      }
    }
  }
}
