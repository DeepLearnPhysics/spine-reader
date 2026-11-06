// Consumer contracts: what the CAFMakers rely on. A failure means a maintainer reviews the SPINE
// change with the CAFMaker maintainers before a release.
//
// kFields has one row per model field a CAFMaker reads: class, field, unit, the default as a JSON
// literal (as tools/fixtures writes values: floats "nan", "inf" and "-inf"), and the CAFMakers
// that read it. The rows were written once, from a scan of the ND, SBN and FD ports on
// 2026-11-05 (DUNE/ND_CAFMaker#213, justinjmueller/sbn_ml_cafmaker#8, DUNE/dune_ml_cafmaker#1) and
// the units and defaults of SPINE 1.4.0: 228 fields. After that a row changes only by hand: add
// one when a CAFMaker starts reading a field
// (tools/release/scan_consumer_fields.py lists the fields a CAFMaker build reads that have no
// row), and edit one only after talking to the CAFMaker maintainers.
//
// Every test case name starts with "contract: ". ctest runs them as their own entry,
// spine_reader_contracts.

#include <doctest.h>
#include <json.hpp>

#include <cstdint>
#include <initializer_list>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <spine/model.hpp>

#include "golden_members.inc"
#include "member_compare.hpp"

namespace {

using nlohmann::json;

// std::string_view, not const char*: doctest prints a char pointer as an address.
struct FieldContract {
  std::string_view class_name;
  std::string_view field;
  std::string_view unit;          ///< the descriptor's units; "" for none
  std::string_view default_json;  ///< the member of a default-constructed object
  std::string_view readers;       ///< the CAFMakers that read it
};

// One row per field: {"Class", "field", "unit", R"(default)", "readers"}, with the class and the
// field on the row's first line. scan_consumer_fields.py --contracts reads them from there.
const FieldContract kFields[] = {
    // RecoParticle
    {"RecoParticle", "is_truth", "", R"(false)", "SBN"},
    {"RecoParticle", "id", "", R"(-1)", "ND SBN"},
    {"RecoParticle", "is_contained", "", R"(false)", "ND SBN FD"},
    {"RecoParticle", "is_time_contained", "", R"(false)", "SBN"},
    {"RecoParticle", "is_cathode_crosser", "", R"(false)", "SBN"},
    {"RecoParticle", "is_matched", "", R"(false)", "SBN"},
    {"RecoParticle", "cathode_offset", "cm", R"("nan")", "SBN"},
    {"RecoParticle", "match_ids", "", R"([])", "ND SBN FD"},
    {"RecoParticle", "match_overlaps", "", R"([])", "ND SBN FD"},
    {"RecoParticle", "interaction_id", "", R"(-1)", "ND SBN"},
    {"RecoParticle", "chi2_pid", "", R"(-1)", "SBN"},
    {"RecoParticle", "is_primary", "", R"(false)", "ND SBN FD"},
    {"RecoParticle", "is_valid", "", R"(true)", "SBN"},
    {"RecoParticle", "length", "instance", R"("nan")", "SBN"},
    {"RecoParticle", "calo_ke", "MeV", R"("nan")", "ND SBN"},
    {"RecoParticle", "csda_ke", "MeV", R"("nan")", "ND SBN"},
    {"RecoParticle", "mcs_ke", "MeV", R"("nan")", "ND SBN"},
    {"RecoParticle", "shape", "", R"(-1)", "ND SBN"},
    {"RecoParticle", "pid", "", R"(-1)", "SBN"},
    {"RecoParticle", "fragment_ids", "instance", R"([])", "SBN"},
    {"RecoParticle", "start_point", "instance", R"(["nan", "nan", "nan"])", "ND SBN FD"},
    {"RecoParticle", "end_point", "instance", R"(["nan", "nan", "nan"])", "ND SBN FD"},
    {"RecoParticle", "chi2_per_pid", "", R"(["nan", "nan", "nan", "nan", "nan", "nan"])", "SBN"},
    {"RecoParticle", "csda_ke_per_pid", "MeV",
     R"(["nan", "nan", "nan", "nan", "nan", "nan"])", "SBN"},
    {"RecoParticle", "mcs_ke_per_pid", "MeV",
     R"(["nan", "nan", "nan", "nan", "nan", "nan"])", "SBN"},
    {"RecoParticle", "start_dedx", "MeV/cm", R"("nan")", "SBN"},
    {"RecoParticle", "vertex_distance", "instance", R"("nan")", "SBN"},
    {"RecoParticle", "start_straightness", "", R"("nan")", "SBN"},
    {"RecoParticle", "directional_spread", "", R"("nan")", "SBN"},
    {"RecoParticle", "axial_spread", "", R"("nan")", "SBN"},
    {"RecoParticle", "start_dir", "", R"(["nan", "nan", "nan"])", "ND SBN"},
    {"RecoParticle", "end_dir", "", R"(["nan", "nan", "nan"])", "ND SBN"},
    {"RecoParticle", "pid_scores", "", R"(["nan", "nan", "nan", "nan", "nan", "nan"])", "SBN"},
    {"RecoParticle", "primary_scores", "", R"(["nan", "nan"])", "SBN"},
    {"RecoParticle", "ppn_ids", "", R"([])", "SBN"},
    {"RecoParticle", "pdg_code", "", R"(0)", "ND SBN FD"},
    {"RecoParticle", "mass", "MeV/c^2", R"("nan")", "ND SBN"},
    {"RecoParticle", "ke", "MeV", R"("nan")", "SBN FD"},
    {"RecoParticle", "momentum", "MeV/c", R"(["nan", "nan", "nan"])", "ND SBN FD"},
    {"RecoParticle", "p", "MeV/c", R"("nan")", "SBN"},
    {"RecoParticle", "num_fragments", "", R"(0)", "SBN"},
    {"RecoParticle", "size", "", R"(0)", "SBN"},
    {"RecoParticle", "depositions_sum", "", R"(0.0)", "SBN"},
    {"RecoParticle", "module_ids", "", R"([])", "SBN"},

    // TruthParticle
    {"TruthParticle", "orig_id", "", R"(-1)", "SBN"},
    {"TruthParticle", "is_truth", "", R"(true)", "SBN"},
    {"TruthParticle", "id", "", R"(-1)", "ND SBN FD"},
    {"TruthParticle", "is_contained", "", R"(false)", "SBN"},
    {"TruthParticle", "is_time_contained", "", R"(false)", "SBN"},
    {"TruthParticle", "is_cathode_crosser", "", R"(false)", "SBN"},
    {"TruthParticle", "is_matched", "", R"(false)", "SBN"},
    {"TruthParticle", "cathode_offset", "cm", R"("nan")", "SBN"},
    {"TruthParticle", "match_ids", "", R"([])", "SBN"},
    {"TruthParticle", "match_overlaps", "", R"([])", "SBN"},
    {"TruthParticle", "interaction_id", "", R"(-1)", "ND SBN FD"},
    {"TruthParticle", "is_primary", "", R"(false)", "ND SBN"},
    {"TruthParticle", "is_valid", "", R"(true)", "SBN"},
    {"TruthParticle", "length", "instance", R"("nan")", "SBN"},
    {"TruthParticle", "calo_ke", "MeV", R"("nan")", "SBN"},
    {"TruthParticle", "csda_ke", "MeV", R"("nan")", "SBN"},
    {"TruthParticle", "mcs_ke", "MeV", R"("nan")", "SBN"},
    {"TruthParticle", "shape", "", R"(-1)", "SBN"},
    {"TruthParticle", "pid", "", R"(-1)", "SBN"},
    {"TruthParticle", "fragment_ids", "instance", R"([])", "SBN"},
    {"TruthParticle", "start_point", "instance", R"(["nan", "nan", "nan"])", "SBN"},
    {"TruthParticle", "end_point", "instance", R"(["nan", "nan", "nan"])", "SBN"},
    {"TruthParticle", "csda_ke_per_pid", "MeV",
     R"(["nan", "nan", "nan", "nan", "nan", "nan"])", "SBN"},
    {"TruthParticle", "mcs_ke_per_pid", "MeV",
     R"(["nan", "nan", "nan", "nan", "nan", "nan"])", "SBN"},
    {"TruthParticle", "parent_id", "", R"(-1)", "SBN"},
    {"TruthParticle", "group_id", "", R"(-1)", "SBN"},
    {"TruthParticle", "nu_id", "", R"(-1)", "SBN"},
    {"TruthParticle", "children_id", "", R"([])", "SBN"},
    {"TruthParticle", "mct_index", "", R"(-1)", "SBN"},
    {"TruthParticle", "mcst_index", "", R"(-1)", "SBN"},
    {"TruthParticle", "group_primary", "", R"(-1)", "SBN"},
    {"TruthParticle", "interaction_primary", "", R"(-1)", "SBN"},
    {"TruthParticle", "track_id", "", R"(-1)", "ND SBN FD"},
    {"TruthParticle", "parent_track_id", "", R"(-1)", "SBN"},
    {"TruthParticle", "ancestor_track_id", "", R"(-1)", "SBN"},
    {"TruthParticle", "pdg_code", "", R"(0)", "ND SBN FD"},
    {"TruthParticle", "parent_pdg_code", "", R"(0)", "SBN"},
    {"TruthParticle", "ancestor_pdg_code", "", R"(0)", "SBN"},
    {"TruthParticle", "num_voxels", "", R"(-1)", "SBN"},
    {"TruthParticle", "t", "ns", R"("nan")", "SBN"},
    {"TruthParticle", "end_t", "ns", R"("nan")", "SBN"},
    {"TruthParticle", "parent_t", "ns", R"("nan")", "SBN"},
    {"TruthParticle", "ancestor_t", "ns", R"("nan")", "SBN"},
    {"TruthParticle", "energy_init", "MeV", R"("nan")", "ND SBN FD"},
    {"TruthParticle", "energy_deposit", "MeV", R"("nan")", "SBN"},
    {"TruthParticle", "distance_travel", "cm", R"("nan")", "SBN"},
    {"TruthParticle", "creation_process", "", R"("")", "SBN"},
    {"TruthParticle", "parent_creation_process", "", R"("")", "SBN"},
    {"TruthParticle", "ancestor_creation_process", "", R"("")", "SBN"},
    {"TruthParticle", "position", "instance", R"(["nan", "nan", "nan"])", "ND SBN FD"},
    {"TruthParticle", "end_position", "instance", R"(["nan", "nan", "nan"])", "ND SBN FD"},
    {"TruthParticle", "parent_position", "instance", R"(["nan", "nan", "nan"])", "SBN"},
    {"TruthParticle", "ancestor_position", "instance", R"(["nan", "nan", "nan"])", "SBN"},
    {"TruthParticle", "first_step", "instance", R"(["nan", "nan", "nan"])", "SBN"},
    {"TruthParticle", "last_step", "instance", R"(["nan", "nan", "nan"])", "SBN"},
    {"TruthParticle", "momentum", "MeV/c", R"(["nan", "nan", "nan"])", "ND SBN FD"},
    {"TruthParticle", "end_momentum", "MeV/c", R"(["nan", "nan", "nan"])", "SBN"},
    {"TruthParticle", "orig_interaction_id", "", R"(-1)", "ND SBN"},
    {"TruthParticle", "orig_parent_id", "", R"(-1)", "SBN"},
    {"TruthParticle", "orig_group_id", "", R"(-1)", "SBN"},
    {"TruthParticle", "reco_length", "", R"("nan")", "SBN"},
    {"TruthParticle", "orig_children_id", "", R"([])", "SBN"},
    {"TruthParticle", "children_counts", "", R"([])", "SBN"},
    {"TruthParticle", "reco_start_dir", "", R"(["nan", "nan", "nan"])", "SBN"},
    {"TruthParticle", "reco_end_dir", "", R"(["nan", "nan", "nan"])", "SBN"},
    {"TruthParticle", "start_dir", "", R"(["nan", "nan", "nan"])", "SBN"},
    {"TruthParticle", "end_dir", "", R"(["nan", "nan", "nan"])", "SBN"},
    {"TruthParticle", "ke", "MeV", R"("nan")", "SBN"},
    {"TruthParticle", "reco_ke", "MeV", R"("nan")", "SBN"},
    {"TruthParticle", "reco_momentum", "MeV/c", R"(["nan", "nan", "nan"])", "SBN"},
    {"TruthParticle", "p", "MeV/c", R"("nan")", "SBN"},
    {"TruthParticle", "end_p", "MeV/c", R"("nan")", "SBN"},
    {"TruthParticle", "mass", "MeV/c^2", R"("nan")", "SBN"},
    {"TruthParticle", "num_fragments", "", R"(0)", "SBN"},
    {"TruthParticle", "size", "", R"(0)", "SBN"},
    {"TruthParticle", "depositions_sum", "", R"(0.0)", "SBN"},
    {"TruthParticle", "depositions_q_sum", "", R"(0.0)", "SBN"},
    {"TruthParticle", "module_ids", "", R"([])", "SBN"},
    {"TruthParticle", "size_adapt", "", R"(0)", "SBN"},
    {"TruthParticle", "size_g4", "", R"(0)", "SBN"},
    {"TruthParticle", "depositions_adapt_sum", "", R"(0.0)", "SBN"},
    {"TruthParticle", "depositions_adapt_q_sum", "", R"(0.0)", "SBN"},
    {"TruthParticle", "depositions_g4_sum", "", R"(0.0)", "SBN"},

    // RecoInteraction
    {"RecoInteraction", "is_truth", "", R"(false)", "SBN"},
    {"RecoInteraction", "id", "", R"(-1)", "ND SBN FD"},
    {"RecoInteraction", "is_contained", "", R"(false)", "SBN"},
    {"RecoInteraction", "is_time_contained", "", R"(false)", "SBN"},
    {"RecoInteraction", "is_cathode_crosser", "", R"(false)", "SBN"},
    {"RecoInteraction", "is_matched", "", R"(false)", "SBN"},
    {"RecoInteraction", "cathode_offset", "cm", R"("nan")", "SBN"},
    {"RecoInteraction", "match_ids", "", R"([])", "ND SBN FD"},
    {"RecoInteraction", "match_overlaps", "", R"([])", "ND SBN FD"},
    {"RecoInteraction", "is_fiducial", "", R"(false)", "SBN"},
    {"RecoInteraction", "is_flash_matched", "", R"(false)", "SBN"},
    {"RecoInteraction", "flash_total_pe", "", R"("nan")", "ND SBN"},
    {"RecoInteraction", "flash_hypo_pe", "", R"("nan")", "ND SBN"},
    {"RecoInteraction", "particle_ids", "", R"([])", "SBN FD"},
    {"RecoInteraction", "vertex", "instance", R"(["nan", "nan", "nan"])", "ND SBN FD"},
    {"RecoInteraction", "flash_ids", "", R"([])", "SBN"},
    {"RecoInteraction", "flash_volume_ids", "", R"([])", "SBN"},
    {"RecoInteraction", "flash_times", "us", R"([])", "SBN"},
    {"RecoInteraction", "flash_scores", "", R"([])", "SBN"},
    {"RecoInteraction", "primary_particle_ids", "", R"([])", "SBN"},
    {"RecoInteraction", "num_particles", "", R"(0)", "SBN"},
    {"RecoInteraction", "num_primary_particles", "", R"(0)", "SBN"},
    {"RecoInteraction", "particle_counts", "", R"([0, 0, 0, 0, 0, 0])", "SBN"},
    {"RecoInteraction", "primary_particle_counts", "", R"([0, 0, 0, 0, 0, 0])", "SBN"},
    {"RecoInteraction", "topology", "", R"("")", "SBN"},
    {"RecoInteraction", "size", "", R"(0)", "SBN"},
    {"RecoInteraction", "depositions_sum", "", R"(0.0)", "SBN"},
    {"RecoInteraction", "module_ids", "", R"([])", "SBN"},

    // TruthInteraction
    {"TruthInteraction", "orig_id", "", R"(-1)", "ND SBN"},
    {"TruthInteraction", "is_truth", "", R"(true)", "SBN"},
    {"TruthInteraction", "id", "", R"(-1)", "ND SBN FD"},
    {"TruthInteraction", "is_contained", "", R"(false)", "SBN"},
    {"TruthInteraction", "is_time_contained", "", R"(false)", "SBN"},
    {"TruthInteraction", "is_cathode_crosser", "", R"(false)", "SBN"},
    {"TruthInteraction", "is_matched", "", R"(false)", "SBN"},
    {"TruthInteraction", "cathode_offset", "cm", R"("nan")", "SBN"},
    {"TruthInteraction", "match_ids", "", R"([])", "SBN"},
    {"TruthInteraction", "match_overlaps", "", R"([])", "SBN"},
    {"TruthInteraction", "is_fiducial", "", R"(false)", "SBN"},
    {"TruthInteraction", "is_flash_matched", "", R"(false)", "SBN"},
    {"TruthInteraction", "flash_total_pe", "", R"("nan")", "SBN"},
    {"TruthInteraction", "flash_hypo_pe", "", R"("nan")", "SBN"},
    {"TruthInteraction", "particle_ids", "", R"([])", "SBN FD"},
    {"TruthInteraction", "vertex", "instance", R"(["nan", "nan", "nan"])", "SBN FD"},
    {"TruthInteraction", "flash_ids", "", R"([])", "SBN"},
    {"TruthInteraction", "flash_volume_ids", "", R"([])", "SBN"},
    {"TruthInteraction", "flash_times", "us", R"([])", "SBN"},
    {"TruthInteraction", "flash_scores", "", R"([])", "SBN"},
    {"TruthInteraction", "current_type", "", R"(-1)", "SBN"},
    {"TruthInteraction", "interaction_mode", "", R"(-1)", "SBN"},
    {"TruthInteraction", "interaction_type", "", R"(-1)", "SBN"},
    {"TruthInteraction", "mct_index", "", R"(-1)", "SBN"},
    {"TruthInteraction", "track_id", "", R"(-1)", "SBN"},
    {"TruthInteraction", "lepton_track_id", "", R"(-1)", "SBN"},
    {"TruthInteraction", "pdg_code", "", R"(0)", "SBN FD"},
    {"TruthInteraction", "lepton_pdg_code", "", R"(0)", "SBN"},
    {"TruthInteraction", "target", "", R"(0)", "SBN"},
    {"TruthInteraction", "nucleon", "", R"(0)", "SBN"},
    {"TruthInteraction", "quark", "", R"(0)", "SBN"},
    {"TruthInteraction", "energy_init", "GeV", R"("nan")", "SBN FD"},
    {"TruthInteraction", "hadronic_invariant_mass", "GeV/c^2", R"("nan")", "SBN"},
    {"TruthInteraction", "momentum_transfer", "(GeV/c)^2", R"("nan")", "SBN"},
    {"TruthInteraction", "momentum_transfer_mag", "GeV/c", R"("nan")", "SBN"},
    {"TruthInteraction", "energy_transfer", "GeV", R"("nan")", "SBN"},
    {"TruthInteraction", "lepton_p", "GeV/c", R"("nan")", "SBN"},
    {"TruthInteraction", "distance_travel", "instance", R"("nan")", "SBN"},
    {"TruthInteraction", "theta", "rad", R"("nan")", "SBN"},
    {"TruthInteraction", "bjorken_x", "", R"("nan")", "SBN"},
    {"TruthInteraction", "inelasticity", "", R"("nan")", "SBN"},
    {"TruthInteraction", "creation_process", "", R"("")", "SBN"},
    {"TruthInteraction", "position", "instance", R"(["nan", "nan", "nan"])", "SBN"},
    {"TruthInteraction", "momentum", "MeV/c", R"(["nan", "nan", "nan"])", "SBN FD"},
    {"TruthInteraction", "nu_id", "", R"(-1)", "SBN"},
    {"TruthInteraction", "reco_vertex", "instance", R"(["nan", "nan", "nan"])", "SBN"},
    {"TruthInteraction", "primary_particle_ids", "", R"([])", "SBN"},
    {"TruthInteraction", "num_particles", "", R"(0)", "SBN"},
    {"TruthInteraction", "num_primary_particles", "", R"(0)", "SBN"},
    {"TruthInteraction", "particle_counts", "", R"([0, 0, 0, 0, 0, 0])", "SBN"},
    {"TruthInteraction", "primary_particle_counts", "", R"([0, 0, 0, 0, 0, 0])", "SBN"},
    {"TruthInteraction", "topology", "", R"("")", "SBN"},
    {"TruthInteraction", "size", "", R"(0)", "SBN"},
    {"TruthInteraction", "depositions_sum", "", R"(0.0)", "SBN"},
    {"TruthInteraction", "depositions_q_sum", "", R"(0.0)", "SBN"},
    {"TruthInteraction", "module_ids", "", R"([])", "SBN"},
    {"TruthInteraction", "size_adapt", "", R"(0)", "SBN"},
    {"TruthInteraction", "size_g4", "", R"(0)", "SBN"},
    {"TruthInteraction", "depositions_adapt_sum", "", R"(0.0)", "SBN"},
    {"TruthInteraction", "depositions_adapt_q_sum", "", R"(0.0)", "SBN"},
    {"TruthInteraction", "depositions_g4_sum", "", R"(0.0)", "SBN"},

    // RunInfo
    {"RunInfo", "run", "", R"(-1)", "ND SBN FD"},
    {"RunInfo", "subrun", "", R"(-1)", "ND SBN FD"},
    {"RunInfo", "event", "", R"(-1)", "ND SBN FD"},

    // Trigger
    {"Trigger", "id", "", R"(-1)", "ND"},
    {"Trigger", "type", "", R"(-1)", "ND"},
    {"Trigger", "time_s", "s", R"(-1)", "ND"},
    {"Trigger", "time_ns", "ns", R"(-1)", "ND"},

    // Flash
    {"Flash", "id", "", R"(-1)", "ND"},
    {"Flash", "volume_id", "", R"(-1)", "ND"},
    {"Flash", "time", "us", R"("nan")", "ND"},
    {"Flash", "time_width", "us", R"("nan")", "ND"},
    {"Flash", "total_pe", "", R"("nan")", "ND"},
};

/// The classes of kFields, in table order.
std::vector<std::string> table_classes() {
  std::vector<std::string> classes;
  for (const FieldContract& row : kFields) {
    if (classes.empty() || classes.back() != row.class_name) {
      classes.emplace_back(row.class_name);
    }
  }
  return classes;
}

/// Checks the row of one member, if it has one, and records that the row was seen.
template <class M>
void check_member(const spine::ClassDescriptor& d, std::string_view name, const M& value,
                  std::set<const FieldContract*>& seen) {
  for (const FieldContract& row : kFields) {
    if (d.name != row.class_name || name != row.field) continue;
    seen.insert(&row);
    INFO(row.class_name << "." << row.field << ", read by " << row.readers);
    const spine::FieldDescriptor* field = d.find(name);
    REQUIRE(field != nullptr);
    CHECK_MESSAGE(field->units == row.unit, "unit: the model has '" << field->units
                                                                    << "', the contract '"
                                                                    << row.unit << "'");
    CHECK_MESSAGE(spine_test::equal_member(value, json::parse(std::string(row.default_json))),
                  "default: the model has " << spine_test::member_json(value).dump()
                                            << ", the contract " << row.default_json);
  }
}

using ClassCheck = void (*)(std::set<const FieldContract*>&);

/// For each model class, a function that checks the rows of its members.
const std::map<std::string, ClassCheck>& class_checks() {
#define CONTRACT_MEMBER(name) check_member(d, #name, object.name, seen);
#define CONTRACT_CLASS(Class)                                      \
  {#Class, [](std::set<const FieldContract*>& seen) {              \
     const spine::Class object{};                                  \
     const spine::ClassDescriptor& d = spine::Class::descriptor(); \
     GOLDEN_MEMBERS_##Class(CONTRACT_MEMBER)                       \
   }},
  static const std::map<std::string, ClassCheck> checks = {
      SPINE_READER_FOR_EACH_CLASS(CONTRACT_CLASS)};
#undef CONTRACT_CLASS
#undef CONTRACT_MEMBER
  return checks;
}

