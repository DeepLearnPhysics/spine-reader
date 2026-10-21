// V1 (region-reference, format_version 1) products. See notes/spine_format.md, section A.

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
#include "h5/error.hpp"
#include "h5/util.hpp"

// V1 files hold legacy dataset-region references (12-byte hdset_reg_ref_t), which only the old
// reference API can follow. HDF5 1.12 and later keep it unless built without deprecated symbols.
#ifdef H5_NO_DEPRECATED_SYMBOLS
#error "V1 files need H5Rget_region, which an HDF5 built without deprecated symbols lacks"
#endif

namespace spine::core {
namespace {

std::string unsupported(ProductKind kind) {
  return "products of kind '" + std::string(to_string(kind)) + "' are not supported";
}

/// Common part of every V1 product: the rows of event e are the region that the reference
/// events[e].<product> selects in the product's dataset.
class V1Product : public ProductSource {
 public:
  V1Product(std::string where, std::uint64_t num_events, ProductInfo info, h5::Dataset events,
            h5::Datatype reference_type, h5::Dataset data)
      : where_(std::move(where)),
        num_events_(num_events),
        info_(std::move(info)),
        events_(std::move(events)),
        reference_type_(std::move(reference_type)),
        data_(std::move(data)) {}

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
  /// SPINE appends the rows of events in order, so an event is one contiguous block of rows
  /// (a hyperslab of the whole width), or nothing: an event without objects has a reference
  /// with no selection at all (H5S_SEL_NONE), for which the block is empty, at row 0.
  virtual RowRange locate(std::uint64_t event) {
    if (event >= num_events_) {
      throw Error("event " + std::to_string(event) + " is out of range, the file has " +
                  std::to_string(num_events_) + " events");
    }
    hdset_reg_ref_t reference;
    std::memset(&reference, 0, sizeof(reference));
    read_elements(events_.get(), event, 1, reference_type_.get(), &reference);
    h5::Dataspace region(
        h5::check(H5Rget_region(data_.get(), H5R_DATASET_REGION, &reference), "H5Rget_region"));

    auto which = [&] { return "the reference of event " + std::to_string(event); };
    const hssize_t selected =
        h5::check(H5Sget_select_npoints(region.get()), "H5Sget_select_npoints");
    if (selected == 0) {
      if (info_.scalar) throw Error("scalar product has no row for event " + std::to_string(event));
      return {0, 0};
    }
    const std::vector<hsize_t> dims = h5::extent(region.get());
    std::vector<hsize_t> low(dims.size());
    std::vector<hsize_t> high(dims.size());
    h5::check(H5Sget_select_bounds(region.get(), low.data(), high.data()),
              "H5Sget_select_bounds");
    hsize_t block = 1;  // elements in the bounding box of the selection
    for (std::size_t d = 0; d < dims.size(); ++d) {
      block *= high[d] - low[d] + 1;
      if (d > 0 && (low[d] != 0 || high[d] + 1 != dims[d])) {
        throw Error(which() + " selects part of the width of the rows");
      }
    }
    if (block != static_cast<hsize_t>(selected)) {
      throw Error(which() + " is not one contiguous block of rows");
    }
    const RowRange rows{low[0], high[0] + 1};
    if (info_.scalar && rows.size() != 1) {
      throw Error("scalar product has " + std::to_string(rows.size()) + " rows for event " +
                  std::to_string(event));
    }
    return rows;
  }

  virtual std::uint64_t count_rows() {
    h5::Dataspace space = h5::dataset_space(data_.get());
    return h5::extent(space.get())[0];
  }
  virtual void read_columns(RowRange rows, const std::vector<ColumnRequest>& requests,
                            std::vector<detail::ColumnBuffer>& out) = 0;

  void check_rows(RowRange rows) {
    const std::uint64_t total = count_rows();
    if (rows.end < rows.begin || rows.end > total) {
      throw Error("rows [" + std::to_string(rows.begin) + ", " + std::to_string(rows.end) +
                  ") are outside the " + std::to_string(total) + " rows of the product");
    }
  }

  std::string where_;
  std::uint64_t num_events_;
  ProductInfo info_;
  h5::Dataset events_;
  h5::Datatype reference_type_;  // compound with just events[].<product>, as a region reference
  h5::Dataset data_;             // the product's dataset; empty for groups (lists)
};

/// A product the reader lists but cannot read: groups holding lists of arrays.
class V1Unsupported final : public V1Product {
 public:
  using V1Product::V1Product;

