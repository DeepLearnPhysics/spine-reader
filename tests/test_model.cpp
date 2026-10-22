// Tests of the generated model (include/spine/model, src/model).
//
// The expected default values were printed from SPINE v1.4.0 itself:
//   RecoParticle() ... FlashHypothesis() default-constructed, then getattr(obj, name).
// Enumerator counts and values are those of spine.constants.enums at the same release.

// Standard and POSIX headers come first on purpose: any macro they define (NULL, EOF, DOMAIN,
// OVERFLOW, errno, major, ...) must not collide with an identifier of the model headers.
#include <cassert>
#include <cerrno>
#include <cfloat>
#include <climits>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <math.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <unistd.h>

#include <spine/model.hpp>

#include <doctest.h>

#include <algorithm>
#include <array>
#include <bitset>
#include <cstdint>
#include <set>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <spine/detail/column_buffer.hpp>

namespace {

using spine::ScalarType;
namespace detail = spine::detail;

// ---------------------------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------------------------

template <class A>
bool all_nan(const A& values) {
  return std::all_of(values.begin(), values.end(), [](auto v) { return std::isnan(v); });
}

template <class T>
detail::ColumnBuffer fixed_column(ScalarType type, std::size_t count,
                                  const std::vector<T>& values) {
  detail::ColumnBuffer c;
  c.type = type;
  c.count = count;
  c.rows = values.size() / count;
  c.data.resize(values.size() * sizeof(T));
  std::memcpy(c.data.data(), values.data(), c.data.size());
  return c;
}

std::size_t index_of(const spine::ClassDescriptor& d, std::string_view name) {
  const spine::FieldDescriptor* f = d.find(name);
  REQUIRE(f != nullptr);
  return d.index_of(*f);
}

/// The last enumerator of each class's Field enum.
template <class C>
struct Last;
#define LAST_FIELD(Class, field)                                             \
  template <>                                                                \
  struct Last<spine::Class> {                                                \
    static constexpr spine::Class::Field value = spine::Class::Field::field; \
  }
LAST_FIELD(RecoParticle, module_ids);
LAST_FIELD(TruthParticle, depositions_g4_sum);
LAST_FIELD(RecoInteraction, module_ids);
LAST_FIELD(TruthInteraction, depositions_g4_sum);
LAST_FIELD(RunInfo, event);
LAST_FIELD(Trigger, beam_time_ns);
LAST_FIELD(ImageMeta3D, count);
LAST_FIELD(Flash, width);
LAST_FIELD(CRTHit, time);
LAST_FIELD(FlashHypothesis, total_pe);
#undef LAST_FIELD

/// Structure every generated class must have, whatever SPINE release it comes from.
template <class C>
void check_structure(std::string_view name, std::size_t expected_fields) {
  const spine::ClassDescriptor& d = C::descriptor();
  INFO(name);
  CHECK(d.name == name);
  CHECK(d.schema_version == SPINE_READER_MODEL_SPINE_VERSION);
  CHECK(d.schema_version == "1.4.0");
  CHECK(C::kNumFields == expected_fields);
  CHECK(d.num_fields == C::kNumFields);
  CHECK(static_cast<std::size_t>(Last<C>::value) + 1 == d.num_fields);
  CHECK(&C::descriptor() == &d);  // one descriptor per class

  std::set<std::string_view> names;
  for (std::size_t i = 0; i < d.num_fields; ++i) {
    const spine::FieldDescriptor& f = d.fields[i];
    INFO(f.name);
    CHECK(names.insert(f.name).second);  // unique
    CHECK(f.assign != nullptr);          // assignable
    CHECK(!f.name.empty());
    CHECK(!f.doc.empty());        // every SPINE field is documented
    CHECK(d.find(f.name) == &f);  // find() and index_of() agree
    CHECK(d.index_of(f) == i);
    if (f.variable) {
      CHECK(f.count == 0);
    } else {
      CHECK(f.count >= 1);
    }
    if (f.type == ScalarType::String) {
      CHECK(f.variable);
      CHECK(f.count == 0);
    }
  }
  CHECK(d.find("no_such_field") == nullptr);

  // A default object has no field present, and has() reads the bit of the enumerator.
  C object;
  for (std::size_t i = 0; i < d.num_fields; ++i) {
    CHECK_FALSE(object.has(static_cast<typename C::Field>(i)));
  }
  std::bitset<C::kNumFields> mask;
  mask.set(static_cast<std::size_t>(Last<C>::value));
  mask.set(0);
  detail::ObjectAccess::set_present(object, mask);
  CHECK(object.has(Last<C>::value));
  CHECK(object.has(static_cast<typename C::Field>(0)));
  if (C::kNumFields > 2) CHECK_FALSE(object.has(static_cast<typename C::Field>(1)));
}

}  // namespace

