// GENERATED from SPINE 1.4.0 by tools/gen/generate.py. Do not edit.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include <spine/descriptor.hpp>
#include <spine/model/common.hpp>
#include <spine/model/enums.hpp>
#include <spine/object.hpp>

namespace spine {

/// Truth interaction information. SPINE `spine.data.out.interaction.TruthInteraction`: 79 fields,
/// in the order SPINE's HDF5 writer stores them. A field the source file lacks keeps its default
/// and has() is false. Fields with units 'instance' are in the unit named by the member `units`.
struct TruthInteraction : Object<79> {
  enum class Field : std::uint16_t {
    orig_id,
    is_truth,
    index_adapt,
    index_g4,
    units,
    id,
    is_contained,
    is_time_contained,
    is_cathode_crosser,
    is_matched,
    cathode_offset,
    index,
    orig_index,
    match_ids,
    match_overlaps,
    is_fiducial,
    is_flash_matched,
    flash_total_pe,
    flash_hypo_pe,
    particle_ids,
    vertex,
    flash_ids,
    flash_volume_ids,
    flash_times,
    flash_scores,
    flash_hypothesis_ids,
    interaction_id,
    current_type,
    interaction_scheme,
    interaction_mode,
    interaction_type,
    mct_index,
    track_id,
    lepton_track_id,
    pdg_code,
    lepton_pdg_code,
    target,
    nucleon,
    quark,
    energy_init,
    hadronic_invariant_mass,
    momentum_transfer,
    momentum_transfer_mag,
    energy_transfer,
    lepton_p,
    distance_travel,
    t,
    theta,
    bjorken_x,
    inelasticity,
    creation_process,
    position,
    momentum,
    nu_id,
    reco_vertex,
    reco_dir,
    time,
    dir,
    primary_particle_ids,
    num_particles,
    num_primary_particles,
    particle_counts,
    primary_particle_counts,
    is_crt_matched,
    crt_ids,
    crt_times,
    crt_scores,
    topology,
    size,
    depositions_sum,
    depositions_q_sum,
    best_match_id,
    best_match_overlap,
    module_ids,
    size_adapt,
    size_g4,
    depositions_adapt_sum,
    depositions_adapt_q_sum,
    depositions_g4_sum,
  };

