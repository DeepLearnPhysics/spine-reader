// Column introspection: types, counts, the variable flag and enum labels, in both layouts.

#include <doctest.h>

#include <map>

#include "core_test_util.hpp"

using namespace spine;
using namespace core_test;
using spine::core::FileEnumMember;
using spine::core::ProductKind;

namespace {

std::map<std::string, std::int64_t> labels(const ColumnInfo& column) {
  std::map<std::string, std::int64_t> out;
  for (const FileEnumMember& member : column.enum_members) out[member.name] = member.value;
  return out;
}

struct Expect {
  ScalarType type;
  std::size_t count;
  bool variable;
};

/// Checks the columns of reco_particles that the SPINE v1.4.0 sample must have, in V1 and V2.
void check_reco_particle_columns(ProductSource& product) {
  const core::ProductInfo& info = product.info();
  CHECK(info.kind == ProductKind::Objects);
  CHECK(info.class_name == "RecoParticle");
  CHECK_FALSE(info.scalar);
  CHECK(info.columns.size() == 55);  // the class stores 55 fields; no _var_offsets_ helpers

  const std::map<std::string, Expect> expect = {
      {"is_truth", {ScalarType::Bool, 1, false}},
      {"id", {ScalarType::Int64, 1, false}},
      {"cathode_offset", {ScalarType::Float64, 1, false}},
      {"shape", {ScalarType::Int64, 1, false}},
      {"pid", {ScalarType::Int64, 1, false}},
      {"start_point", {ScalarType::Float32, 3, false}},
      {"chi2_per_pid", {ScalarType::Float32, 6, false}},
      {"primary_scores", {ScalarType::Float32, 2, false}},
      {"momentum", {ScalarType::Float64, 3, false}},  // the class says float32; the file wins
      {"units", {ScalarType::String, 1, true}},
      {"index", {ScalarType::Int32, 0, true}},
      {"match_ids", {ScalarType::Int32, 0, true}},
      {"match_overlaps", {ScalarType::Float32, 0, true}},
      {"module_ids", {ScalarType::Int32, 0, true}},
      {"best_match_id", {ScalarType::Int64, 1, false}},
  };
  for (const auto& [name, want] : expect) {
    const ColumnInfo* column = info.find(name);
    REQUIRE_MESSAGE(column != nullptr, name);
    CHECK_MESSAGE(column->type == want.type, name);
    CHECK_MESSAGE(column->count == want.count, name);
    CHECK_MESSAGE(column->variable == want.variable, name);
  }
  CHECK(info.find("_var_offsets_0") == nullptr);
  CHECK(info.find("_var_offsets_1") == nullptr);
  CHECK(info.find("no_such_column") == nullptr);

  // enum labels come from the file
  const ColumnInfo& shape = *info.find("shape");
  using Labels = std::map<std::string, std::int64_t>;
  CHECK(labels(shape) == Labels{{"UNKNOWN", -1}, {"SHOWER", 0}, {"TRACK", 1}, {"MICHEL", 2},
                                {"DELTA", 3}, {"LOWE", 4}, {"GHOST", 5}, {"LARCV_UNKNOWN", 6}});
  const ColumnInfo& pid = *info.find("pid");
  CHECK(labels(pid) == Labels{{"UNKNOWN", -1}, {"PHOTON", 0}, {"ELECTRON", 1}, {"MUON", 2},
                              {"PION", 3}, {"PROTON", 4}, {"KAON", 5}});
  CHECK(info.find("chi2_pid")->enum_members.size() == 7);
  // SPINE's bool enum reports Bool (and the file's two labels); plain numbers report none
  CHECK(labels(*info.find("is_truth")) == Labels{{"FALSE", 0}, {"TRUE", 1}});
  CHECK(info.find("id")->enum_members.empty());
  CHECK(info.find("start_point")->enum_members.empty());
}

}  // namespace

TEST_CASE("columns of an object product, V1") {
  Hdf5Lock lock;
  auto file = open("spine_v1_events.h5");
  check_reco_particle_columns(file->product("reco_particles"));
}

TEST_CASE("columns of an object product, V2: pool fields are columns, offsets are hidden") {
  Hdf5Lock lock;
  auto file = open("spine_v2_events.h5");
  check_reco_particle_columns(file->product("reco_particles"));
  // fixed columns first (file order), then the pool fields, pool by pool
  const core::ProductInfo& info = file->product("reco_particles").info();
  CHECK(info.columns.front().name == "is_truth");
  CHECK(info.columns.back().name == "crt_scores");  // last field of pool_2
  CHECK(info.columns[info.columns.size() - 3].name == "match_overlaps");
}

TEST_CASE("V1 and V2 describe every object product with the same columns") {
  Hdf5Lock lock;
  auto v1 = open("spine_v1_events.h5");
  auto v2 = open("spine_v2_events.h5");
  for (const char* name : {"reco_particles", "reco_interactions", "truth_particles",
                           "truth_interactions", "flashes", "crthits", "run_info", "meta",
                           "trigger"}) {
    const core::ProductInfo& a = v1->product(name).info();
    const core::ProductInfo& b = v2->product(name).info();
    CAPTURE(name);
    CHECK(a.kind == b.kind);
    CHECK(a.class_name == b.class_name);
    CHECK(a.scalar == b.scalar);
    REQUIRE(a.columns.size() == b.columns.size());
    for (const ColumnInfo& column : a.columns) {
      const ColumnInfo* other = b.find(column.name);
      REQUIRE_MESSAGE(other != nullptr, column.name);
      CHECK_MESSAGE(column.type == other->type, column.name);
      CHECK_MESSAGE(column.count == other->count, column.name);
      CHECK_MESSAGE(column.variable == other->variable, column.name);
      CHECK_MESSAGE(labels(column) == labels(*other), column.name);
    }
  }
}