// ---------------------------------------------------------------------------------------------
// Inventory and structure
// ---------------------------------------------------------------------------------------------

TEST_CASE("model: SPINE_READER_FOR_EACH_CLASS lists every class once, in dump order") {
  std::vector<std::string_view> names;
#define COLLECT(Class) names.push_back(#Class);
  SPINE_READER_FOR_EACH_CLASS(COLLECT)
#undef COLLECT
  const std::vector<std::string_view> expected = {
      "RecoParticle", "TruthParticle", "RecoInteraction", "TruthInteraction", "RunInfo",
      "Trigger",      "ImageMeta3D",   "Flash",           "CRTHit",           "FlashHypothesis"};
  CHECK(names == expected);

  // Each name in the macro is a class whose descriptor carries that name.
  std::vector<std::string_view> described;
#define DESCRIBE(Class) described.push_back(spine::Class::descriptor().name);
  SPINE_READER_FOR_EACH_CLASS(DESCRIBE)
#undef DESCRIBE
  CHECK(described == expected);
}

TEST_CASE("model: field counts and descriptor structure of every class") {
  check_structure<spine::RecoParticle>("RecoParticle", 55);
  check_structure<spine::TruthParticle>("TruthParticle", 98);
  check_structure<spine::RecoInteraction>("RecoInteraction", 40);
  check_structure<spine::TruthInteraction>("TruthInteraction", 79);
  check_structure<spine::RunInfo>("RunInfo", 3);
  check_structure<spine::Trigger>("Trigger", 6);
  check_structure<spine::ImageMeta3D>("ImageMeta3D", 4);
  check_structure<spine::Flash>("Flash", 14);
  check_structure<spine::CRTHit>("CRTHit", 14);
  check_structure<spine::FlashHypothesis>("FlashHypothesis", 8);
}

TEST_CASE("model: field i has the name of enumerator i") {
  using spine::RecoParticle;
  const auto& d = RecoParticle::descriptor();
  CHECK(d.fields[static_cast<std::size_t>(RecoParticle::Field::is_truth)].name == "is_truth");
  CHECK(d.fields[static_cast<std::size_t>(RecoParticle::Field::units)].name == "units");
  CHECK(d.fields[static_cast<std::size_t>(RecoParticle::Field::start_point)].name == "start_point");
  CHECK(d.fields[static_cast<std::size_t>(RecoParticle::Field::pdg_code)].name == "pdg_code");
  CHECK(d.fields[static_cast<std::size_t>(RecoParticle::Field::module_ids)].name == "module_ids");
  CHECK(static_cast<std::size_t>(RecoParticle::Field::is_truth) == 0);
  CHECK(static_cast<std::size_t>(RecoParticle::Field::module_ids) == 54);

  using spine::TruthInteraction;
  const auto& t = TruthInteraction::descriptor();
  CHECK(t.fields[static_cast<std::size_t>(TruthInteraction::Field::interaction_scheme)].name ==
        "interaction_scheme");
  CHECK(t.fields[static_cast<std::size_t>(TruthInteraction::Field::time)].name == "time");
  CHECK(t.fields[static_cast<std::size_t>(TruthInteraction::Field::dir)].name == "dir");

  using spine::RunInfo;
  CHECK(RunInfo::descriptor().fields[0].name == "run");
  CHECK(RunInfo::descriptor().fields[1].name == "subrun");
  CHECK(RunInfo::descriptor().fields[2].name == "event");
  CHECK(static_cast<std::size_t>(RunInfo::Field::event) == 2);
}

// ---------------------------------------------------------------------------------------------
// Member types (what the binder and the CAFMakers rely on)
// ---------------------------------------------------------------------------------------------

