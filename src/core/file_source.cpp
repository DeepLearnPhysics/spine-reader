// FileSource::open: layout detection and product discovery.

#include <algorithm>
#include <map>

#include <spine/error.hpp>

#include "core/guard.hpp"
#include "core/products.hpp"
#include "core/source.hpp"
#include "h5/error.hpp"
#include "h5/util.hpp"

namespace spine::core {
namespace {

std::string optional_string(hid_t object, const char* name) {
  return h5::has_attribute(object, name) ? h5::read_string_attribute(object, name) : std::string();
}

/// Names of the products of a V1 file: the members of the `events` compound, in its order.
std::vector<std::string> v1_product_names(hid_t events, std::uint64_t& num_events) {
  h5::Datatype type = h5::dataset_type(events);
  if (H5Tget_class(type.get()) != H5T_COMPOUND) throw Error("'events' is not a compound dataset");
  h5::Dataspace space = h5::dataset_space(events);
  const std::vector<hsize_t> dims = h5::extent(space.get());
  if (dims.size() != 1) throw Error("'events' is not one-dimensional");
  num_events = dims[0];
  std::vector<std::string> names;
  const int members = h5::check(H5Tget_nmembers(type.get()), "H5Tget_nmembers");
  for (int m = 0; m < members; ++m) {
    names.push_back(h5::member_name(type.get(), static_cast<unsigned>(m)));
  }
  return names;
}

/// Names of the products of a V2 file: the children of `root` that are groups carrying a `kind`
/// attribute, in `root`'s creation order if it tracks one and in name order if not. (SPINE
/// writes through h5py, which does not track creation order, so in practice: name order.)
/// The `kind` test is what tells products from `info` and `events` when `root` is the file root.
std::vector<std::string> v2_product_names(hid_t root) {
  std::vector<std::string> names;
  for (const std::string& child : h5::child_names(root)) {
    h5::Object object = h5::open_object(root, child);
    if (H5Iget_type(object.get()) == H5I_GROUP && h5::has_attribute(object.get(), "kind")) {
      names.push_back(child);
    }
  }
  return names;
}

class FileImpl final : public FileSource {
 public:
  FileImpl(FileInfo info, h5::File file, h5::Group products_root, std::vector<std::string> names)
      : info_(std::move(info)),
        file_(std::move(file)),
        products_root_(std::move(products_root)),
        names_(std::move(names)) {}

  const FileInfo& info() const override { return info_; }
  const std::vector<std::string>& product_names() const override { return names_; }
  bool has_product(std::string_view name) const override {
    return std::find(names_.begin(), names_.end(), name) != names_.end();
  }

  ProductSource& product(std::string_view name) override {
    const auto found = opened_.find(name);
    if (found != opened_.end()) return *found->second;
    if (!has_product(name)) {
      throw Error("spine-reader: " + info_.path + ": no product '" + std::string(name) + "'");
    }
    const std::string key(name);
    std::unique_ptr<ProductSource> source =
        info_.layout == Layout::V1
            ? open_v1_product(info_.path, file_.get(), key, info_.num_events)
            : open_v2_product(info_.path, products_root_.get(), key, info_.num_events);
    return *opened_.emplace(key, std::move(source)).first->second;
  }

 private:
  FileInfo info_;
  h5::File file_;
  h5::Group products_root_;  // V2 only
  std::vector<std::string> names_;
  std::map<std::string, std::unique_ptr<ProductSource>, std::less<>> opened_;
};

}  // namespace

std::unique_ptr<FileSource> FileSource::open(const std::string& path, const OpenOptions& options) {
  return guarded(path, [&]() -> std::unique_ptr<FileSource> {
    FileInfo info;
    info.path = path;
    h5::File file = h5::open_file(path);
    if (!h5::has_link(file.get(), "info") || !h5::has_link(file.get(), "events")) {
      throw Error("not a SPINE output file: it lacks /info or /events");
    }

    // `info` is a group, or an empty dataset in files written before SPINE v0.11.0.
    h5::Object node = h5::open_object(file.get(), "info");
    const bool has_version = h5::has_attribute(node.get(), "format_version");
    if (has_version) {
      info.format_version = static_cast<int>(h5::read_int_attribute(node.get(), "format_version"));
    }
    info.spine_version = h5::has_attribute(node.get(), "spine_version")
                             ? h5::read_string_attribute(node.get(), "spine_version")
                             : optional_string(node.get(), "version");
    info.complete = !h5::has_attribute(node.get(), "complete") ||
                    h5::read_int_attribute(node.get(), "complete") != 0;
    info.cfg = optional_string(node.get(), "cfg");
    const std::string format = optional_string(node.get(), "format");

    if (format == "cache") {
      throw Error("this is a SPINE cache shard (/info@format is 'cache'), not an output file");
    }
    if (info.format_version != 1 && info.format_version != 2) {
      throw Error("unsupported format_version " + std::to_string(info.format_version));
    }
    if (!info.complete && !options.allow_incomplete) {
      throw Error("the file is incomplete (/info@complete is false): SPINE was still writing it, "
                  "or died; set allow_incomplete to read it anyway");
    }

    // A V1 file may hold a product called "products", so /products only counts when the file
    // does not say it is V1.
    const bool products_group = h5::has_link(file.get(), "products") &&
                                h5::object_kind(file.get(), "products") == H5I_GROUP;
    if (products_group && (!has_version || info.format_version == 2)) {
      info.layout = Layout::V2Products;
    } else if (info.format_version == 2) {
      info.layout = Layout::V2Root;
    } else {
      info.layout = Layout::V1;
    }

    h5::Dataset events = h5::open_dataset(file.get(), "events");
    std::vector<std::string> names;
    h5::Group products_root;
    if (info.layout == Layout::V1) {
      names = v1_product_names(events.get(), info.num_events);
    } else {
      h5::Dataspace space = h5::dataset_space(events.get());
      const std::vector<hsize_t> dims = h5::extent(space.get());
      if (dims.size() != 1) throw Error("'events' is not one-dimensional");
      info.num_events = dims[0];
      products_root =
          h5::open_group(file.get(), info.layout == Layout::V2Products ? "products" : "/");
      names = v2_product_names(products_root.get());
    }
    return std::make_unique<FileImpl>(std::move(info), std::move(file), std::move(products_root),
                                      std::move(names));
  });
}

}  // namespace spine::core
