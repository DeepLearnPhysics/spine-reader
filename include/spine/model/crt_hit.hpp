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

/// CRT hit information. SPINE `spine.data.larcv.crt.CRTHit`: 14 fields, in the order SPINE's HDF5
/// writer stores them. A field the source file lacks keeps its default and has() is false. Fields
/// with units 'instance' are in the unit named by the member `units`.
struct CRTHit : Object<14> {
  enum class Field : std::uint16_t {
    units,
    id,
    plane,
    ts0_s,
    ts0_ns,
    ts0_s_corr,
    ts0_ns_corr,
    ts1_ns,
    total_pe,
    tagger,
    feb_id,
    center,
    width,
    time,
  };

  /// Units in which the position attributes are expressed
  std::string units = "cm";
  /// Index of the CRT hit in the list
  std::int64_t id = -1;
  /// Index of the CRT tagger that registered the hit
  std::int64_t plane = -1;
  /// Absolute time from White Rabbit (seconds component) [units: s]
  std::int64_t ts0_s = -1;
  /// Absolute time from White Rabbit (nanoseconds component) [units: ns]
  double ts0_ns = detail::kNaNd;
  /// Unclear in the documentation, placeholder at this point [units: s]
  double ts0_s_corr = detail::kNaNd;
  /// Unclear in the documentation, placeholder at this point [units: ns]
  double ts0_ns_corr = detail::kNaNd;
  /// Time relative to the trigger (nanoseconds component) [units: ns]
  double ts1_ns = detail::kNaNd;
  /// Total number of PE in the CRT hit
  double total_pe = detail::kNaNd;
  /// Name of the CRT tagger that registered the hit
  std::string tagger;
  /// Address of the FEB board stored as a list of bytes (uint8)
  std::vector<std::uint8_t> feb_id;
  /// Barycenter of the CRT hit in detector coordinates [units: instance]
  std::array<float, 3> center = {detail::kNaNf, detail::kNaNf, detail::kNaNf};
  /// Uncertainty on the barycenter of the CRT hit in detector coordinates [units: instance]
  std::array<float, 3> width = {detail::kNaNf, detail::kNaNf, detail::kNaNf};
  /// Alias for ts1_ns, but scaled to microseconds [units: us] [derived]
  double time = detail::kNaNd;

  bool has(Field f) const { return has_field(static_cast<std::size_t>(f)); }
  static const ClassDescriptor& descriptor();
};

}  // namespace spine
