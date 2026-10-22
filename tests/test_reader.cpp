// Reader and Event on files with several files, ids that are not list positions, dangling
// references, duplicate (run, subrun, event) triples and bound-with-differences products.
//
// tests/data/reader/*.h5 are SPINE-written (make_reader_cases.py); their content is spelled out
// in the comments below and in that script. Three files, read in this order:
//
//   A  V1  3 events   global 0..2   (7,2,101) (7,2,102) (8,1,5)
//   B  V2  2 events   global 3..4   (8,1,5)   (9,0,1)       <- B0 repeats A2's triple
//   C  V1  1 event    global 5      (10,3,77), ImageMeta3D recorded under its old name "Meta"
//
// In event A0 the reco particles are, by position, ids 11 4 8 3 20, the reco interactions ids
// 1 2 0, the truth particles ids 2 0 1 and the truth interactions ids 1 0: no id is its own
// position except one (particle 3 sits at position 3), several are larger than the list, and
// id 1 / 2 / 0 of the interactions form a cycle of positions.

#include <doctest.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <limits>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include <functional>
#include <mutex>

#include <unistd.h>

#include <spine/detail/column_buffer.hpp>
#include <spine/error.hpp>
#include <spine/reader.hpp>

#include "core/source.hpp"

#ifndef SPINE_READER_TEST_DATA
#error "SPINE_READER_TEST_DATA is set by tests/CMakeLists.txt"
#endif

using namespace spine;

namespace {

std::string reader_file(const std::string& name) {
  return std::string(SPINE_READER_TEST_DATA) + "/reader/" + name;
}
std::string core_file(const std::string& name) {
  return std::string(SPINE_READER_TEST_DATA) + "/core/" + name;
}
std::string fixture_file(const std::string& name) {
  return std::string(SPINE_READER_TEST_DATA) + "/fixtures/" + name;
}

const std::vector<std::string>& three_files() {
  static const std::vector<std::string> paths = {reader_file("reader_a_v1.h5"),
                                                 reader_file("reader_b_v2.h5"),
                                                 reader_file("reader_c_v1_meta.h5")};
  return paths;
}

bool contains(const std::string& text, const std::string& part) {
  return text.find(part) != std::string::npos;
}

template <class T>
std::vector<std::int64_t> ids_of(const std::vector<const T*>& objects) {
  std::vector<std::int64_t> ids;
  for (const T* object : objects) ids.push_back(object->id);
  return ids;
}

template <class T>
std::vector<std::int64_t> match_ids_of(const std::vector<Match<T>>& matches) {
  std::vector<std::int64_t> ids;
  for (const Match<T>& match : matches) ids.push_back(match.object->id);
  return ids;
}

template <class T>
std::vector<float> overlaps_of(const std::vector<Match<T>>& matches) {
  std::vector<float> overlaps;
  for (const Match<T>& match : matches) overlaps.push_back(match.overlap);
  return overlaps;
}

using Ids = std::vector<std::int64_t>;
using Overlaps = std::vector<float>;
constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();

/// Same values, with NaN equal to NaN.
bool same_overlaps(const Overlaps& a, const Overlaps& b) {
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (std::isnan(a[i]) != std::isnan(b[i])) return false;
    if (!std::isnan(a[i]) && a[i] != b[i]) return false;
  }
  return true;
}

}  // namespace

// ---------------------------------------------------------------------------------------------
// Several files as one sequence of events
// ---------------------------------------------------------------------------------------------

TEST_CASE("a Reader over three files numbers the events across them") {
  Reader reader(three_files());
  CHECK(reader.num_files() == 3);
  CHECK(reader.num_events() == 6);

  // global index -> (file, entry in file, run_info)
  struct Where {
    std::size_t file;
    std::uint64_t entry;
    std::int64_t run, subrun, event;
  };
  const Where where[] = {{0, 0, 7, 2, 101}, {0, 1, 7, 2, 102}, {0, 2, 8, 1, 5},
                         {1, 0, 8, 1, 5},   {1, 1, 9, 0, 1},   {2, 0, 10, 3, 77}};
  for (std::uint64_t i = 0; i < 6; ++i) {
    CAPTURE(i);
    const Event event = reader.event(i);
    CHECK(event.index() == i);
    CHECK(event.file_index() == where[i].file);
    CHECK(event.entry() == where[i].entry);
    REQUIRE(event.run_info() != nullptr);
    CHECK(event.run_info()->run == where[i].run);
    CHECK(event.run_info()->subrun == where[i].subrun);
    CHECK(event.run_info()->event == where[i].event);
  }

  // the layouts are mixed: V1, V2, V1
  CHECK(reader.schema_report(0).layout == "V1");
  CHECK(reader.schema_report(1).layout == "V2Products");
  CHECK(reader.schema_report(2).layout == "V1");
  CHECK(reader.schema_report(1).format_version == 2);
  CHECK(reader.schema_report(0).path == three_files()[0]);
  CHECK(reader.schema_report(2).path == three_files()[2]);
}