 protected:
  RowRange locate(std::uint64_t) override { throw Error(unsupported(info_.kind)); }
  std::uint64_t count_rows() override { throw Error(unsupported(info_.kind)); }
  void read_columns(RowRange, const std::vector<ColumnRequest>&,
                    std::vector<detail::ColumnBuffer>&) override {
    throw Error(unsupported(info_.kind));
  }
};

/// A dataset of plain numbers, one row per element, width in `count`.
class V1Array final : public V1Product {
 public:
  using V1Product::V1Product;

 protected:
  void read_columns(RowRange rows, const std::vector<ColumnRequest>& requests,
                    std::vector<detail::ColumnBuffer>& out) override {
    check_rows(rows);
    out.resize(requests.size());
    for (std::size_t i = 0; i < requests.size(); ++i) {
      const ColumnInfo& column = info_.columns[column_index(info_, requests[i].column)];
      check_request(column, requests[i].type);
      read_array_rows(data_.get(), rows, column, requests[i].type, out[i]);
    }
  }
};

/// A dataset of variable-length UTF-8 strings, one per row.
class V1String final : public V1Product {
 public:
  using V1Product::V1Product;

 protected:
  void read_columns(RowRange rows, const std::vector<ColumnRequest>& requests,
                    std::vector<detail::ColumnBuffer>& out) override {
    check_rows(rows);
    const auto count = static_cast<std::size_t>(rows.size());
    out.resize(requests.size());
    for (std::size_t i = 0; i < requests.size(); ++i) {
      const ColumnInfo& column = info_.columns[column_index(info_, requests[i].column)];
      check_request(column, requests[i].type);
      prepare(out[i], column, ScalarType::String, count);
    }
    if (count == 0 || requests.empty()) return;

    // The strings are read once and given to every request. The buffer is zeroed and
    // reclaimed whatever happens, since HDF5 allocates the strings in it.
    h5::Datatype file_type = h5::dataset_type(data_.get());
    h5::Datatype memory = h5::copy_type(file_type.get());
    RowSelection selection(data_.get(), rows);
    std::vector<unsigned char> packed(count * sizeof(char*), 0);
    h5::Reclaim reclaim(memory.get(), selection.memory.get(), packed.data());
    h5::check(H5Dread(data_.get(), memory.get(), selection.memory.get(), selection.file.get(),
                      H5P_DEFAULT, packed.data()),
              "H5Dread");
    for (detail::ColumnBuffer& buffer : out) copy_strings(packed.data(), sizeof(char*), 0, buffer);
  }
};

/// A compound dataset with one row per object.
class V1Objects final : public V1Product {
 public:
  V1Objects(std::string where, std::uint64_t num_events, ProductInfo info, h5::Dataset events,
            h5::Datatype reference_type, h5::Dataset data, std::vector<h5::Datatype> file_types)
      : V1Product(std::move(where), num_events, std::move(info), std::move(events),
                  std::move(reference_type), std::move(data)),
        file_types_(std::move(file_types)) {}

 protected:
  void read_columns(RowRange rows, const std::vector<ColumnRequest>& requests,
                    std::vector<detail::ColumnBuffer>& out) override {
    check_rows(rows);
    const auto count = static_cast<std::size_t>(rows.size());
    out.resize(requests.size());

    RowLayout layout;
    std::vector<std::size_t> member_offset(requests.size(), 0);
    for (std::size_t i = 0; i < requests.size(); ++i) {
      const std::size_t index = column_index(info_, requests[i].column);
      const ColumnInfo& column = info_.columns[index];
      check_request(column, requests[i].type);
      prepare(out[i], column, requests[i].type, count);
      member_offset[i] = layout.add(
          column.name, memory_type(column, requests[i].type, file_types_[index].get()));
    }
    if (count == 0 || layout.empty()) return;

    // One read of the requested members of all rows. The buffer is zeroed and reclaimed
    // whatever happens, since HDF5 allocates the VLEN sequences and strings in it.
    RowSelection selection(data_.get(), rows);
    h5::Datatype memory = layout.build();
    std::vector<unsigned char> packed(count * layout.row_size(), 0);
    h5::Reclaim reclaim(memory.get(), selection.memory.get(), packed.data());
    h5::check(H5Dread(data_.get(), memory.get(), selection.memory.get(), selection.file.get(),
                      H5P_DEFAULT, packed.data()),
              "H5Dread");

    for (std::size_t i = 0; i < requests.size(); ++i) {
      const ColumnInfo& column = *requests[i].column;
      if (column.type == ScalarType::String) {
        copy_strings(packed.data(), layout.row_size(), member_offset[i], out[i]);
      } else if (column.variable) {
        copy_vlen(packed.data(), layout.row_size(), member_offset[i], out[i]);
      } else {
        copy_fixed(packed.data(), layout.row_size(), member_offset[i], out[i]);
      }
      if (requests[i].type == ScalarType::Bool) normalize_bool(out[i]);
    }
  }

