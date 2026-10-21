// GENERATED from SPINE 1.4.0 by tools/gen/generate.py. Do not edit.
#include <spine/model/trigger.hpp>

#include <cstddef>
#include <iterator>

#include "model/field_check.hpp"

namespace spine {

namespace {

constexpr FieldDescriptor kFields[] = {
    {"id", ScalarType::Int64, 1, false, 0, "", nullptr, "trigger", "same",
     "Trigger ID",
     &detail::assign<&Trigger::id>},
    {"type", ScalarType::Int64, 1, false, kCategorical, "", nullptr, "", "",
     "DAQ-specific trigger type",
     &detail::assign<&Trigger::type>},
    {"time_s", ScalarType::Int64, 1, false, 0, "s", nullptr, "", "",
     "Integer seconds component of the UNIX trigger time",
     &detail::assign<&Trigger::time_s>},
    {"time_ns", ScalarType::Int64, 1, false, 0, "ns", nullptr, "", "",
     "Integer nanoseconds component of the UNIX trigger time",
     &detail::assign<&Trigger::time_ns>},
    {"beam_time_s", ScalarType::Int64, 1, false, 0, "s", nullptr, "", "",
     "Integer seconds component of the UNIX beam pulse time",
     &detail::assign<&Trigger::beam_time_s>},
    {"beam_time_ns", ScalarType::Int64, 1, false, 0, "ns", nullptr, "", "",
     "Integer seconds component of the UNIX beam pulse time",
     &detail::assign<&Trigger::beam_time_ns>},
};

constexpr ClassDescriptor kDescriptor{"Trigger", kFields, std::size(kFields), "1.4.0"};

// The table has one row per Field enumerator, in order, and every member's C++ type
// agrees with its row (element type, count, variable flag, enum or not).
static_assert(std::size(kFields) == Trigger::kNumFields);
static_assert(static_cast<std::size_t>(Trigger::Field::beam_time_ns) + 1 == Trigger::kNumFields);
SPINE_MODEL_CHECK_FIELD(Trigger, id);
SPINE_MODEL_CHECK_FIELD(Trigger, type);
SPINE_MODEL_CHECK_FIELD(Trigger, time_s);
SPINE_MODEL_CHECK_FIELD(Trigger, time_ns);
SPINE_MODEL_CHECK_FIELD(Trigger, beam_time_s);
SPINE_MODEL_CHECK_FIELD(Trigger, beam_time_ns);

}  // namespace

const ClassDescriptor& Trigger::descriptor() { return kDescriptor; }

}  // namespace spine
