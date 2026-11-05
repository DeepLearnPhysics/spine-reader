#pragma once

namespace spine {

/// Release number of the linked spine-reader library, "X.Y.Z": `project(VERSION)` in the
/// top-level CMakeLists.txt, which is the only place it is written. A release is the git tag
/// "vX.Y.Z" made from it; between releases, builds of main report the last release number.
/// The SPINE release the C++ model was generated from is `SPINE_READER_MODEL_SPINE_VERSION`
/// (spine/model.hpp), a different number.
const char* version() noexcept;

}  // namespace spine
