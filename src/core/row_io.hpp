#pragma once

// Building blocks for reading rows of a dataset into ColumnBuffers, shared by the V1 and V2
// readers: a packed memory compound, row selections, and the copies out of a packed buffer.

#include <cstddef>
#include <string>
#include <vector>

#include <hdf5.h>

#include <spine/detail/column_buffer.hpp>

#include "core/source.hpp"
#include "h5/handle.hpp"

namespace spine::core {

/// Memory compound type for reading a subset of the members of a compound dataset: members
/// are packed one after the other, so a row is `row_size()` bytes and member `i` starts at the
/// offset add() returned. Members are matched to the file's by name.
class RowLayout {
 public:
  /// Adds the file member `name` read as `type`; returns its byte offset in a row. Throws
  /// spine::Error if `name` was already added.
  std::size_t add(const std::string& name, h5::Datatype type);
  std::size_t row_size() const { return row_size_; }
  bool empty() const { return members_.empty(); }
  /// The H5T_COMPOUND type. Call after the last add().
  h5::Datatype build() const;

 private:
  struct Member {
    std::string name;
    h5::Datatype type;
    std::size_t offset;
  };
  std::vector<Member> members_;
  std::size_t row_size_ = 0;
};

/// Rows [begin, end) of a 1-D dataset as the file selection, plus the memory space of the
/// same number of elements. Needs a non-empty range.
struct RowSelection {
  RowSelection(hid_t dataset, RowRange rows);
  h5::Dataspace file;
  h5::Dataspace memory;
};

/// Reads `count` elements of the 1-D dataset starting at `start`, converted to `memory_type`.
void read_elements(hid_t dataset, std::uint64_t start, std::uint64_t count, hid_t memory_type,
                   void* destination);

/// Resets `buffer` to answer `column` read as `type` over `rows` rows, with zeroed storage
/// for fixed numeric columns and `rows + 1` zero offsets for variable ones.
void prepare(detail::ColumnBuffer& buffer, const ColumnInfo& column, ScalarType type,
             std::size_t rows);

/// Copies a fixed numeric column out of `rows` packed rows of `row_size` bytes in `packed`,
/// where the column starts at byte `offset` of each row. `buffer` comes from prepare().
void copy_fixed(const unsigned char* packed, std::size_t row_size, std::size_t offset,
                detail::ColumnBuffer& buffer);
/// The same for a VLEN column, stored as hvl_t.
void copy_vlen(const unsigned char* packed, std::size_t row_size, std::size_t offset,
               detail::ColumnBuffer& buffer);
/// The same for a variable-length string column, stored as char*.
void copy_strings(const unsigned char* packed, std::size_t row_size, std::size_t offset,
                  detail::ColumnBuffer& buffer);

/// Makes every byte of a Bool buffer 0 or 1.
void normalize_bool(detail::ColumnBuffer& buffer);

/// The column of a plain dataset whose rows are its first dimension: element type from the
/// dataset, `count` the product of the other dimensions, named `name`. Throws spine::Error if
/// the element type is not a plain number.
ColumnInfo describe_array_dataset(const std::string& name, hid_t dataset);

/// Reads rows [begin, end) of an array dataset (any rank) as `type`, full width, into `buffer`.
void read_array_rows(hid_t dataset, RowRange rows, const ColumnInfo& column, ScalarType type,
                     detail::ColumnBuffer& buffer);

/// Index in `info.columns` of `column`, which must point into it.
std::size_t column_index(const ProductInfo& info, const ColumnInfo* column);

}  // namespace spine::core
