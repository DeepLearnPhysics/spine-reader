// GENERATED from SPINE 1.4.0 by tools/gen/generate.py. Do not edit.
#include <spine/model/run_info.hpp>

#include <cstddef>
#include <iterator>

#include "model/field_check.hpp"

namespace spine {

namespace {

constexpr FieldDescriptor kFields[] = {
    {"run", ScalarType::Int64, 1, false, 0, "", nullptr, "", "",
     "Run ID",
     &detail::assign<&RunInfo::run>},
    {"subrun", ScalarType::Int64, 1, false, 0, "", nullptr, "", "",
     "Sub-run ID",
     &detail::assign<&RunInfo::subrun>},
    {"event", ScalarType::Int64, 1, false, 0, "", nullptr, "", "",
     "Event ID",
     &detail::assign<&RunInfo::event>},
};

constexpr ClassDescriptor kDescriptor{"RunInfo", kFields, std::size(kFields), "1.4.0"};

// The table has one row per Field enumerator, in order, and every member's C++ type
// agrees with its row (element type, count, variable flag, enum or not).
static_assert(std::size(kFields) == RunInfo::kNumFields);
static_assert(static_cast<std::size_t>(RunInfo::Field::event) + 1 == RunInfo::kNumFields);
SPINE_MODEL_CHECK_FIELD(RunInfo, run);
SPINE_MODEL_CHECK_FIELD(RunInfo, subrun);
SPINE_MODEL_CHECK_FIELD(RunInfo, event);

}  // namespace

const ClassDescriptor& RunInfo::descriptor() { return kDescriptor; }

}  // namespace spine
