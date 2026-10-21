// GENERATED from SPINE 1.4.0 by tools/gen/generate.py. Do not edit.
#include <spine/model/reco_interaction.hpp>

#include <cstddef>
#include <iterator>

#include "model/field_check.hpp"

namespace spine {

namespace {

constexpr FieldDescriptor kFields[] = {
    {"is_truth", ScalarType::Bool, 1, false, 0, "", nullptr, "", "",
     "Whether this is a truth object (always False for reconstructed)",
     &detail::assign<&RecoInteraction::is_truth>},
    {"units", ScalarType::String, 0, true, 0, "", nullptr, "", "",
     "Units in which coordinates are expressed",
     &detail::assign<&RecoInteraction::units>},
    {"id", ScalarType::Int64, 1, false, kIndex, "", nullptr, "object", "same",
     "Unique index of the object within the object list",
     &detail::assign<&RecoInteraction::id>},
    {"is_contained", ScalarType::Bool, 1, false, 0, "", nullptr, "", "",
     "Whether this object is fully contained within the detector",
     &detail::assign<&RecoInteraction::is_contained>},
    {"is_time_contained", ScalarType::Bool, 1, false, 0, "", nullptr, "", "",
     "Whether this object's points are within the expected readout window",
     &detail::assign<&RecoInteraction::is_time_contained>},
    {"is_cathode_crosser", ScalarType::Bool, 1, false, 0, "", nullptr, "", "",
     "True if the particle crossed a cathode, i.e. if it is made up of space points coming from > "
     "1 TPC in one module",
     &detail::assign<&RecoInteraction::is_cathode_crosser>},
    {"is_matched", ScalarType::Bool, 1, false, 0, "", nullptr, "", "",
     "True if a true object match was found",
     &detail::assign<&RecoInteraction::is_matched>},
    {"cathode_offset", ScalarType::Float64, 1, false, 0, "cm", nullptr, "", "",
     "If the particle is a cathode crosser, this is how far in cm one needs to move its points "
     "along the drift direction to reconcile at the cathode. This is directly proportional to time "
     "through time=offset/vdrift",
     &detail::assign<&RecoInteraction::cathode_offset>},
    {"index", ScalarType::Int32, 0, true, kLiteSkip, "", nullptr, "", "",
     "(N) Voxel indexes corresponding to this object in the input tensor",
     &detail::assign<&RecoInteraction::index>},
    {"orig_index", ScalarType::Int32, 0, true, kLiteSkip, "", nullptr, "", "",
     "(N) Original voxel indexes corresponding to this object in the original point cloud (before "
     "any filtering or adaptation)",
     &detail::assign<&RecoInteraction::orig_index>},
    {"match_ids", ScalarType::Int32, 0, true, 0, "", nullptr, "object", "opposite",
     "List of true object IDs this object is matched to",
     &detail::assign<&RecoInteraction::match_ids>},
    {"match_overlaps", ScalarType::Float32, 0, true, 0, "", nullptr, "", "",
     "List of match overlaps (IoU) between the object and its matches",
     &detail::assign<&RecoInteraction::match_overlaps>},
    {"is_fiducial", ScalarType::Bool, 1, false, 0, "", nullptr, "", "",
     "Whether this interaction vertex is inside the fiducial volume",
     &detail::assign<&RecoInteraction::is_fiducial>},
    {"is_flash_matched", ScalarType::Bool, 1, false, 0, "", nullptr, "", "",
     "True if the interaction was matched to an optical flash",
     &detail::assign<&RecoInteraction::is_flash_matched>},
    {"flash_total_pe", ScalarType::Float64, 1, false, 0, "", nullptr, "", "",
     "Total number of photoelectrons associated with the flash",
     &detail::assign<&RecoInteraction::flash_total_pe>},
    {"flash_hypo_pe", ScalarType::Float64, 1, false, 0, "", nullptr, "", "",
     "Total number of photoelectrons expected to be produced by the interaction",
     &detail::assign<&RecoInteraction::flash_hypo_pe>},
    {"particle_ids", ScalarType::Int32, 0, true, 0, "", nullptr, "particle", "same",
     "List of Particle IDs that make up this interaction",
     &detail::assign<&RecoInteraction::particle_ids>},
    {"vertex", ScalarType::Float32, 3, false, kPosition, "instance", nullptr, "", "",
     "(3) Coordinates of the interaction vertex",
     &detail::assign<&RecoInteraction::vertex>},
    {"flash_ids", ScalarType::Int32, 0, true, 0, "", nullptr, "flash", "external",
     "(F) Indices of the optical flashes the interaction is matched to",
     &detail::assign<&RecoInteraction::flash_ids>},
    {"flash_volume_ids", ScalarType::Int32, 0, true, 0, "", nullptr, "optical_volume", "external",
     "(F) Indices of the optical volumes the flashes where recorded in",
     &detail::assign<&RecoInteraction::flash_volume_ids>},
    {"flash_times", ScalarType::Float32, 0, true, 0, "us", nullptr, "", "",
     "(F) Times at which the flashes occurred in microseconds",
     &detail::assign<&RecoInteraction::flash_times>},
    {"flash_scores", ScalarType::Float32, 0, true, 0, "", nullptr, "", "",
     "(F) Flash matching quality scores reported for each match",
     &detail::assign<&RecoInteraction::flash_scores>},
    {"flash_hypothesis_ids", ScalarType::Int32, 0, true, 0, "", nullptr, "flash_hypothesis",
     "external",
     "(H) Indices of optical hypotheses associated with the interaction",
     &detail::assign<&RecoInteraction::flash_hypothesis_ids>},
    {"dir", ScalarType::Float32, 3, false, kVector, "", nullptr, "", "",
     "(3) Direction of the interaction",
     &detail::assign<&RecoInteraction::dir>},
    {"primary_particle_ids", ScalarType::Int32, 0, true, kDerived, "", nullptr, "particle", "same",
     "List of primary Particle IDs associated with this interaction",
     &detail::assign<&RecoInteraction::primary_particle_ids>},
    {"num_particles", ScalarType::Int64, 1, false, kDerived, "", nullptr, "", "",
     "Number of particles that make up this interaction",
     &detail::assign<&RecoInteraction::num_particles>},
    {"num_primary_particles", ScalarType::Int64, 1, false, kDerived, "", nullptr, "", "",
     "Number of primary particles associated with this interaction",
     &detail::assign<&RecoInteraction::num_primary_particles>},
    {"particle_counts", ScalarType::Int32, 6, false, kDerived, "", nullptr, "", "",
     "(P) Number of particles of each species in this interaction",
     &detail::assign<&RecoInteraction::particle_counts>},
    {"primary_particle_counts", ScalarType::Int32, 6, false, kDerived, "", nullptr, "", "",
     "(P) Number of primary particles of each species in this interaction",
     &detail::assign<&RecoInteraction::primary_particle_counts>},
    {"is_crt_matched", ScalarType::Bool, 1, false, kDerived, "", nullptr, "", "",
     "True if any particle in the interaction was matched to a CRT hit",
     &detail::assign<&RecoInteraction::is_crt_matched>},
    {"crt_ids", ScalarType::Int32, 0, true, kDerived, "", nullptr, "crthit", "external",
     "(C) Indices of the CRT hits the interaction is matched to",
     &detail::assign<&RecoInteraction::crt_ids>},
    {"crt_times", ScalarType::Float32, 0, true, kDerived, "us", nullptr, "", "",
     "(C) Times at which the CRT hits occurred in microseconds",
     &detail::assign<&RecoInteraction::crt_times>},
    {"crt_scores", ScalarType::Float32, 0, true, kDerived, "", nullptr, "", "",
     "(C) Quality metric associated with the CRT matches",
     &detail::assign<&RecoInteraction::crt_scores>},
    {"topology", ScalarType::String, 0, true, kDerived, "", nullptr, "", "",
     "String representing the interaction topology",
     &detail::assign<&RecoInteraction::topology>},
    {"size", ScalarType::Int64, 1, false, kDerived, "", nullptr, "", "",
     "Number of points, N, that make up this object",
     &detail::assign<&RecoInteraction::size>},
    {"depositions_sum", ScalarType::Float64, 1, false, kDerived, "", nullptr, "", "",
     "Total amount of depositions",
     &detail::assign<&RecoInteraction::depositions_sum>},
    {"depositions_q_sum", ScalarType::Float64, 1, false, kDerived, "", nullptr, "", "",
     "Total amount of input charge",
     &detail::assign<&RecoInteraction::depositions_q_sum>},
    {"best_match_id", ScalarType::Int64, 1, false, kDerived, "", nullptr, "object", "opposite",
     "ID of the highest-overlap matched object, or -1 when unmatched",
     &detail::assign<&RecoInteraction::best_match_id>},
    {"best_match_overlap", ScalarType::Float64, 1, false, kDerived, "", nullptr, "", "",
     "Overlap with the highest-overlap matched object, or -1 when unmatched",
     &detail::assign<&RecoInteraction::best_match_overlap>},
    {"module_ids", ScalarType::Int32, 0, true, kDerived, "", nullptr, "module", "external",
     "(M) List of unique module indexes that make up this object",
     &detail::assign<&RecoInteraction::module_ids>},
};

constexpr ClassDescriptor kDescriptor{"RecoInteraction", kFields, std::size(kFields), "1.4.0"};

// The table has one row per Field enumerator, in order, and every member's C++ type
// agrees with its row (element type, count, variable flag, enum or not).
static_assert(std::size(kFields) == RecoInteraction::kNumFields);
static_assert(static_cast<std::size_t>(RecoInteraction::Field::module_ids)
              + 1 == RecoInteraction::kNumFields);
SPINE_MODEL_CHECK_FIELD(RecoInteraction, is_truth);
SPINE_MODEL_CHECK_FIELD(RecoInteraction, units);
SPINE_MODEL_CHECK_FIELD(RecoInteraction, id);
SPINE_MODEL_CHECK_FIELD(RecoInteraction, is_contained);
SPINE_MODEL_CHECK_FIELD(RecoInteraction, is_time_contained);
SPINE_MODEL_CHECK_FIELD(RecoInteraction, is_cathode_crosser);
SPINE_MODEL_CHECK_FIELD(RecoInteraction, is_matched);
SPINE_MODEL_CHECK_FIELD(RecoInteraction, cathode_offset);
SPINE_MODEL_CHECK_FIELD(RecoInteraction, index);
SPINE_MODEL_CHECK_FIELD(RecoInteraction, orig_index);
SPINE_MODEL_CHECK_FIELD(RecoInteraction, match_ids);
SPINE_MODEL_CHECK_FIELD(RecoInteraction, match_overlaps);
SPINE_MODEL_CHECK_FIELD(RecoInteraction, is_fiducial);
SPINE_MODEL_CHECK_FIELD(RecoInteraction, is_flash_matched);
SPINE_MODEL_CHECK_FIELD(RecoInteraction, flash_total_pe);
SPINE_MODEL_CHECK_FIELD(RecoInteraction, flash_hypo_pe);
SPINE_MODEL_CHECK_FIELD(RecoInteraction, particle_ids);
SPINE_MODEL_CHECK_FIELD(RecoInteraction, vertex);
SPINE_MODEL_CHECK_FIELD(RecoInteraction, flash_ids);
SPINE_MODEL_CHECK_FIELD(RecoInteraction, flash_volume_ids);
SPINE_MODEL_CHECK_FIELD(RecoInteraction, flash_times);
SPINE_MODEL_CHECK_FIELD(RecoInteraction, flash_scores);
SPINE_MODEL_CHECK_FIELD(RecoInteraction, flash_hypothesis_ids);
SPINE_MODEL_CHECK_FIELD(RecoInteraction, dir);
SPINE_MODEL_CHECK_FIELD(RecoInteraction, primary_particle_ids);
SPINE_MODEL_CHECK_FIELD(RecoInteraction, num_particles);
SPINE_MODEL_CHECK_FIELD(RecoInteraction, num_primary_particles);
SPINE_MODEL_CHECK_FIELD(RecoInteraction, particle_counts);
SPINE_MODEL_CHECK_FIELD(RecoInteraction, primary_particle_counts);
SPINE_MODEL_CHECK_FIELD(RecoInteraction, is_crt_matched);
SPINE_MODEL_CHECK_FIELD(RecoInteraction, crt_ids);
SPINE_MODEL_CHECK_FIELD(RecoInteraction, crt_times);
SPINE_MODEL_CHECK_FIELD(RecoInteraction, crt_scores);
SPINE_MODEL_CHECK_FIELD(RecoInteraction, topology);
SPINE_MODEL_CHECK_FIELD(RecoInteraction, size);
SPINE_MODEL_CHECK_FIELD(RecoInteraction, depositions_sum);
SPINE_MODEL_CHECK_FIELD(RecoInteraction, depositions_q_sum);
SPINE_MODEL_CHECK_FIELD(RecoInteraction, best_match_id);
SPINE_MODEL_CHECK_FIELD(RecoInteraction, best_match_overlap);
SPINE_MODEL_CHECK_FIELD(RecoInteraction, module_ids);

}  // namespace

const ClassDescriptor& RecoInteraction::descriptor() { return kDescriptor; }

}  // namespace spine
