#include "core/row_io.hpp"

#include <cstring>
#include <functional>
#include <optional>
#include <utility>

#include <spine/error.hpp>

#include "core/columns.hpp"
#include "h5/error.hpp"
#include "h5/util.hpp"

namespace spine::core {

std::size_t RowLayout::add(const std::string& name, h5::Datatype type) {
  for (const Member& member : members_) {
    if (member.name == name) throw Error("column '" + name + "' is requested twice");
  }
  const std::size_t offset = row_size_;
  row_size_ += H5Tget_size(type.get());
  members_.push_back({name, std::move(type), offset});
  return offset;
}

h5::Datatype RowLayout::build() const {
  h5::Datatype compound(h5::check(H5Tcreate(H5T_COMPOUND, row_size_), "H5Tcreate"));
  for (const Member& member : members_) {
    h5::check(H5Tinsert(compound.get(), member.name.c_str(), member.offset, member.type.get()),
              "H5Tinsert", member.name);
  }
  return compound;
}

RowSelection::RowSelection(hid_t dataset, RowRange rows) {
  file = h5::dataset_space(dataset);
  hsize_t start = rows.begin;
  hsize_t count = rows.size();
  h5::check(H5Sselect_hyperslab(file.get(), H5S_SELECT_SET, &start, nullptr, &count, nullptr),
            "H5Sselect_hyperslab");
  memory = h5::Dataspace(h5::check(H5Screate_simple(1, &count, nullptr), "H5Screate_simple"));
}

void read_elements(hid_t dataset, std::uint64_t start, std::uint64_t count, hid_t memory_type,
                   void* destination) {
  if (count == 0) return;
  RowSelection selection(dataset, {start, start + count});
  h5::check(H5Dread(dataset, memory_type, selection.memory.get(), selection.file.get(), H5P_DEFAULT,
                    destination),
            "H5Dread");
}

void prepare(detail::ColumnBuffer& buffer, const ColumnInfo& column, ScalarType type,
             std::size_t rows) {
  buffer.type = type;
  buffer.variable = column.variable;
  buffer.rows = rows;
  buffer.data.clear();
  buffer.offsets.clear();
  buffer.strings.clear();
  if (type == ScalarType::String) {
    buffer.count = 1;
    buffer.strings.reserve(rows);
  } else if (column.variable) {
    buffer.count = 0;
    buffer.offsets.assign(rows + 1, 0);
  } else {
    buffer.count = column.count;
    buffer.data.resize(rows * column.count * scalar_size(type));
  }
}

void copy_fixed(const unsigned char* packed, std::size_t row_size, std::size_t offset,
                detail::ColumnBuffer& buffer) {
  const std::size_t bytes = buffer.count * scalar_size(buffer.type);
  unsigned char* destination = buffer.data.data();
  for (std::size_t r = 0; r < buffer.rows; ++r) {
    std::memcpy(destination + r * bytes, packed + r * row_size + offset, bytes);
  }
}

void copy_vlen(const unsigned char* packed, std::size_t row_size, std::size_t offset,
               detail::ColumnBuffer& buffer) {
  const std::size_t element = scalar_size(buffer.type);
  std::vector<hvl_t> rows(buffer.rows);
  for (std::size_t r = 0; r < buffer.rows; ++r) {
    std::memcpy(&rows[r], packed + r * row_size + offset, sizeof(hvl_t));
    buffer.offsets[r + 1] = buffer.offsets[r] + rows[r].len;
  }
  buffer.data.resize(buffer.offsets[buffer.rows] * element);
  for (std::size_t r = 0; r < buffer.rows; ++r) {
    if (rows[r].len > 0) {
      std::memcpy(buffer.data.data() + buffer.offsets[r] * element, rows[r].p,
                  rows[r].len * element);
    }
  }
}

void copy_strings(const unsigned char* packed, std::size_t row_size, std::size_t offset,
                  detail::ColumnBuffer& buffer) {
  for (std::size_t r = 0; r < buffer.rows; ++r) {
    const char* text = nullptr;
    std::memcpy(&text, packed + r * row_size + offset, sizeof(text));
    buffer.strings.emplace_back(text != nullptr ? text : "");
  }
}

void normalize_bool(detail::ColumnBuffer& buffer) {
  for (unsigned char& byte : buffer.data) byte = byte != 0 ? 1 : 0;
}

ColumnInfo describe_array_dataset(const std::string& name, hid_t dataset) {
  h5::Datatype type = h5::dataset_type(dataset);
  std::optional<ColumnInfo> column = describe_type(name, type.get());
  if (!column || column->variable || column->count != 1 || column->type == ScalarType::String) {
    throw Error("dataset of '" + name + "' does not hold plain numbers");
  }
  h5::Dataspace space = h5::dataset_space(dataset);
  const std::vector<hsize_t> dims = h5::extent(space.get());
  if (dims.empty()) throw Error("dataset of '" + name + "' is a scalar, not an array of rows");
  for (std::size_t d = 1; d < dims.size(); ++d) column->count *= static_cast<std::size_t>(dims[d]);
  return *column;
}

void read_array_rows(hid_t dataset, RowRange rows, const ColumnInfo& column, ScalarType type,
                     detail::ColumnBuffer& buffer) {
  prepare(buffer, column, type, static_cast<std::size_t>(rows.size()));
  if (rows.size() == 0) return;
  h5::Dataspace file = h5::dataset_space(dataset);
  const std::vector<hsize_t> dims = h5::extent(file.get());
  std::vector<hsize_t> start(dims.size(), 0);
  std::vector<hsize_t> count = dims;
  start[0] = rows.begin;
  count[0] = rows.size();
  h5::check(H5Sselect_hyperslab(file.get(), H5S_SELECT_SET, start.data(), nullptr, count.data(),
                                nullptr),
            "H5Sselect_hyperslab");
  const hsize_t elements = rows.size() * column.count;
  h5::Dataspace memory(h5::check(H5Screate_simple(1, &elements, nullptr), "H5Screate_simple"));
  h5::check(H5Dread(dataset, native_type(type), memory.get(), file.get(), H5P_DEFAULT,
                    buffer.data.data()),
            "H5Dread");
  if (type == ScalarType::Bool) normalize_bool(buffer);
}

std::size_t column_index(const ProductInfo& info, const ColumnInfo* column) {
  const ColumnInfo* first = info.columns.data();
  const ColumnInfo* last = first + info.columns.size();
  std::less<const ColumnInfo*> before;
  if (column == nullptr || before(column, first) || !before(column, last)) {
    throw Error("a requested column does not belong to product '" + info.name + "'");
  }
  return static_cast<std::size_t>(column - first);
}

}  // namespace spine::core