TEST_CASE("strings, scalars and arrays: kinds, counts and the single column ''") {
  Hdf5Lock lock;
  for (const char* name : {"spine_v1_events.h5", "spine_v2_events.h5"}) {
    CAPTURE(name);
    auto file = open(name);

    const core::ProductInfo& run_info = file->product("run_info").info();
    CHECK(run_info.scalar);
    CHECK(run_info.columns.size() == 3);
    CHECK(run_info.find("run")->type == ScalarType::Int64);

    const core::ProductInfo& meta = file->product("meta").info();
    CHECK(meta.class_name == "ImageMeta3D");
    CHECK(meta.scalar);
    CHECK(meta.find("lower")->type == ScalarType::Float32);
    CHECK(meta.find("lower")->count == 3);
    CHECK(meta.find("count")->type == ScalarType::Int64);
    CHECK(meta.find("count")->count == 3);

    const core::ProductInfo& index = file->product("index").info();
    CHECK(index.kind == ProductKind::Array);
    CHECK(index.scalar);
    CHECK(index.class_name.empty());
    REQUIRE(index.columns.size() == 1);
    CHECK(index.columns[0].name.empty());
    CHECK(index.columns[0].type == ScalarType::Int64);
    CHECK(index.columns[0].count == 1);
    CHECK_FALSE(index.columns[0].variable);

    const core::ProductInfo& points = file->product("points").info();
    CHECK(points.kind == ProductKind::Array);
    CHECK_FALSE(points.scalar);
    CHECK(points.columns[0].type == ScalarType::Float32);
    CHECK(points.columns[0].count == 3);  // (N, 3): the width is the count
    CHECK(file->product("depositions").info().columns[0].count == 1);
  }
}

TEST_CASE("exotic products: strings, 2-D arrays, lists") {
  Hdf5Lock lock;
  for (const char* name : {"spine_v1_exotic.h5", "spine_v2_exotic.h5"}) {
    CAPTURE(name);
    auto file = open(name);

    const core::ProductInfo& label = file->product("label").info();
    CHECK(label.kind == ProductKind::String);
    CHECK(label.scalar);
    REQUIRE(label.columns.size() == 1);
    CHECK(label.columns[0].type == ScalarType::String);
    CHECK(label.columns[0].variable);

    const core::ProductInfo& wide = file->product("arr_f32_n3").info();
    CHECK(wide.kind == ProductKind::Array);
    CHECK(wide.columns[0].type == ScalarType::Float32);
    CHECK(wide.columns[0].count == 3);
    CHECK(file->product("arr_i64").info().columns[0].type == ScalarType::Int64);
    CHECK(file->product("scalar_float").info().columns[0].type == ScalarType::Float64);

    // Lists are described but not readable.
    const core::ProductInfo& same = file->product("lists_same_width").info();
    CHECK(same.kind == ProductKind::List);
    REQUIRE(same.columns.size() == 1);
    CHECK(same.columns[0].type == ScalarType::Int64);
    const core::ProductInfo& mixed = file->product("lists_mixed_width").info();
    CHECK(mixed.kind == ProductKind::MultiList);
    REQUIRE(mixed.columns.size() == 3);
    CHECK(mixed.columns[0].name == "element_0");
    CHECK(mixed.columns[0].count == 3);
    CHECK(mixed.columns[1].count == 1);
    CHECK(mixed.columns[2].count == 2);
  }
}

TEST_CASE("lite and litified files simply have fewer columns") {
  Hdf5Lock lock;
  for (const char* name : {"spine_v1_events_lite.h5", "spine_v2_events_lite.h5"}) {
    CAPTURE(name);
    auto file = open(name);
    const core::ProductInfo& info = file->product("reco_particles").info();
    CHECK(info.find("index") == nullptr);  // dropped by the lite writer
    CHECK(info.find("orig_index") == nullptr);
    CHECK(info.find("match_ids") != nullptr);
    CHECK(info.columns.size() == 53);
  }

  // litified (fixed_only): no pools at all, so no strings or variable-length columns
  auto litified = open("spine_v2_events_litified.h5");
  const core::ProductInfo& info = litified->product("reco_particles").info();
  CHECK(info.columns.size() == 44);
  for (const ColumnInfo& column : info.columns) {
    CHECK_MESSAGE(!column.variable, column.name);
    CHECK_MESSAGE(column.type != ScalarType::String, column.name);
  }
  CHECK(info.find("units") == nullptr);
  CHECK(info.find("start_point") != nullptr);
}

TEST_CASE("a SPINE v0.16.3 file differs from v1.4.0 in its schema, not its layout") {
  Hdf5Lock lock;
  auto old = open("mini_v2_v0.16.3.h5");
  auto current = open("mini_v2_v1.4.0.h5");
  const core::ProductInfo& a = old->product("reco_particles").info();
  const core::ProductInfo& b = current->product("reco_particles").info();
  CHECK(a.find("depositions_q_sum") == nullptr);  // added in v0.17.0
  CHECK(b.find("depositions_q_sum") != nullptr);
  CHECK(a.find("chi2_pid")->enum_members.empty());  // a plain integer until v0.17.3
  CHECK(b.find("chi2_pid")->enum_members.size() == 7);
  CHECK(a.find("chi2_pid")->type == ScalarType::Int64);
  CHECK(a.columns.size() + 1 == b.columns.size());
}
