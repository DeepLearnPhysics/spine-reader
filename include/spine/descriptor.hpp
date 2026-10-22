#pragma once

// Static description of the SPINE object model. One ClassDescriptor per generated class,
// built by tools/gen from SPINE's own FieldMetadata at a pinned release.

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace spine {

namespace detail {
struct ColumnBuffer;
}

/// In-memory element type of a field (or of one element of an array field).
enum class ScalarType : std::uint8_t {
  Bool,
  Int8,
  Int16,
  Int32,
  Int64,
  UInt8,
  UInt16,
  UInt32,
  UInt64,
  Float32,
  Float64,
  String,  // UTF-8; one std::string per object
};

/// Size in bytes of one element in a ColumnBuffer. Bool is stored as one byte (0 or 1).
/// String has no fixed size and returns 0.
constexpr std::size_t scalar_size(ScalarType t) {
  switch (t) {
    case ScalarType::Bool:
    case ScalarType::Int8:
    case ScalarType::UInt8:
      return 1;
    case ScalarType::Int16:
    case ScalarType::UInt16:
      return 2;
    case ScalarType::Int32:
    case ScalarType::UInt32:
    case ScalarType::Float32:
      return 4;
    case ScalarType::Int64:
    case ScalarType::UInt64:
    case ScalarType::Float64:
      return 8;
    case ScalarType::String:
      return 0;
  }
  return 0;
}

std::string_view to_string(ScalarType t);

struct EnumMember {
  std::string_view name;
  std::int64_t value;
};

/// Enumeration as defined by SPINE (e.g. Shape, ParticlePID). Values are what the file
/// stores; names are the SPINE spelling at the model's release. Files map by value. A file
/// whose labels cross the model's (Track=0, Shower=1 against TRACK=1, SHOWER=0) does not bind.
struct EnumDescriptor {
  std::string_view name;
  const EnumMember* members;
  std::size_t size;

  /// Name for a value, or an empty view when the value is unknown to the model.
  std::string_view name_of(std::int64_t value) const;
};

/// FieldMetadata flags carried over from SPINE.
enum FieldFlag : std::uint32_t {
  kDerived = 1u << 0,           // SPINE stored_property: computed by SPINE, stored for consumers
  kPosition = 1u << 1,          // spatial coordinate (units follow the object's `units`)
  kVector = 1u << 2,            // non-unitary vector quantity
  kIndex = 1u << 3,             // an identifier or index
  kCategorical = 1u << 4,       // discrete categories rather than a quantity
  kPdg = 1u << 5,               // PDG code
  kPdgMinusOneValid = 1u << 6,  // PDG -1 is a real anti-down quark, not "missing"
  kPointwise = 1u << 7,         // one entry per point of the object's point cloud
  kLiteSkip = 1u << 8,          // dropped by SPINE's lite writer mode
};

struct FieldDescriptor {
  std::string_view name;  // SPINE attribute name, identical to the HDF5 column name
  ScalarType type;        // element type of the C++ member
  std::uint32_t count;    // fixed-size arrays: length; scalars: 1; variable-length: 0
  bool variable;          // variable-length array or string (V1 VLEN, V2 pool)
  std::uint32_t flags;    // FieldFlag bits
  std::string_view units;            // "MeV", "ns", ...; "instance" = the object's `units`; "" = none
  const EnumDescriptor* enumeration;  // non-null for enum-typed fields
  std::string_view reference;        // collection an id refers to, e.g. "particle"; "" = none
  std::string_view reference_space;  // "same", "opposite", "external" or ""
  std::string_view doc;              // one-line description from SPINE
  // Copies row `row` of `column` into this field of `object` (a pointer to the generated class).
  void (*assign)(void* object, const detail::ColumnBuffer& column, std::size_t row);

  bool derived() const { return (flags & kDerived) != 0; }
};

struct ClassDescriptor {
  std::string_view name;  // SPINE class name, e.g. "RecoParticle"; matches the file's class_name
  const FieldDescriptor* fields;  // ordered as the class's Field enum
  std::size_t num_fields;
  std::string_view schema_version;  // SPINE release the model was generated from

  /// Field by SPINE name, or nullptr.
  const FieldDescriptor* find(std::string_view field_name) const;
  /// Position of `field` in `fields` (== the value of the class's Field enum).
  std::size_t index_of(const FieldDescriptor& field) const { return &field - fields; }
};

}  // namespace spine
