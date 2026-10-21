// Smoke test on real SPINE output. Runs only when SPINE_READER_REAL_FILES names a directory;
// every *.h5 in it is read: all columns of reco_particles and reco_interactions, all events.
//
//   SPINE_READER_REAL_FILES=<directory> ./spine_reader_tests -tc='real*'

#include <doctest.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>

#include "core_test_util.hpp"

using namespace spine;
using namespace core_test;

namespace {

using Clock = std::chrono::steady_clock;

double milliseconds(Clock::time_point from) {
  return std::chrono::duration<double, std::milli>(Clock::now() - from).count();
}

/// Every column of `name`, event by event; returns the objects read and checks the buffers.
/// `ms` gains the time of a plain pass over the events (rows and one read per event), made
/// before the checks so that they are not in it.
std::uint64_t read_all_events(FileSource& file, const std::string& name, double& ms) {
  ProductSource& product = file.product(name);
  const std::vector<ColumnRequest> requests = natural_requests(product.info());
  std::uint64_t objects = 0;
  std::vector<ColumnBuffer> out;
  const auto start = Clock::now();
  for (std::uint64_t e = 0; e < file.info().num_events; ++e) {
    product.read(product.rows(e), requests, out);
  }
  ms += milliseconds(start);

  for (std::uint64_t e = 0; e < file.info().num_events; ++e) {
    const RowRange rows = product.rows(e);
    product.read(rows, requests, out);
    objects += rows.size();
    for (std::size_t i = 0; i < out.size(); ++i) {
      const ColumnInfo& column = *requests[i].column;
      CHECK_MESSAGE(out[i].rows == rows.size(), name << "." << column.name << " event " << e);
      if (column.type == ScalarType::String) {
        CHECK(out[i].strings.size() == rows.size());
      } else if (column.variable) {
        REQUIRE(out[i].offsets.size() == rows.size() + 1);
        CHECK(out[i].offsets.back() * scalar_size(column.type) == out[i].data.size());
        CHECK(std::is_sorted(out[i].offsets.begin(), out[i].offsets.end()));
      } else {
        CHECK(out[i].data.size() == rows.size() * column.count * scalar_size(column.type));
        if (column.type == ScalarType::Bool) {
          for (unsigned char byte : out[i].data) CHECK(byte <= 1);
        }
      }
    }
  }
  return objects;
}

}  // namespace

TEST_CASE("real files: all columns of reco_particles and reco_interactions, all events") {
  const char* directory = std::getenv("SPINE_READER_REAL_FILES");
  if (directory == nullptr || *directory == '\0') {
    MESSAGE("SPINE_READER_REAL_FILES is not set: skipped");
    return;
  }
  Hdf5Lock lock;
  std::size_t files = 0;
  for (const auto& entry : std::filesystem::directory_iterator(directory)) {
    if (entry.path().extension() != ".h5") continue;
    ++files;
    const std::string path = entry.path().string();
    CAPTURE(path);

    const auto opening = Clock::now();
    auto file = FileSource::open(path);
    const double open_ms = milliseconds(opening);
    const std::uint64_t events = file->info().num_events;
    REQUIRE(events > 0);

    double ms = 0;
    std::uint64_t objects = 0;
    for (const char* name : {"reco_particles", "reco_interactions"}) {
      if (!file->has_product(name)) continue;
      const auto first_use = Clock::now();
      ProductSource& product = file->product(name);
      const double product_ms = milliseconds(first_use);
      const std::uint64_t read = read_all_events(*file, name, ms);
      CHECK(read == product.num_rows());
      objects += read;
      std::fprintf(stderr, "  %s: %llu objects, %zu columns, first use %.2f ms\n", name,
                   static_cast<unsigned long long>(read), product.info().columns.size(),
                   product_ms);
    }
    const std::string layout(core::to_string(file->info().layout));
    std::fprintf(stderr,
                 "%s (%s, SPINE %s): %llu events, open %.2f ms, %llu objects read in %.2f ms = "
                 "%.3f ms/event\n",
                 entry.path().filename().string().c_str(), layout.c_str(),
                 file->info().spine_version.c_str(), static_cast<unsigned long long>(events),
                 open_ms, static_cast<unsigned long long>(objects), ms,
                 ms / static_cast<double>(events));
    CHECK(objects > 0);
  }
  CHECK(files > 0);
}
