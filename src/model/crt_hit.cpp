// GENERATED from SPINE 1.4.0 by tools/gen/generate.py. Do not edit.
#include <spine/model/crt_hit.hpp>

#include <cstddef>
#include <iterator>

#include "model/field_check.hpp"

namespace spine {

namespace {

constexpr FieldDescriptor kFields[] = {
    {"units", ScalarType::String, 0, true, 0, "", nullptr, "", "",
     "Units in which the position attributes are expressed",
     &detail::assign<&CRTHit::units>},
    {"id", ScalarType::Int64, 1, false, kIndex, "", nullptr, "crthit", "same",
     "Index of the CRT hit in the list",
     &detail::assign<&CRTHit::id>},
    {"plane", ScalarType::Int64, 1, false, 0, "", nullptr, "crt_plane", "external",
     "Index of the CRT tagger that registered the hit",
     &detail::assign<&CRTHit::plane>},
    {"ts0_s", ScalarType::Int64, 1, false, 0, "s", nullptr, "", "",
     "Absolute time from White Rabbit (seconds component)",
     &detail::assign<&CRTHit::ts0_s>},
    {"ts0_ns", ScalarType::Float64, 1, false, 0, "ns", nullptr, "", "",
     "Absolute time from White Rabbit (nanoseconds component)",
     &detail::assign<&CRTHit::ts0_ns>},
    {"ts0_s_corr", ScalarType::Float64, 1, false, 0, "s", nullptr, "", "",
     "Unclear in the documentation, placeholder at this point",
     &detail::assign<&CRTHit::ts0_s_corr>},
    {"ts0_ns_corr", ScalarType::Float64, 1, false, 0, "ns", nullptr, "", "",
     "Unclear in the documentation, placeholder at this point",
     &detail::assign<&CRTHit::ts0_ns_corr>},
    {"ts1_ns", ScalarType::Float64, 1, false, 0, "ns", nullptr, "", "",
     "Time relative to the trigger (nanoseconds component)",
     &detail::assign<&CRTHit::ts1_ns>},
    {"total_pe", ScalarType::Float64, 1, false, 0, "", nullptr, "", "",
     "Total number of PE in the CRT hit",
     &detail::assign<&CRTHit::total_pe>},
    {"tagger", ScalarType::String, 0, true, 0, "", nullptr, "", "",
     "Name of the CRT tagger that registered the hit",
     &detail::assign<&CRTHit::tagger>},
    {"feb_id", ScalarType::UInt8, 0, true, 0, "", nullptr, "crt_feb", "external",
     "Address of the FEB board stored as a list of bytes (uint8)",
     &detail::assign<&CRTHit::feb_id>},
    {"center", ScalarType::Float32, 3, false, kPosition, "instance", nullptr, "", "",
     "Barycenter of the CRT hit in detector coordinates",
     &detail::assign<&CRTHit::center>},
    {"width", ScalarType::Float32, 3, false, kVector, "instance", nullptr, "", "",
     "Uncertainty on the barycenter of the CRT hit in detector coordinates",
     &detail::assign<&CRTHit::width>},
    {"time", ScalarType::Float64, 1, false, kDerived, "us", nullptr, "", "",
     "Alias for ts1_ns, but scaled to microseconds",
     &detail::assign<&CRTHit::time>},
};

constexpr ClassDescriptor kDescriptor{"CRTHit", kFields, std::size(kFields), "1.4.0"};

// The table has one row per Field enumerator, in order, and every member's C++ type
// agrees with its row (element type, count, variable flag, enum or not).
static_assert(std::size(kFields) == CRTHit::kNumFields);
static_assert(static_cast<std::size_t>(CRTHit::Field::time) + 1 == CRTHit::kNumFields);
SPINE_MODEL_CHECK_FIELD(CRTHit, units);
SPINE_MODEL_CHECK_FIELD(CRTHit, id);
SPINE_MODEL_CHECK_FIELD(CRTHit, plane);
SPINE_MODEL_CHECK_FIELD(CRTHit, ts0_s);
SPINE_MODEL_CHECK_FIELD(CRTHit, ts0_ns);
SPINE_MODEL_CHECK_FIELD(CRTHit, ts0_s_corr);
SPINE_MODEL_CHECK_FIELD(CRTHit, ts0_ns_corr);
SPINE_MODEL_CHECK_FIELD(CRTHit, ts1_ns);
SPINE_MODEL_CHECK_FIELD(CRTHit, total_pe);
SPINE_MODEL_CHECK_FIELD(CRTHit, tagger);
SPINE_MODEL_CHECK_FIELD(CRTHit, feb_id);
SPINE_MODEL_CHECK_FIELD(CRTHit, center);
SPINE_MODEL_CHECK_FIELD(CRTHit, width);
SPINE_MODEL_CHECK_FIELD(CRTHit, time);

}  // namespace

const ClassDescriptor& CRTHit::descriptor() { return kDescriptor; }

}  // namespace spine