TEST_CASE("objects come from the file and entry the global index maps to") {
  Reader reader(three_files());
  // the sizes differ everywhere, so reading the wrong entry or file shows
  const std::size_t reco_particles[] = {5, 2, 0, 1, 2, 2};
  const std::size_t reco_interactions[] = {3, 1, 0, 1, 2, 1};
  const std::size_t truth_particles[] = {3, 0, 0, 0, 2, 0};
  const std::size_t truth_interactions[] = {2, 0, 0, 0, 0, 0};
  for (std::uint64_t i = 0; i < 6; ++i) {
    CAPTURE(i);
    const Event event = reader.event(i);
    CHECK(event.reco_particles().size() == reco_particles[i]);
    CHECK(event.reco_interactions().size() == reco_interactions[i]);
    CHECK(event.truth_particles().size() == truth_particles[i]);
    CHECK(event.truth_interactions().size() == truth_interactions[i]);
  }
  // the first particle of each event
  CHECK(reader.event(0).reco_particles()[0].id == 11);
  CHECK(reader.event(1).reco_particles()[0].id == 0);
  CHECK(reader.event(3).reco_particles()[0].id == 0);
  CHECK(reader.event(4).reco_particles()[0].id == 1);  // B's second event: ids 1, 0
  CHECK(reader.event(5).reco_particles()[0].id == 2);  // C: ids 2, 0

  // the file boundaries: last of A, first of B, last of B, only of C
  CHECK(reader.event(2).file_index() == 0);
  CHECK(reader.event(3).file_index() == 1);
  CHECK(reader.event(4).file_index() == 1);
  CHECK(reader.event(5).file_index() == 2);
}

TEST_CASE("a file without events in the list changes no index but the file numbers") {
  const std::string empty = core_file("edge_v2_empty.h5");
  Reader reader({three_files()[0], empty, three_files()[1], empty});
  CHECK(reader.num_files() == 4);
  CHECK(reader.num_events() == 5);
  // A's three events, then B's two, whichever empty files sit between
  const std::uint64_t file_of[] = {0, 0, 0, 2, 2};
  const std::uint64_t entry_of[] = {0, 1, 2, 0, 1};
  for (std::uint64_t i = 0; i < 5; ++i) {
    CAPTURE(i);
    const Event event = reader.event(i);
    CHECK(event.file_index() == file_of[i]);
    CHECK(event.entry() == entry_of[i]);
  }
  CHECK(reader.event(3).run_info()->event == 5);
  CHECK(reader.event(4).run_info()->run == 9);
  CHECK_THROWS_AS(reader.event(5), Error);
  CHECK(reader.find(9, 0, 1) == 4);
  CHECK(reader.find(8, 1, 5) == 2);

  // the empty file first, or alone
  Reader first({empty, three_files()[1]});
  CHECK(first.num_events() == 2);
  CHECK(first.event(0).file_index() == 1);
  CHECK(first.event(1).run_info()->run == 9);
  Reader alone(empty);
  CHECK(alone.num_events() == 0);
  CHECK_THROWS_AS(alone.event(0), Error);
  CHECK_FALSE(alone.find(1, 2, 3).has_value());
}

TEST_CASE("an event index past the last file is an error that says how many events there are") {
  Reader reader(three_files());
  try {
    reader.event(6);
    FAIL("event 6 does not exist");
  } catch (const Error& error) {
    CHECK(std::string(error.what()) == "spine-reader: event 6 is out of range (6 events)");
  }
  CHECK_THROWS_AS(reader.event(1000000), Error);
  CHECK_THROWS_AS(reader.event(std::numeric_limits<std::uint64_t>::max()), Error);
  CHECK(reader.event(5).index() == 5);  // and the last one is fine

  Reader single(three_files()[1]);
  CHECK(single.num_files() == 1);
  CHECK(single.num_events() == 2);
  CHECK_THROWS_AS(single.event(2), Error);
}

TEST_CASE("find() locates a triple across files; the first of a repeated triple wins") {
  Reader reader(three_files());
  CHECK(reader.find(7, 2, 101) == 0);
  CHECK(reader.find(7, 2, 102) == 1);
  CHECK(reader.find(9, 0, 1) == 4);   // in the second file
  CHECK(reader.find(10, 3, 77) == 5);  // in the third
  // (8, 1, 5) is event 2 of A and event 0 of B (global 3): the first one
  CHECK(reader.find(8, 1, 5) == 2);
  // misses, and the same numbers in another order
  CHECK_FALSE(reader.find(1, 2, 3).has_value());
  CHECK_FALSE(reader.find(7, 101, 2).has_value());
  CHECK_FALSE(reader.find(7, 2, 103).has_value());
  CHECK_FALSE(reader.find(0, 0, 0).has_value());
  // asking again gives the same answers
  CHECK(reader.find(10, 3, 77) == 5);

  // a file on its own: its events number from 0
  Reader b(three_files()[1]);
  CHECK(b.find(8, 1, 5) == 0);
  CHECK(b.find(9, 0, 1) == 1);
  CHECK_FALSE(b.find(7, 2, 101).has_value());
}

TEST_CASE("find() works whichever products the Reader was asked to read") {
  ReaderOptions options;
  options.products = {"reco_particles"};
  Reader reader(three_files(), options);
  CHECK(reader.find(9, 0, 1) == 4);
  CHECK(reader.event(4).run_info() == nullptr);  // run_info was not asked for
}