  /// If matched to an MC truth instance, ID of the original instance
  std::int64_t orig_id = -1;
  /// Whether this is a truth object (always True for truth objects)
  bool is_truth = true;
  /// (N') Voxel indexes corresponding to this object in the adapted cluster label tensor
  /// [lite_skip]
  std::vector<std::int32_t> index_adapt;
  /// (N'') Fragment voxel indexes in the true Geant4 energy deposition tensor [lite_skip]
  std::vector<std::int32_t> index_g4;
  /// Units in which the position coordinates are expressed
  std::string units = "cm";
  /// Index of the neutrino in the list
  std::int64_t id = -1;
  /// Whether this object is fully contained within the detector
  bool is_contained = false;
  /// Whether this object's points are within the expected readout window
  bool is_time_contained = false;
  /// True if the particle crossed a cathode, i.e. if it is made up of space points coming from > 1
  /// TPC in one module
  bool is_cathode_crosser = false;
  /// True if a true object match was found
  bool is_matched = false;
  /// If the particle is a cathode crosser, this is how far in cm one needs to move its points along
  /// the drift direction to reconcile at the cathode. This is directly proportional to time through
  /// time=offset/vdrift [units: cm]
  double cathode_offset = detail::kNaNd;
  /// (N) Voxel indexes corresponding to this object in the input tensor [lite_skip]
  std::vector<std::int32_t> index;
  /// (N) Original voxel indexes corresponding to this object in the original point cloud (before
  /// any filtering or adaptation) [lite_skip]
  std::vector<std::int32_t> orig_index;
  /// List of true object IDs this object is matched to
  std::vector<std::int32_t> match_ids;
  /// List of match overlaps (IoU) between the object and its matches
  std::vector<float> match_overlaps;
  /// Whether this interaction vertex is inside the fiducial volume
  bool is_fiducial = false;
  /// True if the interaction was matched to an optical flash
  bool is_flash_matched = false;
  /// Total number of photoelectrons associated with the flash
  double flash_total_pe = detail::kNaNd;
  /// Total number of photoelectrons expected to be produced by the interaction
  double flash_hypo_pe = detail::kNaNd;
  /// List of Particle IDs that make up this interaction
  std::vector<std::int32_t> particle_ids;
  /// (3) Coordinates of the interaction vertex [units: instance]
  std::array<float, 3> vertex = {detail::kNaNf, detail::kNaNf, detail::kNaNf};
  /// (F) Indices of the optical flashes the interaction is matched to
  std::vector<std::int32_t> flash_ids;
  /// (F) Indices of the optical volumes the flashes where recorded in
  std::vector<std::int32_t> flash_volume_ids;
  /// (F) Times at which the flashes occurred in microseconds [units: us]
  std::vector<float> flash_times;
  /// (F) Flash matching quality scores reported for each match
  std::vector<float> flash_scores;
  /// (H) Indices of optical hypotheses associated with the interaction
  std::vector<std::int32_t> flash_hypothesis_ids;
  /// Index of the neutrino at the generator stage (e.g. Genie)
  std::int64_t interaction_id = -1;
  /// Enumerated current type of the neutrino interaction
  NuCurrentType current_type = NuCurrentType::UNKNOWN;
  /// Enumerated scheme used to interpret the interaction mode/type codes
  NuInteractionScheme interaction_scheme = NuInteractionScheme::UNKNOWN;
  /// Source-native neutrino interaction mode code
  std::int64_t interaction_mode = -1;
  /// Source-native neutrino interaction type code
  std::int64_t interaction_type = -1;
  /// Index in the original MCTruth array from whence it came
  std::int64_t mct_index = -1;
  /// Geant4 track ID of the neutrino
  std::int64_t track_id = -1;
  /// Geant4 track ID of the lepton (if CC)
  std::int64_t lepton_track_id = -1;
  /// PDG code of the neutrino
  std::int64_t pdg_code = 0;
  /// PDG code of the outgoing lepton
  std::int64_t lepton_pdg_code = 0;
  /// PDG code of the target object
  std::int64_t target = 0;
  /// PDG code of the target nucleon (if QE)
  std::int64_t nucleon = 0;
  /// PDG code of the target quark (if DIS)
  std::int64_t quark = 0;
  /// Energy of the neutrino at its interaction point in GeV [units: GeV]
  double energy_init = detail::kNaNd;
  /// Hadronic invariant mass (W) in GeV/c^2 [units: GeV/c^2]
  double hadronic_invariant_mass = detail::kNaNd;
  /// Squared momentum transfer (Q^2) in (GeV/c)^2 [units: (GeV/c)^2]
  double momentum_transfer = detail::kNaNd;
  /// Magnitude of the momentum transfer (Q3) in GeV/c [units: GeV/c]
  double momentum_transfer_mag = detail::kNaNd;
  /// Energy transfer (Q0) in GeV [units: GeV]
  double energy_transfer = detail::kNaNd;
  /// Absolute momentum of the lepton [units: GeV/c]
  double lepton_p = detail::kNaNd;
  /// True amount of distance traveled by the neutrino before interacting [units: instance]
  double distance_travel = detail::kNaNd;
  /// Interaction time (ns) [units: ns]
  double t = detail::kNaNd;
  /// Angle between incoming and outgoing leptons in radians [units: rad]
  double theta = detail::kNaNd;
  /// Bjorken scaling factor (x)
  double bjorken_x = detail::kNaNd;
  /// Inelasticity (y)
  double inelasticity = detail::kNaNd;
  /// Creation process of the neutrino
  std::string creation_process;
  /// Location of the neutrino interaction [units: instance]
  std::array<float, 3> position = {detail::kNaNf, detail::kNaNf, detail::kNaNf};
  /// 3-momentum of the neutrino at its interaction point [units: MeV/c]
  std::array<float, 3> momentum = {detail::kNaNf, detail::kNaNf, detail::kNaNf};
  /// Index of the neutrino matched to this interaction
  std::int64_t nu_id = -1;
  /// (3) Coordinates of the reconstructed interaction vertex [units: instance]
  std::array<float, 3> reco_vertex = {detail::kNaNf, detail::kNaNf, detail::kNaNf};
  /// (3) Reconstructed direction of the interaction
  std::array<float, 3> reco_dir = {detail::kNaNf, detail::kNaNf, detail::kNaNf};
  /// Interaction time (ns), taken from the attached neutrino when available or from the earliest
  /// constituent particle otherwise [units: ns] [derived]
  double time = detail::kNaNd;
  /// Direction of the neutrino at its interaction point [derived]
  std::array<float, 3> dir = {detail::kNaNf, detail::kNaNf, detail::kNaNf};
  /// List of primary Particle IDs associated with this interaction [derived]
  std::vector<std::int32_t> primary_particle_ids;
  /// Number of particles that make up this interaction [derived]
  std::int64_t num_particles = 0;
  /// Number of primary particles associated with this interaction [derived]
  std::int64_t num_primary_particles = 0;
  /// (P) Number of particles of each species in this interaction [derived]
  std::array<std::int32_t, 6> particle_counts = detail::filled<std::int32_t, 6>(0);
  /// (P) Number of primary particles of each species in this interaction [derived]
  std::array<std::int32_t, 6> primary_particle_counts = detail::filled<std::int32_t, 6>(0);
  /// True if any particle in the interaction was matched to a CRT hit [derived]
  bool is_crt_matched = false;
  /// (C) Indices of the CRT hits the interaction is matched to [derived]
  std::vector<std::int32_t> crt_ids;
  /// (C) Times at which the CRT hits occurred in microseconds [units: us] [derived]
  std::vector<float> crt_times;
  /// (C) Quality metric associated with the CRT matches [derived]
  std::vector<float> crt_scores;
  /// String representing the interaction topology [derived]
  std::string topology;
  /// Number of points, N, that make up this object [derived]
  std::int64_t size = 0;
  /// Total amount of depositions [derived]
  double depositions_sum = 0.0;
  /// Total amount of input charge [derived]
  double depositions_q_sum = 0.0;
  /// ID of the highest-overlap matched object, or -1 when unmatched [derived]
  std::int64_t best_match_id = -1;
  /// Overlap with the highest-overlap matched object, or -1 when unmatched [derived]
  double best_match_overlap = -1.0;
  /// (M) List of unique module indexes that make up this object [derived]
  std::vector<std::int32_t> module_ids;
  /// Number of points, N', that make up this object in the adapted cluster label tensor [derived]
  std::int64_t size_adapt = 0;
  /// Number of points, N'', that make up this object in the true Geant4 energy deposition tensor
  /// [derived]
  std::int64_t size_g4 = 0;
  /// Total amount of depositions in adapted cluster label tensor [derived]
  double depositions_adapt_sum = 0.0;
  /// Total amount of depositions in adapted cluster label tensor in the same units as the input
  /// image [derived]
  double depositions_adapt_q_sum = 0.0;
  /// Total amount of true Geant4 depositions [derived]
  double depositions_g4_sum = 0.0;

  bool has(Field f) const { return has_field(static_cast<std::size_t>(f)); }
  static const ClassDescriptor& descriptor();
};

}  // namespace spine
