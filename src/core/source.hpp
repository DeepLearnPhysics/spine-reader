#pragma once

// Internal columnar core. Presents one SPINE HDF5 file, whatever its layout, as a set of
// products made of named columns, with a row range per event. Only the binder and the
// tests use this header; it is not installed.

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include <spine/descriptor.hpp>
#include <spine/detail/column_buffer.hpp>

namespace spine::core {

/// On-disk layout. V1: `events` compound of region references plus VLEN members.
/// V2Root: format_version 2 as written by SPINE v0.16.x, product groups at the file root.
/// V2Products: format_version 2 from SPINE v0.17.0 on, product groups under /products.
enum class Layout { V1, V2Root, V2Products };
std::string_view to_string(Layout layout);

/// V2 product kinds; V1 products map onto Objects (compound with class_name) or Array.
enum class ProductKind { Objects, Array, String, List, MultiList };
std::string_view to_string(ProductKind kind);

struct FileEnumMember {
  std::string name;
  std::int64_t value;
};

struct ColumnInfo {
  std::string name;
  /// Natural type of one file element. SPINE's {FALSE, TRUE} enum and plain uint8 bools
  /// written before v0.11.0 both report Bool and UInt8 respectively; other HDF5 enums
  /// report their integer base type and fill enum_members.
  ScalarType type = ScalarType::Int64;
  std::size_t count = 1;  // fixed-size array length; 1 for scalars; 0 for variable
  bool variable = false;  // V1 VLEN member or V2 pool field
  std::vector<FileEnumMember> enum_members;  // the file's enum labels; empty if not an enum
};

struct ProductInfo {
  std::string name;  // product key, e.g. "reco_particles"
  ProductKind kind = ProductKind::Objects;
  std::string class_name;  // SPINE class for Objects, e.g. "RecoParticle"; else ""
  bool scalar = false;     // exactly one row per event (run_info, meta, index, ...)
  /// Objects: the class's columns in file order, with V2 helper columns
  /// (_var_offsets_<i>) hidden. Array and String: one column named "" holding each row.
  std::vector<ColumnInfo> columns;

  const ColumnInfo* find(std::string_view column) const;
};

/// Half-open range of rows [begin, end) in a product.
struct RowRange {
  std::uint64_t begin = 0;
  std::uint64_t end = 0;
  std::uint64_t size() const { return end - begin; }
};

struct ColumnRequest {
  const ColumnInfo* column;  // from this product's ProductInfo
  ScalarType type;           // memory type wanted; String for string columns
};

class ProductSource {
 public:
  virtual ~ProductSource() = default;
  virtual const ProductInfo& info() const = 0;
  /// Rows belonging to `event`. Scalar products always return exactly one row.
  virtual RowRange rows(std::uint64_t event) = 0;
  /// Total number of rows in the file.
  virtual std::uint64_t num_rows() = 0;
  /// Reads the requested columns over `rows`, converting each to its requested type, with as
  /// few HDF5 reads as possible (one compound read for all fixed columns). `out` is resized to
  /// reqs.size() and out[i] answers reqs[i]. Numeric columns convert to any numeric type, enums
  /// to integers, bools to Bool; anything else throws spine::Error.
  virtual void read(RowRange rows, const std::vector<ColumnRequest>& reqs,
                    std::vector<detail::ColumnBuffer>& out) = 0;
};

struct OpenOptions {
  bool allow_incomplete = false;  // SPINE marks files still being written with complete=False
};

struct FileInfo {
  std::string path;
  Layout layout = Layout::V1;
  int format_version = 1;     // /info@format_version; 1 when absent
  std::string spine_version;  // /info@spine_version (or @version); "" when absent
  bool complete = true;       // /info@complete; true when absent
  std::uint64_t num_events = 0;
  std::string cfg;  // /info@cfg (SPINE configuration YAML); "" when absent
};

class FileSource {
 public:
  /// Opens a SPINE HDF5 file and detects its layout. Throws spine::Error on files that are not
  /// SPINE outputs, SPINE cache shards (/info@format == "cache"), or incomplete files unless
  /// allowed.
  static std::unique_ptr<FileSource> open(const std::string& path, const OpenOptions& options = {});

  virtual ~FileSource() = default;
  virtual const FileInfo& info() const = 0;
  virtual const std::vector<std::string>& product_names() const = 0;  // file order
  virtual bool has_product(std::string_view name) const = 0;
  /// The product, opened on first use and owned by this FileSource. Throws if absent.
  virtual ProductSource& product(std::string_view name) = 0;
};

/// Serialises every HDF5 call made by spine-reader, since the HDF5 builds the experiments use
/// are not thread-safe. Public entry points lock it; code in core/ and h5/ assumes it is held.
std::recursive_mutex& hdf5_mutex();

}  // namespace spine::core