TEST_CASE("find() says what is wrong when a file has no run_info") {
  // edge_v1_columns.h5 has no SPINE products at all, only 'objs' and 'labels'
  Reader reader(core_file("edge_v1_columns.h5"));
  try {
    reader.find(1, 2, 3);
    FAIL("no run_info");
  } catch (const Error& error) {
    CHECK(contains(error.what(), "edge_v1_columns.h5"));
    CHECK(contains(error.what(), "no run_info"));
  }
}

// ---------------------------------------------------------------------------------------------
// Event relations, with ids that are not positions
// ---------------------------------------------------------------------------------------------

TEST_CASE("interaction_of finds the interaction by id, not by position") {
  Reader reader(three_files());
  const Event e = reader.event(0);
  const auto& particles = e.reco_particles();
  const auto& interactions = e.reco_interactions();
  REQUIRE(particles.size() == 5);
  REQUIRE(interactions.size() == 3);
  // positions of the interactions: id 1 at 0, id 2 at 1, id 0 at 2
  CHECK(e.interaction_of(particles[0]) == &interactions[0]);  // particle 11 -> interaction 1
  CHECK(e.interaction_of(particles[1]) == &interactions[1]);  // particle 4  -> interaction 2
  CHECK(e.interaction_of(particles[2]) == &interactions[2]);  // particle 8  -> interaction 0
  CHECK(e.interaction_of(particles[3]) == &interactions[0]);  // particle 3  -> interaction 1
  CHECK(e.interaction_of(particles[4]) == nullptr);           // particle 20 -> -1: none

  // truth side: interaction ids 1 and 0 sit at positions 0 and 1
  const auto& truth = e.truth_particles();
  const auto& truth_interactions = e.truth_interactions();
  REQUIRE(truth.size() == 3);
  REQUIRE(truth_interactions.size() == 2);
  CHECK(e.interaction_of(truth[0]) == &truth_interactions[0]);  // particle 2 -> interaction 1
  CHECK(e.interaction_of(truth[1]) == &truth_interactions[1]);  // particle 0 -> interaction 0
  CHECK(e.interaction_of(truth[2]) == &truth_interactions[0]);  // particle 1 -> interaction 1

  // ids equal to positions (A1), and ids swapped between two positions (B1)
  const Event a1 = reader.event(1);
  CHECK(a1.interaction_of(a1.reco_particles()[0]) == &a1.reco_interactions()[0]);
  CHECK(a1.interaction_of(a1.reco_particles()[1]) == &a1.reco_interactions()[0]);
  const Event b1 = reader.event(4);
  REQUIRE(b1.reco_particles().size() == 2);
  CHECK(b1.reco_particles()[0].id == 1);
  // id 1 sits at position 0, id 0 at position 1
  CHECK(b1.interaction_of(b1.reco_particles()[0]) == &b1.reco_interactions()[0]);
  CHECK(b1.interaction_of(b1.reco_particles()[1]) == &b1.reco_interactions()[1]);
  CHECK(b1.interaction_of(b1.reco_particles()[0])->id == 1);
}

TEST_CASE("particles_of resolves particle_ids by id, in order, skipping ids that point nowhere") {
  Reader reader(three_files());
  const Event e = reader.event(0);
  const auto& particles = e.reco_particles();
  const auto& interactions = e.reco_interactions();
  // interaction id 1 lists particles 11 and 3: positions 0 and 3
  const auto first = e.particles_of(interactions[0]);
  REQUIRE(first.size() == 2);
  CHECK(first[0] == &particles[0]);
  CHECK(first[1] == &particles[3]);
  // interaction id 2 lists 4 and 99; there is no particle 99
  const auto second = e.particles_of(interactions[1]);
  REQUIRE(second.size() == 1);
  CHECK(second[0] == &particles[1]);
  // interaction id 0 lists 8
  CHECK(e.particles_of(interactions[2]) == std::vector<const RecoParticle*>{&particles[2]});

  const auto& truth = e.truth_particles();
  const auto& truth_interactions = e.truth_interactions();
  // truth interaction id 1 lists particles 2 and 1: positions 0 and 2; id 0 lists 0: position 1
  CHECK(e.particles_of(truth_interactions[0]) ==
        std::vector<const TruthParticle*>{&truth[0], &truth[2]});
  CHECK(e.particles_of(truth_interactions[1]) == std::vector<const TruthParticle*>{&truth[1]});

  // in A1 ids are positions and the interaction lists 0, 1 and 2: there is no particle 2, and 2 is
  // the number of particles
  const Event a1 = reader.event(1);
  REQUIRE(a1.reco_interactions()[0].particle_ids == std::vector<std::int32_t>{0, 1, 2});
  CHECK(a1.particles_of(a1.reco_interactions()[0]) ==
        std::vector<const RecoParticle*>{&a1.reco_particles()[0], &a1.reco_particles()[1]});

  // in C the particle ids are listed as 2, 0 and the particles come as 2, 0
  const Event c = reader.event(5);
  CHECK(ids_of(c.particles_of(c.reco_interactions()[0])) == Ids{2, 0});
  // and in B1 an interaction with id 1 at position 0 lists particle id 1, at position 0
  const Event b1 = reader.event(4);
  CHECK(ids_of(b1.particles_of(b1.reco_interactions()[1])) == Ids{0});
}

