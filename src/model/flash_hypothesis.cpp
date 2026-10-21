// GENERATED from SPINE 1.4.0 by tools/gen/generate.py. Do not edit.
#include <spine/model/flash_hypothesis.hpp>

#include <cstddef>
#include <iterator>

#include "model/field_check.hpp"

namespace spine {

namespace {

constexpr FieldDescriptor kFields[] = {
    {"id", ScalarType::Int64, 1, false, kIndex, "", nullptr, "flash_hypothesis", "same",
     "Index of the hypothesis in the event-level hypothesis collection.",
     &detail::assign<&FlashHypothesis::id>},
    {"interaction_id", ScalarType::Int64, 1, false, 0, "", nullptr, "interaction", "same",
     "ID of the interaction which produced the predicted light.",
     &detail::assign<&FlashHypothesis::interaction_id>},
    {"volume_id", ScalarType::Int64, 1, false, 0, "", nullptr, "optical_volume", "external",
     "Optical volume in which the response is predicted.",
     &detail::assign<&FlashHypothesis::volume_id>},
    {"score", ScalarType::Float64, 1, false, 0, "", nullptr, "", "",
     "Match score associated with this prediction, if applicable.",
     &detail::assign<&FlashHypothesis::score>},
    {"is_truth", ScalarType::Bool, 1, false, 0, "", nullptr, "", "",
     "Whether the source interaction is a truth interaction.",
     &detail::assign<&FlashHypothesis::is_truth>},
    {"flash_ids", ScalarType::Int32, 0, true, 0, "", nullptr, "flash", "external",
     "IDs of observed flashes associated with this prediction. This may contain multiple IDs when "
     "measured flashes were merged before matching.",
     &detail::assign<&FlashHypothesis::flash_ids>},
    {"pe_per_ch", ScalarType::Float32, 0, true, 0, "", nullptr, "", "",
     "Predicted number of photoelectrons per optical channel.",
     &detail::assign<&FlashHypothesis::pe_per_ch>},
    {"total_pe", ScalarType::Float64, 1, false, kDerived, "", nullptr, "", "",
     "Total predicted number of photoelectrons.",
     &detail::assign<&FlashHypothesis::total_pe>},
};

constexpr ClassDescriptor kDescriptor{"FlashHypothesis", kFields, std::size(kFields), "1.4.0"};

// The table has one row per Field enumerator, in order, and every member's C++ type
// agrees with its row (element type, count, variable flag, enum or not).
static_assert(std::size(kFields) == FlashHypothesis::kNumFields);
static_assert(static_cast<std::size_t>(FlashHypothesis::Field::total_pe)
              + 1 == FlashHypothesis::kNumFields);
SPINE_MODEL_CHECK_FIELD(FlashHypothesis, id);
SPINE_MODEL_CHECK_FIELD(FlashHypothesis, interaction_id);
SPINE_MODEL_CHECK_FIELD(FlashHypothesis, volume_id);
SPINE_MODEL_CHECK_FIELD(FlashHypothesis, score);
SPINE_MODEL_CHECK_FIELD(FlashHypothesis, is_truth);
SPINE_MODEL_CHECK_FIELD(FlashHypothesis, flash_ids);
SPINE_MODEL_CHECK_FIELD(FlashHypothesis, pe_per_ch);
SPINE_MODEL_CHECK_FIELD(FlashHypothesis, total_pe);

}  // namespace

const ClassDescriptor& FlashHypothesis::descriptor() { return kDescriptor; }

}  // namespace spine