TEST_CASE("model: C++ types follow SPINE's declared types") {
  using spine::RecoInteraction;
  using spine::RecoParticle;
  using spine::RunInfo;
  using spine::TruthInteraction;
  using spine::TruthParticle;
  static_assert(std::is_same_v<decltype(RecoParticle::id), std::int64_t>);
  static_assert(std::is_same_v<decltype(RecoParticle::interaction_id), std::int64_t>);
  static_assert(std::is_same_v<decltype(RecoParticle::match_ids), std::vector<std::int32_t>>);
  static_assert(std::is_same_v<decltype(RecoParticle::match_overlaps), std::vector<float>>);
  static_assert(std::is_same_v<decltype(RecoInteraction::particle_ids), std::vector<std::int32_t>>);
  static_assert(std::is_same_v<decltype(TruthParticle::interaction_id), std::int64_t>);
  static_assert(std::is_same_v<decltype(RunInfo::run), std::int64_t>);
  static_assert(std::is_same_v<decltype(RunInfo::subrun), std::int64_t>);
  static_assert(std::is_same_v<decltype(RunInfo::event), std::int64_t>);
  static_assert(std::is_same_v<decltype(RecoParticle::cathode_offset), double>);
  static_assert(std::is_same_v<decltype(RecoParticle::start_point), std::array<float, 3>>);
  static_assert(std::is_same_v<decltype(RecoParticle::chi2_per_pid), std::array<float, 6>>);
  static_assert(std::is_same_v<decltype(RecoParticle::is_valid), bool>);
  static_assert(std::is_same_v<decltype(RecoParticle::units), std::string>);
  static_assert(std::is_same_v<decltype(RecoParticle::shape), spine::ParticleShape>);
  static_assert(std::is_same_v<decltype(RecoParticle::pid), spine::ParticlePID>);
  static_assert(std::is_same_v<decltype(RecoParticle::chi2_pid), spine::ParticlePID>);
  static_assert(std::is_same_v<decltype(TruthInteraction::current_type), spine::NuCurrentType>);
  static_assert(std::is_same_v<decltype(TruthInteraction::interaction_scheme),
                               spine::NuInteractionScheme>);
  // The mode and type codes depend on the scheme, so they stay plain integers.
  static_assert(std::is_same_v<decltype(TruthInteraction::interaction_mode), std::int64_t>);
  static_assert(std::is_same_v<decltype(TruthInteraction::interaction_type), std::int64_t>);
  static_assert(std::is_same_v<decltype(spine::CRTHit::feb_id), std::vector<std::uint8_t>>);
  static_assert(std::is_same_v<decltype(spine::ImageMeta3D::count), std::array<std::int64_t, 3>>);
  static_assert(std::is_same_v<decltype(spine::ImageMeta3D::lower), std::array<float, 3>>);
  static_assert(std::is_same_v<decltype(spine::Trigger::time_ns), std::int64_t>);
  static_assert(std::is_same_v<spine::Shape, spine::ParticleShape>);

  // The descriptors say the same as the members.
  const auto* mode = TruthInteraction::descriptor().find("interaction_mode");
  const auto* type = TruthInteraction::descriptor().find("interaction_type");
  const auto* scheme = TruthInteraction::descriptor().find("interaction_scheme");
  REQUIRE(mode != nullptr);
  REQUIRE(type != nullptr);
  REQUIRE(scheme != nullptr);
  CHECK(mode->type == ScalarType::Int64);
  CHECK(mode->enumeration == nullptr);
  CHECK(type->type == ScalarType::Int64);
  CHECK(type->enumeration == nullptr);
  CHECK(scheme->enumeration == &spine::enum_descriptor<spine::NuInteractionScheme>());

  const auto& rp = RecoParticle::descriptor();
  CHECK(rp.find("units")->type == ScalarType::String);
  CHECK(rp.find("units")->variable);
  CHECK(rp.find("is_valid")->type == ScalarType::Bool);
  CHECK(rp.find("start_point")->type == ScalarType::Float32);
  CHECK(rp.find("start_point")->count == 3);
  CHECK_FALSE(rp.find("start_point")->variable);
  CHECK(rp.find("match_ids")->type == ScalarType::Int32);
  CHECK(rp.find("match_ids")->count == 0);
  CHECK(rp.find("match_ids")->variable);
  CHECK(rp.find("cathode_offset")->type == ScalarType::Float64);
  CHECK(rp.find("id")->type == ScalarType::Int64);
  CHECK(rp.find("id")->count == 1);
  CHECK(spine::CRTHit::descriptor().find("feb_id")->type == ScalarType::UInt8);
  CHECK(spine::ImageMeta3D::descriptor().find("count")->type == ScalarType::Int64);
}

