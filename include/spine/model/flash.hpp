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
#include <spine/object.hpp>

namespace spine {

/// Optical flash information. SPINE `spine.data.larcv.optical.Flash`: 14 fields, in the order
/// SPINE's HDF5 writer stores them. A field the source file lacks keeps its default and has() is
/// false. Fields with units 'instance' are in the unit named by the member `units`.
struct Flash : Object<14> {
  enum class Field : std::uint16_t {
    units,
    id,
    volume_id,
    frame,
    on_beam_time,
    in_beam_frame,
    time,
    time_width,
    time_abs,
    total_pe,
    fast_to_total,
    pe_per_ch,
    center,
    width,
  };

  /// Units in which the position coordinates are expressed
  std::string units = "cm";
  /// Index of the flash in the list
  std::int64_t id = -1;
  /// Index of the optical volume in which the flash was recorded
  std::int64_t volume_id = -1;
  /// Frame number
  std::int64_t frame = -1;
  /// Whether the flash time is consistent with the beam window (inheriting typing from LArCV, so
  /// int instead of bool)
  std::int64_t on_beam_time = -1;
  /// Whether the flash is in the beam frame
  bool in_beam_frame = false;
  /// Time with respect to the trigger in microseconds [units: us]
  double time = detail::kNaNd;
  /// Width of the flash in microseconds [units: us]
  double time_width = detail::kNaNd;
  /// Time in units of PMT readout clock
  double time_abs = detail::kNaNd;
  /// Total number of PE in the flash
  double total_pe = detail::kNaNd;
  /// Fraction of the total PE contributed by the fast component
  double fast_to_total = detail::kNaNd;
  /// (N) Fixed-length array of the number of PE per optical detector
  std::vector<float> pe_per_ch;
  /// Barycenter of the flash in detector coordinates [units: instance]
  std::array<float, 3> center = {detail::kNaNf, detail::kNaNf, detail::kNaNf};
  /// Spatial width of the flash in detector coordinates [units: instance]
  std::array<float, 3> width = {detail::kNaNf, detail::kNaNf, detail::kNaNf};

  bool has(Field f) const { return has_field(static_cast<std::size_t>(f)); }
  static const ClassDescriptor& descriptor();
};

}  // namespace spine
