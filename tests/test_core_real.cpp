// Real SPINE output. Needs SPINE_READER_REAL_FILES to name a directory; every *.h5 in it is read:
// all columns of reco_particles and reco_interactions, all events, then the same files through
// the Reader, where the relations SPINE guarantees must hold.
//
// Without the directory the test is skipped, and says so: doctest reports it as skipped, and
// the ctest entry spine_reader_real_files (which runs it with --no-skip) reports "Skipped".
//
//   SPINE_READER_REAL_FILES=<directory> ./spine_reader_tests -tc='real files*'
//   ctest -R real_files    (SPINE_READER_REAL_FILES set, or cmake -DSPINE_READER_REAL_FILES=...)

#include <doctest.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>

#include <algorithm>
#include <set>

#include <spine/reader.hpp>

#include "core_test_util.hpp"

using namespace spine;
using namespace core_test;

namespace {

using Clock = std::chrono::steady_clock;

std::string real_files_directory() {
  const char* directory = std::getenv("SPINE_READER_REAL_FILES");
  return directory != nullptr ? directory : "";
}

std::vector<std::string> real_files(const std::string& directory) {
  std::vector<std::string> paths;
  for (const auto& entry : std::filesystem::directory_iterator(directory)) {
    if (entry.path().extension() == ".h5") paths.push_back(entry.path().string());
  }
  std::sort(paths.begin(), paths.end());
  return paths;
}

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

TEST_CASE("real files: all columns of reco_particles and reco_interactions, all events" *
          doctest::skip(real_files_directory().empty())) {
  const std::string directory = real_files_directory();
  if (directory.empty()) {  // only reached with --no-skip
    std::fprintf(stderr, "SPINE_READER_REAL_FILES is not set: the real-file test did not run\n");
    return;
  }
  Hdf5Lock lock;
  std::size_t files = 0;
  for (const std::string& path : real_files(directory)) {
    ++files;
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
                 std::filesystem::path(path).filename().string().c_str(), layout.c_str(),
                 file->info().spine_version.c_str(), static_cast<unsigned long long>(events),
                 open_ms, static_cast<unsigned long long>(objects), ms,
                 ms / static_cast<double>(events));
    CHECK(objects > 0);
  }
  CHECK(files > 0);
}

TEST_CASE("real files: through the Reader, the relations SPINE guarantees hold" *
          doctest::skip(real_files_directory().empty())) {
  const std::string directory = real_files_directory();
  if (directory.empty()) {  // only reached with --no-skip
    std::fprintf(stderr, "SPINE_READER_REAL_FILES is not set: the real-file test did not run\n");
    return;
  }
  const std::vector<std::string> paths = real_files(directory);
  REQUIRE_FALSE(paths.empty());
  std::size_t particles_checked = 0;
  std::size_t matches_checked = 0;
  for (const std::string& path : paths) {
    CAPTURE(path);
    Reader reader(path);
    REQUIRE(reader.num_events() > 0);
    const SchemaReport& report = reader.schema_report();
    for (const ProductReport& product : report.products) {
      INFO("product " << product.product << "\n" << report.summary());
      CHECK(product.mismatched.empty());  // the file and the model agree on every shared column
      CHECK(product.missing.empty());
    }

    for (std::uint64_t e = 0; e < reader.num_events(); ++e) {
      CAPTURE(e);
      const Event event = reader.event(e);
      REQUIRE(event.run_info() != nullptr);
      // find() gives back an event with the same triple (the first, if a triple repeats)
      const auto found = reader.find(event.run_info()->run, event.run_info()->subrun,
                                     event.run_info()->event);
      REQUIRE(found.has_value());
      CHECK(*found <= e);
      CHECK(reader.event(*found).run_info()->event == event.run_info()->event);

      std::set<std::int64_t> ids;
      for (const RecoParticle& particle : event.reco_particles()) {
        CHECK(ids.insert(particle.id).second);  // ids are unique within an event
        const RecoInteraction* interaction = event.interaction_of(particle);
        if (particle.interaction_id < 0) {
          CHECK(interaction == nullptr);
          continue;
        }
        REQUIRE(interaction != nullptr);
        CHECK(interaction->id == particle.interaction_id);
        // the interaction lists the particle that points to it
        const std::vector<const RecoParticle*> siblings = event.particles_of(*interaction);
        CHECK(std::count(siblings.begin(), siblings.end(), &particle) == 1);
        ++particles_checked;
      }
      for (const RecoInteraction& interaction : event.reco_interactions()) {
        CHECK(event.particles_of(interaction).size() == interaction.particle_ids.size());
        for (const RecoParticle* member : event.particles_of(interaction)) {
          CHECK(member->interaction_id == interaction.id);
        }
      }
      for (const RecoParticle& particle : event.reco_particles()) {
        const auto matches = event.matches(particle);
        // SPINE lists matches best first
        for (std::size_t k = 1; k < matches.size(); ++k) {
          CHECK(matches[k - 1].overlap >= matches[k].overlap);
        }
        matches_checked += matches.size();
      }
    }
  }
  CHECK(particles_checked > 0);
  MESSAGE(particles_checked << " particles and " << matches_checked << " matches checked");
}
