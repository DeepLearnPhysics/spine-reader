// V2 (offset-based, format_version 2) products. See notes/spine_format.md, section B.

#include <algorithm>
#include <cstring>
#include <optional>
#include <string_view>
#include <utility>

#include <spine/error.hpp>

#include "core/columns.hpp"
#include "core/guard.hpp"
#include "core/products.hpp"
#include "core/row_io.hpp"
#include "core/yaml_list.hpp"
#include "h5/error.hpp"
#include "h5/util.hpp"

namespace spine::core {
namespace {

constexpr std::string_view kOffsetsPrefix = "_var_offsets_";
constexpr std::string_view kPoolPrefix = "pool_";
constexpr std::string_view kElementPrefix = "element_";

/// `index` of a name of the form <prefix><index>, such as pool_3, or nothing.
std::optional<std::size_t> numbered(const std::string& name, std::string_view prefix) {
  if (name.size() <= prefix.size() || name.compare(0, prefix.size(), prefix) != 0) {
    return std::nullopt;
  }
  std::size_t index = 0;
  for (std::size_t i = prefix.size(); i < name.size(); ++i) {
    if (name[i] < '0' || name[i] > '9') return std::nullopt;
    index = index * 10 + static_cast<std::size_t>(name[i] - '0');
  }
  return index;
}

ProductKind parse_kind(const std::string& kind) {
  if (kind == "objects") return ProductKind::Objects;
  if (kind == "array") return ProductKind::Array;
  if (kind == "string") return ProductKind::String;
  if (kind == "list") return ProductKind::List;
  if (kind == "multi_list") return ProductKind::MultiList;
  throw Error("unknown product kind '" + kind + "'");
}

std::string unsupported(ProductKind kind) {
  return "products of kind '" + std::string(to_string(kind)) + "' are not supported";
}

/// Common part of every V2 product: locating the rows of an event.
class V2Product : public ProductSource {
 public:
  V2Product(std::string where, std::uint64_t num_events, ProductInfo info,
            h5::Dataset event_offsets)
      : where_(std::move(where)),
        num_events_(num_events),
        info_(std::move(info)),
        event_offsets_(std::move(event_offsets)) {}

  const ProductInfo& info() const override { return info_; }
  RowRange rows(std::uint64_t event) override {
    return guarded(where_, [&] { return locate(event); });
  }
  std::uint64_t num_rows() override {
    return guarded(where_, [&] { return count_rows(); });
  }
  void read(RowRange rows, const std::vector<ColumnRequest>& requests,
            std::vector<detail::ColumnBuffer>& out) override {
    guarded(where_, [&] { read_columns(rows, requests, out); });
  }

 protected:
  virtual RowRange locate(std::uint64_t event) {
    if (event >= num_events_) {
      throw Error("event " + std::to_string(event) + " is out of range, the file has " +
                  std::to_string(num_events_) + " events");
    }
    // A scalar product has one row per event; this saves reading event_offsets.
    if (info_.scalar) return {event, event + 1};
    std::int64_t bounds[2];
    read_elements(event_offsets_.get(), event, 2, H5T_NATIVE_INT64, bounds);
    if (bounds[0] < 0 || bounds[1] < bounds[0]) {
      throw Error("event_offsets is corrupt at event " + std::to_string(event));
    }
    return {static_cast<std::uint64_t>(bounds[0]), static_cast<std::uint64_t>(bounds[1])};
  }
  virtual std::uint64_t count_rows() = 0;
  virtual void read_columns(RowRange rows, const std::vector<ColumnRequest>& requests,
                            std::vector<detail::ColumnBuffer>& out) = 0;

  /// Throws unless `rows` lies within the `total` rows of the product.
  static void check_rows(RowRange rows, std::uint64_t total) {
    if (rows.end < rows.begin || rows.end > total) {
      throw Error("rows [" + std::to_string(rows.begin) + ", " + std::to_string(rows.end) +
                  ") are outside the " + std::to_string(total) + " rows of the product");
    }
  }

  std::string where_;
  std::uint64_t num_events_;
  ProductInfo info_;
  h5::Dataset event_offsets_;
};

/// A product the reader lists but cannot read: lists of arrays.
class V2Unsupported final : public V2Product {
 public:
  using V2Product::V2Product;