struct MemberContract {
  std::string_view name;
  std::int64_t value;
};

/// Every listed member is in the enum, with that value. Members SPINE adds are not a change.
void check_members(const spine::EnumDescriptor& d, std::initializer_list<MemberContract> members) {
  INFO(d.name);
  for (const MemberContract& m : members) {
    INFO(m.name);
    const spine::EnumMember* found = nullptr;
    for (std::size_t i = 0; i < d.size; ++i) {
      if (d.members[i].name == m.name) found = &d.members[i];
    }
    CHECK_MESSAGE(found != nullptr, "not in the model");
    if (found != nullptr) CHECK(found->value == m.value);
  }
}

}  // namespace

TEST_CASE("contract: every class a CAFMaker reads is in the model") {
  for (const std::string& name : table_classes()) {
    CHECK_MESSAGE(class_checks().count(name) == 1,
                  name << ": in the contract table, not in the model");
  }
}

TEST_CASE("contract: the fields the CAFMakers read keep their units and defaults") {
  std::set<const FieldContract*> seen;
  for (const std::string& name : table_classes()) {
    const auto check = class_checks().find(name);
    if (check != class_checks().end()) check->second(seen);
  }
  // a row whose field the model no longer has
  for (const FieldContract& row : kFields) {
    if (seen.count(&row) == 0 && class_checks().count(std::string(row.class_name)) == 1) {
      FAIL_CHECK(row.class_name << "." << row.field << ": listed for " << row.readers
                                << ", not in the model");
    }
  }
}

TEST_CASE("contract: the enum values the CAFMakers rely on") {
  // ND compares shape with ParticleShape::TRACK and SHOWER, and current_type with
  // NuCurrentType::CC and NC. SBN stores the numbers of shape, pid, chi2_pid and current_type in
  // its CAF. No CAFMaker reads interaction_scheme or uses the interaction tables: SBN stores
  // interaction_mode and interaction_type as SPINE's numbers.
  check_members(spine::enum_descriptor<spine::ParticleShape>(),
                {{"UNKNOWN", -1},
                 {"SHOWER", 0},
                 {"TRACK", 1},
                 {"MICHEL", 2},
                 {"DELTA", 3},
                 {"LOWE", 4},
                 {"GHOST", 5},
                 {"LARCV_UNKNOWN", 6}});
  check_members(spine::enum_descriptor<spine::ParticlePID>(),
                {{"UNKNOWN", -1},
                 {"PHOTON", 0},
                 {"ELECTRON", 1},
                 {"MUON", 2},
                 {"PION", 3},
                 {"PROTON", 4},
                 {"KAON", 5}});
  check_members(spine::enum_descriptor<spine::NuCurrentType>(),
                {{"UNKNOWN", -1}, {"CC", 0}, {"NC", 1}});
}
