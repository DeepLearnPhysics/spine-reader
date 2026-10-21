#pragma once

// Every public entry point of FileSource and ProductSource runs its body through guarded():
// HDF5's own error printing is off while it runs, and any spine::Error that comes out says
// which file and product it concerns.

#include <string>
#include <utility>

#include <spine/error.hpp>

#include "h5/error.hpp"

namespace spine::core {

template <class Body>
decltype(auto) guarded(const std::string& where, Body&& body) {
  h5::ErrorSilencer silence;
  try {
    return std::forward<Body>(body)();
  } catch (const Error& error) {
    throw Error("spine-reader: " + where + ": " + error.what());
  }
}

}  // namespace spine::core