TEST_CASE("model: descriptors carry SPINE's flags, units, references and enums") {
  using spine::RecoParticle;
  using spine::TruthInteraction;
  using spine::TruthParticle;
  const auto& rp = RecoParticle::descriptor();

  const auto* id = rp.find("id");
  CHECK((id->flags & spine::kIndex) != 0);
  CHECK(id->reference == "object");
  CHECK(id->reference_space == "same");
  CHECK(rp.find("interaction_id")->reference == "interaction");
  CHECK(rp.find("match_ids")->reference == "object");
  CHECK(rp.find("match_ids")->reference_space == "opposite");
  CHECK(rp.find("crt_ids")->reference == "crthit");
  CHECK(rp.find("crt_ids")->reference_space == "external");

  CHECK(rp.find("calo_ke")->units == "MeV");
  CHECK(rp.find("crt_times")->units == "us");
  CHECK(rp.find("start_point")->units == "instance");
  CHECK((rp.find("start_point")->flags & spine::kPosition) != 0);
  CHECK((rp.find("start_dir")->flags & spine::kVector) != 0);
  CHECK(rp.find("momentum")->units == "MeV/c");
  CHECK(rp.find("mass")->units == "MeV/c^2");
  CHECK(rp.find("id")->units.empty());

  // Derived fields (SPINE stored_property) and fields dropped from lite files.
  for (const char* name : {"pdg_code", "mass", "ke", "momentum", "p", "num_fragments", "size",
                           "depositions_sum", "best_match_id", "module_ids"}) {
    INFO(name);
    CHECK(rp.find(name)->derived());
  }
  for (const char* name : {"id", "units", "shape", "start_point", "match_ids", "index"}) {
    INFO(name);
    CHECK_FALSE(rp.find(name)->derived());
  }
  CHECK((rp.find("index")->flags & spine::kLiteSkip) != 0);
  CHECK((rp.find("orig_index")->flags & spine::kLiteSkip) != 0);
  CHECK((rp.find("match_ids")->flags & spine::kLiteSkip) == 0);
  CHECK((TruthParticle::descriptor().find("index_g4")->flags & spine::kLiteSkip) != 0);
  CHECK((rp.find("pdg_code")->flags & spine::kPdg) != 0);
  CHECK((rp.find("pdg_code")->flags & spine::kCategorical) != 0);

  const auto& ti = TruthInteraction::descriptor();
  CHECK((ti.find("quark")->flags & spine::kPdgMinusOneValid) != 0);
  CHECK((ti.find("target")->flags & spine::kPdgMinusOneValid) == 0);
  CHECK(ti.find("energy_init")->units == "GeV");
  CHECK(TruthParticle::descriptor().find("energy_init")->units == "MeV");
  CHECK((spine::RecoInteraction::descriptor().find("topology")->flags & spine::kDerived) != 0);

  // Enum-typed fields point at the generated EnumDescriptors.
  CHECK(rp.find("shape")->enumeration == &spine::enum_descriptor<spine::ParticleShape>());
  CHECK(rp.find("pid")->enumeration == &spine::enum_descriptor<spine::ParticlePID>());
  CHECK(rp.find("chi2_pid")->enumeration == &spine::enum_descriptor<spine::ParticlePID>());
  CHECK(ti.find("current_type")->enumeration == &spine::enum_descriptor<spine::NuCurrentType>());
  CHECK(rp.find("id")->enumeration == nullptr);
  CHECK(rp.find("is_valid")->enumeration == nullptr);  // bool, never an enum
}

// ---------------------------------------------------------------------------------------------
// Defaults against SPINE (values printed from default-constructed SPINE objects)
// ---------------------------------------------------------------------------------------------