 private:
  std::vector<h5::Datatype> file_types_;  // per column
};

/// Columns of a group product: the `elements` dataset of a list, or the element_<k> datasets
/// of a mixed-width list.
void describe_group(hid_t group, ProductInfo& info) {
  if (h5::has_link(group, "elements")) {
    info.kind = ProductKind::List;
    h5::Dataset elements = h5::open_dataset(group, "elements");
    info.columns.push_back(describe_array_dataset("", elements.get()));
    return;
  }
  constexpr std::string_view prefix = "element_";
  std::vector<std::pair<std::size_t, std::string>> elements;
  for (const std::string& child : h5::child_names(group)) {
    if (child.size() > prefix.size() && child.compare(0, prefix.size(), prefix) == 0 &&
        child.find_first_not_of("0123456789", prefix.size()) == std::string::npos) {
      elements.emplace_back(std::stoul(child.substr(prefix.size())), child);
    }
  }
  if (!h5::has_link(group, "index") || elements.empty()) {
    throw Error("group is neither a list of arrays nor a list of mixed-width arrays");
  }
  info.kind = ProductKind::MultiList;
  std::sort(elements.begin(), elements.end());
  for (const auto& [k, child] : elements) {
    h5::Dataset values = h5::open_dataset(group, child);
    info.columns.push_back(describe_array_dataset(child, values.get()));
  }
}

bool is_scalar(hid_t object) {
  return h5::has_attribute(object, "scalar") && h5::read_int_attribute(object, "scalar") != 0;
}

/// A compound dataset: one column per member spine-reader can read. The matching file types
/// go to `file_types`, one per column.
void describe_objects(hid_t data, hid_t type, ProductInfo& info,
                      std::vector<h5::Datatype>& file_types) {
  info.kind = ProductKind::Objects;
  if (h5::has_attribute(data, "class_name")) {
    info.class_name = h5::read_string_attribute(data, "class_name");
  }
  const int members = h5::check(H5Tget_nmembers(type), "H5Tget_nmembers");
  for (int m = 0; m < members; ++m) {
    const auto position = static_cast<unsigned>(m);
    h5::Datatype member = h5::member_type(type, position);
    std::optional<ColumnInfo> column =
        describe_type(h5::member_name(type, position), member.get());
    if (!column) continue;  // a type spine-reader cannot read; the column is left out
    info.columns.push_back(std::move(*column));
    file_types.push_back(std::move(member));
  }
}

}  // namespace

std::unique_ptr<ProductSource> open_v1_product(const std::string& path, hid_t file,
                                               const std::string& name,
                                               std::uint64_t num_events) {
  const std::string where = path + ", product '" + name + "'";
  return guarded(where, [&]() -> std::unique_ptr<ProductSource> {
    h5::Dataset events = h5::open_dataset(file, "events");
    h5::Datatype reference_type(
        h5::check(H5Tcreate(H5T_COMPOUND, sizeof(hdset_reg_ref_t)), "H5Tcreate"));
    h5::check(H5Tinsert(reference_type.get(), name.c_str(), 0, H5T_STD_REF_DSETREG), "H5Tinsert",
              name);

    ProductInfo info;
    info.name = name;
    h5::Object object = h5::open_object(file, name);
    if (H5Iget_type(object.get()) == H5I_GROUP) {
      describe_group(object.get(), info);
      info.scalar = is_scalar(object.get());
      return std::make_unique<V1Unsupported>(where, num_events, std::move(info),
                                             std::move(events), std::move(reference_type),
                                             h5::Dataset());
    }
    h5::Dataset data = h5::open_dataset(file, name);
    info.scalar = is_scalar(data.get());
    h5::Datatype type = h5::dataset_type(data.get());

    switch (H5Tget_class(type.get())) {
      case H5T_COMPOUND: {
        std::vector<h5::Datatype> file_types;
        describe_objects(data.get(), type.get(), info, file_types);
        return std::make_unique<V1Objects>(where, num_events, std::move(info), std::move(events),
                                           std::move(reference_type), std::move(data),
                                           std::move(file_types));
      }
      case H5T_STRING: {
        info.kind = ProductKind::String;
        std::optional<ColumnInfo> column = describe_type("", type.get());
        if (!column) throw Error("only variable-length strings are supported");
        info.columns.push_back(std::move(*column));
        return std::make_unique<V1String>(where, num_events, std::move(info), std::move(events),
                                          std::move(reference_type), std::move(data));
      }
      default:
        info.kind = ProductKind::Array;
        info.columns.push_back(describe_array_dataset("", data.get()));
        return std::make_unique<V1Array>(where, num_events, std::move(info), std::move(events),
                                         std::move(reference_type), std::move(data));
    }
  });
}

}  // namespace spine::core
