#pragma once

// Error capture for the HDF5 C API. Nothing here prints: failures become spine::Error.

#include <hdf5.h>

#include <string_view>

#include <spine/error.hpp>

namespace spine::h5 {

/// Switches HDF5's automatic error printing off for its lifetime and puts the previous setting
/// back afterwards. The host application may use HDF5 too, so this never changes the setting
/// for good. One is enough per public entry point.
class ErrorSilencer {
 public:
  ErrorSilencer();
  ~ErrorSilencer();
  ErrorSilencer(const ErrorSilencer&) = delete;
  ErrorSilencer& operator=(const ErrorSilencer&) = delete;

 private:
  H5E_auto2_t function_ = nullptr;
  void* data_ = nullptr;
};

/// Throws spine::Error for the HDF5 call `call` that has just failed. The message carries the
/// call, `object` (what it was applied to, may be empty) and the innermost message on HDF5's
/// error stack, found with H5Ewalk2.
[[noreturn]] void fail(std::string_view call, std::string_view object = {});

/// Returns `result` unless it is negative, which is how every HDF5 call reports an error
/// (hid_t, herr_t, htri_t and hssize_t alike).
template <class T>
T check(T result, std::string_view call, std::string_view object = {}) {
  if (result < 0) fail(call, object);
  return result;
}

}  // namespace spine::h5