TEST_CASE("the relations of an event hold both ways") {
  Reader reader(three_files());
  for (std::uint64_t i = 0; i < reader.num_events(); ++i) {
    CAPTURE(i);
    const Event e = reader.event(i);
    for (const RecoParticle& particle : e.reco_particles()) {
      const RecoInteraction* interaction = e.interaction_of(particle);
      if (interaction == nullptr) {
        CHECK(particle.interaction_id < 0);
        continue;
      }
      CHECK(interaction->id == particle.interaction_id);
      const auto siblings = e.particles_of(*interaction);
      CHECK(std::count(siblings.begin(), siblings.end(), &particle) == 1);
    }
    for (const TruthParticle& particle : e.truth_particles()) {
      const TruthInteraction* interaction = e.interaction_of(particle);
      if (interaction == nullptr) {
        CHECK(particle.interaction_id < 0);
        continue;
      }
      CHECK(interaction->id == particle.interaction_id);
      const auto siblings = e.particles_of(*interaction);
      CHECK(std::count(siblings.begin(), siblings.end(), &particle) == 1);
    }
  }
}

TEST_CASE("matches keep SPINE's order and overlaps, and skip ids that point nowhere") {
  Reader reader(three_files());
  const Event e = reader.event(0);
  const auto& reco = e.reco_particles();
  const auto& truth = e.truth_particles();

  // particle 11 matches truth 1, 2 and 7 (0.75, 0.5, 0.25); there is no truth 7
  auto m = e.matches(reco[0]);
  CHECK(match_ids_of(m) == Ids{1, 2});
  CHECK(same_overlaps(overlaps_of(m), {0.75f, 0.5f}));
  CHECK(m[0].object == &truth[2]);  // truth id 1 sits at position 2
  CHECK(m[1].object == &truth[0]);  // truth id 2 sits at position 0
  CHECK(e.matches(reco[1]).empty());                                  // particle 4: no match
  CHECK(match_ids_of(e.matches(reco[2])) == Ids{0});                  // particle 8
  CHECK(same_overlaps(overlaps_of(e.matches(reco[2])), {0.5f}));
  // particle 3 has two matches and one score: the second overlap is NaN
  m = e.matches(reco[3]);
  CHECK(match_ids_of(m) == Ids{2, 0});
  CHECK(same_overlaps(overlaps_of(m), {0.5f, kNaN}));

  // truth -> reco
  auto t = e.matches(truth[0]);  // truth 2: reco 4 (0.625), reco 11 (0.125)
  CHECK(match_ids_of(t) == Ids{4, 11});
  CHECK(same_overlaps(overlaps_of(t), {0.625f, 0.125f}));
  CHECK(t[0].object == &reco[1]);
  CHECK(t[1].object == &reco[0]);
  CHECK(match_ids_of(e.matches(truth[1])) == Ids{8});
  CHECK(match_ids_of(e.matches(truth[2])) == Ids{11});

  // interactions
  const auto& reco_interactions = e.reco_interactions();
  const auto& truth_interactions = e.truth_interactions();
  auto ri = e.matches(reco_interactions[0]);  // interaction 1: truth 0 (0.875), truth 1 (0.5)
  CHECK(match_ids_of(ri) == Ids{0, 1});
  CHECK(same_overlaps(overlaps_of(ri), {0.875f, 0.5f}));
  CHECK(ri[0].object == &truth_interactions[1]);  // truth interaction 0 is at position 1
  CHECK(ri[1].object == &truth_interactions[0]);
  CHECK(e.matches(reco_interactions[1]).empty());
  auto ti = e.matches(truth_interactions[1]);  // truth interaction 0: reco 2 (0.75), reco 1 (0.25)
  CHECK(match_ids_of(ti) == Ids{2, 1});
  CHECK(same_overlaps(overlaps_of(ti), {0.75f, 0.25f}));
  CHECK(ti[0].object == &reco_interactions[1]);
  CHECK(ti[1].object == &reco_interactions[0]);
  CHECK(match_ids_of(e.matches(truth_interactions[0])) == Ids{1});
}

TEST_CASE("matches into products that are empty or absent are empty, not errors") {
  Reader reader(three_files());
  // A1: the reco particle 0 names truth 0, but the event has no truth objects at all
  const Event a1 = reader.event(1);
  REQUIRE(a1.truth_particles().empty());
  CHECK(a1.matches(a1.reco_particles()[0]).empty());
  CHECK(a1.matches(a1.reco_interactions()[0]).empty());

  // B1 has truth particles that match back
  const Event b1 = reader.event(4);
  REQUIRE(b1.truth_particles().size() == 2);
  // truth id 1 -> reco id 0, which sits at position 1
  auto m = b1.matches(b1.truth_particles()[0]);
  REQUIRE(m.size() == 1);
  CHECK(m[0].object == &b1.reco_particles()[1]);
  CHECK(m[0].overlap == 0.25f);
  m = b1.matches(b1.truth_particles()[1]);  // truth id 0 -> reco id 1, at position 0
  REQUIRE(m.size() == 1);
  CHECK(m[0].object == &b1.reco_particles()[0]);
  CHECK(m[0].overlap == 0.5f);

  // and with only some products read, the objects of the others are not there to match
  ReaderOptions options;
  options.products = {"reco_particles", "reco_interactions"};
  Reader partial(three_files()[0], options);
  const Event e = partial.event(0);
  CHECK(e.truth_particles().empty());
  CHECK(e.matches(e.reco_particles()[0]).empty());
  // relations inside the products that were read work
  CHECK(e.particles_of(e.reco_interactions()[0]).size() == 2);
}

