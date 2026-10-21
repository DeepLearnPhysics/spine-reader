// GENERATED from SPINE 1.4.0 by tools/gen/generate.py. Do not edit.
#include <spine/model/image_meta3d.hpp>

#include <cstddef>
#include <iterator>

#include "model/field_check.hpp"

namespace spine {

namespace {

constexpr FieldDescriptor kFields[] = {
    {"lower", ScalarType::Float32, 3, false, kVector, "cm", nullptr, "", "",
     "(3) Array of image lower bounds in detector coordinates (cm)",
     &detail::assign<&ImageMeta3D::lower>},
    {"upper", ScalarType::Float32, 3, false, kVector, "cm", nullptr, "", "",
     "(3) Array of image upper bounds in detector coordinates (cm)",
     &detail::assign<&ImageMeta3D::upper>},
    {"size", ScalarType::Float32, 3, false, kVector, "cm", nullptr, "", "",
     "(3) Array of pixel size in each dimension (cm)",
     &detail::assign<&ImageMeta3D::size>},
    {"count", ScalarType::Int64, 3, false, kVector, "", nullptr, "", "",
     "(3) Array of pixel count in each dimension",
     &detail::assign<&ImageMeta3D::count>},
};

constexpr ClassDescriptor kDescriptor{"ImageMeta3D", kFields, std::size(kFields), "1.4.0"};

// The table has one row per Field enumerator, in order, and every member's C++ type
// agrees with its row (element type, count, variable flag, enum or not).
static_assert(std::size(kFields) == ImageMeta3D::kNumFields);
static_assert(static_cast<std::size_t>(ImageMeta3D::Field::count) + 1 == ImageMeta3D::kNumFields);
SPINE_MODEL_CHECK_FIELD(ImageMeta3D, lower);
SPINE_MODEL_CHECK_FIELD(ImageMeta3D, upper);
SPINE_MODEL_CHECK_FIELD(ImageMeta3D, size);
SPINE_MODEL_CHECK_FIELD(ImageMeta3D, count);

}  // namespace

const ClassDescriptor& ImageMeta3D::descriptor() { return kDescriptor; }

}  // namespace spine
