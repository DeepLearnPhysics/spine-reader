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

/// Truth particle information. SPINE `spine.data.out.particle.TruthParticle`: 98 fields, in the
/// order SPINE's HDF5 writer stores them. A field the source file lacks keeps its default and has()
/// is false. Fields with units 'instance' are in the unit named by the member `units`.
struct TruthParticle : Object<98> {
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
    interaction_id,
    chi2_pid,
    is_primary,
    is_crt_matched,
    is_valid,
    length,
    calo_ke,
    csda_ke,
    mcs_ke,
    shape,
    pid,
    fragment_ids,
    start_point,
    end_point,
    chi2_per_pid,
    csda_ke_per_pid,
    mcs_ke_per_pid,
    crt_ids,
    crt_times,
    crt_scores,
    parent_id,
    group_id,
    nu_id,
    children_id,
    mct_index,
    mcst_index,
    group_primary,
    interaction_primary,
    track_id,
    parent_track_id,
    ancestor_track_id,
    pdg_code,
    parent_pdg_code,
    ancestor_pdg_code,
    num_voxels,
    t,
    end_t,
    first_step_t,
    last_step_t,
    parent_t,
    ancestor_t,
    energy_init,
    energy_deposit,
    distance_travel,
    creation_process,
    parent_creation_process,
    ancestor_creation_process,
    position,
    end_position,
    parent_position,
    ancestor_position,
    first_step,
    last_step,
    momentum,
    end_momentum,
    orig_interaction_id,
    orig_parent_id,
    orig_group_id,
    reco_length,
    orig_children_id,
    children_counts,
    reco_start_dir,
    reco_end_dir,
    start_dir,
    end_dir,
    ke,
    reco_ke,
    reco_momentum,
    p,
    end_p,
    mass,
    num_fragments,
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
  /// Units in which the position attributes are expressed
  std::string units = "cm";
  /// Index of the particle in the list
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
  /// Index of the interaction the partile belongs to
  std::int64_t interaction_id = -1;
  /// Particle species as predicted by the chi2 template method (Muon (2), Charged Pion (3), Proton
  /// (4), Kaon (5)) of this particle
  ParticlePID chi2_pid = ParticlePID::UNKNOWN;
  /// Whether this particle was the first in the particle group
  bool is_primary = false;
  /// True if the particle was matched to a CRT hit
  bool is_crt_matched = false;
  /// Whether this particle counts towards an interaction topology. This may be False if a particle
  /// is below some defined energy threshold.
  bool is_valid = true;
  /// Length of the particle (only assigned to track objects) [units: instance]
  double length = detail::kNaNd;
  /// Kinetic energy reconstructed from the energy depositions alone in MeV [units: MeV]
  double calo_ke = detail::kNaNd;
  /// Kinetic energy reconstructed from the particle range in MeV [units: MeV]
  double csda_ke = detail::kNaNd;
  /// Kinetic energy reconstructed using the MCS method in MeV [units: MeV]
  double mcs_ke = detail::kNaNd;
  /// Enumerated semantic type of the particle
  ParticleShape shape = ParticleShape::UNKNOWN;
  /// Enumerated particle species type of the particle
  ParticlePID pid = ParticlePID::UNKNOWN;
  /// List of Fragment IDs that make up this particle [units: instance]
  std::vector<std::int32_t> fragment_ids;
  /// (3) Particle start point [units: instance]
  std::array<float, 3> start_point = {detail::kNaNf, detail::kNaNf, detail::kNaNf};
  /// (3) Particle end point (only assigned to track objects) [units: instance]
  std::array<float, 3> end_point = {detail::kNaNf, detail::kNaNf, detail::kNaNf};
  /// (P) Array of chi2 values associated with each particle class
  std::array<float, 6> chi2_per_pid = detail::filled<float, 6>(detail::kNaNf);
  /// (P) Same as `csda_ke` but for every available track PID hypothesis [units: MeV]
  std::array<float, 6> csda_ke_per_pid = detail::filled<float, 6>(detail::kNaNf);
  /// (P) Same as `mcs_ke` but for every available track PID hypothesis [units: MeV]
  std::array<float, 6> mcs_ke_per_pid = detail::filled<float, 6>(detail::kNaNf);
  /// (C) Indices of the CRT hits the particle is matched to
  std::vector<std::int32_t> crt_ids;
  /// (C) Times at which the CRT hits occurred in microseconds [units: us]
  std::vector<float> crt_times;
  /// (C) Quality metric associated with the CRT matches
  std::vector<float> crt_scores;
  /// Index of the parent particle
  std::int64_t parent_id = -1;
  /// Index of the group the particle belongs to
  std::int64_t group_id = -1;
  /// Index of the neutrino this particle belongs to
  std::int64_t nu_id = -1;
  /// List of indexes of the children particles
  std::vector<std::int32_t> children_id;
  /// Index in the original MCTruth array from whence it came
  std::int64_t mct_index = -1;
  /// Index in the original MCTrack/MCShower array from whence it came
  std::int64_t mcst_index = -1;
  /// Whether this particle is primary in its group or not
  std::int64_t group_primary = -1;
  /// Whether the particle is primary in its interaction or not
  std::int64_t interaction_primary = -1;
  /// Geant4 track ID
  std::int64_t track_id = -1;
  /// Geant4 track ID of the parent particle
  std::int64_t parent_track_id = -1;
  /// Geant4 track ID of the ancestor particle
  std::int64_t ancestor_track_id = -1;
  /// Particle PDG code
  std::int64_t pdg_code = 0;
  /// Particle PDG code of the parent particle
  std::int64_t parent_pdg_code = 0;
  /// Particle PDG code of the ancestor particle
  std::int64_t ancestor_pdg_code = 0;
  /// Number of voxels matched to this particle instance
  std::int64_t num_voxels = -1;
  /// Particle creation time (ns) [units: ns]
  double t = detail::kNaNd;
  /// Particle death time (ns) [units: ns]
  double end_t = detail::kNaNd;
  /// Time of the first energy deposition of the particle (ns) [units: ns]
  double first_step_t = detail::kNaNd;
  /// Time of the last energy deposition of the particle (ns) [units: ns]
  double last_step_t = detail::kNaNd;
  /// Particle creation time of the parent particle (ns) [units: ns]
  double parent_t = detail::kNaNd;
  /// Particle creation time of the ancestor particle (ns) [units: ns]
  double ancestor_t = detail::kNaNd;
  /// True initial energy in MeV [units: MeV]
  double energy_init = detail::kNaNd;
  /// Amount of energy matched to this particle instance in MeV [units: MeV]
  double energy_deposit = detail::kNaNd;
  /// True amount of distance traveled by the particle in the active volume [units: cm]
  double distance_travel = detail::kNaNd;
  /// Creation process
  std::string creation_process;
  /// Creation process of the parent particle
  std::string parent_creation_process;
  /// Creation process of the ancestor particle
  std::string ancestor_creation_process;
  /// Location of the creation point of the particle [units: instance]
  std::array<float, 3> position = {detail::kNaNf, detail::kNaNf, detail::kNaNf};
  /// Location where the particle stopped [units: instance]
  std::array<float, 3> end_position = {detail::kNaNf, detail::kNaNf, detail::kNaNf};
  /// Location of the creation point of the parent particle [units: instance]
  std::array<float, 3> parent_position = {detail::kNaNf, detail::kNaNf, detail::kNaNf};
  /// Location of the creation point of the ancestor particle [units: instance]
  std::array<float, 3> ancestor_position = {detail::kNaNf, detail::kNaNf, detail::kNaNf};
  /// Location of the first energy deposition of the particle [units: instance]
  std::array<float, 3> first_step = {detail::kNaNf, detail::kNaNf, detail::kNaNf};
  /// Location of the last energy deposition of the particle [units: instance]
  std::array<float, 3> last_step = {detail::kNaNf, detail::kNaNf, detail::kNaNf};
  /// 3-momentum of the particle at the production point [units: MeV/c]
  std::array<float, 3> momentum = {detail::kNaNf, detail::kNaNf, detail::kNaNf};
  /// 3-momentum of the particle at where it stops or exits the detector [units: MeV/c]
  std::array<float, 3> end_momentum = {detail::kNaNf, detail::kNaNf, detail::kNaNf};
  /// Unaltered index of the interaction in the original MC particle list
  std::int64_t orig_interaction_id = -1;
  /// Unaltered index of the particle parent in the original MC particle list
  std::int64_t orig_parent_id = -1;
  /// Unaltered index of the particle group in the original MC particle list
  std::int64_t orig_group_id = -1;
  /// Reconstructed length of the particle (only assigned to track objects)
  double reco_length = detail::kNaNd;
  /// Unaltered list of the particle children in the original MC particle list
  std::vector<std::int32_t> orig_children_id;
  /// (P) Number of truth child particle of each shape
  std::vector<std::int32_t> children_counts;
  /// (3) Particle direction estimate w.r.t. the start point
  std::array<float, 3> reco_start_dir = {detail::kNaNf, detail::kNaNf, detail::kNaNf};
  /// (3) Particle direction estimate w.r.t. the end point (only assigned to track objects)
  std::array<float, 3> reco_end_dir = {detail::kNaNf, detail::kNaNf, detail::kNaNf};
  /// (3) Particle direction w.r.t. the start point [derived]
  std::array<float, 3> start_dir = {detail::kNaNf, detail::kNaNf, detail::kNaNf};
  /// (3) Particle direction w.r.t. the end point (only assigned to track objects) [derived]
  std::array<float, 3> end_dir = {detail::kNaNf, detail::kNaNf, detail::kNaNf};
  /// Kinetic energy of the particle in MeV [units: MeV] [derived]
  double ke = detail::kNaNd;
  /// Best-guess reconstructed KE of the particle [units: MeV] [derived]
  double reco_ke = detail::kNaNd;
  /// Best-guess reconstructed momentum of the particle [units: MeV/c] [derived]
  std::array<float, 3> reco_momentum = {detail::kNaNf, detail::kNaNf, detail::kNaNf};
  /// Momentum magnitude of the particle at the production point [units: MeV/c] [derived]
  double p = detail::kNaNd;
  /// Momentum magnitude of the particle where it stops or exits the detector [units: MeV/c]
  /// [derived]
  double end_p = detail::kNaNd;
  /// Rest mass of the particle in MeV/c^2 [units: MeV/c^2] [derived]
  double mass = detail::kNaNd;
  /// Number of fragments that make up this particle [derived]
  std::int64_t num_fragments = 0;
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
