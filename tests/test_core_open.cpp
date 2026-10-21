// FileSource::open: layout detection, file information, product listing, rejected files.

#include <doctest.h>

#include <algorithm>
#include <set>

#include "core_test_util.hpp"

using namespace spine;
using namespace core_test;
using spine::core::Layout;
using spine::core::ProductKind;

namespace {

bool contains(const std::string& text, const std::string& part) {
  return text.find(part) != std::string::npos;
}

/// The message of the spine::Error that opening `name` raises; fails the test if there is none.
std::string open_error(const std::string& name, const core::OpenOptions& options = {}) {
  try {
    open(name, options);
  } catch (const Error& error) {
    return error.what();
  }
  FAIL("opening " << name << " should have thrown");
  return {};
}

const std::vector<std::string> kSampleProducts = {
    "crthits",           "depositions", "file_entry_index", "file_index",
    "flashes",           "index",       "input_data",       "meta",
    "points",            "reco_interactions", "reco_particles", "run_info",
    "source_file_entry_index", "source_file_index", "trigger", "truth_interactions",
    "truth_particles"};

}  // namespace

TEST_CASE("layout detection: V1, V2 under /products, V2 at the file root") {
  Hdf5Lock lock;

  auto v1 = open("spine_v1_events.h5");
  CHECK(v1->info().layout == Layout::V1);
  CHECK(v1->info().format_version == 1);
  CHECK(v1->info().spine_version == "1.4.0");
  CHECK(v1->info().complete);
  CHECK(v1->info().num_events == 4);
  CHECK_FALSE(v1->info().cfg.empty());
  CHECK(v1->info().path == data_path("spine_v1_events.h5"));

  auto v2 = open("spine_v2_events.h5");
  CHECK(v2->info().layout == Layout::V2Products);
  CHECK(v2->info().format_version == 2);
  CHECK(v2->info().spine_version == "1.4.0");
  CHECK(v2->info().num_events == 4);

  // SPINE v0.16.x: format_version 2, product groups directly under "/"
  auto root = open("mini_v2_v0.16.3.h5");
  CHECK(root->info().layout == Layout::V2Root);
  CHECK(root->info().format_version == 2);
  CHECK(root->info().spine_version == "0.16.3");
  CHECK(root->info().num_events == 2);

  auto modern = open("mini_v2_v1.4.0.h5");
  CHECK(modern->info().layout == Layout::V2Products);
  CHECK(modern->info().num_events == 2);

  // The litified, lite and exotic variants keep their layouts.
  CHECK(open("spine_v2_events_litified.h5")->info().layout == Layout::V2Products);
  CHECK(open("spine_v1_events_lite.h5")->info().layout == Layout::V1);
  CHECK(open("spine_v2_exotic.h5")->info().layout == Layout::V2Products);
  CHECK(open("spine_v1_exotic.h5")->info().num_events == 3);
}

TEST_CASE("a V1 file may have a product called 'products'") {
  Hdf5Lock lock;
  auto file = open("edge_v1_products.h5");
  CHECK(file->info().layout == Layout::V1);  // format_version says 1: the dataset is data
  CHECK(file->product_names() == std::vector<std::string>{"products"});
  CHECK(file->product("products").num_rows() == 3);
}

