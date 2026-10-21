#pragma once

// RAII owners for HDF5 identifiers. Each handle type closes with its own H5*close; a handle
// that was moved from or never set holds H5I_INVALID_HID and closes nothing. Closing is
// part of the HDF5 API, so, like every other HDF5 call, it needs core::hdf5_mutex().

#include <hdf5.h>

#include <utility>

namespace spine::h5 {

struct FileClose {
  static herr_t close(hid_t id) { return H5Fclose(id); }
};
struct GroupClose {
  static herr_t close(hid_t id) { return H5Gclose(id); }
};
struct DatasetClose {
  static herr_t close(hid_t id) { return H5Dclose(id); }
};
struct ObjectClose {
  static herr_t close(hid_t id) { return H5Oclose(id); }
};
struct DatatypeClose {
  static herr_t close(hid_t id) { return H5Tclose(id); }
};
struct DataspaceClose {
  static herr_t close(hid_t id) { return H5Sclose(id); }
};
struct AttributeClose {
  static herr_t close(hid_t id) { return H5Aclose(id); }
};

/// Owns one identifier. Build it from the result of an H5*open/create/get call after checking
/// that result (see check() in error.hpp); the usual way is through the factories in util.hpp.
template <class Closer>
class Handle {
 public:
  Handle() = default;
  explicit Handle(hid_t id) noexcept : id_(id) {}
  Handle(const Handle&) = delete;
  Handle& operator=(const Handle&) = delete;
  Handle(Handle&& other) noexcept : id_(std::exchange(other.id_, H5I_INVALID_HID)) {}
  Handle& operator=(Handle&& other) noexcept {
    if (this != &other) {
      reset();
      id_ = std::exchange(other.id_, H5I_INVALID_HID);
    }
    return *this;
  }
  ~Handle() { reset(); }

  hid_t get() const noexcept { return id_; }
  explicit operator bool() const noexcept { return id_ >= 0; }

  void reset() noexcept {
    if (id_ >= 0) Closer::close(id_);  // a failing close cannot be reported from here
    id_ = H5I_INVALID_HID;
  }

 private:
  hid_t id_ = H5I_INVALID_HID;
};

using File = Handle<FileClose>;
using Group = Handle<GroupClose>;
using Dataset = Handle<DatasetClose>;
using Object = Handle<ObjectClose>;  // a group or dataset opened without knowing which
using Datatype = Handle<DatatypeClose>;
using Dataspace = Handle<DataspaceClose>;
using Attribute = Handle<AttributeClose>;

}  // namespace spine::h5
