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

/// Trigger information. SPINE `spine.data.larcv.trigger.Trigger`: 6 fields, in the order SPINE's
/// HDF5 writer stores them. A field the source file lacks keeps its default and has() is false.
struct Trigger : Object<6> {
  enum class Field : std::uint16_t {
    id,
    type,
    time_s,
    time_ns,
    beam_time_s,
    beam_time_ns,
  };

  /// Trigger ID
  std::int64_t id = -1;
  /// DAQ-specific trigger type
  std::int64_t type = -1;
  /// Integer seconds component of the UNIX trigger time [units: s]
  std::int64_t time_s = -1;
  /// Integer nanoseconds component of the UNIX trigger time [units: ns]
  std::int64_t time_ns = -1;
  /// Integer seconds component of the UNIX beam pulse time [units: s]
  std::int64_t beam_time_s = -1;
  /// Integer seconds component of the UNIX beam pulse time [units: ns]
  std::int64_t beam_time_ns = -1;

  bool has(Field f) const { return has_field(static_cast<std::size_t>(f)); }
  static const ClassDescriptor& descriptor();
};

}  // namespace spine
