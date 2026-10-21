// GENERATED from SPINE 1.4.0 by tools/gen/generate.py. Do not edit.
#include <spine/model/truth_particle.hpp>

#include <cstddef>
#include <iterator>

#include "model/field_check.hpp"

namespace spine {

namespace {

constexpr FieldDescriptor kFields[] = {
    {"orig_id", ScalarType::Int64, 1, false, 0, "", nullptr, "object", "external",
     "If matched to an MC truth instance, ID of the original instance",
     &detail::assign<&TruthParticle::orig_id>},
    {"is_truth", ScalarType::Bool, 1, false, 0, "", nullptr, "", "",
     "Whether this is a truth object (always True for truth objects)",
     &detail::assign<&TruthParticle::is_truth>},
    {"index_adapt", ScalarType::Int32, 0, true, kLiteSkip, "", nullptr, "", "",
     "(N') Voxel indexes corresponding to this object in the adapted cluster label tensor",
     &detail::assign<&TruthParticle::index_adapt>},
    {"index_g4", ScalarType::Int32, 0, true, kLiteSkip, "", nullptr, "", "",
     "(N'') Fragment voxel indexes in the true Geant4 energy deposition tensor",
     &detail::assign<&TruthParticle::index_g4>},
    {"units", ScalarType::String, 0, true, 0, "", nullptr, "", "",
     "Units in which the position attributes are expressed",
     &detail::assign<&TruthParticle::units>},
    {"id", ScalarType::Int64, 1, false, kIndex, "", nullptr, "particle", "same",
     "Index of the particle in the list",
     &detail::assign<&TruthParticle::id>},
    {"is_contained", ScalarType::Bool, 1, false, 0, "", nullptr, "", "",
     "Whether this object is fully contained within the detector",
     &detail::assign<&TruthParticle::is_contained>},
    {"is_time_contained", ScalarType::Bool, 1, false, 0, "", nullptr, "", "",
     "Whether this object's points are within the expected readout window",
     &detail::assign<&TruthParticle::is_time_contained>},
    {"is_cathode_crosser", ScalarType::Bool, 1, false, 0, "", nullptr, "", "",
     "True if the particle crossed a cathode, i.e. if it is made up of space points coming from > "
     "1 TPC in one module",
     &detail::assign<&TruthParticle::is_cathode_crosser>},
    {"is_matched", ScalarType::Bool, 1, false, 0, "", nullptr, "", "",
     "True if a true object match was found",
     &detail::assign<&TruthParticle::is_matched>},
    {"cathode_offset", ScalarType::Float64, 1, false, 0, "cm", nullptr, "", "",
     "If the particle is a cathode crosser, this is how far in cm one needs to move its points "
     "along the drift direction to reconcile at the cathode. This is directly proportional to time "
     "through time=offset/vdrift",
     &detail::assign<&TruthParticle::cathode_offset>},
    {"index", ScalarType::Int32, 0, true, kLiteSkip, "", nullptr, "", "",
     "(N) Voxel indexes corresponding to this object in the input tensor",
     &detail::assign<&TruthParticle::index>},
    {"orig_index", ScalarType::Int32, 0, true, kLiteSkip, "", nullptr, "", "",
     "(N) Original voxel indexes corresponding to this object in the original point cloud (before "
     "any filtering or adaptation)",
     &detail::assign<&TruthParticle::orig_index>},
    {"match_ids", ScalarType::Int32, 0, true, 0, "", nullptr, "object", "opposite",
     "List of true object IDs this object is matched to",
     &detail::assign<&TruthParticle::match_ids>},
    {"match_overlaps", ScalarType::Float32, 0, true, 0, "", nullptr, "", "",
     "List of match overlaps (IoU) between the object and its matches",
     &detail::assign<&TruthParticle::match_overlaps>},
    {"interaction_id", ScalarType::Int64, 1, false, kIndex, "", nullptr, "interaction", "same",
     "Index of the interaction the partile belongs to",
     &detail::assign<&TruthParticle::interaction_id>},
    {"chi2_pid", ScalarType::Int64, 1, false, 0, "", &kParticlePIDEnum, "", "",
     "Particle species as predicted by the chi2 template method (Muon (2), Charged Pion (3), "
     "Proton (4), Kaon (5)) of this particle",
     &detail::assign<&TruthParticle::chi2_pid>},
    {"is_primary", ScalarType::Bool, 1, false, 0, "", nullptr, "", "",
     "Whether this particle was the first in the particle group",
     &detail::assign<&TruthParticle::is_primary>},
    {"is_crt_matched", ScalarType::Bool, 1, false, 0, "", nullptr, "", "",
     "True if the particle was matched to a CRT hit",
     &detail::assign<&TruthParticle::is_crt_matched>},
    {"is_valid", ScalarType::Bool, 1, false, 0, "", nullptr, "", "",
     "Whether this particle counts towards an interaction topology. This may be False if a "
     "particle is below some defined energy threshold.",
     &detail::assign<&TruthParticle::is_valid>},
    {"length", ScalarType::Float64, 1, false, 0, "instance", nullptr, "", "",
     "Length of the particle (only assigned to track objects)",
     &detail::assign<&TruthParticle::length>},
    {"calo_ke", ScalarType::Float64, 1, false, 0, "MeV", nullptr, "", "",
     "Kinetic energy reconstructed from the energy depositions alone in MeV",
     &detail::assign<&TruthParticle::calo_ke>},
    {"csda_ke", ScalarType::Float64, 1, false, 0, "MeV", nullptr, "", "",
     "Kinetic energy reconstructed from the particle range in MeV",
     &detail::assign<&TruthParticle::csda_ke>},
    {"mcs_ke", ScalarType::Float64, 1, false, 0, "MeV", nullptr, "", "",
     "Kinetic energy reconstructed using the MCS method in MeV",
     &detail::assign<&TruthParticle::mcs_ke>},
    {"shape", ScalarType::Int64, 1, false, 0, "", &kParticleShapeEnum, "", "",
     "Enumerated semantic type of the particle",
     &detail::assign<&TruthParticle::shape>},
    {"pid", ScalarType::Int64, 1, false, 0, "", &kParticlePIDEnum, "", "",
     "Enumerated particle species type of the particle",
     &detail::assign<&TruthParticle::pid>},
    {"fragment_ids", ScalarType::Int32, 0, true, 0, "instance", nullptr, "fragment", "same",
     "List of Fragment IDs that make up this particle",
     &detail::assign<&TruthParticle::fragment_ids>},
    {"start_point", ScalarType::Float32, 3, false, kPosition, "instance", nullptr, "", "",
     "(3) Particle start point",
     &detail::assign<&TruthParticle::start_point>},
    {"end_point", ScalarType::Float32, 3, false, kPosition, "instance", nullptr, "", "",
     "(3) Particle end point (only assigned to track objects)",
     &detail::assign<&TruthParticle::end_point>},
    {"chi2_per_pid", ScalarType::Float32, 6, false, 0, "", nullptr, "", "",
     "(P) Array of chi2 values associated with each particle class",
     &detail::assign<&TruthParticle::chi2_per_pid>},
    {"csda_ke_per_pid", ScalarType::Float32, 6, false, 0, "MeV", nullptr, "", "",
     "(P) Same as `csda_ke` but for every available track PID hypothesis",
     &detail::assign<&TruthParticle::csda_ke_per_pid>},
    {"mcs_ke_per_pid", ScalarType::Float32, 6, false, 0, "MeV", nullptr, "", "",
     "(P) Same as `mcs_ke` but for every available track PID hypothesis",
     &detail::assign<&TruthParticle::mcs_ke_per_pid>},
    {"crt_ids", ScalarType::Int32, 0, true, 0, "", nullptr, "crthit", "external",
     "(C) Indices of the CRT hits the particle is matched to",
     &detail::assign<&TruthParticle::crt_ids>},
    {"crt_times", ScalarType::Float32, 0, true, 0, "us", nullptr, "", "",
     "(C) Times at which the CRT hits occurred in microseconds",
     &detail::assign<&TruthParticle::crt_times>},
    {"crt_scores", ScalarType::Float32, 0, true, 0, "", nullptr, "", "",
     "(C) Quality metric associated with the CRT matches",
     &detail::assign<&TruthParticle::crt_scores>},
    {"parent_id", ScalarType::Int64, 1, false, kIndex, "", nullptr, "particle", "same",
     "Index of the parent particle",
     &detail::assign<&TruthParticle::parent_id>},
    {"group_id", ScalarType::Int64, 1, false, kIndex, "", nullptr, "particle", "same",
     "Index of the group the particle belongs to",
     &detail::assign<&TruthParticle::group_id>},
    {"nu_id", ScalarType::Int64, 1, false, kIndex, "", nullptr, "neutrino", "same",
     "Index of the neutrino this particle belongs to",
     &detail::assign<&TruthParticle::nu_id>},
    {"children_id", ScalarType::Int32, 0, true, kIndex, "", nullptr, "particle", "same",
     "List of indexes of the children particles",
     &detail::assign<&TruthParticle::children_id>},
    {"mct_index", ScalarType::Int64, 1, false, kCategorical, "", nullptr, "", "",
     "Index in the original MCTruth array from whence it came",
     &detail::assign<&TruthParticle::mct_index>},
    {"mcst_index", ScalarType::Int64, 1, false, kCategorical, "", nullptr, "", "",
     "Index in the original MCTrack/MCShower array from whence it came",
     &detail::assign<&TruthParticle::mcst_index>},
    {"group_primary", ScalarType::Int64, 1, false, kCategorical, "", nullptr, "", "",
     "Whether this particle is primary in its group or not",
     &detail::assign<&TruthParticle::group_primary>},
    {"interaction_primary", ScalarType::Int64, 1, false, kCategorical, "", nullptr, "", "",
     "Whether the particle is primary in its interaction or not",
     &detail::assign<&TruthParticle::interaction_primary>},
    {"track_id", ScalarType::Int64, 1, false, 0, "", nullptr, "geant_track", "external",
     "Geant4 track ID",
     &detail::assign<&TruthParticle::track_id>},
    {"parent_track_id", ScalarType::Int64, 1, false, 0, "", nullptr, "geant_track", "external",
     "Geant4 track ID of the parent particle",
     &detail::assign<&TruthParticle::parent_track_id>},
    {"ancestor_track_id", ScalarType::Int64, 1, false, 0, "", nullptr, "geant_track", "external",
     "Geant4 track ID of the ancestor particle",
     &detail::assign<&TruthParticle::ancestor_track_id>},
    {"pdg_code", ScalarType::Int64, 1, false, kCategorical | kPdg, "", nullptr, "", "",
     "Particle PDG code",
     &detail::assign<&TruthParticle::pdg_code>},
    {"parent_pdg_code", ScalarType::Int64, 1, false, kCategorical | kPdg, "", nullptr, "", "",
     "Particle PDG code of the parent particle",
     &detail::assign<&TruthParticle::parent_pdg_code>},
    {"ancestor_pdg_code", ScalarType::Int64, 1, false, kCategorical | kPdg, "", nullptr, "", "",
     "Particle PDG code of the ancestor particle",
     &detail::assign<&TruthParticle::ancestor_pdg_code>},
    {"num_voxels", ScalarType::Int64, 1, false, 0, "", nullptr, "", "",
     "Number of voxels matched to this particle instance",
     &detail::assign<&TruthParticle::num_voxels>},
    {"t", ScalarType::Float64, 1, false, 0, "ns", nullptr, "", "",
     "Particle creation time (ns)",
     &detail::assign<&TruthParticle::t>},
    {"end_t", ScalarType::Float64, 1, false, 0, "ns", nullptr, "", "",
     "Particle death time (ns)",
     &detail::assign<&TruthParticle::end_t>},
    {"first_step_t", ScalarType::Float64, 1, false, 0, "ns", nullptr, "", "",
     "Time of the first energy deposition of the particle (ns)",
     &detail::assign<&TruthParticle::first_step_t>},
    {"last_step_t", ScalarType::Float64, 1, false, 0, "ns", nullptr, "", "",
     "Time of the last energy deposition of the particle (ns)",
     &detail::assign<&TruthParticle::last_step_t>},
    {"parent_t", ScalarType::Float64, 1, false, 0, "ns", nullptr, "", "",
     "Particle creation time of the parent particle (ns)",
     &detail::assign<&TruthParticle::parent_t>},
    {"ancestor_t", ScalarType::Float64, 1, false, 0, "ns", nullptr, "", "",
     "Particle creation time of the ancestor particle (ns)",
     &detail::assign<&TruthParticle::ancestor_t>},
    {"energy_init", ScalarType::Float64, 1, false, 0, "MeV", nullptr, "", "",
     "True initial energy in MeV",
     &detail::assign<&TruthParticle::energy_init>},
    {"energy_deposit", ScalarType::Float64, 1, false, 0, "MeV", nullptr, "", "",
     "Amount of energy matched to this particle instance in MeV",
     &detail::assign<&TruthParticle::energy_deposit>},
    {"distance_travel", ScalarType::Float64, 1, false, 0, "cm", nullptr, "", "",
     "True amount of distance traveled by the particle in the active volume",
     &detail::assign<&TruthParticle::distance_travel>},
    {"creation_process", ScalarType::String, 0, true, 0, "", nullptr, "", "",
     "Creation process",
     &detail::assign<&TruthParticle::creation_process>},
    {"parent_creation_process", ScalarType::String, 0, true, 0, "", nullptr, "", "",
     "Creation process of the parent particle",
     &detail::assign<&TruthParticle::parent_creation_process>},
    {"ancestor_creation_process", ScalarType::String, 0, true, 0, "", nullptr, "", "",
     "Creation process of the ancestor particle",
     &detail::assign<&TruthParticle::ancestor_creation_process>},
    {"position", ScalarType::Float32, 3, false, kPosition, "instance", nullptr, "", "",
     "Location of the creation point of the particle",
     &detail::assign<&TruthParticle::position>},
    {"end_position", ScalarType::Float32, 3, false, kPosition, "instance", nullptr, "", "",
     "Location where the particle stopped",
     &detail::assign<&TruthParticle::end_position>},
    {"parent_position", ScalarType::Float32, 3, false, kPosition, "instance", nullptr, "", "",
     "Location of the creation point of the parent particle",
     &detail::assign<&TruthParticle::parent_position>},
    {"ancestor_position", ScalarType::Float32, 3, false, kPosition, "instance", nullptr, "", "",
     "Location of the creation point of the ancestor particle",
     &detail::assign<&TruthParticle::ancestor_position>},
    {"first_step", ScalarType::Float32, 3, false, kPosition, "instance", nullptr, "", "",
     "Location of the first energy deposition of the particle",
     &detail::assign<&TruthParticle::first_step>},
    {"last_step", ScalarType::Float32, 3, false, kPosition, "instance", nullptr, "", "",
     "Location of the last energy deposition of the particle",
     &detail::assign<&TruthParticle::last_step>},
    {"momentum", ScalarType::Float32, 3, false, kVector, "MeV/c", nullptr, "", "",
     "3-momentum of the particle at the production point",
     &detail::assign<&TruthParticle::momentum>},
    {"end_momentum", ScalarType::Float32, 3, false, kVector, "MeV/c", nullptr, "", "",
     "3-momentum of the particle at where it stops or exits the detector",
     &detail::assign<&TruthParticle::end_momentum>},
    {"orig_interaction_id", ScalarType::Int64, 1, false, 0, "", nullptr, "interaction", "external",
     "Unaltered index of the interaction in the original MC particle list",
     &detail::assign<&TruthParticle::orig_interaction_id>},
    {"orig_parent_id", ScalarType::Int64, 1, false, 0, "", nullptr, "particle", "external",
     "Unaltered index of the particle parent in the original MC particle list",
     &detail::assign<&TruthParticle::orig_parent_id>},
    {"orig_group_id", ScalarType::Int64, 1, false, 0, "", nullptr, "particle", "external",
     "Unaltered index of the particle group in the original MC particle list",
     &detail::assign<&TruthParticle::orig_group_id>},
    {"reco_length", ScalarType::Float64, 1, false, 0, "", nullptr, "", "",
     "Reconstructed length of the particle (only assigned to track objects)",
     &detail::assign<&TruthParticle::reco_length>},
    {"orig_children_id", ScalarType::Int32, 0, true, 0, "", nullptr, "particle", "external",
     "Unaltered list of the particle children in the original MC particle list",
     &detail::assign<&TruthParticle::orig_children_id>},
    {"children_counts", ScalarType::Int32, 0, true, 0, "", nullptr, "", "",
     "(P) Number of truth child particle of each shape",
     &detail::assign<&TruthParticle::children_counts>},
    {"reco_start_dir", ScalarType::Float32, 3, false, kVector, "", nullptr, "", "",
     "(3) Particle direction estimate w.r.t. the start point",
     &detail::assign<&TruthParticle::reco_start_dir>},
    {"reco_end_dir", ScalarType::Float32, 3, false, kVector, "", nullptr, "", "",
     "(3) Particle direction estimate w.r.t. the end point (only assigned to track objects)",
     &detail::assign<&TruthParticle::reco_end_dir>},
    {"start_dir", ScalarType::Float32, 3, false, kDerived | kVector, "", nullptr, "", "",
     "(3) Particle direction w.r.t. the start point",
     &detail::assign<&TruthParticle::start_dir>},
    {"end_dir", ScalarType::Float32, 3, false, kDerived | kVector, "", nullptr, "", "",
     "(3) Particle direction w.r.t. the end point (only assigned to track objects)",
     &detail::assign<&TruthParticle::end_dir>},
    {"ke", ScalarType::Float64, 1, false, kDerived, "MeV", nullptr, "", "",
     "Kinetic energy of the particle in MeV",
     &detail::assign<&TruthParticle::ke>},
    {"reco_ke", ScalarType::Float64, 1, false, kDerived, "MeV", nullptr, "", "",
     "Best-guess reconstructed KE of the particle",
     &detail::assign<&TruthParticle::reco_ke>},
    {"reco_momentum", ScalarType::Float32, 3, false, kDerived, "MeV/c", nullptr, "", "",
     "Best-guess reconstructed momentum of the particle",
     &detail::assign<&TruthParticle::reco_momentum>},
    {"p", ScalarType::Float64, 1, false, kDerived, "MeV/c", nullptr, "", "",
     "Momentum magnitude of the particle at the production point",
     &detail::assign<&TruthParticle::p>},
    {"end_p", ScalarType::Float64, 1, false, kDerived, "MeV/c", nullptr, "", "",
     "Momentum magnitude of the particle where it stops or exits the detector",
     &detail::assign<&TruthParticle::end_p>},
    {"mass", ScalarType::Float64, 1, false, kDerived, "MeV/c^2", nullptr, "", "",
     "Rest mass of the particle in MeV/c^2",
     &detail::assign<&TruthParticle::mass>},
    {"num_fragments", ScalarType::Int64, 1, false, kDerived, "", nullptr, "", "",
     "Number of fragments that make up this particle",
     &detail::assign<&TruthParticle::num_fragments>},
    {"size", ScalarType::Int64, 1, false, kDerived, "", nullptr, "", "",
     "Number of points, N, that make up this object",
     &detail::assign<&TruthParticle::size>},
    {"depositions_sum", ScalarType::Float64, 1, false, kDerived, "", nullptr, "", "",
     "Total amount of depositions",
     &detail::assign<&TruthParticle::depositions_sum>},
    {"depositions_q_sum", ScalarType::Float64, 1, false, kDerived, "", nullptr, "", "",
     "Total amount of input charge",
     &detail::assign<&TruthParticle::depositions_q_sum>},
    {"best_match_id", ScalarType::Int64, 1, false, kDerived, "", nullptr, "object", "opposite",
     "ID of the highest-overlap matched object, or -1 when unmatched",
     &detail::assign<&TruthParticle::best_match_id>},
    {"best_match_overlap", ScalarType::Float64, 1, false, kDerived, "", nullptr, "", "",
     "Overlap with the highest-overlap matched object, or -1 when unmatched",
     &detail::assign<&TruthParticle::best_match_overlap>},
    {"module_ids", ScalarType::Int32, 0, true, kDerived, "", nullptr, "module", "external",
     "(M) List of unique module indexes that make up this object",
     &detail::assign<&TruthParticle::module_ids>},
    {"size_adapt", ScalarType::Int64, 1, false, kDerived, "", nullptr, "", "",
     "Number of points, N', that make up this object in the adapted cluster label tensor",
     &detail::assign<&TruthParticle::size_adapt>},
    {"size_g4", ScalarType::Int64, 1, false, kDerived, "", nullptr, "", "",
     "Number of points, N'', that make up this object in the true Geant4 energy deposition tensor",
     &detail::assign<&TruthParticle::size_g4>},
    {"depositions_adapt_sum", ScalarType::Float64, 1, false, kDerived, "", nullptr, "", "",
     "Total amount of depositions in adapted cluster label tensor",
     &detail::assign<&TruthParticle::depositions_adapt_sum>},
    {"depositions_adapt_q_sum", ScalarType::Float64, 1, false, kDerived, "", nullptr, "", "",
     "Total amount of depositions in adapted cluster label tensor in the same units as the input "
     "image",
     &detail::assign<&TruthParticle::depositions_adapt_q_sum>},
    {"depositions_g4_sum", ScalarType::Float64, 1, false, kDerived, "", nullptr, "", "",
     "Total amount of true Geant4 depositions",
     &detail::assign<&TruthParticle::depositions_g4_sum>},
};

constexpr ClassDescriptor kDescriptor{"TruthParticle", kFields, std::size(kFields), "1.4.0"};

// The table has one row per Field enumerator, in order, and every member's C++ type
// agrees with its row (element type, count, variable flag, enum or not).
static_assert(std::size(kFields) == TruthParticle::kNumFields);
static_assert(static_cast<std::size_t>(TruthParticle::Field::depositions_g4_sum)
              + 1 == TruthParticle::kNumFields);
SPINE_MODEL_CHECK_FIELD(TruthParticle, orig_id);
SPINE_MODEL_CHECK_FIELD(TruthParticle, is_truth);
SPINE_MODEL_CHECK_FIELD(TruthParticle, index_adapt);
SPINE_MODEL_CHECK_FIELD(TruthParticle, index_g4);
SPINE_MODEL_CHECK_FIELD(TruthParticle, units);
SPINE_MODEL_CHECK_FIELD(TruthParticle, id);
SPINE_MODEL_CHECK_FIELD(TruthParticle, is_contained);
SPINE_MODEL_CHECK_FIELD(TruthParticle, is_time_contained);
SPINE_MODEL_CHECK_FIELD(TruthParticle, is_cathode_crosser);
SPINE_MODEL_CHECK_FIELD(TruthParticle, is_matched);
SPINE_MODEL_CHECK_FIELD(TruthParticle, cathode_offset);
SPINE_MODEL_CHECK_FIELD(TruthParticle, index);
SPINE_MODEL_CHECK_FIELD(TruthParticle, orig_index);
SPINE_MODEL_CHECK_FIELD(TruthParticle, match_ids);
SPINE_MODEL_CHECK_FIELD(TruthParticle, match_overlaps);
SPINE_MODEL_CHECK_FIELD(TruthParticle, interaction_id);
SPINE_MODEL_CHECK_FIELD(TruthParticle, chi2_pid);
SPINE_MODEL_CHECK_FIELD(TruthParticle, is_primary);
SPINE_MODEL_CHECK_FIELD(TruthParticle, is_crt_matched);
SPINE_MODEL_CHECK_FIELD(TruthParticle, is_valid);
SPINE_MODEL_CHECK_FIELD(TruthParticle, length);
SPINE_MODEL_CHECK_FIELD(TruthParticle, calo_ke);
SPINE_MODEL_CHECK_FIELD(TruthParticle, csda_ke);
SPINE_MODEL_CHECK_FIELD(TruthParticle, mcs_ke);
SPINE_MODEL_CHECK_FIELD(TruthParticle, shape);
SPINE_MODEL_CHECK_FIELD(TruthParticle, pid);
SPINE_MODEL_CHECK_FIELD(TruthParticle, fragment_ids);
SPINE_MODEL_CHECK_FIELD(TruthParticle, start_point);
SPINE_MODEL_CHECK_FIELD(TruthParticle, end_point);
SPINE_MODEL_CHECK_FIELD(TruthParticle, chi2_per_pid);
SPINE_MODEL_CHECK_FIELD(TruthParticle, csda_ke_per_pid);
SPINE_MODEL_CHECK_FIELD(TruthParticle, mcs_ke_per_pid);
SPINE_MODEL_CHECK_FIELD(TruthParticle, crt_ids);
SPINE_MODEL_CHECK_FIELD(TruthParticle, crt_times);
SPINE_MODEL_CHECK_FIELD(TruthParticle, crt_scores);
SPINE_MODEL_CHECK_FIELD(TruthParticle, parent_id);
SPINE_MODEL_CHECK_FIELD(TruthParticle, group_id);
SPINE_MODEL_CHECK_FIELD(TruthParticle, nu_id);
SPINE_MODEL_CHECK_FIELD(TruthParticle, children_id);
SPINE_MODEL_CHECK_FIELD(TruthParticle, mct_index);
SPINE_MODEL_CHECK_FIELD(TruthParticle, mcst_index);
SPINE_MODEL_CHECK_FIELD(TruthParticle, group_primary);
SPINE_MODEL_CHECK_FIELD(TruthParticle, interaction_primary);
SPINE_MODEL_CHECK_FIELD(TruthParticle, track_id);
SPINE_MODEL_CHECK_FIELD(TruthParticle, parent_track_id);
SPINE_MODEL_CHECK_FIELD(TruthParticle, ancestor_track_id);
SPINE_MODEL_CHECK_FIELD(TruthParticle, pdg_code);
SPINE_MODEL_CHECK_FIELD(TruthParticle, parent_pdg_code);
SPINE_MODEL_CHECK_FIELD(TruthParticle, ancestor_pdg_code);
SPINE_MODEL_CHECK_FIELD(TruthParticle, num_voxels);
SPINE_MODEL_CHECK_FIELD(TruthParticle, t);
SPINE_MODEL_CHECK_FIELD(TruthParticle, end_t);
SPINE_MODEL_CHECK_FIELD(TruthParticle, first_step_t);
SPINE_MODEL_CHECK_FIELD(TruthParticle, last_step_t);
SPINE_MODEL_CHECK_FIELD(TruthParticle, parent_t);
SPINE_MODEL_CHECK_FIELD(TruthParticle, ancestor_t);
SPINE_MODEL_CHECK_FIELD(TruthParticle, energy_init);
SPINE_MODEL_CHECK_FIELD(TruthParticle, energy_deposit);
SPINE_MODEL_CHECK_FIELD(TruthParticle, distance_travel);
SPINE_MODEL_CHECK_FIELD(TruthParticle, creation_process);
SPINE_MODEL_CHECK_FIELD(TruthParticle, parent_creation_process);
SPINE_MODEL_CHECK_FIELD(TruthParticle, ancestor_creation_process);
SPINE_MODEL_CHECK_FIELD(TruthParticle, position);
SPINE_MODEL_CHECK_FIELD(TruthParticle, end_position);
SPINE_MODEL_CHECK_FIELD(TruthParticle, parent_position);
SPINE_MODEL_CHECK_FIELD(TruthParticle, ancestor_position);
SPINE_MODEL_CHECK_FIELD(TruthParticle, first_step);
SPINE_MODEL_CHECK_FIELD(TruthParticle, last_step);
SPINE_MODEL_CHECK_FIELD(TruthParticle, momentum);
SPINE_MODEL_CHECK_FIELD(TruthParticle, end_momentum);
SPINE_MODEL_CHECK_FIELD(TruthParticle, orig_interaction_id);
SPINE_MODEL_CHECK_FIELD(TruthParticle, orig_parent_id);
SPINE_MODEL_CHECK_FIELD(TruthParticle, orig_group_id);
SPINE_MODEL_CHECK_FIELD(TruthParticle, reco_length);
SPINE_MODEL_CHECK_FIELD(TruthParticle, orig_children_id);
SPINE_MODEL_CHECK_FIELD(TruthParticle, children_counts);
SPINE_MODEL_CHECK_FIELD(TruthParticle, reco_start_dir);
SPINE_MODEL_CHECK_FIELD(TruthParticle, reco_end_dir);
SPINE_MODEL_CHECK_FIELD(TruthParticle, start_dir);
SPINE_MODEL_CHECK_FIELD(TruthParticle, end_dir);
SPINE_MODEL_CHECK_FIELD(TruthParticle, ke);
SPINE_MODEL_CHECK_FIELD(TruthParticle, reco_ke);
SPINE_MODEL_CHECK_FIELD(TruthParticle, reco_momentum);
SPINE_MODEL_CHECK_FIELD(TruthParticle, p);
SPINE_MODEL_CHECK_FIELD(TruthParticle, end_p);
SPINE_MODEL_CHECK_FIELD(TruthParticle, mass);
SPINE_MODEL_CHECK_FIELD(TruthParticle, num_fragments);
SPINE_MODEL_CHECK_FIELD(TruthParticle, size);
SPINE_MODEL_CHECK_FIELD(TruthParticle, depositions_sum);
SPINE_MODEL_CHECK_FIELD(TruthParticle, depositions_q_sum);
SPINE_MODEL_CHECK_FIELD(TruthParticle, best_match_id);
SPINE_MODEL_CHECK_FIELD(TruthParticle, best_match_overlap);
SPINE_MODEL_CHECK_FIELD(TruthParticle, module_ids);
SPINE_MODEL_CHECK_FIELD(TruthParticle, size_adapt);
SPINE_MODEL_CHECK_FIELD(TruthParticle, size_g4);
SPINE_MODEL_CHECK_FIELD(TruthParticle, depositions_adapt_sum);
SPINE_MODEL_CHECK_FIELD(TruthParticle, depositions_adapt_q_sum);
SPINE_MODEL_CHECK_FIELD(TruthParticle, depositions_g4_sum);

}  // namespace

const ClassDescriptor& TruthParticle::descriptor() { return kDescriptor; }

}  // namespace spine
