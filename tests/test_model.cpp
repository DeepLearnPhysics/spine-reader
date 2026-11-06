// Tests of the generated model (include/spine/model, src/model): the structure every generated
// class has, the C++ types of the members, the assign pointers and the enum descriptors. They hold
// for any SPINE tag. The units, defaults and enum values the CAFMakers rely on are checked by
// test_contracts.cpp, and the fields against the fixtures' columns by test_golden.cpp.

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

#include "golden_members.inc"
#include "member_compare.hpp"

namespace {

using spine::ScalarType;
using spine_test::nan_equal;
namespace detail = spine::detail;

// ---------------------------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------------------------

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

/// Structure every generated class must have, whatever SPINE release it comes from.
template <class C>
void check_structure(std::string_view name) {
  const spine::ClassDescriptor& d = C::descriptor();
  INFO(name);
  CHECK(d.name == name);
  CHECK(d.schema_version == SPINE_READER_MODEL_SPINE_VERSION);
  CHECK(d.num_fields == C::kNumFields);
  CHECK(&C::descriptor() == &d);  // one descriptor per class

  std::set<std::string_view> names;
  for (std::size_t i = 0; i < d.num_fields; ++i) {
    const spine::FieldDescriptor& f = d.fields[i];
    INFO(f.name);
    CHECK(names.insert(f.name).second);  // unique
    CHECK(f.assign != nullptr);          // assignable
    CHECK(!f.name.empty());
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
  mask.set(C::kNumFields - 1);
  mask.set(0);
  detail::ObjectAccess::set_present(object, mask);
  CHECK(object.has(static_cast<typename C::Field>(C::kNumFields - 1)));
  CHECK(object.has(static_cast<typename C::Field>(0)));
  if (C::kNumFields > 2) CHECK_FALSE(object.has(static_cast<typename C::Field>(1)));
}

}  // namespace

// ---------------------------------------------------------------------------------------------
// Inventory and structure
// ---------------------------------------------------------------------------------------------

TEST_CASE("model: SPINE_READER_FOR_EACH_CLASS lists every class once") {
  std::vector<std::string_view> names;
#define COLLECT(Class) names.push_back(#Class);
  SPINE_READER_FOR_EACH_CLASS(COLLECT)
#undef COLLECT
  CHECK_FALSE(names.empty());
  CHECK(std::set<std::string_view>(names.begin(), names.end()).size() == names.size());

  // Each name in the macro is a class whose descriptor carries that name.
  std::vector<std::string_view> described;
#define DESCRIBE(Class) described.push_back(spine::Class::descriptor().name);
  SPINE_READER_FOR_EACH_CLASS(DESCRIBE)
#undef DESCRIBE
  CHECK(described == names);
}

TEST_CASE("model: descriptor structure of every class") {
#define CHECK_STRUCTURE(Class) check_structure<spine::Class>(#Class);
  SPINE_READER_FOR_EACH_CLASS(CHECK_STRUCTURE)
#undef CHECK_STRUCTURE
}

TEST_CASE("model: field i has the name of enumerator i, for every field of every class") {
  // golden_members.inc lists every member of every class, from the schema
#define CHECK_FIELD_NAME(member)                                                  \
  CHECK(d.fields[static_cast<std::size_t>(Cls::Field::member)].name == #member); \
  ++listed;
#define CHECK_CLASS_FIELD_NAMES(Class)                   \
  {                                                      \
    using Cls = spine::Class;                            \
    const spine::ClassDescriptor& d = Cls::descriptor(); \
    INFO(#Class);                                        \
    std::size_t listed = 0;                              \
    GOLDEN_MEMBERS_##Class(CHECK_FIELD_NAME)             \
    CHECK(listed == Cls::kNumFields);                    \
  }
  SPINE_READER_FOR_EACH_CLASS(CHECK_CLASS_FIELD_NAMES)
#undef CHECK_CLASS_FIELD_NAMES
#undef CHECK_FIELD_NAME
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

TEST_CASE("model: enum-typed fields point at the generated EnumDescriptors") {
  using spine::RecoParticle;
  using spine::TruthInteraction;
  const auto& rp = RecoParticle::descriptor();
  const auto& ti = TruthInteraction::descriptor();
  CHECK(rp.find("shape")->enumeration == &spine::enum_descriptor<spine::ParticleShape>());
  CHECK(rp.find("pid")->enumeration == &spine::enum_descriptor<spine::ParticlePID>());
  CHECK(rp.find("chi2_pid")->enumeration == &spine::enum_descriptor<spine::ParticlePID>());
  CHECK(ti.find("current_type")->enumeration == &spine::enum_descriptor<spine::NuCurrentType>());
  CHECK(rp.find("id")->enumeration == nullptr);
  CHECK(rp.find("is_valid")->enumeration == nullptr);  // bool, never an enum
}

// ---------------------------------------------------------------------------------------------
// Assign pointers reach the right members
// ---------------------------------------------------------------------------------------------

TEST_CASE("assign: table rows write the member of their own name") {
  spine::RecoParticle p;
  const auto& d = spine::RecoParticle::descriptor();

  auto ids = fixed_column<std::int64_t>(ScalarType::Int64, 1, {41, 42});
  d.fields[index_of(d, "id")].assign(&p, ids, 1);
  CHECK(p.interaction_id == spine::RecoParticle{}.interaction_id);  // the neighbour is untouched

  const auto track = static_cast<std::int64_t>(spine::ParticleShape::TRACK);
  auto shapes = fixed_column<std::int64_t>(ScalarType::Int64, 1, {track});
  d.fields[index_of(d, "shape")].assign(&p, shapes, 0);
  CHECK(p.shape == spine::ParticleShape::TRACK);
  CHECK(p.pid == spine::RecoParticle{}.pid);

  const auto muon = static_cast<std::int64_t>(spine::ParticlePID::MUON);
  auto pids = fixed_column<std::int64_t>(ScalarType::Int64, 1, {muon});
  d.fields[index_of(d, "pid")].assign(&p, pids, 0);
  CHECK(p.pid == spine::ParticlePID::MUON);

  auto flags = fixed_column<std::uint8_t>(ScalarType::Bool, 1, {0});
  d.fields[index_of(d, "is_valid")].assign(&p, flags, 0);
  CHECK_FALSE(p.is_valid);

  auto start = fixed_column<double>(ScalarType::Float64, 3, {1.5, 2.5, 3.5});
  d.fields[index_of(d, "start_point")].assign(&p, start, 0);
  CHECK(p.start_point == std::array<float, 3>{1.5f, 2.5f, 3.5f});
  CHECK(nan_equal(p.end_point, spine::RecoParticle{}.end_point));

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
  CHECK(run.run == spine::RunInfo{}.run);
  CHECK(run.event == spine::RunInfo{}.event);

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
void check_enum(std::string_view spine_name) {
  const spine::EnumDescriptor& d = spine::enum_descriptor<E>();
  INFO(spine_name);
  CHECK(d.name == spine_name);
  CHECK(d.size > 0);
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

TEST_CASE("enums: every EnumDescriptor round-trips") {
  using namespace spine;
  check_enum<ParticleShape>("ParticleShape");
  check_enum<ParticlePID>("ParticlePID");
  check_enum<NuCurrentType>("NuCurrentType");
  check_enum<NuInteractionScheme>("NuInteractionScheme");
  check_enum<LArSoftNuInteractionType>("LArSoftNuInteractionType");
  check_enum<GenieNuInteractionType>("GenieNuInteractionType");
  check_enum<GiBUUNuInteractionType>("GiBUUNuInteractionType");
  check_enum<NuWroNuInteractionType>("NuWroNuInteractionType");
  check_enum<NeutNuInteractionType>("NeutNuInteractionType");
}

TEST_CASE("enums: reserved names are renamed, and Shape is ParticleShape") {
  using namespace spine;
  // NULL is a macro: the enumerator is renamed, the descriptor keeps SPINE's spelling.
  CHECK(static_cast<std::int64_t>(GenieNuInteractionType::NULL_) == 0);
  CHECK(to_string(GenieNuInteractionType::NULL_) == "NULL");
  CHECK(enum_descriptor<GenieNuInteractionType>().name_of(0) == "NULL");

  // The alias used by the model sketch.
  CHECK(Shape::TRACK == ParticleShape::TRACK);
}

TEST_CASE("enums: the interaction tables of every scheme are generated descriptors") {
  // Which table a scheme selects is SPINE's choice and may change; that each scheme selects one
  // of the generated tables or none, and that a value outside the enum selects none, does not.
  using namespace spine;
  const std::set<const EnumDescriptor*> tables = {
      &enum_descriptor<LArSoftNuInteractionType>(), &enum_descriptor<GenieNuInteractionType>(),
      &enum_descriptor<GiBUUNuInteractionType>(), &enum_descriptor<NuWroNuInteractionType>(),
      &enum_descriptor<NeutNuInteractionType>()};
  const EnumDescriptor& schemes = enum_descriptor<NuInteractionScheme>();
  std::int64_t outside = 0;
  for (std::size_t i = 0; i < schemes.size; ++i) {
    const EnumMember& m = schemes.members[i];
    INFO(m.name);
    const auto scheme = static_cast<NuInteractionScheme>(m.value);
    for (const EnumDescriptor* table :
         {interaction_mode_enum(scheme), interaction_type_enum(scheme)}) {
      CHECK((table == nullptr || tables.count(table) == 1));
    }
    outside = std::max(outside, m.value + 1);
  }
  CHECK(interaction_mode_enum(static_cast<NuInteractionScheme>(outside)) == nullptr);
  CHECK(interaction_type_enum(static_cast<NuInteractionScheme>(outside)) == nullptr);
}
