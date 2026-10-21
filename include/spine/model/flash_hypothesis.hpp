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

/// Predicted optical response for one interaction in one optical volume. SPINE
/// `spine.data.out.optical.FlashHypothesis`: 8 fields, in the order SPINE's HDF5 writer stores
/// them. A field the source file lacks keeps its default and has() is false.
struct FlashHypothesis : Object<8> {
  enum class Field : std::uint16_t {
    id,
    interaction_id,
    volume_id,
    score,
    is_truth,
    flash_ids,
    pe_per_ch,
    total_pe,
  };

  /// Index of the hypothesis in the event-level hypothesis collection.
  std::int64_t id = -1;
  /// ID of the interaction which produced the predicted light.
  std::int64_t interaction_id = -1;
  /// Optical volume in which the response is predicted.
  std::int64_t volume_id = -1;
  /// Match score associated with this prediction, if applicable.
  double score = detail::kNaNd;
  /// Whether the source interaction is a truth interaction.
  bool is_truth = false;
  /// IDs of observed flashes associated with this prediction. This may contain multiple IDs when
  /// measured flashes were merged before matching.
  std::vector<std::int32_t> flash_ids;
  /// Predicted number of photoelectrons per optical channel.
  std::vector<float> pe_per_ch;
  /// Total predicted number of photoelectrons. [derived]
  double total_pe = 0.0;

  bool has(Field f) const { return has_field(static_cast<std::size_t>(f)); }
  static const ClassDescriptor& descriptor();
};

}  // namespace spine
