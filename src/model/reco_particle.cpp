// GENERATED from SPINE 1.4.0 by tools/gen/generate.py. Do not edit.
#include <spine/model/reco_particle.hpp>

#include <cstddef>
#include <iterator>

#include "model/field_check.hpp"

namespace spine {

namespace {

constexpr FieldDescriptor kFields[] = {
    {"is_truth", ScalarType::Bool, 1, false, 0, "", nullptr, "", "",
     "Whether this is a truth object (always False for reconstructed)",
     &detail::assign<&RecoParticle::is_truth>},
    {"units", ScalarType::String, 0, true, 0, "", nullptr, "", "",
     "Units in which coordinates are expressed",
     &detail::assign<&RecoParticle::units>},
    {"id", ScalarType::Int64, 1, false, kIndex, "", nullptr, "object", "same",
     "Unique index of the object within the object list",
     &detail::assign<&RecoParticle::id>},
    {"is_contained", ScalarType::Bool, 1, false, 0, "", nullptr, "", "",
     "Whether this object is fully contained within the detector",
     &detail::assign<&RecoParticle::is_contained>},
    {"is_time_contained", ScalarType::Bool, 1, false, 0, "", nullptr, "", "",
     "Whether this object's points are within the expected readout window",
     &detail::assign<&RecoParticle::is_time_contained>},
    {"is_cathode_crosser", ScalarType::Bool, 1, false, 0, "", nullptr, "", "",
     "True if the particle crossed a cathode, i.e. if it is made up of space points coming from > "
     "1 TPC in one module",
     &detail::assign<&RecoParticle::is_cathode_crosser>},
    {"is_matched", ScalarType::Bool, 1, false, 0, "", nullptr, "", "",
     "True if a true object match was found",
     &detail::assign<&RecoParticle::is_matched>},
    {"cathode_offset", ScalarType::Float64, 1, false, 0, "cm", nullptr, "", "",
     "If the particle is a cathode crosser, this is how far in cm one needs to move its points "
     "along the drift direction to reconcile at the cathode. This is directly proportional to time "
     "through time=offset/vdrift",
     &detail::assign<&RecoParticle::cathode_offset>},
    {"index", ScalarType::Int32, 0, true, kLiteSkip, "", nullptr, "", "",
     "(N) Voxel indexes corresponding to this object in the input tensor",
     &detail::assign<&RecoParticle::index>},
    {"orig_index", ScalarType::Int32, 0, true, kLiteSkip, "", nullptr, "", "",
     "(N) Original voxel indexes corresponding to this object in the original point cloud (before "
     "any filtering or adaptation)",
     &detail::assign<&RecoParticle::orig_index>},
    {"match_ids", ScalarType::Int32, 0, true, 0, "", nullptr, "object", "opposite",
     "List of true object IDs this object is matched to",
     &detail::assign<&RecoParticle::match_ids>},
    {"match_overlaps", ScalarType::Float32, 0, true, 0, "", nullptr, "", "",
     "List of match overlaps (IoU) between the object and its matches",
     &detail::assign<&RecoParticle::match_overlaps>},
    {"interaction_id", ScalarType::Int64, 1, false, 0, "", nullptr, "interaction", "same",
     "Index of the interaction this particle belongs to",
     &detail::assign<&RecoParticle::interaction_id>},
    {"chi2_pid", ScalarType::Int64, 1, false, 0, "", &kParticlePIDEnum, "", "",
     "Particle species as predicted by the chi2 template method (Muon (2), Charged Pion (3), "
     "Proton (4), Kaon (5)) of this particle",
     &detail::assign<&RecoParticle::chi2_pid>},
    {"is_primary", ScalarType::Bool, 1, false, 0, "", nullptr, "", "",
     "Whether this particle was the first in the particle group",
     &detail::assign<&RecoParticle::is_primary>},
    {"is_crt_matched", ScalarType::Bool, 1, false, 0, "", nullptr, "", "",
     "True if the particle was matched to a CRT hit",
     &detail::assign<&RecoParticle::is_crt_matched>},
    {"is_valid", ScalarType::Bool, 1, false, 0, "", nullptr, "", "",
     "Whether this particle counts towards an interaction topology. This may be False if a "
     "particle is below some defined energy threshold.",
     &detail::assign<&RecoParticle::is_valid>},
    {"length", ScalarType::Float64, 1, false, 0, "instance", nullptr, "", "",
     "Length of the particle (only assigned to track objects)",
     &detail::assign<&RecoParticle::length>},
    {"calo_ke", ScalarType::Float64, 1, false, 0, "MeV", nullptr, "", "",
     "Kinetic energy reconstructed from the energy depositions alone in MeV",
     &detail::assign<&RecoParticle::calo_ke>},
    {"csda_ke", ScalarType::Float64, 1, false, 0, "MeV", nullptr, "", "",
     "Kinetic energy reconstructed from the particle range in MeV",
     &detail::assign<&RecoParticle::csda_ke>},
    {"mcs_ke", ScalarType::Float64, 1, false, 0, "MeV", nullptr, "", "",
     "Kinetic energy reconstructed using the MCS method in MeV",
     &detail::assign<&RecoParticle::mcs_ke>},
    {"shape", ScalarType::Int64, 1, false, 0, "", &kParticleShapeEnum, "", "",
     "Semantic type (shower (0), track (1), Michel (2), delta (3), low energy scatter (4)) of this "
     "particle",
     &detail::assign<&RecoParticle::shape>},
    {"pid", ScalarType::Int64, 1, false, 0, "", &kParticlePIDEnum, "", "",
     "Particle species (Photon (0), Electron (1), Muon (2), Charged Pion (3), Proton (4), Kaon "
     "(5)) of this particle",
     &detail::assign<&RecoParticle::pid>},
    {"fragment_ids", ScalarType::Int32, 0, true, 0, "instance", nullptr, "fragment", "same",
     "List of Fragment IDs that make up this particle",
     &detail::assign<&RecoParticle::fragment_ids>},
    {"start_point", ScalarType::Float32, 3, false, kPosition, "instance", nullptr, "", "",
     "(3) Particle start point",
     &detail::assign<&RecoParticle::start_point>},
    {"end_point", ScalarType::Float32, 3, false, kPosition, "instance", nullptr, "", "",
     "(3) Particle end point (only assigned to track objects)",
     &detail::assign<&RecoParticle::end_point>},
    {"chi2_per_pid", ScalarType::Float32, 6, false, 0, "", nullptr, "", "",
     "(P) Array of chi2 values associated with each particle class",
     &detail::assign<&RecoParticle::chi2_per_pid>},
    {"csda_ke_per_pid", ScalarType::Float32, 6, false, 0, "MeV", nullptr, "", "",
     "(P) Same as `csda_ke` but for every available track PID hypothesis",
     &detail::assign<&RecoParticle::csda_ke_per_pid>},
    {"mcs_ke_per_pid", ScalarType::Float32, 6, false, 0, "MeV", nullptr, "", "",
     "(P) Same as `mcs_ke` but for every available track PID hypothesis",
     &detail::assign<&RecoParticle::mcs_ke_per_pid>},
    {"crt_ids", ScalarType::Int32, 0, true, 0, "", nullptr, "crthit", "external",
     "(C) Indices of the CRT hits the particle is matched to",
     &detail::assign<&RecoParticle::crt_ids>},
    {"crt_times", ScalarType::Float32, 0, true, 0, "us", nullptr, "", "",
     "(C) Times at which the CRT hits occurred in microseconds",
     &detail::assign<&RecoParticle::crt_times>},
    {"crt_scores", ScalarType::Float32, 0, true, 0, "", nullptr, "", "",
     "(C) Quality metric associated with the CRT matches",
     &detail::assign<&RecoParticle::crt_scores>},
    {"start_dedx", ScalarType::Float64, 1, false, 0, "MeV/cm", nullptr, "", "",
     "dE/dx around a user-defined neighborhood of the start point in MeV/cm",
     &detail::assign<&RecoParticle::start_dedx>},
    {"end_dedx", ScalarType::Float64, 1, false, 0, "MeV/cm", nullptr, "", "",
     "dE/dx around a user-defined neighborhood of the end point in MeV/cm",
     &detail::assign<&RecoParticle::end_dedx>},
    {"vertex_distance", ScalarType::Float64, 1, false, 0, "instance", nullptr, "", "",
     "Set-to-point distance between all particle points and the parent interaction vertex position "
     "in cm",
     &detail::assign<&RecoParticle::vertex_distance>},
    {"start_straightness", ScalarType::Float64, 1, false, 0, "", nullptr, "", "",
     "Explained variance ratio of the beginning of the particle",
     &detail::assign<&RecoParticle::start_straightness>},
    {"directional_spread", ScalarType::Float64, 1, false, 0, "", nullptr, "", "",
     "Estimate of the angular spread of the particle (cosine spread)",
     &detail::assign<&RecoParticle::directional_spread>},
    {"axial_spread", ScalarType::Float64, 1, false, 0, "", nullptr, "", "",
     "Pearson correlation coefficient of the axial profile of the particle w.r.t. to the distance "
     "from its start point",
     &detail::assign<&RecoParticle::axial_spread>},
    {"start_dir", ScalarType::Float32, 3, false, kVector, "", nullptr, "", "",
     "(3) Particle direction w.r.t. the start point",
     &detail::assign<&RecoParticle::start_dir>},
    {"end_dir", ScalarType::Float32, 3, false, kVector, "", nullptr, "", "",
     "(3) Particle direction w.r.t. the end point (only assigned to track objects)",
     &detail::assign<&RecoParticle::end_dir>},
    {"pid_scores", ScalarType::Float32, 6, false, 0, "", nullptr, "", "",
     "(P) Array of softmax scores associated with each particle class",
     &detail::assign<&RecoParticle::pid_scores>},
    {"primary_scores", ScalarType::Float32, 2, false, 0, "", nullptr, "", "",
     "(2) Array of softmax scores associated with secondary and primary",
     &detail::assign<&RecoParticle::primary_scores>},
    {"ppn_ids", ScalarType::Int32, 0, true, 0, "", nullptr, "ppn", "same",
     "(M) List of indexes of PPN points associated with this particle",
     &detail::assign<&RecoParticle::ppn_ids>},
    {"pdg_code", ScalarType::Int64, 1, false, kDerived | kCategorical | kPdg, "", nullptr, "", "",
     "PDG code corresponding to the PID number",
     &detail::assign<&RecoParticle::pdg_code>},
    {"mass", ScalarType::Float64, 1, false, kDerived, "MeV/c^2", nullptr, "", "",
     "Rest mass of the particle in MeV/c^2",
     &detail::assign<&RecoParticle::mass>},
    {"ke", ScalarType::Float64, 1, false, kDerived, "MeV", nullptr, "", "",
     "Best guess kinetic energy of the particle in MeV",
     &detail::assign<&RecoParticle::ke>},
    {"momentum", ScalarType::Float32, 3, false, kDerived, "MeV/c", nullptr, "", "",
     "3-momentum of the particle at the production point in MeV/c",
     &detail::assign<&RecoParticle::momentum>},
    {"p", ScalarType::Float64, 1, false, kDerived, "MeV/c", nullptr, "", "",
     "Momentum magnitude of the particle at the production point in MeV/c",
     &detail::assign<&RecoParticle::p>},
    {"num_fragments", ScalarType::Int64, 1, false, kDerived, "", nullptr, "", "",
     "Number of fragments that make up this particle",
     &detail::assign<&RecoParticle::num_fragments>},
    {"size", ScalarType::Int64, 1, false, kDerived, "", nullptr, "", "",
     "Number of points, N, that make up this object",
     &detail::assign<&RecoParticle::size>},
    {"depositions_sum", ScalarType::Float64, 1, false, kDerived, "", nullptr, "", "",
     "Total amount of depositions",
     &detail::assign<&RecoParticle::depositions_sum>},
    {"depositions_q_sum", ScalarType::Float64, 1, false, kDerived, "", nullptr, "", "",
     "Total amount of input charge",
     &detail::assign<&RecoParticle::depositions_q_sum>},
    {"best_match_id", ScalarType::Int64, 1, false, kDerived, "", nullptr, "object", "opposite",
     "ID of the highest-overlap matched object, or -1 when unmatched",
     &detail::assign<&RecoParticle::best_match_id>},
    {"best_match_overlap", ScalarType::Float64, 1, false, kDerived, "", nullptr, "", "",
     "Overlap with the highest-overlap matched object, or -1 when unmatched",
     &detail::assign<&RecoParticle::best_match_overlap>},
    {"module_ids", ScalarType::Int32, 0, true, kDerived, "", nullptr, "module", "external",
     "(M) List of unique module indexes that make up this object",
     &detail::assign<&RecoParticle::module_ids>},
};

constexpr ClassDescriptor kDescriptor{"RecoParticle", kFields, std::size(kFields), "1.4.0"};

// The table has one row per Field enumerator, in order, and every member's C++ type
// agrees with its row (element type, count, variable flag, enum or not).
static_assert(std::size(kFields) == RecoParticle::kNumFields);
static_assert(static_cast<std::size_t>(RecoParticle::Field::module_ids)
              + 1 == RecoParticle::kNumFields);
SPINE_MODEL_CHECK_FIELD(RecoParticle, is_truth);
SPINE_MODEL_CHECK_FIELD(RecoParticle, units);
SPINE_MODEL_CHECK_FIELD(RecoParticle, id);
SPINE_MODEL_CHECK_FIELD(RecoParticle, is_contained);
SPINE_MODEL_CHECK_FIELD(RecoParticle, is_time_contained);
SPINE_MODEL_CHECK_FIELD(RecoParticle, is_cathode_crosser);
SPINE_MODEL_CHECK_FIELD(RecoParticle, is_matched);
SPINE_MODEL_CHECK_FIELD(RecoParticle, cathode_offset);
SPINE_MODEL_CHECK_FIELD(RecoParticle, index);
SPINE_MODEL_CHECK_FIELD(RecoParticle, orig_index);
SPINE_MODEL_CHECK_FIELD(RecoParticle, match_ids);
SPINE_MODEL_CHECK_FIELD(RecoParticle, match_overlaps);
SPINE_MODEL_CHECK_FIELD(RecoParticle, interaction_id);
SPINE_MODEL_CHECK_FIELD(RecoParticle, chi2_pid);
SPINE_MODEL_CHECK_FIELD(RecoParticle, is_primary);
SPINE_MODEL_CHECK_FIELD(RecoParticle, is_crt_matched);
SPINE_MODEL_CHECK_FIELD(RecoParticle, is_valid);
SPINE_MODEL_CHECK_FIELD(RecoParticle, length);
SPINE_MODEL_CHECK_FIELD(RecoParticle, calo_ke);
SPINE_MODEL_CHECK_FIELD(RecoParticle, csda_ke);
SPINE_MODEL_CHECK_FIELD(RecoParticle, mcs_ke);
SPINE_MODEL_CHECK_FIELD(RecoParticle, shape);
SPINE_MODEL_CHECK_FIELD(RecoParticle, pid);
SPINE_MODEL_CHECK_FIELD(RecoParticle, fragment_ids);
SPINE_MODEL_CHECK_FIELD(RecoParticle, start_point);
SPINE_MODEL_CHECK_FIELD(RecoParticle, end_point);
SPINE_MODEL_CHECK_FIELD(RecoParticle, chi2_per_pid);
SPINE_MODEL_CHECK_FIELD(RecoParticle, csda_ke_per_pid);
SPINE_MODEL_CHECK_FIELD(RecoParticle, mcs_ke_per_pid);
SPINE_MODEL_CHECK_FIELD(RecoParticle, crt_ids);
SPINE_MODEL_CHECK_FIELD(RecoParticle, crt_times);
SPINE_MODEL_CHECK_FIELD(RecoParticle, crt_scores);
SPINE_MODEL_CHECK_FIELD(RecoParticle, start_dedx);
SPINE_MODEL_CHECK_FIELD(RecoParticle, end_dedx);
SPINE_MODEL_CHECK_FIELD(RecoParticle, vertex_distance);
SPINE_MODEL_CHECK_FIELD(RecoParticle, start_straightness);
SPINE_MODEL_CHECK_FIELD(RecoParticle, directional_spread);
SPINE_MODEL_CHECK_FIELD(RecoParticle, axial_spread);
SPINE_MODEL_CHECK_FIELD(RecoParticle, start_dir);
SPINE_MODEL_CHECK_FIELD(RecoParticle, end_dir);
SPINE_MODEL_CHECK_FIELD(RecoParticle, pid_scores);
SPINE_MODEL_CHECK_FIELD(RecoParticle, primary_scores);
SPINE_MODEL_CHECK_FIELD(RecoParticle, ppn_ids);
SPINE_MODEL_CHECK_FIELD(RecoParticle, pdg_code);
SPINE_MODEL_CHECK_FIELD(RecoParticle, mass);
SPINE_MODEL_CHECK_FIELD(RecoParticle, ke);
SPINE_MODEL_CHECK_FIELD(RecoParticle, momentum);
SPINE_MODEL_CHECK_FIELD(RecoParticle, p);
SPINE_MODEL_CHECK_FIELD(RecoParticle, num_fragments);
SPINE_MODEL_CHECK_FIELD(RecoParticle, size);
SPINE_MODEL_CHECK_FIELD(RecoParticle, depositions_sum);
SPINE_MODEL_CHECK_FIELD(RecoParticle, depositions_q_sum);
SPINE_MODEL_CHECK_FIELD(RecoParticle, best_match_id);
SPINE_MODEL_CHECK_FIELD(RecoParticle, best_match_overlap);
SPINE_MODEL_CHECK_FIELD(RecoParticle, module_ids);

}  // namespace

const ClassDescriptor& RecoParticle::descriptor() { return kDescriptor; }

}  // namespace spine