TEST_CASE("product listing") {
  Hdf5Lock lock;

  auto v2 = open("spine_v2_events.h5");
  // The groups of the products root, in name order (the file does not track creation order).
  CHECK(v2->product_names() == kSampleProducts);

  // V1: the members of /events, in the order of that compound, which is arbitrary.
  auto v1 = open("spine_v1_events.h5");
  const std::vector<std::string>& names = v1->product_names();
  CHECK(std::set<std::string>(names.begin(), names.end()) ==
        std::set<std::string>(kSampleProducts.begin(), kSampleProducts.end()));
  CHECK(names.size() == kSampleProducts.size());
  CHECK(names.front() == "input_data");  // first member of /events, see h5dump -H
  CHECK(names.back() == "truth_particles");

  CHECK(v1->has_product("reco_particles"));
  CHECK(v2->has_product("reco_particles"));
  CHECK_FALSE(v1->has_product("events"));  // not a product
  CHECK_FALSE(v2->has_product("info"));
  CHECK_FALSE(v2->has_product("no_such_product"));

  // Root-level V2: only the groups that carry a `kind`, not events or info.
  auto root = open("mini_v2_v0.16.3.h5");
  CHECK(root->product_names() ==
        std::vector<std::string>{"index", "points", "reco_particles", "run_info"});

  auto litified = open("spine_v2_events_litified.h5");
  CHECK_FALSE(litified->has_product("points"));
  CHECK(litified->has_product("reco_particles"));
}

TEST_CASE("products open on first use and are owned by the file") {
  Hdf5Lock lock;
  auto file = open("spine_v2_events.h5");
  ProductSource& first = file->product("reco_particles");
  CHECK(&file->product("reco_particles") == &first);
  CHECK(first.info().name == "reco_particles");
  CHECK(first.info().kind == ProductKind::Objects);
  CHECK(first.info().class_name == "RecoParticle");

  try {
    file->product("no_such_product");
    FAIL("an absent product must throw");
  } catch (const Error& error) {
    CHECK(contains(error.what(), data_path("spine_v2_events.h5")));
    CHECK(contains(error.what(), "no_such_product"));
  }
}

TEST_CASE("files that are not complete SPINE outputs are rejected") {
  Hdf5Lock lock;

  const std::string not_spine = open_error("edge_not_spine.h5");
  CHECK(contains(not_spine, data_path("edge_not_spine.h5")));
  CHECK(contains(not_spine, "/info"));

  const std::string cache = open_error("edge_cache.h5");
  CHECK(contains(cache, data_path("edge_cache.h5")));
  CHECK(contains(cache, "cache"));
  // A cache shard stays rejected when incomplete files are allowed.
  CHECK(contains(open_error("edge_cache.h5", {true}), "cache"));

  const std::string version = open_error("edge_bad_version.h5");
  CHECK(contains(version, data_path("edge_bad_version.h5")));
  CHECK(contains(version, "format_version 3"));

  for (const char* name : {"edge_incomplete_v1.h5", "edge_incomplete_v2.h5"}) {
    const std::string incomplete = open_error(name);
    CHECK(contains(incomplete, data_path(name)));
    CHECK(contains(incomplete, "incomplete"));
  }
}

TEST_CASE("incomplete files open when the caller says so") {
  Hdf5Lock lock;
  auto v2 = open("edge_incomplete_v2.h5", {true});
  CHECK_FALSE(v2->info().complete);
  CHECK(v2->info().layout == Layout::V2Products);
  CHECK(v2->info().num_events == 3);
  ProductSource& index = v2->product("index");
  CHECK(read_column(index, "", all_rows(index), ScalarType::Int64).rows == 3);

  auto v1 = open("edge_incomplete_v1.h5", {true});
  CHECK_FALSE(v1->info().complete);
  CHECK(v1->info().layout == Layout::V1);
}

TEST_CASE("a missing file and a file that is not HDF5 give errors with the path") {
  Hdf5Lock lock;
  const std::string missing = data_path("does_not_exist.h5");
  try {
    FileSource::open(missing);
    FAIL("missing file");
  } catch (const Error& error) {
    CHECK(contains(error.what(), missing));
    CHECK(contains(error.what(), "H5Fopen"));
  }
  // expected.json is a perfectly good file that is not HDF5
  const std::string json_path = data_path("expected.json");
  try {
    FileSource::open(json_path);
    FAIL("not an HDF5 file");
  } catch (const Error& error) {
    CHECK(contains(error.what(), json_path));
  }
}