 protected:
  RowRange locate(std::uint64_t) override { throw Error(unsupported(info_.kind)); }
  std::uint64_t count_rows() override { throw Error(unsupported(info_.kind)); }
  void read_columns(RowRange, const std::vector<ColumnRequest>&,
                    std::vector<detail::ColumnBuffer>&) override {
    throw Error(unsupported(info_.kind));
  }
};

/// kind "array": numbers, one row of `values` per element, width in `count`.
class V2Array final : public V2Product {
 public:
  V2Array(std::string where, std::uint64_t num_events, ProductInfo info,
          h5::Dataset event_offsets, h5::Dataset values)
      : V2Product(std::move(where), num_events, std::move(info), std::move(event_offsets)),
        values_(std::move(values)) {
    h5::Dataspace space = h5::dataset_space(values_.get());
    num_rows_ = h5::extent(space.get())[0];
  }

 protected:
  std::uint64_t count_rows() override { return num_rows_; }
  void read_columns(RowRange rows, const std::vector<ColumnRequest>& requests,
                    std::vector<detail::ColumnBuffer>& out) override {
    check_rows(rows, num_rows_);
    out.resize(requests.size());
    for (std::size_t i = 0; i < requests.size(); ++i) {
      const ColumnInfo& column = info_.columns[column_index(info_, requests[i].column)];
      check_request(column, requests[i].type);
      read_array_rows(values_.get(), rows, column, requests[i].type, out[i]);
    }
  }

 private:
  h5::Dataset values_;
  std::uint64_t num_rows_ = 0;
};

/// kind "string": UTF-8 bytes in `values`; event e is values[offsets[e] : offsets[e + 1]], so
/// the rows of a string product are events whether or not it is flagged scalar.
class V2String final : public V2Product {
 public:
  V2String(std::string where, std::uint64_t num_events, ProductInfo info,
           h5::Dataset event_offsets, h5::Dataset values)
      : V2Product(std::move(where), num_events, std::move(info), std::move(event_offsets)),
        values_(std::move(values)) {
    h5::Dataspace space = h5::dataset_space(values_.get());
    num_bytes_ = h5::extent(space.get())[0];
    info_.scalar = true;
  }

 protected:
  std::uint64_t count_rows() override { return num_events_; }
  void read_columns(RowRange rows, const std::vector<ColumnRequest>& requests,
                    std::vector<detail::ColumnBuffer>& out) override {
    check_rows(rows, num_events_);
    const auto count = static_cast<std::size_t>(rows.size());
    out.resize(requests.size());
    for (std::size_t i = 0; i < requests.size(); ++i) {
      const ColumnInfo& column = info_.columns[column_index(info_, requests[i].column)];
      check_request(column, requests[i].type);
      prepare(out[i], column, ScalarType::String, count);
    }
    if (count == 0 || requests.empty()) return;

    std::vector<std::int64_t> bounds(count + 1);
    read_elements(event_offsets_.get(), rows.begin, bounds.size(), H5T_NATIVE_INT64,
                  bounds.data());
    const bool sane = bounds.front() >= 0 && std::is_sorted(bounds.begin(), bounds.end()) &&
                      static_cast<std::uint64_t>(bounds.back()) <= num_bytes_;
    if (!sane) {
      throw Error("event_offsets is corrupt for events [" + std::to_string(rows.begin) + ", " +
                  std::to_string(rows.end) + ")");
    }
    const auto first = static_cast<std::uint64_t>(bounds.front());
    std::vector<char> bytes(static_cast<std::size_t>(bounds.back() - bounds.front()));
    read_elements(values_.get(), first, bytes.size(), H5T_NATIVE_UINT8, bytes.data());
    for (detail::ColumnBuffer& buffer : out) {
      for (std::size_t r = 0; r < count; ++r) {
        buffer.strings.emplace_back(bytes.data() + (bounds[r] - bounds.front()),
                                    static_cast<std::size_t>(bounds[r + 1] - bounds[r]));
      }
    }
  }

