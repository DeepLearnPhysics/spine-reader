// GENERATED from SPINE 1.4.0 by tools/gen/generate.py. Do not edit.
#include <spine/model/truth_interaction.hpp>

#include <cstddef>
#include <iterator>

#include "model/field_check.hpp"

namespace spine {

namespace {

constexpr FieldDescriptor kFields[] = {
    {"orig_id", ScalarType::Int64, 1, false, 0, "", nullptr, "object", "external",
     "If matched to an MC truth instance, ID of the original instance",
     &detail::assign<&TruthInteraction::orig_id>},
    {"is_truth", ScalarType::Bool, 1, false, 0, "", nullptr, "", "",
     "Whether this is a truth object (always True for truth objects)",
     &detail::assign<&TruthInteraction::is_truth>},
    {"index_adapt", ScalarType::Int32, 0, true, kLiteSkip, "", nullptr, "", "",
     "(N') Voxel indexes corresponding to this object in the adapted cluster label tensor",
     &detail::assign<&TruthInteraction::index_adapt>},
    {"index_g4", ScalarType::Int32, 0, true, kLiteSkip, "", nullptr, "", "",
     "(N'') Fragment voxel indexes in the true Geant4 energy deposition tensor",
     &detail::assign<&TruthInteraction::index_g4>},
    {"units", ScalarType::String, 0, true, 0, "", nullptr, "", "",
     "Units in which the position coordinates are expressed",
     &detail::assign<&TruthInteraction::units>},
    {"id", ScalarType::Int64, 1, false, kIndex, "", nullptr, "interaction", "same",
     "Index of the neutrino in the list",
     &detail::assign<&TruthInteraction::id>},
    {"is_contained", ScalarType::Bool, 1, false, 0, "", nullptr, "", "",
     "Whether this object is fully contained within the detector",
     &detail::assign<&TruthInteraction::is_contained>},
    {"is_time_contained", ScalarType::Bool, 1, false, 0, "", nullptr, "", "",
     "Whether this object's points are within the expected readout window",
     &detail::assign<&TruthInteraction::is_time_contained>},
    {"is_cathode_crosser", ScalarType::Bool, 1, false, 0, "", nullptr, "", "",
     "True if the particle crossed a cathode, i.e. if it is made up of space points coming from > "
     "1 TPC in one module",
     &detail::assign<&TruthInteraction::is_cathode_crosser>},
    {"is_matched", ScalarType::Bool, 1, false, 0, "", nullptr, "", "",
     "True if a true object match was found",
     &detail::assign<&TruthInteraction::is_matched>},
    {"cathode_offset", ScalarType::Float64, 1, false, 0, "cm", nullptr, "", "",
     "If the particle is a cathode crosser, this is how far in cm one needs to move its points "
     "along the drift direction to reconcile at the cathode. This is directly proportional to time "
     "through time=offset/vdrift",
     &detail::assign<&TruthInteraction::cathode_offset>},
    {"index", ScalarType::Int32, 0, true, kLiteSkip, "", nullptr, "", "",
     "(N) Voxel indexes corresponding to this object in the input tensor",
     &detail::assign<&TruthInteraction::index>},
    {"orig_index", ScalarType::Int32, 0, true, kLiteSkip, "", nullptr, "", "",
     "(N) Original voxel indexes corresponding to this object in the original point cloud (before "
     "any filtering or adaptation)",
     &detail::assign<&TruthInteraction::orig_index>},
    {"match_ids", ScalarType::Int32, 0, true, 0, "", nullptr, "object", "opposite",
     "List of true object IDs this object is matched to",
     &detail::assign<&TruthInteraction::match_ids>},
    {"match_overlaps", ScalarType::Float32, 0, true, 0, "", nullptr, "", "",
     "List of match overlaps (IoU) between the object and its matches",
     &detail::assign<&TruthInteraction::match_overlaps>},
    {"is_fiducial", ScalarType::Bool, 1, false, 0, "", nullptr, "", "",
     "Whether this interaction vertex is inside the fiducial volume",
     &detail::assign<&TruthInteraction::is_fiducial>},
    {"is_flash_matched", ScalarType::Bool, 1, false, 0, "", nullptr, "", "",
     "True if the interaction was matched to an optical flash",
     &detail::assign<&TruthInteraction::is_flash_matched>},
    {"flash_total_pe", ScalarType::Float64, 1, false, 0, "", nullptr, "", "",
     "Total number of photoelectrons associated with the flash",
     &detail::assign<&TruthInteraction::flash_total_pe>},
    {"flash_hypo_pe", ScalarType::Float64, 1, false, 0, "", nullptr, "", "",
     "Total number of photoelectrons expected to be produced by the interaction",
     &detail::assign<&TruthInteraction::flash_hypo_pe>},
    {"particle_ids", ScalarType::Int32, 0, true, 0, "", nullptr, "particle", "same",
     "List of Particle IDs that make up this interaction",
     &detail::assign<&TruthInteraction::particle_ids>},
    {"vertex", ScalarType::Float32, 3, false, kPosition, "instance", nullptr, "", "",
     "(3) Coordinates of the interaction vertex",
     &detail::assign<&TruthInteraction::vertex>},
    {"flash_ids", ScalarType::Int32, 0, true, 0, "", nullptr, "flash", "external",
     "(F) Indices of the optical flashes the interaction is matched to",
     &detail::assign<&TruthInteraction::flash_ids>},
    {"flash_volume_ids", ScalarType::Int32, 0, true, 0, "", nullptr, "optical_volume", "external",
     "(F) Indices of the optical volumes the flashes where recorded in",
     &detail::assign<&TruthInteraction::flash_volume_ids>},
    {"flash_times", ScalarType::Float32, 0, true, 0, "us", nullptr, "", "",
     "(F) Times at which the flashes occurred in microseconds",
     &detail::assign<&TruthInteraction::flash_times>},
    {"flash_scores", ScalarType::Float32, 0, true, 0, "", nullptr, "", "",
     "(F) Flash matching quality scores reported for each match",
     &detail::assign<&TruthInteraction::flash_scores>},
    {"flash_hypothesis_ids", ScalarType::Int32, 0, true, 0, "", nullptr, "flash_hypothesis",
     "external",
     "(H) Indices of optical hypotheses associated with the interaction",
     &detail::assign<&TruthInteraction::flash_hypothesis_ids>},
    {"interaction_id", ScalarType::Int64, 1, false, kIndex, "", nullptr, "interaction", "same",
     "Index of the neutrino at the generator stage (e.g. Genie)",
     &detail::assign<&TruthInteraction::interaction_id>},
    {"current_type", ScalarType::Int64, 1, false, 0, "", &kNuCurrentTypeEnum, "", "",
     "Enumerated current type of the neutrino interaction",
     &detail::assign<&TruthInteraction::current_type>},
    {"interaction_scheme", ScalarType::Int64, 1, false, 0, "", &kNuInteractionSchemeEnum, "", "",
     "Enumerated scheme used to interpret the interaction mode/type codes",
     &detail::assign<&TruthInteraction::interaction_scheme>},
    {"interaction_mode", ScalarType::Int64, 1, false, kCategorical, "", nullptr, "", "",
     "Source-native neutrino interaction mode code",
     &detail::assign<&TruthInteraction::interaction_mode>},
    {"interaction_type", ScalarType::Int64, 1, false, kCategorical, "", nullptr, "", "",
     "Source-native neutrino interaction type code",
     &detail::assign<&TruthInteraction::interaction_type>},
    {"mct_index", ScalarType::Int64, 1, false, kCategorical, "", nullptr, "", "",
     "Index in the original MCTruth array from whence it came",
     &detail::assign<&TruthInteraction::mct_index>},
    {"track_id", ScalarType::Int64, 1, false, 0, "", nullptr, "geant_track", "external",
     "Geant4 track ID of the neutrino",
     &detail::assign<&TruthInteraction::track_id>},
    {"lepton_track_id", ScalarType::Int64, 1, false, 0, "", nullptr, "geant_track", "external",
     "Geant4 track ID of the lepton (if CC)",
     &detail::assign<&TruthInteraction::lepton_track_id>},
    {"pdg_code", ScalarType::Int64, 1, false, kCategorical | kPdg, "", nullptr, "", "",
     "PDG code of the neutrino",
     &detail::assign<&TruthInteraction::pdg_code>},
    {"lepton_pdg_code", ScalarType::Int64, 1, false, kCategorical | kPdg, "", nullptr, "", "",
     "PDG code of the outgoing lepton",
     &detail::assign<&TruthInteraction::lepton_pdg_code>},
    {"target", ScalarType::Int64, 1, false, kCategorical | kPdg, "", nullptr, "", "",
     "PDG code of the target object",
     &detail::assign<&TruthInteraction::target>},
    {"nucleon", ScalarType::Int64, 1, false, kCategorical | kPdg, "", nullptr, "", "",
     "PDG code of the target nucleon (if QE)",
     &detail::assign<&TruthInteraction::nucleon>},
    {"quark", ScalarType::Int64, 1, false, kCategorical | kPdg | kPdgMinusOneValid, "", nullptr, "",
     "",
     "PDG code of the target quark (if DIS)",
     &detail::assign<&TruthInteraction::quark>},
    {"energy_init", ScalarType::Float64, 1, false, 0, "GeV", nullptr, "", "",
     "Energy of the neutrino at its interaction point in GeV",
     &detail::assign<&TruthInteraction::energy_init>},
    {"hadronic_invariant_mass", ScalarType::Float64, 1, false, 0, "GeV/c^2", nullptr, "", "",
     "Hadronic invariant mass (W) in GeV/c^2",
     &detail::assign<&TruthInteraction::hadronic_invariant_mass>},
    {"momentum_transfer", ScalarType::Float64, 1, false, 0, "(GeV/c)^2", nullptr, "", "",
     "Squared momentum transfer (Q^2) in (GeV/c)^2",
     &detail::assign<&TruthInteraction::momentum_transfer>},
    {"momentum_transfer_mag", ScalarType::Float64, 1, false, 0, "GeV/c", nullptr, "", "",
     "Magnitude of the momentum transfer (Q3) in GeV/c",
     &detail::assign<&TruthInteraction::momentum_transfer_mag>},
    {"energy_transfer", ScalarType::Float64, 1, false, 0, "GeV", nullptr, "", "",
     "Energy transfer (Q0) in GeV",
     &detail::assign<&TruthInteraction::energy_transfer>},
    {"lepton_p", ScalarType::Float64, 1, false, 0, "GeV/c", nullptr, "", "",
     "Absolute momentum of the lepton",
     &detail::assign<&TruthInteraction::lepton_p>},
    {"distance_travel", ScalarType::Float64, 1, false, 0, "instance", nullptr, "", "",
     "True amount of distance traveled by the neutrino before interacting",
     &detail::assign<&TruthInteraction::distance_travel>},
    {"t", ScalarType::Float64, 1, false, 0, "ns", nullptr, "", "",
     "Interaction time (ns)",
     &detail::assign<&TruthInteraction::t>},
    {"theta", ScalarType::Float64, 1, false, 0, "rad", nullptr, "", "",
     "Angle between incoming and outgoing leptons in radians",
     &detail::assign<&TruthInteraction::theta>},
    {"bjorken_x", ScalarType::Float64, 1, false, 0, "", nullptr, "", "",
     "Bjorken scaling factor (x)",
     &detail::assign<&TruthInteraction::bjorken_x>},
    {"inelasticity", ScalarType::Float64, 1, false, 0, "", nullptr, "", "",
     "Inelasticity (y)",
     &detail::assign<&TruthInteraction::inelasticity>},
    {"creation_process", ScalarType::String, 0, true, 0, "", nullptr, "", "",
     "Creation process of the neutrino",
     &detail::assign<&TruthInteraction::creation_process>},
    {"position", ScalarType::Float32, 3, false, kPosition, "instance", nullptr, "", "",
     "Location of the neutrino interaction",
     &detail::assign<&TruthInteraction::position>},
    {"momentum", ScalarType::Float32, 3, false, kVector, "MeV/c", nullptr, "", "",
     "3-momentum of the neutrino at its interaction point",
     &detail::assign<&TruthInteraction::momentum>},
    {"nu_id", ScalarType::Int64, 1, false, 0, "", nullptr, "neutrino", "same",
     "Index of the neutrino matched to this interaction",
     &detail::assign<&TruthInteraction::nu_id>},
    {"reco_vertex", ScalarType::Float32, 3, false, kPosition, "instance", nullptr, "", "",
     "(3) Coordinates of the reconstructed interaction vertex",
     &detail::assign<&TruthInteraction::reco_vertex>},
    {"reco_dir", ScalarType::Float32, 3, false, kVector, "", nullptr, "", "",
     "(3) Reconstructed direction of the interaction",
     &detail::assign<&TruthInteraction::reco_dir>},
    {"time", ScalarType::Float64, 1, false, kDerived, "ns", nullptr, "", "",
     "Interaction time (ns), taken from the attached neutrino when available or from the earliest "
     "constituent particle otherwise",
     &detail::assign<&TruthInteraction::time>},
    {"dir", ScalarType::Float32, 3, false, kDerived | kVector, "", nullptr, "", "",
     "Direction of the neutrino at its interaction point",
     &detail::assign<&TruthInteraction::dir>},
    {"primary_particle_ids", ScalarType::Int32, 0, true, kDerived, "", nullptr, "particle", "same",
     "List of primary Particle IDs associated with this interaction",
     &detail::assign<&TruthInteraction::primary_particle_ids>},
    {"num_particles", ScalarType::Int64, 1, false, kDerived, "", nullptr, "", "",
     "Number of particles that make up this interaction",
     &detail::assign<&TruthInteraction::num_particles>},
    {"num_primary_particles", ScalarType::Int64, 1, false, kDerived, "", nullptr, "", "",
     "Number of primary particles associated with this interaction",
     &detail::assign<&TruthInteraction::num_primary_particles>},
    {"particle_counts", ScalarType::Int32, 6, false, kDerived, "", nullptr, "", "",
     "(P) Number of particles of each species in this interaction",
     &detail::assign<&TruthInteraction::particle_counts>},
    {"primary_particle_counts", ScalarType::Int32, 6, false, kDerived, "", nullptr, "", "",
     "(P) Number of primary particles of each species in this interaction",
     &detail::assign<&TruthInteraction::primary_particle_counts>},
    {"is_crt_matched", ScalarType::Bool, 1, false, kDerived, "", nullptr, "", "",
     "True if any particle in the interaction was matched to a CRT hit",
     &detail::assign<&TruthInteraction::is_crt_matched>},
    {"crt_ids", ScalarType::Int32, 0, true, kDerived, "", nullptr, "crthit", "external",
     "(C) Indices of the CRT hits the interaction is matched to",
     &detail::assign<&TruthInteraction::crt_ids>},
    {"crt_times", ScalarType::Float32, 0, true, kDerived, "us", nullptr, "", "",
     "(C) Times at which the CRT hits occurred in microseconds",
     &detail::assign<&TruthInteraction::crt_times>},
    {"crt_scores", ScalarType::Float32, 0, true, kDerived, "", nullptr, "", "",
     "(C) Quality metric associated with the CRT matches",
     &detail::assign<&TruthInteraction::crt_scores>},
    {"topology", ScalarType::String, 0, true, kDerived, "", nullptr, "", "",
     "String representing the interaction topology",
     &detail::assign<&TruthInteraction::topology>},
    {"size", ScalarType::Int64, 1, false, kDerived, "", nullptr, "", "",
     "Number of points, N, that make up this object",
     &detail::assign<&TruthInteraction::size>},
    {"depositions_sum", ScalarType::Float64, 1, false, kDerived, "", nullptr, "", "",
     "Total amount of depositions",
     &detail::assign<&TruthInteraction::depositions_sum>},
    {"depositions_q_sum", ScalarType::Float64, 1, false, kDerived, "", nullptr, "", "",
     "Total amount of input charge",
     &detail::assign<&TruthInteraction::depositions_q_sum>},
    {"best_match_id", ScalarType::Int64, 1, false, kDerived, "", nullptr, "object", "opposite",
     "ID of the highest-overlap matched object, or -1 when unmatched",
     &detail::assign<&TruthInteraction::best_match_id>},
    {"best_match_overlap", ScalarType::Float64, 1, false, kDerived, "", nullptr, "", "",
     "Overlap with the highest-overlap matched object, or -1 when unmatched",
     &detail::assign<&TruthInteraction::best_match_overlap>},
    {"module_ids", ScalarType::Int32, 0, true, kDerived, "", nullptr, "module", "external",
     "(M) List of unique module indexes that make up this object",
     &detail::assign<&TruthInteraction::module_ids>},
    {"size_adapt", ScalarType::Int64, 1, false, kDerived, "", nullptr, "", "",
     "Number of points, N', that make up this object in the adapted cluster label tensor",
     &detail::assign<&TruthInteraction::size_adapt>},
    {"size_g4", ScalarType::Int64, 1, false, kDerived, "", nullptr, "", "",
     "Number of points, N'', that make up this object in the true Geant4 energy deposition tensor",
     &detail::assign<&TruthInteraction::size_g4>},
    {"depositions_adapt_sum", ScalarType::Float64, 1, false, kDerived, "", nullptr, "", "",
     "Total amount of depositions in adapted cluster label tensor",
     &detail::assign<&TruthInteraction::depositions_adapt_sum>},
    {"depositions_adapt_q_sum", ScalarType::Float64, 1, false, kDerived, "", nullptr, "", "",
     "Total amount of depositions in adapted cluster label tensor in the same units as the input "
     "image",
     &detail::assign<&TruthInteraction::depositions_adapt_q_sum>},
    {"depositions_g4_sum", ScalarType::Float64, 1, false, kDerived, "", nullptr, "", "",
     "Total amount of true Geant4 depositions",
     &detail::assign<&TruthInteraction::depositions_g4_sum>},
};

constexpr ClassDescriptor kDescriptor{"TruthInteraction", kFields, std::size(kFields), "1.4.0"};

// The table has one row per Field enumerator, in order, and every member's C++ type
// agrees with its row (element type, count, variable flag, enum or not).
static_assert(std::size(kFields) == TruthInteraction::kNumFields);
static_assert(static_cast<std::size_t>(TruthInteraction::Field::depositions_g4_sum)
              + 1 == TruthInteraction::kNumFields);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, orig_id);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, is_truth);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, index_adapt);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, index_g4);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, units);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, id);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, is_contained);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, is_time_contained);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, is_cathode_crosser);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, is_matched);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, cathode_offset);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, index);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, orig_index);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, match_ids);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, match_overlaps);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, is_fiducial);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, is_flash_matched);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, flash_total_pe);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, flash_hypo_pe);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, particle_ids);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, vertex);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, flash_ids);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, flash_volume_ids);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, flash_times);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, flash_scores);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, flash_hypothesis_ids);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, interaction_id);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, current_type);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, interaction_scheme);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, interaction_mode);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, interaction_type);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, mct_index);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, track_id);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, lepton_track_id);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, pdg_code);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, lepton_pdg_code);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, target);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, nucleon);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, quark);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, energy_init);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, hadronic_invariant_mass);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, momentum_transfer);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, momentum_transfer_mag);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, energy_transfer);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, lepton_p);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, distance_travel);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, t);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, theta);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, bjorken_x);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, inelasticity);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, creation_process);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, position);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, momentum);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, nu_id);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, reco_vertex);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, reco_dir);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, time);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, dir);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, primary_particle_ids);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, num_particles);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, num_primary_particles);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, particle_counts);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, primary_particle_counts);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, is_crt_matched);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, crt_ids);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, crt_times);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, crt_scores);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, topology);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, size);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, depositions_sum);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, depositions_q_sum);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, best_match_id);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, best_match_overlap);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, module_ids);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, size_adapt);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, size_g4);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, depositions_adapt_sum);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, depositions_adapt_q_sum);
SPINE_MODEL_CHECK_FIELD(TruthInteraction, depositions_g4_sum);

}  // namespace

const ClassDescriptor& TruthInteraction::descriptor() { return kDescriptor; }

}  // namespace spine
