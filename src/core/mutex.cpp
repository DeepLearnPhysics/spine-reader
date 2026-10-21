#include "core/source.hpp"

namespace spine::core {

std::recursive_mutex& hdf5_mutex() {
  static std::recursive_mutex mutex;  // function-local: safe to use during static initialisation
  return mutex;
}

}  // namespace spine::core