 private:
  h5::Dataset values_;
  std::uint64_t num_bytes_ = 0;
};

/// One variables/pool_<i> group: the variable-length fields of one element type.
struct Pool {
  std::vector<std::string> fields;
  bool is_string = false;
  ScalarType type = ScalarType::UInt8;  // element type of `values`
  std::uint64_t size = 0;               // elements in `values`
  h5::Dataset values;
};

/// Where the data of a column of an Objects product is.
struct Source {
  bool in_pool = false;   // false: a column of `fixed`
  std::size_t pool = 0;   // for pool fields: index into the product's pools,
  std::size_t field = 0;  // and position in that pool's field list
};

/// Where the fields of `count` consecutive objects are in one pool, from their rows of
/// `_var_offsets_<p>`: field j of object r is values[begin(r, j) : begin(r, j + 1)]. The fields
/// of one object are contiguous in the pool and objects follow each other, so the objects
/// cover one range of the pool, [low, high), which is read once.
class PoolSlices {
 public:
  PoolSlices(const unsigned char* packed, std::size_t row_size, std::size_t offsets_at,
             std::size_t count, std::size_t pool_index, const Pool& pool)
      : width_(pool.fields.size() + 1), count_(count), bounds_(count * width_) {
    for (std::size_t r = 0; r < count_; ++r) {
      std::memcpy(&bounds_[r * width_], packed + r * row_size + offsets_at,
                  width_ * sizeof(std::int64_t));
    }
    low_ = bounds_.front();
    high_ = bounds_[(count_ - 1) * width_ + width_ - 1];
    bool sane = low_ >= 0 && high_ >= low_ && static_cast<std::uint64_t>(high_) <= pool.size;
    for (std::size_t r = 0; sane && r < count_; ++r) {
      const std::int64_t* row = &bounds_[r * width_];
      sane = row[0] >= low_ && row[width_ - 1] <= high_ && std::is_sorted(row, row + width_);
    }
    if (!sane) throw Error("the offsets into pool_" + std::to_string(pool_index) + " are corrupt");
  }

  std::uint64_t first() const { return static_cast<std::uint64_t>(low_); }
  std::uint64_t length() const { return static_cast<std::uint64_t>(high_ - low_); }
  /// Start, relative to first(), of field j of object r; field `fields` is the end of the object.
  std::size_t start(std::size_t r, std::size_t j) const {
    return static_cast<std::size_t>(bounds_[r * width_ + j] - low_);
  }
  std::size_t size(std::size_t r, std::size_t j) const { return start(r, j + 1) - start(r, j); }

 private:
  std::size_t width_;
  std::size_t count_;
  std::vector<std::int64_t> bounds_;
  std::int64_t low_ = 0;
  std::int64_t high_ = 0;
};

/// kind "objects": a `fixed` compound plus pools for the variable-length fields.
class V2Objects final : public V2Product {
 public:
  V2Objects(std::string where, std::uint64_t num_events, ProductInfo info,
            h5::Dataset event_offsets, h5::Dataset fixed, std::vector<h5::Datatype> file_types,
            std::vector<Pool> pools, std::vector<Source> sources)
      : V2Product(std::move(where), num_events, std::move(info), std::move(event_offsets)),
        fixed_(std::move(fixed)),
        file_types_(std::move(file_types)),
        pools_(std::move(pools)),
        sources_(std::move(sources)) {
    h5::Dataspace space = h5::dataset_space(fixed_.get());
    num_rows_ = h5::extent(space.get())[0];
  }

 protected:
  std::uint64_t count_rows() override { return num_rows_; }

