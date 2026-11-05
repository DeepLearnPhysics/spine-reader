#include <spine/version.hpp>

#ifndef SPINE_READER_VERSION_STRING
#error "SPINE_READER_VERSION_STRING must be defined by the build (CMakeLists.txt)"
#endif

namespace spine {

const char* version() noexcept { return SPINE_READER_VERSION_STRING; }

}  // namespace spine