TEST_CASE("an Event outlives its Reader and is cheap to copy") {
  Event survivor = [] {
    Reader reader(three_files());
    return reader.event(0);
  }();
  const Event copy = survivor;
  CHECK(copy.reco_particles().size() == 5);
  CHECK(&copy.reco_particles() == &survivor.reco_particles());  // the copy shares the objects
  CHECK(copy.interaction_of(copy.reco_particles()[0]) == &copy.reco_interactions()[0]);
}

// ---------------------------------------------------------------------------------------------
// Event accessors
// ---------------------------------------------------------------------------------------------

TEST_CASE("has() and objects<> say what was read; a wrong class is an error") {
  Reader reader(three_files());
  const Event e = reader.event(0);
  CHECK(e.has("reco_particles"));
  CHECK(e.has("run_info"));
  CHECK_FALSE(e.has("meta"));  // A has no meta
  CHECK_FALSE(e.has("flashes"));
  CHECK_FALSE(e.has("no_such_product"));
  CHECK(e.objects<RecoParticle>("reco_particles").size() == 5);
  CHECK(e.objects<RecoParticle>("no_such_product").empty());
  CHECK(e.objects<Flash>("flashes").empty());
  try {
    e.objects<RecoInteraction>("reco_particles");
    FAIL("the product holds RecoParticle");
  } catch (const Error& error) {
    CHECK(contains(error.what(), "reco_particles"));
    CHECK(contains(error.what(), "RecoParticle"));
    CHECK(contains(error.what(), "RecoInteraction"));
  }
  // the 'index' product of the file is not an object product: not read, not present
  CHECK_FALSE(e.has("index"));

  // run_info() and meta() are pointers, null when the product is absent
  CHECK(e.run_info() != nullptr);
  CHECK(e.meta() == nullptr);
  const Event c = reader.event(5);
  REQUIRE(c.meta() != nullptr);
  CHECK(c.meta()->lower == std::array<float, 3>{-10, -20, -30});
  CHECK(c.meta()->upper == std::array<float, 3>{10, 20, 30});
  CHECK(c.meta()->size == std::array<float, 3>{1, 2, 3});
  CHECK(c.meta()->count == std::array<std::int64_t, 3>{20, 20, 20});
}

TEST_CASE("the old class name Meta is read as ImageMeta3D") {
  Reader reader(three_files()[2]);
  const SchemaReport& report = reader.schema_report();
  const auto meta = std::find_if(report.products.begin(), report.products.end(),
                                 [](const ProductReport& p) { return p.product == "meta"; });
  REQUIRE(meta != report.products.end());
  CHECK(meta->class_name == "Meta");  // as the file spells it
  CHECK(meta->complete());
  CHECK(std::none_of(report.skipped.begin(), report.skipped.end(),
                     [](const std::string& s) { return contains(s, "meta"); }));
  REQUIRE(reader.event(0).meta() != nullptr);
  CHECK(reader.event(0).meta()->count[0] == 20);
}

// ---------------------------------------------------------------------------------------------
// ReaderOptions
// ---------------------------------------------------------------------------------------------

TEST_CASE("ReaderOptions::products selects the object products to read") {
  ReaderOptions options;
  options.products = {"reco_interactions", "truth_particles"};
  Reader reader(three_files()[0], options);
  const Event e = reader.event(0);
  CHECK(e.has("reco_interactions"));
  CHECK(e.has("truth_particles"));
  CHECK_FALSE(e.has("reco_particles"));
  CHECK_FALSE(e.has("truth_interactions"));
  CHECK_FALSE(e.has("run_info"));
  CHECK(e.reco_interactions().size() == 3);
  CHECK(e.truth_particles().size() == 3);
  CHECK(e.reco_particles().empty());
  CHECK(e.run_info() == nullptr);

  const SchemaReport& report = reader.schema_report();
  REQUIRE(report.products.size() == 2);
  std::vector<std::string> bound = {report.products[0].product, report.products[1].product};
  std::sort(bound.begin(), bound.end());
  CHECK(bound == std::vector<std::string>{"reco_interactions", "truth_particles"});

  // a name that is not a product selects nothing, and is not an error
  ReaderOptions none;
  none.products = {"no_such_product"};
  Reader nothing(three_files()[0], none);
  CHECK(nothing.num_events() == 3);
  CHECK(nothing.event(0).reco_particles().empty());
  CHECK(nothing.schema_report().products.empty());

  // the default reads every object product the model knows
  Reader all(three_files()[0]);
  CHECK(all.schema_report().products.size() == 5);  // run_info, 2 reco, 2 truth
  CHECK(all.event(0).has("run_info"));
}