  void read_columns(RowRange rows, const std::vector<ColumnRequest>& requests,
                    std::vector<detail::ColumnBuffer>& out) override {
    check_rows(rows, num_rows_);
    const auto count = static_cast<std::size_t>(rows.size());
    out.resize(requests.size());

    // One compound read of `fixed` brings the requested fixed columns and, for the pools the
    // request touches, the offsets that locate each row's fields in the pool.
    RowLayout layout;
    std::vector<std::size_t> member_offset(requests.size(), 0);
    std::vector<bool> pool_needed(pools_.size(), false);
    for (std::size_t i = 0; i < requests.size(); ++i) {
      const std::size_t index = column_index(info_, requests[i].column);
      const ColumnInfo& column = info_.columns[index];
      check_request(column, requests[i].type);
      prepare(out[i], column, requests[i].type, count);
      if (sources_[index].in_pool) {
        pool_needed[sources_[index].pool] = true;
      } else {
        member_offset[i] = layout.add(
            column.name, memory_type(column, requests[i].type, file_types_[index].get()));
      }
    }
    if (count == 0) return;

    std::vector<std::size_t> offsets_at(pools_.size(), 0);
    for (std::size_t p = 0; p < pools_.size(); ++p) {
      if (!pool_needed[p]) continue;
      const hsize_t length = pools_[p].fields.size() + 1;
      h5::Datatype type(
          h5::check(H5Tarray_create2(H5T_NATIVE_INT64, 1, &length), "H5Tarray_create2"));
      offsets_at[p] = layout.add(std::string(kOffsetsPrefix) + std::to_string(p), std::move(type));
    }
    if (layout.empty()) return;

    std::vector<unsigned char> packed(count * layout.row_size());
    {
      RowSelection selection(fixed_.get(), rows);
      h5::Datatype memory = layout.build();
      h5::check(H5Dread(fixed_.get(), memory.get(), selection.memory.get(), selection.file.get(),
                        H5P_DEFAULT, packed.data()),
                "H5Dread", "fixed");
    }

    for (std::size_t i = 0; i < requests.size(); ++i) {
      if (sources_[column_index(info_, requests[i].column)].in_pool) continue;
      copy_fixed(packed.data(), layout.row_size(), member_offset[i], out[i]);
      if (requests[i].type == ScalarType::Bool) normalize_bool(out[i]);
    }
    for (std::size_t p = 0; p < pools_.size(); ++p) {
      if (!pool_needed[p]) continue;
      const PoolSlices slices(packed.data(), layout.row_size(), offsets_at[p], count, p, pools_[p]);
      if (pools_[p].is_string) {
        fill_strings(p, slices, requests, out);
      } else {
        fill_numbers(p, slices, requests, out);
      }
    }
  }

 private:
  /// The requests that ask for a field of pool `p`.
  std::vector<std::size_t> requests_in(std::size_t p, const std::vector<ColumnRequest>& requests) {
    std::vector<std::size_t> found;
    for (std::size_t i = 0; i < requests.size(); ++i) {
      const Source& source = sources_[column_index(info_, requests[i].column)];
      if (source.in_pool && source.pool == p) found.push_back(i);
    }
    return found;
  }

  void fill_strings(std::size_t p, const PoolSlices& slices,
                    const std::vector<ColumnRequest>& requests,
                    std::vector<detail::ColumnBuffer>& out) {
    std::vector<char> bytes(static_cast<std::size_t>(slices.length()));
    read_elements(pools_[p].values.get(), slices.first(), slices.length(), H5T_NATIVE_UINT8,
                  bytes.data());
    for (std::size_t i : requests_in(p, requests)) {
      const std::size_t field = sources_[column_index(info_, requests[i].column)].field;
      for (std::size_t r = 0; r < out[i].rows; ++r) {
        out[i].strings.emplace_back(bytes.data() + slices.start(r, field), slices.size(r, field));
      }
    }
  }

  /// Numbers are read once for each element type asked for (normally just one) and then cut up.
  void fill_numbers(std::size_t p, const PoolSlices& slices,
                    const std::vector<ColumnRequest>& requests,
                    std::vector<detail::ColumnBuffer>& out) {
    const std::vector<std::size_t> wanted = requests_in(p, requests);
    std::vector<ScalarType> types;
    for (std::size_t i : wanted) {
      if (std::find(types.begin(), types.end(), requests[i].type) == types.end()) {
        types.push_back(requests[i].type);
      }
    }
    for (ScalarType type : types) {
      const std::size_t size = scalar_size(type);
      std::vector<unsigned char> values(static_cast<std::size_t>(slices.length()) * size);
      read_elements(pools_[p].values.get(), slices.first(), slices.length(), native_type(type),
                    values.data());
      for (std::size_t i : wanted) {
        if (requests[i].type != type) continue;
        const std::size_t field = sources_[column_index(info_, requests[i].column)].field;
        detail::ColumnBuffer& buffer = out[i];
        for (std::size_t r = 0; r < buffer.rows; ++r) {
          buffer.offsets[r + 1] = buffer.offsets[r] + slices.size(r, field);
        }
        buffer.data.resize(static_cast<std::size_t>(buffer.offsets[buffer.rows]) * size);
        for (std::size_t r = 0; r < buffer.rows; ++r) {
          const std::size_t elements = slices.size(r, field);
          if (elements == 0) continue;
          std::memcpy(buffer.data.data() + static_cast<std::size_t>(buffer.offsets[r]) * size,
                      values.data() + slices.start(r, field) * size, elements * size);
        }
        if (type == ScalarType::Bool) normalize_bool(buffer);
      }
    }
  }

