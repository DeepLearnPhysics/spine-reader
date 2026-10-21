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

/// Run information related to a specific event. SPINE `spine.data.larcv.run_info.RunInfo`: 3
/// fields, in the order SPINE's HDF5 writer stores them. A field the source file lacks keeps its
/// default and has() is false.
struct RunInfo : Object<3> {
  enum class Field : std::uint16_t {
    run,
    subrun,
    event,
  };

  /// Run ID
  std::int64_t run = -1;
  /// Sub-run ID
  std::int64_t subrun = -1;
  /// Event ID
  std::int64_t event = -1;

  bool has(Field f) const { return has_field(static_cast<std::size_t>(f)); }
  static const ClassDescriptor& descriptor();
};

}  // namespace spine