TEST_CASE("non-object products and products of unknown classes are listed as not read") {
  Reader reader(three_files()[0]);
  const SchemaReport& report = reader.schema_report();
  CHECK(std::find(report.skipped.begin(), report.skipped.end(), "index: array product") !=
        report.skipped.end());
  CHECK(report.skipped.size() == 1);

  // the summary says so, after the products
  const std::string summary = report.summary();
  CHECK(contains(summary, three_files()[0] +
                              "\n  SPINE 1.4.0, format_version 1 (V1), read with model 1.4.0\n"));
  CHECK(contains(summary, "\n  reco_particles [RecoParticle]: all model fields read\n"));
  const std::string tail = "  not read:\n    index: array product\n";
  REQUIRE(summary.size() > tail.size());
  CHECK(summary.compare(summary.size() - tail.size(), tail.size(), tail) == 0);

  // a file that does not say which SPINE wrote it
  Reader unversioned(core_file("edge_v2_noversion.h5"));
  CHECK(contains(unversioned.schema_report().summary(),
                 "\n  SPINE unknown, format_version 1 (V2Products), read with model 1.4.0\n"));

  // edge_v1_columns.h5 has an 'Edge' class no model knows
  Reader edge(core_file("edge_v1_columns.h5"));
  const SchemaReport& edge_report = edge.schema_report();
  CHECK(edge_report.products.empty());
  CHECK(std::find(edge_report.skipped.begin(), edge_report.skipped.end(),
                  "objs: no model class Edge") != edge_report.skipped.end());
  CHECK(std::find(edge_report.skipped.begin(), edge_report.skipped.end(),
                  "labels: string product") != edge_report.skipped.end());
  CHECK(contains(edge_report.summary(), "objs: no model class Edge"));
}

TEST_CASE("strict mode refuses a product that lacks model fields, and only then") {
  ReaderOptions strict;
  strict.strict = true;

  // a complete SPINE v1.4.0 file reads
  CHECK_NOTHROW(Reader(fixture_file("v1.4.0_v1.h5"), strict));
  CHECK_NOTHROW(Reader(fixture_file("v1.4.0_v2.h5"), strict));

  // the same file written in lite mode lacks index and orig_index; the oldest release lacks more
  for (const char* name : {"v1.4.0_v1_lite.h5", "v0.11.1_v1.h5", "v1.4.0_v2_fixed_only.h5"}) {
    CAPTURE(name);
    CHECK_NOTHROW(Reader(fixture_file(name)));  // not strict: fine
    try {
      Reader reader(fixture_file(name), strict);
      FAIL("a product lacks model fields");
    } catch (const Error& error) {
      CHECK(contains(error.what(), fixture_file(name)));
      CHECK(contains(error.what(), "lacks model fields"));
      CHECK(contains(error.what(), "missing"));  // the summary of the report follows
    }
  }

  // strict only looks at the products it was asked to read
  ReaderOptions only_runs = strict;
  only_runs.products = {"run_info"};
  CHECK_NOTHROW(Reader(fixture_file("v0.11.1_v1.h5"), only_runs));
}

TEST_CASE("incomplete files are refused unless ReaderOptions::allow_incomplete") {
  const std::string path = fixture_file("incomplete.h5");
  try {
    Reader reader(path);
    FAIL("the file is incomplete");
  } catch (const Error& error) {
    CHECK(contains(error.what(), path));
    CHECK(contains(error.what(), "the file is incomplete (/info@complete is false)"));
  }
  ReaderOptions options;
  options.allow_incomplete = true;
  Reader reader(path, options);
  CHECK(reader.num_events() == 2);
  CHECK(reader.event(1).run_info() != nullptr);
  // one incomplete file among complete ones is enough to refuse the lot
  CHECK_THROWS_AS(Reader(std::vector<std::string>{three_files()[0], path}), Error);
  CHECK(Reader(std::vector<std::string>{three_files()[0], path}, options).num_events() == 5);
}

TEST_CASE("a file that cannot be opened is an error that names it") {
  const std::string missing = reader_file("no_such_file.h5");
  try {
    Reader reader(missing);
    FAIL("no such file");
  } catch (const Error& error) {
    CHECK(contains(error.what(), missing));
  }
  // and the good files before it do not keep the Reader alive
  CHECK_THROWS_AS(Reader(std::vector<std::string>{three_files()[0], missing}), Error);
}

TEST_CASE("a Reader can be moved") {
  Reader a(three_files());
  Reader b = std::move(a);
  CHECK(b.num_events() == 6);
  CHECK(b.event(5).run_info()->event == 77);
  Reader c(three_files()[0]);
  c = std::move(b);
  CHECK(c.num_events() == 6);
}

// ---------------------------------------------------------------------------------------------
// Binding, through the whole chain
// ---------------------------------------------------------------------------------------------

