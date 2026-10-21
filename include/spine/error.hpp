#pragma once

#include <stdexcept>
#include <string>

namespace spine {

/// Every failure reported by spine-reader. The message names the file, the product and,
/// when the failure came from HDF5, the innermost HDF5 error description.
class Error : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

}  // namespace spine