  h5::Dataset fixed_;
  std::vector<h5::Datatype> file_types_;  // per column; unused for pool fields
  std::vector<Pool> pools_;
  std::vector<Source> sources_;  // per column
  std::uint64_t num_rows_ = 0;
};

/// Opens variables/pool_<i> for every i found, and returns them indexed by i.
std::vector<Pool> open_pools(hid_t product) {
  std::vector<Pool> pools;
  if (!h5::has_link(product, "variables")) return pools;
  h5::Group variables = h5::open_group(product, "variables");
  for (const std::string& child : h5::child_names(variables.get())) {
    const std::optional<std::size_t> index = numbered(child, kPoolPrefix);
    if (!index) continue;
    h5::Group group = h5::open_group(variables.get(), child);
    Pool pool;
    pool.fields = parse_yaml_list(h5::read_string_attribute(group.get(), "fields"));
    const std::string kind = h5::read_string_attribute(group.get(), "kind");
    if (kind != "string" && kind != "array") {
      throw Error(child + " has unknown kind '" + kind + "'");
    }
    pool.is_string = kind == "string";
    pool.values = h5::open_dataset(group.get(), "values");
    h5::Dataspace space = h5::dataset_space(pool.values.get());
    const std::vector<hsize_t> dims = h5::extent(space.get());
    if (dims.size() != 1) throw Error(child + "/values is not one-dimensional");
    pool.size = dims[0];
    if (!pool.is_string) pool.type = describe_array_dataset(child, pool.values.get()).type;
    if (pools.size() <= *index) pools.resize(*index + 1);
    if (pools[*index].values) throw Error("duplicate " + child);
    pools[*index] = std::move(pool);
  }
  return pools;
}

std::unique_ptr<ProductSource> open_objects(const std::string& where, hid_t group,
                                            std::uint64_t num_events, ProductInfo info,
                                            h5::Dataset event_offsets) {
  if (h5::has_attribute(group, "class_name")) {
    info.class_name = h5::read_string_attribute(group, "class_name");
  }
  h5::Dataset fixed = h5::open_dataset(group, "fixed");
  h5::Datatype fixed_type = h5::dataset_type(fixed.get());
  if (H5Tget_class(fixed_type.get()) != H5T_COMPOUND) {
    throw Error("'fixed' is not a compound dataset");
  }
  std::vector<Pool> pools = open_pools(group);

  // The ordinary columns, and the offsets columns (_var_offsets_<i>) that belong to pools.
  std::vector<h5::Datatype> file_types;
  std::vector<Source> sources;
  std::vector<std::optional<hsize_t>> offsets_length(pools.size());
  const int members = h5::check(H5Tget_nmembers(fixed_type.get()), "H5Tget_nmembers");
  for (int m = 0; m < members; ++m) {
    const auto position = static_cast<unsigned>(m);
    const std::string name = h5::member_name(fixed_type.get(), position);
    h5::Datatype type = h5::member_type(fixed_type.get(), position);
    if (const std::optional<std::size_t> pool = numbered(name, kOffsetsPrefix)) {
      const bool one_dimensional =
          H5Tget_class(type.get()) == H5T_ARRAY &&
          h5::check(H5Tget_array_ndims(type.get()), "H5Tget_array_ndims") == 1;
      if (!one_dimensional) throw Error(name + " is not a one-dimensional array");
      hsize_t length = 0;
      h5::check(H5Tget_array_dims2(type.get(), &length), "H5Tget_array_dims2");
      if (*pool >= pools.size() || !pools[*pool].values) {
        throw Error(name + " has no matching variables/pool_" + std::to_string(*pool));
      }
      offsets_length[*pool] = length;
      continue;
    }
    std::optional<ColumnInfo> column = describe_type(name, type.get());
    if (!column) continue;  // a type spine-reader cannot read; the column is left out
    info.columns.push_back(std::move(*column));
    sources.push_back({});
    file_types.push_back(std::move(type));
  }

  // The fields of each pool, after the ordinary columns, pool by pool.
  for (std::size_t p = 0; p < pools.size(); ++p) {
    if (!pools[p].values) continue;
    const std::string number = std::to_string(p);
    if (!offsets_length[p]) {
      throw Error("variables/pool_" + number + " has no _var_offsets_" + number +
                  " column in 'fixed'");
    }
    if (*offsets_length[p] != pools[p].fields.size() + 1) {
      throw Error("_var_offsets_" + number + " has " + std::to_string(*offsets_length[p]) +
                  " entries but pool_" + number + " has " + std::to_string(pools[p].fields.size()) +
                  " fields");
    }
    for (std::size_t j = 0; j < pools[p].fields.size(); ++j) {
      ColumnInfo column;
      column.name = pools[p].fields[j];
      column.type = pools[p].is_string ? ScalarType::String : pools[p].type;
      column.variable = true;
      column.count = pools[p].is_string ? 1 : 0;
      info.columns.push_back(std::move(column));
      sources.push_back({true, p, j});
      file_types.emplace_back();
    }
  }
  return std::make_unique<V2Objects>(where, num_events, std::move(info), std::move(event_offsets),
                                     std::move(fixed), std::move(file_types), std::move(pools),
                                     std::move(sources));
}

/// Columns of a list product: its `values`, or the values of each of its element_<k> children.
void describe_lists(hid_t group, ProductInfo& info) {
  if (info.kind == ProductKind::List) {
    h5::Dataset values = h5::open_dataset(group, "values");
    info.columns.push_back(describe_array_dataset("", values.get()));
    return;
  }
  std::vector<std::pair<std::size_t, std::string>> elements;
  for (const std::string& child : h5::child_names(group)) {
    if (const std::optional<std::size_t> k = numbered(child, kElementPrefix)) {
      elements.emplace_back(*k, child);
    }
  }
  std::sort(elements.begin(), elements.end());
  for (const auto& [k, child] : elements) {
    h5::Group element = h5::open_group(group, child);
    h5::Dataset values = h5::open_dataset(element.get(), "values");
    info.columns.push_back(describe_array_dataset(child, values.get()));
  }
}

}  // namespace

std::unique_ptr<ProductSource> open_v2_product(const std::string& path, hid_t products_root,
                                               const std::string& name,
                                               std::uint64_t num_events) {
  const std::string where = path + ", product '" + name + "'";
  return guarded(where, [&]() -> std::unique_ptr<ProductSource> {
    h5::Group group = h5::open_group(products_root, name);
    ProductInfo info;
    info.name = name;
    info.kind = parse_kind(h5::read_string_attribute(group.get(), "kind"));
    info.scalar = h5::has_attribute(group.get(), "scalar") &&
                  h5::read_int_attribute(group.get(), "scalar") != 0;

    // A multi_list has no datasets of its own: each element_<k> child has its own offsets.
    h5::Dataset event_offsets;
    if (info.kind != ProductKind::MultiList) {
      event_offsets = h5::open_dataset(group.get(), "event_offsets");
    }

    switch (info.kind) {
      case ProductKind::Objects:
        return open_objects(where, group.get(), num_events, std::move(info),
                            std::move(event_offsets));
      case ProductKind::Array: {
        h5::Dataset values = h5::open_dataset(group.get(), "values");
        info.columns.push_back(describe_array_dataset("", values.get()));
        return std::make_unique<V2Array>(where, num_events, std::move(info),
                                         std::move(event_offsets), std::move(values));
      }
      case ProductKind::String: {
        h5::Dataset values = h5::open_dataset(group.get(), "values");
        describe_array_dataset("values", values.get());  // checks that it is a numeric array
        ColumnInfo column;
        column.type = ScalarType::String;
        column.variable = true;
        info.columns.push_back(column);
        return std::make_unique<V2String>(where, num_events, std::move(info),
                                          std::move(event_offsets), std::move(values));
      }
      case ProductKind::List:
      case ProductKind::MultiList:
        describe_lists(group.get(), info);
        return std::make_unique<V2Unsupported>(where, num_events, std::move(info),
                                               std::move(event_offsets));
    }
    throw Error("unreachable product kind");
  });
}

}  // namespace spine::core
