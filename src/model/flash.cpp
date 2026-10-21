// GENERATED from SPINE 1.4.0 by tools/gen/generate.py. Do not edit.
#include <spine/model/flash.hpp>

#include <cstddef>
#include <iterator>

#include "model/field_check.hpp"

namespace spine {

namespace {

constexpr FieldDescriptor kFields[] = {
    {"units", ScalarType::String, 0, true, 0, "", nullptr, "", "",
     "Units in which the position coordinates are expressed",
     &detail::assign<&Flash::units>},
    {"id", ScalarType::Int64, 1, false, kIndex, "", nullptr, "flash", "same",
     "Index of the flash in the list",
     &detail::assign<&Flash::id>},
    {"volume_id", ScalarType::Int64, 1, false, 0, "", nullptr, "optical_volume", "external",
     "Index of the optical volume in which the flash was recorded",
     &detail::assign<&Flash::volume_id>},
    {"frame", ScalarType::Int64, 1, false, kCategorical, "", nullptr, "", "",
     "Frame number",
     &detail::assign<&Flash::frame>},
    {"on_beam_time", ScalarType::Int64, 1, false, kCategorical, "", nullptr, "", "",
     "Whether the flash time is consistent with the beam window (inheriting typing from LArCV, so "
     "int instead of bool)",
     &detail::assign<&Flash::on_beam_time>},
    {"in_beam_frame", ScalarType::Bool, 1, false, 0, "", nullptr, "", "",
     "Whether the flash is in the beam frame",
     &detail::assign<&Flash::in_beam_frame>},
    {"time", ScalarType::Float64, 1, false, 0, "us", nullptr, "", "",
     "Time with respect to the trigger in microseconds",
     &detail::assign<&Flash::time>},
    {"time_width", ScalarType::Float64, 1, false, 0, "us", nullptr, "", "",
     "Width of the flash in microseconds",
     &detail::assign<&Flash::time_width>},
    {"time_abs", ScalarType::Float64, 1, false, 0, "", nullptr, "", "",
     "Time in units of PMT readout clock",
     &detail::assign<&Flash::time_abs>},
    {"total_pe", ScalarType::Float64, 1, false, 0, "", nullptr, "", "",
     "Total number of PE in the flash",
     &detail::assign<&Flash::total_pe>},
    {"fast_to_total", ScalarType::Float64, 1, false, 0, "", nullptr, "", "",
     "Fraction of the total PE contributed by the fast component",
     &detail::assign<&Flash::fast_to_total>},
    {"pe_per_ch", ScalarType::Float32, 0, true, 0, "", nullptr, "", "",
     "(N) Fixed-length array of the number of PE per optical detector",
     &detail::assign<&Flash::pe_per_ch>},
    {"center", ScalarType::Float32, 3, false, kPosition, "instance", nullptr, "", "",
     "Barycenter of the flash in detector coordinates",
     &detail::assign<&Flash::center>},
    {"width", ScalarType::Float32, 3, false, kVector, "instance", nullptr, "", "",
     "Spatial width of the flash in detector coordinates",
     &detail::assign<&Flash::width>},
};

constexpr ClassDescriptor kDescriptor{"Flash", kFields, std::size(kFields), "1.4.0"};

// The table has one row per Field enumerator, in order, and every member's C++ type
// agrees with its row (element type, count, variable flag, enum or not).
static_assert(std::size(kFields) == Flash::kNumFields);
static_assert(static_cast<std::size_t>(Flash::Field::width) + 1 == Flash::kNumFields);
SPINE_MODEL_CHECK_FIELD(Flash, units);
SPINE_MODEL_CHECK_FIELD(Flash, id);
SPINE_MODEL_CHECK_FIELD(Flash, volume_id);
SPINE_MODEL_CHECK_FIELD(Flash, frame);
SPINE_MODEL_CHECK_FIELD(Flash, on_beam_time);
SPINE_MODEL_CHECK_FIELD(Flash, in_beam_frame);
SPINE_MODEL_CHECK_FIELD(Flash, time);
SPINE_MODEL_CHECK_FIELD(Flash, time_width);
SPINE_MODEL_CHECK_FIELD(Flash, time_abs);
SPINE_MODEL_CHECK_FIELD(Flash, total_pe);
SPINE_MODEL_CHECK_FIELD(Flash, fast_to_total);
SPINE_MODEL_CHECK_FIELD(Flash, pe_per_ch);
SPINE_MODEL_CHECK_FIELD(Flash, center);
SPINE_MODEL_CHECK_FIELD(Flash, width);

}  // namespace

const ClassDescriptor& Flash::descriptor() { return kDescriptor; }

}  // namespace spine