TEST_CASE("a product with every kind of difference binds as the report says") {
  // edge_model_binding_v1.h5: 'reco_particles', class RecoParticle, columns
  //   id (int64), is_primary (plain uint8: 2, 0, 1), is_valid (bool enum: 1, 0, 2),
  //   shape (enum {UNKNOWN -1, TRACK 0, SHOWER 1, NOVEL 9}: 0, 1, 9; numbered across the model's,
  //   so it is not read), units, match_ids (VLEN),
  //   start_point (float64[4]: the model has float32[3]), brand_new (float32).
  Reader reader(core_file("edge_model_binding_v1.h5"));
  REQUIRE(reader.num_events() == 2);
  const SchemaReport& report = reader.schema_report();
  REQUIRE(report.products.size() == 1);
  const ProductReport& p = report.products[0];
  CHECK(p.product == "reco_particles");
  CHECK(p.class_name == "RecoParticle");
  CHECK_FALSE(p.complete());
  CHECK(p.mismatched ==
        std::vector<std::string>{"shape: enum numbering differs: file SHOWER=1 and TRACK=0, "
                                 "model SHOWER=0 and TRACK=1",
                                 "start_point: file float64[4] vs model float32[3]"});
  CHECK(p.extra == std::vector<std::string>{"brand_new"});
  // 55 fields; 5 bound, 2 mismatched
  CHECK(p.missing.size() == 55 - 5 - 2);
  CHECK(std::find(p.missing.begin(), p.missing.end(), "end_point") != p.missing.end());
  CHECK(std::find(p.missing.begin(), p.missing.end(), "length") != p.missing.end());
  for (const char* bound :
       {"id", "is_primary", "is_valid", "shape", "units", "match_ids", "start_point"}) {
    CHECK_MESSAGE(std::find(p.missing.begin(), p.missing.end(), bound) == p.missing.end(), bound);
  }
  CHECK(p.enum_notes.empty());  // the only enum is not read

  // what the summary prints
  const std::string summary = report.summary();
  CHECK(contains(summary, "reco_particles [RecoParticle]: some model fields missing"));
  CHECK(contains(summary, "mismatched (2):"));
  CHECK_FALSE(contains(summary, "enum labels"));
  CHECK(contains(summary, "start_point: file float64[4] vs model float32[3]"));
  CHECK(contains(summary, "not in model (1):"));
  CHECK(contains(summary, "brand_new"));
  CHECK(contains(summary, "SPINE 1.4.0, format_version 1 (V1)"));
  CHECK(contains(summary, core_file("edge_model_binding_v1.h5")));

  // the values
  const Event e0 = reader.event(0);
  const Event e1 = reader.event(1);
  REQUIRE(e0.reco_particles().size() == 2);
  REQUIRE(e1.reco_particles().size() == 1);
  const RecoParticle& a = e0.reco_particles()[0];
  const RecoParticle& b = e0.reco_particles()[1];
  const RecoParticle& c = e1.reco_particles()[0];
  CHECK(a.id == 3);
  CHECK(b.id == 4);
  CHECK(c.id == 5);
  // a bool stored as a plain integer: non-zero is true
  CHECK(a.is_primary);
  CHECK_FALSE(b.is_primary);
  CHECK(c.is_primary);
  // an enum bool holding 2 is true, and 0 is false although the default is true
  CHECK(a.is_valid);
  CHECK_FALSE(b.is_valid);
  CHECK(c.is_valid);
  // the crossed enum keeps its default
  CHECK(a.shape == ParticleShape::UNKNOWN);
  CHECK(b.shape == ParticleShape::UNKNOWN);
  CHECK(c.shape == ParticleShape::UNKNOWN);
  CHECK(a.units == "cm");
  CHECK(b.units == "px");
  CHECK(a.match_ids == std::vector<std::int32_t>{1, 2});
  CHECK(b.match_ids.empty());
  CHECK(c.match_ids == std::vector<std::int32_t>{3});
  // presence
  using F = RecoParticle::Field;
  for (F field : {F::id, F::is_primary, F::is_valid, F::units, F::match_ids}) {
    CHECK_MESSAGE(a.has(field), "field " << static_cast<int>(field));
  }
  for (F field :
       {F::shape, F::start_point, F::end_point, F::length, F::is_truth, F::interaction_id, F::pid}) {
    CHECK_MESSAGE(!a.has(field), "field " << static_cast<int>(field));
  }
  // a mismatched or missing field keeps SPINE's default
  CHECK(std::isnan(a.start_point[0]));
  CHECK(std::isnan(a.length));
  CHECK(a.interaction_id == -1);
  CHECK(a.pid == ParticlePID::UNKNOWN);

  // strict: this product is incomplete
  ReaderOptions strict;
  strict.strict = true;
  try {
    Reader refused(core_file("edge_model_binding_v1.h5"), strict);
    FAIL("incomplete product");
  } catch (const Error& error) {
    CHECK(contains(error.what(), "reco_particles"));
    CHECK(contains(error.what(), "start_point: file float64[4] vs model float32[3]"));
  }
}

// ---------------------------------------------------------------------------------------------
// Threads, and silence
// ---------------------------------------------------------------------------------------------