TEST_CASE("defaults: RecoParticle") {
  spine::RecoParticle p;
  CHECK(p.id == -1);
  CHECK(p.units == "cm");
  CHECK_FALSE(p.is_truth);
  CHECK(p.is_valid);  // True in SPINE, unlike the other bools
  CHECK_FALSE(p.is_primary);
  CHECK(std::isnan(p.cathode_offset));
  CHECK(std::isnan(p.length));
  CHECK(all_nan(p.start_point));
  CHECK(all_nan(p.chi2_per_pid));
  CHECK(p.pid == spine::ParticlePID::UNKNOWN);
  CHECK(p.shape == spine::ParticleShape::UNKNOWN);
  CHECK(p.chi2_pid == spine::ParticlePID::UNKNOWN);
  CHECK(p.match_ids.empty());
  CHECK(p.match_overlaps.empty());
  CHECK(p.interaction_id == -1);
  // derived: value of the property on a default object
  CHECK(p.pdg_code == 0);
  CHECK(std::isnan(p.mass));
  CHECK(std::isnan(p.p));
  CHECK(p.size == 0);
  CHECK(p.depositions_sum == 0.0);
  CHECK(p.best_match_id == -1);
  CHECK(p.best_match_overlap == -1.0);
  CHECK(p.num_fragments == 0);
  CHECK(p.module_ids.empty());
  CHECK(std::isnan(p.vertex_distance));
}

TEST_CASE("defaults: TruthParticle") {
  spine::TruthParticle p;
  CHECK(p.id == -1);
  CHECK(p.parent_id == -1);
  CHECK(p.pdg_code == 0);  // INVALID_PDG
  CHECK(std::isnan(p.t));
  CHECK(all_nan(p.momentum));
  CHECK(all_nan(p.position));
  CHECK(p.creation_process.empty());
  CHECK(p.children_id.empty());
  CHECK(p.units == "cm");
  CHECK(std::isnan(p.mass));
  CHECK(p.shape == spine::ParticleShape::UNKNOWN);
  CHECK(p.pid == spine::ParticlePID::UNKNOWN);
  CHECK(std::isnan(p.energy_init));
  CHECK(p.num_voxels == -1);
  CHECK(p.orig_id == -1);
  CHECK(p.is_truth);
  CHECK(p.orig_children_id.empty());
  CHECK(std::isnan(p.reco_length));
  CHECK(all_nan(p.reco_start_dir));
  CHECK(all_nan(p.start_dir));
  CHECK(std::isnan(p.ke));
  CHECK(p.size == 0);
}

TEST_CASE("defaults: RecoInteraction") {
  spine::RecoInteraction i;
  CHECK(i.id == -1);
  CHECK_FALSE(i.is_fiducial);
  CHECK(std::isnan(i.flash_total_pe));
  CHECK(all_nan(i.vertex));
  CHECK(i.particle_counts == std::array<std::int32_t, 6>{0, 0, 0, 0, 0, 0});
  CHECK(i.primary_particle_counts == std::array<std::int32_t, 6>{0, 0, 0, 0, 0, 0});
  CHECK(i.topology.empty());
  CHECK(i.num_particles == 0);
  CHECK(i.units == "cm");
  CHECK_FALSE(i.is_truth);
  CHECK(all_nan(i.dir));
  CHECK_FALSE(i.is_crt_matched);
  CHECK(i.flash_ids.empty());
  CHECK(i.size == 0);
  CHECK(i.best_match_overlap == -1.0);
}

TEST_CASE("defaults: TruthInteraction") {
  spine::TruthInteraction i;
  CHECK(i.id == -1);
  CHECK(i.nu_id == -1);
  CHECK(i.interaction_scheme == spine::NuInteractionScheme::UNKNOWN);
  CHECK(i.current_type == spine::NuCurrentType::UNKNOWN);
  CHECK(i.interaction_mode == -1);
  CHECK(i.interaction_type == -1);
  CHECK(std::isnan(i.energy_init));
  CHECK(i.pdg_code == 0);
  CHECK(i.quark == 0);
  CHECK(i.target == 0);
  CHECK(all_nan(i.dir));
  CHECK(all_nan(i.position));
  CHECK(all_nan(i.momentum));
  CHECK(std::isnan(i.time));
  CHECK(i.creation_process.empty());
  CHECK(i.units == "cm");
  CHECK(i.is_truth);
  CHECK(i.track_id == -1);
  CHECK(all_nan(i.vertex));
  CHECK(all_nan(i.reco_vertex));
  CHECK(i.topology.empty());
  CHECK(i.particle_counts == std::array<std::int32_t, 6>{0, 0, 0, 0, 0, 0});
  CHECK(std::isnan(i.t));
  CHECK(std::isnan(i.lepton_p));
}

