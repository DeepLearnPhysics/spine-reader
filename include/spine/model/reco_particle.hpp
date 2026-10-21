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

/// Reconstructed particle information. SPINE `spine.data.out.particle.RecoParticle`: 55 fields, in
/// the order SPINE's HDF5 writer stores them. A field the source file lacks keeps its default and
/// has() is false. Fields with units 'instance' are in the unit named by the member `units`.
struct RecoParticle : Object<55> {
  enum class Field : std::uint16_t {
    is_truth,
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
    start_dedx,
    end_dedx,
    vertex_distance,
    start_straightness,
    directional_spread,
    axial_spread,
    start_dir,
    end_dir,
    pid_scores,
    primary_scores,
    ppn_ids,
    pdg_code,
    mass,
    ke,
    momentum,
    p,
    num_fragments,
    size,
    depositions_sum,
    depositions_q_sum,
    best_match_id,
    best_match_overlap,
    module_ids,
  };

  /// Whether this is a truth object (always False for reconstructed)
  bool is_truth = false;
  /// Units in which coordinates are expressed
  std::string units = "cm";
  /// Unique index of the object within the object list
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
  /// Index of the interaction this particle belongs to
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
  /// Semantic type (shower (0), track (1), Michel (2), delta (3), low energy scatter (4)) of this
  /// particle
  ParticleShape shape = ParticleShape::UNKNOWN;
  /// Particle species (Photon (0), Electron (1), Muon (2), Charged Pion (3), Proton (4), Kaon (5))
  /// of this particle
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
  /// dE/dx around a user-defined neighborhood of the start point in MeV/cm [units: MeV/cm]
  double start_dedx = detail::kNaNd;
  /// dE/dx around a user-defined neighborhood of the end point in MeV/cm [units: MeV/cm]
  double end_dedx = detail::kNaNd;
  /// Set-to-point distance between all particle points and the parent interaction vertex position
  /// in cm [units: instance]
  double vertex_distance = detail::kNaNd;
  /// Explained variance ratio of the beginning of the particle
  double start_straightness = detail::kNaNd;
  /// Estimate of the angular spread of the particle (cosine spread)
  double directional_spread = detail::kNaNd;
  /// Pearson correlation coefficient of the axial profile of the particle w.r.t. to the distance
  /// from its start point
  double axial_spread = detail::kNaNd;
  /// (3) Particle direction w.r.t. the start point
  std::array<float, 3> start_dir = {detail::kNaNf, detail::kNaNf, detail::kNaNf};
  /// (3) Particle direction w.r.t. the end point (only assigned to track objects)
  std::array<float, 3> end_dir = {detail::kNaNf, detail::kNaNf, detail::kNaNf};
  /// (P) Array of softmax scores associated with each particle class
  std::array<float, 6> pid_scores = detail::filled<float, 6>(detail::kNaNf);
  /// (2) Array of softmax scores associated with secondary and primary
  std::array<float, 2> primary_scores = {detail::kNaNf, detail::kNaNf};
  /// (M) List of indexes of PPN points associated with this particle
  std::vector<std::int32_t> ppn_ids;
  /// PDG code corresponding to the PID number [derived]
  std::int64_t pdg_code = 0;
  /// Rest mass of the particle in MeV/c^2 [units: MeV/c^2] [derived]
  double mass = detail::kNaNd;
  /// Best guess kinetic energy of the particle in MeV [units: MeV] [derived]
  double ke = detail::kNaNd;
  /// 3-momentum of the particle at the production point in MeV/c [units: MeV/c] [derived]
  std::array<float, 3> momentum = {detail::kNaNf, detail::kNaNf, detail::kNaNf};
  /// Momentum magnitude of the particle at the production point in MeV/c [units: MeV/c] [derived]
  double p = detail::kNaNd;
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

  bool has(Field f) const { return has_field(static_cast<std::size_t>(f)); }
  static const ClassDescriptor& descriptor();
};

}  // namespace spine
