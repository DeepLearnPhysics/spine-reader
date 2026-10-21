#pragma once

// Helpers shared by the test_core_*.cpp tests: sample file paths, the HDF5 lock, and
// comparisons of ColumnBuffers with each other and with the h5py-decoded expected.json.

#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <json.hpp>

#include <spine/error.hpp>

#include "core/source.hpp"

namespace core_test {

using spine::ScalarType;
using spine::core::ColumnInfo;
using spine::core::ColumnRequest;
using spine::core::FileSource;
using spine::core::ProductSource;
using spine::core::RowRange;
using spine::detail::ColumnBuffer;
using nlohmann::json;

/// Every HDF5 call needs this; tests hold it for their whole body. Declare it before any
/// FileSource, so that the file is closed under the lock.
struct Hdf5Lock {
  std::lock_guard<std::recursive_mutex> guard{spine::core::hdf5_mutex()};
};

inline std::string data_path(const std::string& name) {
  return std::string(SPINE_READER_TEST_DATA) + "/core/" + name;
}

inline std::unique_ptr<FileSource> open(const std::string& name,
                                        const spine::core::OpenOptions& options = {}) {
  return FileSource::open(data_path(name), options);
}

/// expected.json, decoded by make_expected.py with h5py alone.
const json& expected();

/// A request for every column of `info`, each at the column's own type.
inline std::vector<ColumnRequest> natural_requests(const spine::core::ProductInfo& info) {
  std::vector<ColumnRequest> requests;
  for (const ColumnInfo& column : info.columns) requests.push_back({&column, column.type});
  return requests;
}

inline ColumnBuffer read_column(ProductSource& product, const ColumnInfo& column, RowRange rows,
                                ScalarType type) {
  std::vector<ColumnBuffer> out;
  product.read(rows, {{&column, type}}, out);
  return std::move(out.at(0));
}

inline ColumnBuffer read_column(ProductSource& product, const std::string& name, RowRange rows,
                                ScalarType type) {
  const ColumnInfo* column = product.info().find(name);
  if (column == nullptr) throw std::runtime_error("no column " + name);
  return read_column(product, *column, rows, type);
}

inline RowRange all_rows(ProductSource& product) { return {0, product.num_rows()}; }

/// Element `i` of a numeric buffer as a double (int64 values above 2^53 lose precision).
inline double element(const ColumnBuffer& b, std::size_t i) {
  switch (b.type) {
    case ScalarType::Bool:
    case ScalarType::UInt8: return b.elements<std::uint8_t>()[i];
    case ScalarType::Int8: return b.elements<std::int8_t>()[i];
    case ScalarType::Int16: return b.elements<std::int16_t>()[i];
    case ScalarType::UInt16: return b.elements<std::uint16_t>()[i];
    case ScalarType::Int32: return b.elements<std::int32_t>()[i];
    case ScalarType::UInt32: return b.elements<std::uint32_t>()[i];
    case ScalarType::Int64: return static_cast<double>(b.elements<std::int64_t>()[i]);
    case ScalarType::UInt64: return static_cast<double>(b.elements<std::uint64_t>()[i]);
    case ScalarType::Float32: return b.elements<float>()[i];
    case ScalarType::Float64: return b.elements<double>()[i];
    case ScalarType::String: break;
  }
  return std::nan("");
}

/// memcmp that accepts the null pointers of empty vectors.
inline bool same_bytes(const void* a, const void* b, std::size_t bytes) {
  return bytes == 0 || std::memcmp(a, b, bytes) == 0;
}

/// Exactly equal, bit for bit (so NaN equals NaN), in every field of the buffer.
inline bool same_buffer(const ColumnBuffer& a, const ColumnBuffer& b) {
  return a.type == b.type && a.variable == b.variable && a.count == b.count && a.rows == b.rows &&
         a.data.size() == b.data.size() &&
         same_bytes(a.data.data(), b.data.data(), a.data.size()) && a.offsets == b.offsets &&
         a.strings == b.strings;
}

/// True if `value` (a JSON number, or "nan"/"inf"/"-inf") is what element `i` of the numeric
/// buffer holds. Integers must match exactly; floats are compared as doubles.
inline bool matches(const ColumnBuffer& b, std::size_t i, const json& value) {
  const double got = element(b, i);
  if (value.is_string()) {
    const std::string text = value.get<std::string>();
    if (text == "nan") return std::isnan(got);
    if (text == "inf") return std::isinf(got) && got > 0;
    if (text == "-inf") return std::isinf(got) && got < 0;
    return false;
  }
  if (value.is_number_integer() && b.type == ScalarType::Int64) {
    return b.elements<std::int64_t>()[i] == value.get<std::int64_t>();
  }
  if (value.is_number_integer() && b.type == ScalarType::UInt64) {
    return b.elements<std::uint64_t>()[i] == value.get<std::uint64_t>();
  }
  return got == value.get<double>();
}

/// Empty if the buffer holds `rows` (one entry per row: a number, a string, or a list), else a
/// description of the first difference.
inline std::string difference(const ColumnBuffer& b, const json& rows) {
  if (b.rows != rows.size()) {
    return "rows " + std::to_string(b.rows) + " vs expected " + std::to_string(rows.size());
  }
  for (std::size_t r = 0; r < rows.size(); ++r) {
    const json& row = rows[r];
    if (b.type == ScalarType::String) {
      if (b.strings.size() != rows.size()) return "string count differs";
      if (b.strings[r] != row.get<std::string>()) {
        return "row " + std::to_string(r) + ": '" + b.strings[r] + "' vs '" +
               row.get<std::string>() + "'";
      }
      continue;
    }
    std::size_t begin = r * b.count;
    std::size_t length = b.count;
    if (b.variable) {
      begin = static_cast<std::size_t>(b.offsets.at(r));
      length = static_cast<std::size_t>(b.offsets.at(r + 1) - b.offsets.at(r));
    }
    const bool listed = row.is_array();
    if ((listed ? row.size() : 1) != length) {
      return "row " + std::to_string(r) + ": " + std::to_string(length) + " elements vs expected " +
             std::to_string(listed ? row.size() : 1);
    }
    for (std::size_t k = 0; k < length; ++k) {
      if (!matches(b, begin + k, listed ? row[k] : row)) {
        return "row " + std::to_string(r) + " element " + std::to_string(k) + ": " +
               std::to_string(element(b, begin + k)) + " vs expected " +
               (listed ? row[k] : row).dump();
      }
    }
  }
  return {};
}

}  // namespace core_test