TEST_CASE("defaults: RunInfo, Trigger, ImageMeta3D") {
  spine::RunInfo r;
  CHECK(r.run == -1);
  CHECK(r.subrun == -1);
  CHECK(r.event == -1);

  spine::Trigger t;
  CHECK(t.id == -1);
  CHECK(t.type == -1);
  CHECK(t.time_s == -1);
  CHECK(t.time_ns == -1);
  CHECK(t.beam_time_s == -1);
  CHECK(t.beam_time_ns == -1);

  spine::ImageMeta3D m;
  CHECK(all_nan(m.lower));
  CHECK(all_nan(m.upper));
  CHECK(all_nan(m.size));
  CHECK(m.count == std::array<std::int64_t, 3>{-1, -1, -1});
}

TEST_CASE("defaults: Flash, CRTHit, FlashHypothesis") {
  spine::Flash f;
  CHECK(f.units == "cm");
  CHECK(f.id == -1);
  CHECK(f.volume_id == -1);
  CHECK(f.frame == -1);
  CHECK(f.on_beam_time == -1);
  CHECK_FALSE(f.in_beam_frame);
  CHECK(std::isnan(f.time));
  CHECK(std::isnan(f.time_width));
  CHECK(std::isnan(f.time_abs));
  CHECK(std::isnan(f.total_pe));
  CHECK(std::isnan(f.fast_to_total));
  CHECK(f.pe_per_ch.empty());
  CHECK(all_nan(f.center));
  CHECK(all_nan(f.width));

  spine::CRTHit c;
  CHECK(c.units == "cm");
  CHECK(c.id == -1);
  CHECK(c.plane == -1);
  CHECK(c.ts0_s == -1);
  CHECK(std::isnan(c.ts0_ns));
  CHECK(std::isnan(c.ts0_s_corr));
  CHECK(std::isnan(c.ts0_ns_corr));
  CHECK(std::isnan(c.ts1_ns));
  CHECK(std::isnan(c.total_pe));
  CHECK(c.tagger.empty());
  CHECK(c.feb_id.empty());
  CHECK(all_nan(c.center));
  CHECK(all_nan(c.width));
  CHECK(std::isnan(c.time));

  spine::FlashHypothesis h;
  CHECK(h.id == -1);
  CHECK(h.interaction_id == -1);
  CHECK(h.volume_id == -1);
  CHECK(std::isnan(h.score));
  CHECK_FALSE(h.is_truth);
  CHECK(h.flash_ids.empty());
  CHECK(h.pe_per_ch.empty());
  CHECK(h.total_pe == 0.0);
}

// ---------------------------------------------------------------------------------------------
// Assign pointers reach the right members
// ---------------------------------------------------------------------------------------------