namespace {

/// Everything an event holds that the tests above look at, flattened into numbers.
std::vector<std::int64_t> digest(const Event& e) {
  std::vector<std::int64_t> d = {static_cast<std::int64_t>(e.index()),
                                 static_cast<std::int64_t>(e.file_index()),
                                 static_cast<std::int64_t>(e.entry())};
  if (const RunInfo* run = e.run_info()) {
    d.insert(d.end(), {run->run, run->subrun, run->event});
  }
  for (const RecoParticle& p : e.reco_particles()) {
    d.push_back(p.id);
    d.push_back(p.interaction_id);
    d.insert(d.end(), p.match_ids.begin(), p.match_ids.end());
    d.push_back(-1000);
  }
  for (const RecoInteraction& i : e.reco_interactions()) {
    d.push_back(i.id);
    d.insert(d.end(), i.particle_ids.begin(), i.particle_ids.end());
    d.push_back(-1001);
  }
  for (const TruthParticle& p : e.truth_particles()) {
    d.push_back(p.id);
    d.insert(d.end(), p.match_ids.begin(), p.match_ids.end());
    d.push_back(-1002);
  }
  return d;
}

/// Redirects file descriptor 2 to a temporary file while alive.
class StderrCapture {
 public:
  StderrCapture() {
    std::fflush(stderr);
    file_ = std::tmpfile();
    saved_ = ::dup(2);
    ::dup2(::fileno(file_), 2);
  }
  ~StderrCapture() {
    restore();
    if (file_ != nullptr) std::fclose(file_);
  }
  /// Stops capturing; returns what was written to descriptor 2.
  std::string text() {
    restore();
    std::string out;
    std::rewind(file_);
    char buffer[512];
    std::size_t n;
    while ((n = std::fread(buffer, 1, sizeof(buffer), file_)) > 0) out.append(buffer, n);
    return out;
  }

 private:
  void restore() {
    if (saved_ >= 0) {
      std::fflush(stderr);
      ::dup2(saved_, 2);
      ::close(saved_);
      saved_ = -1;
    }
  }
  std::FILE* file_ = nullptr;
  int saved_ = -1;
};

}  // namespace

TEST_CASE("one Reader serves several threads, one at a time") {
  Reader reader(three_files());
  std::vector<std::vector<std::int64_t>> want;
  for (std::uint64_t i = 0; i < reader.num_events(); ++i) want.push_back(digest(reader.event(i)));
  REQUIRE(want[0].size() > 20);

  std::atomic<int> wrong{0};
  std::atomic<int> thrown{0};
  std::atomic<int> reads{0};
  std::vector<std::thread> threads;
  for (int t = 0; t < 4; ++t) {
    threads.emplace_back([&, t] {
      try {
        for (int iteration = 0; iteration < 80; ++iteration) {
          const auto i = static_cast<std::uint64_t>((t * 5 + iteration) % 6);
          if (digest(reader.event(i)) != want[i]) ++wrong;
          const auto found = reader.find(want[i][3], want[i][4], want[i][5]);
          // (8, 1, 5) is both event 2 and event 3: the first wins
          const std::uint64_t first = (i == 3) ? 2 : i;
          if (!found || *found != first) ++wrong;
          ++reads;
        }
      } catch (...) {
        ++thrown;
      }
    });
  }
  for (std::thread& thread : threads) thread.join();
  CHECK(reads == 320);
  CHECK(thrown == 0);
  CHECK(wrong == 0);
}

TEST_CASE("the first find() of a fresh Reader can come from several threads at once") {
  // find() builds its index on the first call. Under a thread sanitizer (ci/sanitize.sh thread)
  // an unlocked build is a reported race; without one it can still crash or misreport.
  std::atomic<int> wrong{0};
  for (int round = 0; round < 25; ++round) {
    Reader reader(three_files());
    std::atomic<int> arrived{0};
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
      threads.emplace_back([&] {
        ++arrived;
        while (arrived < 4) std::this_thread::yield();  // all four start together
        const auto found = reader.find(10, 3, 77);
        if (!found || *found != 5) ++wrong;
        const auto again = reader.find(8, 1, 5);
        if (!again || *again != 2) ++wrong;
      });
    }
    for (std::thread& thread : threads) thread.join();
  }
  CHECK(wrong == 0);
}

TEST_CASE("nothing is printed, however a call fails") {
  // HDF5 prints its error stack to stderr unless told not to; the library must not let it.
  const std::string missing = reader_file("no_such_file.h5");
  std::string text;
  int failures = 0;
  {
    StderrCapture capture;
    const auto fails = [&](const std::function<void()>& call) {
      try {
        call();
      } catch (const Error&) {
        ++failures;
      }
    };
    fails([&] { Reader reader(missing); });
    fails([&] { Reader reader(core_file("edge_not_spine.h5")); });
    fails([&] { Reader reader(core_file("edge_bad_version.h5")); });
    fails([&] { Reader reader(three_files()[0]); reader.event(99); });
    fails([&] {  // a corrupt V2 file, opened by the Reader and read
      ReaderOptions options;
      Reader reader(core_file("edge_v2_corrupt.h5"), options);
      reader.event(0);
    });
    {
      std::lock_guard<std::recursive_mutex> lock(core::hdf5_mutex());
      fails([&] { core::FileSource::open(missing); });
      auto file = core::FileSource::open(core_file("edge_v2_corrupt.h5"));
      fails([&] { file->product("wrong_width"); });
      core::ProductSource& past = file->product("past_pool");
      fails([&] {
        std::vector<detail::ColumnBuffer> out;
        past.read({0, 1}, {{past.info().find("a"), ScalarType::Int32}}, out);
      });
      fails([&] { past.rows(99); });
    }
    text = capture.text();
  }
  CHECK(failures >= 8);  // the failures happened, so there was something to print
  CHECK(text.empty());
}
