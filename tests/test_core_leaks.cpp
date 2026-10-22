// HDF5 allocates the memory of variable-length sequences and strings while it reads; the caller
// must give it back (H5Treclaim, wrapped in h5::Reclaim). A forgotten reclaim changes no value,
// so no value test can see it. These tests read the same thing many times and look at how much
// heap is in use afterwards, with glibc's mallinfo. A read that forgot to reclaim leaks a few
// hundred bytes to some kilobytes per call, so the hundreds to thousands of reads below leave
// hundreds of kilobytes to megabytes behind, against the 128 kB of noise allowed.
//
// Where mallinfo does not exist the tests are skipped, and doctest says so.

#include <doctest.h>

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

#if defined(__GLIBC__)
#include <malloc.h>
#define SPINE_READER_HAVE_MALLINFO 1
#else
#define SPINE_READER_HAVE_MALLINFO 0
#endif

#include "core_test_util.hpp"
#include "h5/error.hpp"
#include "h5/handle.hpp"
#include "h5/util.hpp"

using namespace spine;
using namespace core_test;

namespace {

#if SPINE_READER_HAVE_MALLINFO
/// Bytes of heap in use (malloc'ed and not freed): arenas plus mmapped chunks.
std::size_t heap_in_use() {
#if defined(__GLIBC_PREREQ) && __GLIBC_PREREQ(2, 33)
  const struct mallinfo2 info = mallinfo2();
  return info.uordblks + info.hblkhd;
#else
  const struct mallinfo info = mallinfo();  // int fields: fine for the sizes here
  return static_cast<std::size_t>(static_cast<unsigned>(info.uordblks)) +
         static_cast<std::size_t>(static_cast<unsigned>(info.hblkhd));
#endif
}
#else
std::size_t heap_in_use() { return 0; }
#endif

/// Growth of the heap over `iterations` calls of `call`, after `warm_up` calls that let HDF5 and
/// the allocator reach their steady state.
std::ptrdiff_t heap_growth(const std::function<void()>& call, int warm_up, int iterations) {
  for (int i = 0; i < warm_up; ++i) call();
  const std::size_t before = heap_in_use();
  for (int i = 0; i < iterations; ++i) call();
  const std::size_t after = heap_in_use();
  return static_cast<std::ptrdiff_t>(after) - static_cast<std::ptrdiff_t>(before);
}

constexpr std::ptrdiff_t kAllowed = 128 * 1024;  // noise: caches and the like (up to 64 kB seen)
constexpr int kWarmUp = 100;
constexpr bool kSkip = !SPINE_READER_HAVE_MALLINFO;

}  // namespace

TEST_CASE("V1 reads give back the memory of VLEN columns and strings" * doctest::skip(kSkip)) {
  Hdf5Lock lock;
  auto file = open("spine_v1_events.h5");

  // a compound with VLEN members and strings: all columns, event 0 (5 particles)
  ProductSource& particles = file->product("reco_particles");
  const auto requests = natural_requests(particles.info());
  const RowRange rows = particles.rows(0);
  REQUIRE(rows.size() == 5);
  std::vector<ColumnBuffer> out;
  const std::ptrdiff_t compound =
      heap_growth([&] { particles.read(rows, requests, out); }, kWarmUp, 300);
  CHECK_MESSAGE(compound < kAllowed, "leaked " << compound << " bytes in 300 reads");

  // a dataset of variable-length strings (a V1 string product)
  auto edge = open("edge_v1_columns.h5");
  ProductSource& labels = edge->product("labels");
  const auto label_requests = natural_requests(labels.info());
  std::vector<ColumnBuffer> label_out;
  const std::ptrdiff_t strings =
      heap_growth([&] { labels.read(all_rows(labels), label_requests, label_out); }, kWarmUp, 4000);
  CHECK_MESSAGE(strings < kAllowed, "leaked " << strings << " bytes in 4000 reads");

  // the same compound product, through the strings-and-VLEN columns of the edge file
  ProductSource& objs = edge->product("objs");
  const auto objs_requests = natural_requests(objs.info());
  std::vector<ColumnBuffer> objs_out;
  const std::ptrdiff_t edge_objs =
      heap_growth([&] { objs.read(all_rows(objs), objs_requests, objs_out); }, kWarmUp, 600);
  CHECK_MESSAGE(edge_objs < kAllowed, "leaked " << edge_objs << " bytes in 600 reads");
}

TEST_CASE("a variable-length string attribute is read without leaking" * doctest::skip(kSkip)) {
  Hdf5Lock lock;
  h5::ErrorSilencer silence;
  h5::File file = h5::open_file(data_path("edge_attributes.h5"));
  std::string text;
  const std::ptrdiff_t grown = heap_growth(
      [&] { text = h5::read_string_attribute(file.get(), "vlen_utf8"); }, kWarmUp, 10000);
  CHECK(text == "h\xc3\xa9llo \xc2\xb5");
  CHECK_MESSAGE(grown < kAllowed, "leaked " << grown << " bytes in 10000 reads");
}

TEST_CASE("V2 reads, which allocate nothing in HDF5, do not leak either" * doctest::skip(kSkip)) {
  Hdf5Lock lock;
  auto file = open("spine_v2_events.h5");
  ProductSource& particles = file->product("reco_particles");
  const auto requests = natural_requests(particles.info());
  const RowRange rows = particles.rows(0);
  std::vector<ColumnBuffer> out;
  const std::ptrdiff_t grown =
      heap_growth([&] { particles.read(rows, requests, out); }, kWarmUp, 150);
  CHECK_MESSAGE(grown < kAllowed, "leaked " << grown << " bytes in 150 reads");
}