TEST_CASE("assign: table rows write the member of their own name") {
  spine::RecoParticle p;
  const auto& d = spine::RecoParticle::descriptor();

  auto ids = fixed_column<std::int64_t>(ScalarType::Int64, 1, {41, 42});
  d.fields[index_of(d, "id")].assign(&p, ids, 1);
  CHECK(p.id == 42);
  CHECK(p.interaction_id == -1);  // the neighbour is untouched

  auto shapes = fixed_column<std::int64_t>(ScalarType::Int64, 1, {1});
  d.fields[index_of(d, "shape")].assign(&p, shapes, 0);
  CHECK(p.shape == spine::ParticleShape::TRACK);
  CHECK(p.pid == spine::ParticlePID::UNKNOWN);

  auto pids = fixed_column<std::int64_t>(ScalarType::Int64, 1, {2});
  d.fields[index_of(d, "pid")].assign(&p, pids, 0);
  CHECK(p.pid == spine::ParticlePID::MUON);

  auto flags = fixed_column<std::uint8_t>(ScalarType::Bool, 1, {0});
  d.fields[index_of(d, "is_valid")].assign(&p, flags, 0);
  CHECK_FALSE(p.is_valid);

  auto start = fixed_column<double>(ScalarType::Float64, 3, {1.5, 2.5, 3.5});
  d.fields[index_of(d, "start_point")].assign(&p, start, 0);
  CHECK(p.start_point == std::array<float, 3>{1.5f, 2.5f, 3.5f});
  CHECK(all_nan(p.end_point));

  auto matches = fixed_column<std::int32_t>(ScalarType::Int32, 1, {7, 8, 9});
  matches.variable = true;
  matches.rows = 1;
  matches.offsets = {0, 3};
  d.fields[index_of(d, "match_ids")].assign(&p, matches, 0);
  CHECK(p.match_ids == std::vector<std::int32_t>{7, 8, 9});

  detail::ColumnBuffer units;
  units.type = ScalarType::String;
  units.rows = 1;
  units.strings = {"px"};
  d.fields[index_of(d, "units")].assign(&p, units, 0);
  CHECK(p.units == "px");

  auto mass = fixed_column<float>(ScalarType::Float32, 1, {105.5f});
  d.fields[index_of(d, "mass")].assign(&p, mass, 0);
  CHECK(p.mass == 105.5);

  spine::RunInfo run;
  auto run_numbers = fixed_column<std::int64_t>(ScalarType::Int64, 1, {9001});
  spine::RunInfo::descriptor().find("subrun")->assign(&run, run_numbers, 0);
  CHECK(run.subrun == 9001);
  CHECK(run.run == -1);
  CHECK(run.event == -1);

  spine::CRTHit hit;
  auto feb = fixed_column<std::uint8_t>(ScalarType::UInt8, 1, {0xAB, 0xCD});
  feb.variable = true;
  feb.rows = 1;
  feb.offsets = {0, 2};
  spine::CRTHit::descriptor().find("feb_id")->assign(&hit, feb, 0);
  CHECK(hit.feb_id == std::vector<std::uint8_t>{0xAB, 0xCD});
}

// ---------------------------------------------------------------------------------------------
// Enums
// ---------------------------------------------------------------------------------------------

namespace {

template <class E>
void check_enum(std::string_view spine_name, std::size_t members) {
  const spine::EnumDescriptor& d = spine::enum_descriptor<E>();
  INFO(spine_name);
  CHECK(d.name == spine_name);
  CHECK(d.size == members);
  std::set<std::string_view> names;
  std::set<std::int64_t> values;
  for (std::size_t i = 0; i < d.size; ++i) {
    const spine::EnumMember& m = d.members[i];
    INFO(m.name);
    CHECK(names.insert(m.name).second);
    CHECK(values.insert(m.value).second);
    // value -> name -> value
    CHECK(d.name_of(m.value) == m.name);
    CHECK(spine::to_string(static_cast<E>(m.value)) == m.name);
    const auto found = std::find_if(d.members, d.members + d.size,
                                    [&](const spine::EnumMember& o) { return o.name == m.name; });
    REQUIRE(found != d.members + d.size);
    CHECK(found->value == m.value);
  }
  // A value SPINE does not define has no name.
  CHECK(d.name_of(987654321).empty());
  CHECK(spine::to_string(static_cast<E>(987654321)).empty());
  static_assert(std::is_same_v<std::underlying_type_t<E>, std::int64_t>);
}

}  // namespace

TEST_CASE("enums: every EnumDescriptor round-trips and matches SPINE's member counts") {
  using namespace spine;
  check_enum<ParticleShape>("ParticleShape", 8);
  check_enum<ParticlePID>("ParticlePID", 7);
  check_enum<NuCurrentType>("NuCurrentType", 3);
  check_enum<NuInteractionScheme>("NuInteractionScheme", 6);
  check_enum<LArSoftNuInteractionType>("LArSoftNuInteractionType", 63);
  check_enum<GenieNuInteractionType>("GenieNuInteractionType", 23);
  check_enum<GiBUUNuInteractionType>("GiBUUNuInteractionType", 20);
  check_enum<NuWroNuInteractionType>("NuWroNuInteractionType", 11);
  check_enum<NeutNuInteractionType>("NeutNuInteractionType", 56);
}

