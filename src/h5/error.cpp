#include "h5/error.hpp"

#include <string>

namespace spine::h5 {
namespace {

/// With H5E_WALK_DOWNWARD the stack is visited from the API call that failed to the library
/// internals where the failure started, so the last description seen is the innermost one.
herr_t visit(unsigned, const H5E_error2_t* entry, void* data) {
  auto& innermost = *static_cast<std::string*>(data);
  try {
    innermost = entry->desc != nullptr ? entry->desc : "";
  } catch (...) {
    return -1;  // must not let an exception cross the C library
  }
  return 0;
}

}  // namespace

ErrorSilencer::ErrorSilencer() {
  H5Eget_auto2(H5E_DEFAULT, &function_, &data_);
  H5Eset_auto2(H5E_DEFAULT, nullptr, nullptr);
}

ErrorSilencer::~ErrorSilencer() { H5Eset_auto2(H5E_DEFAULT, function_, data_); }

void fail(std::string_view call, std::string_view object) {
  std::string innermost;
  H5Ewalk2(H5E_DEFAULT, H5E_WALK_DOWNWARD, visit, &innermost);
  std::string message(call);
  if (!object.empty()) message += " on '" + std::string(object) + "'";
  message += " failed";
  message += innermost.empty() ? " (no message on the HDF5 error stack)" : ": " + innermost;
  throw Error(message);
}

}  // namespace spine::h5
