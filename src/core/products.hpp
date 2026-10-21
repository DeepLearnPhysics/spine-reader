#pragma once

// Factories for the per-layout ProductSource implementations (v1_product.cpp, v2_product.cpp).
// Both assume core::hdf5_mutex() is held, run under guarded() and name the file and product in
// every error.

#include <cstdint>
#include <memory>
#include <string>

#include <hdf5.h>

#include "core/source.hpp"

namespace spine::core {

/// Product `name` of a V1 file: a dataset or group at the root of `file`, with rows located
/// through the region references of the `events` dataset (`num_events` rows).
std::unique_ptr<ProductSource> open_v1_product(const std::string& path, hid_t file,
                                               const std::string& name, std::uint64_t num_events);

/// Product `name` of a V2 file: a group of `products_root` (/products, or / in SPINE v0.16.x).
std::unique_ptr<ProductSource> open_v2_product(const std::string& path, hid_t products_root,
                                               const std::string& name, std::uint64_t num_events);

}  // namespace spine::core