TEST_CASE("enums: values and names are SPINE's") {
  using namespace spine;
  CHECK(static_cast<std::int64_t>(ParticleShape::UNKNOWN) == -1);
  CHECK(static_cast<std::int64_t>(ParticleShape::SHOWER) == 0);
  CHECK(static_cast<std::int64_t>(ParticleShape::TRACK) == 1);
  CHECK(static_cast<std::int64_t>(ParticleShape::LOWE) == 4);
  CHECK(static_cast<std::int64_t>(ParticleShape::LARCV_UNKNOWN) == 6);
  CHECK(to_string(ParticleShape::MICHEL) == "MICHEL");
  CHECK(static_cast<std::int64_t>(ParticlePID::PROTON) == 4);
  CHECK(to_string(ParticlePID::KAON) == "KAON");
  CHECK(static_cast<std::int64_t>(NuCurrentType::NC) == 1);
  CHECK(static_cast<std::int64_t>(NuInteractionScheme::NEUT) == 4);
  CHECK(to_string(NuInteractionScheme::GIBUU) == "GIBUU");
  CHECK(static_cast<std::int64_t>(LArSoftNuInteractionType::MEC2P2H) == 1100);
  CHECK(static_cast<std::int64_t>(GenieNuInteractionType::UNKNOWN) == -100);
  CHECK(static_cast<std::int64_t>(GiBUUNuInteractionType::NC_DIS) == 650);
  CHECK(static_cast<std::int64_t>(NuWroNuInteractionType::CCHYPERON) == 100);
  CHECK(static_cast<std::int64_t>(NeutNuInteractionType::NC_DIS_NUBAR) == 675);

  // NULL is a macro: the enumerator is renamed, the descriptor keeps SPINE's spelling.
  CHECK(static_cast<std::int64_t>(GenieNuInteractionType::NULL_) == 0);
  CHECK(to_string(GenieNuInteractionType::NULL_) == "NULL");
  CHECK(enum_descriptor<GenieNuInteractionType>().name_of(0) == "NULL");

  // The alias used by the model sketch.
  CHECK(Shape::TRACK == ParticleShape::TRACK);
}

TEST_CASE("enums: interaction_mode and interaction_type tables depend on the scheme") {
  using namespace spine;
  // interaction_mode: GIBUU, NUWRO and NEUT fall back to the GENIE table in SPINE.
  CHECK(interaction_mode_enum(NuInteractionScheme::LARSOFT) ==
        &enum_descriptor<LArSoftNuInteractionType>());
  CHECK(interaction_mode_enum(NuInteractionScheme::GENIE) ==
        &enum_descriptor<GenieNuInteractionType>());
  CHECK(interaction_mode_enum(NuInteractionScheme::GIBUU) ==
        &enum_descriptor<GenieNuInteractionType>());
  CHECK(interaction_mode_enum(NuInteractionScheme::NUWRO) ==
        &enum_descriptor<GenieNuInteractionType>());
  CHECK(interaction_mode_enum(NuInteractionScheme::NEUT) ==
        &enum_descriptor<GenieNuInteractionType>());
  CHECK(interaction_mode_enum(NuInteractionScheme::UNKNOWN) == nullptr);

  CHECK(interaction_type_enum(NuInteractionScheme::LARSOFT) ==
        &enum_descriptor<LArSoftNuInteractionType>());
  CHECK(interaction_type_enum(NuInteractionScheme::GENIE) ==
        &enum_descriptor<GenieNuInteractionType>());
  CHECK(interaction_type_enum(NuInteractionScheme::GIBUU) ==
        &enum_descriptor<GiBUUNuInteractionType>());
  CHECK(interaction_type_enum(NuInteractionScheme::NUWRO) ==
        &enum_descriptor<NuWroNuInteractionType>());
  CHECK(interaction_type_enum(NuInteractionScheme::NEUT) ==
        &enum_descriptor<NeutNuInteractionType>());
  CHECK(interaction_type_enum(NuInteractionScheme::UNKNOWN) == nullptr);
  CHECK(interaction_type_enum(static_cast<NuInteractionScheme>(77)) == nullptr);
}
