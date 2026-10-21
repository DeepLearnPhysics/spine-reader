#pragma once

// Small helpers over the HDF5 C API: opening objects, listing children, reading attributes.
// Every function throws spine::Error on failure and assumes core::hdf5_mutex() is held.

#include <cstdint>
#include <string>
#include <vector>

#include "h5/handle.hpp"

namespace spine::h5 {

/// Opens a file read-only.
File open_file(const std::string& path);
/// Opens the object `name` (a link in `parent`), whatever it is.
Object open_object(hid_t parent, const std::string& name);
Group open_group(hid_t parent, const std::string& name);
Dataset open_dataset(hid_t parent, const std::string& name);

/// H5I_GROUP or H5I_DATASET for the object `name` in `parent`; other kinds are not told apart.
H5I_type_t object_kind(hid_t parent, const std::string& name);
/// True if `parent` has a link called `name` (a single path component).
bool has_link(hid_t parent, const std::string& name);

Datatype dataset_type(hid_t dataset);
Dataspace dataset_space(hid_t dataset);
Datatype copy_type(hid_t type);
Datatype member_type(hid_t compound, unsigned index);
std::string member_name(hid_t compound, unsigned index);

/// Extent of a simple dataspace, one entry per dimension.
std::vector<hsize_t> extent(hid_t space);

/// Names of the links of `group`, in creation order if the group tracks it and in name order
/// otherwise (h5py, which SPINE writes with, does not track creation order by default).
/// `*by_creation_order` tells which one was used.
std::vector<std::string> child_names(hid_t group, bool* by_creation_order = nullptr);

bool has_attribute(hid_t object, const std::string& name);
/// Integer attribute; enums (SPINE's bool) read as their integer value. Must hold one element.
std::int64_t read_int_attribute(hid_t object, const std::string& name);
/// String attribute, variable-length (UTF-8 or ASCII) or fixed-length. Must hold one element.
std::string read_string_attribute(hid_t object, const std::string& name);

/// Frees what H5Dread/H5Aread allocated for the VLEN and variable-length string elements of
/// `buffer` (H5Treclaim). Create it before the read, on a buffer filled with zeros, so that
/// it is safe whatever happens in between, and destroy it before `type` and `space`.
/// `space` must describe the buffer: the memory dataspace used for the read.
class Reclaim {
 public:
  Reclaim(hid_t type, hid_t space, void* buffer) noexcept
      : type_(type), space_(space), buffer_(buffer) {}
  Reclaim(const Reclaim&) = delete;
  Reclaim& operator=(const Reclaim&) = delete;
  ~Reclaim() { H5Treclaim(type_, space_, H5P_DEFAULT, buffer_); }

 private:
  hid_t type_;
  hid_t space_;
  void* buffer_;
};

}  // namespace spine::h5
