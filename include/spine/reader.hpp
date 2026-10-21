#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <spine/event.hpp>
#include <spine/schema_report.hpp>

namespace spine {

struct ReaderOptions {
  /// Object products to read, by key. Empty: every product whose class the model knows.
  std::vector<std::string> products;
  /// Accept files SPINE has not finished writing (/info@complete == false).
  bool allow_incomplete = false;
  /// Throw when a product to read lacks model fields, instead of reporting it.
  bool strict = false;
};

/// Reads SPINE HDF5 files, V1 or V2, as one sequence of events.
///
/// All HDF5 access is serialised behind one library-wide lock, so a Reader may be used from
/// several threads, but reads do not run in parallel.
class Reader {
 public:
  explicit Reader(const std::string& path, ReaderOptions options = {});
  explicit Reader(const std::vector<std::string>& paths, ReaderOptions options = {});
  ~Reader();
  Reader(Reader&&) noexcept;
  Reader& operator=(Reader&&) noexcept;

  std::uint64_t num_events() const;
  /// Event `index`, counting across files in the order given. Throws if out of range.
  Event event(std::uint64_t index) const;

  std::size_t num_files() const;
  /// What was found in file `file`: layout, writer version, and how each product bound.
  const SchemaReport& schema_report(std::size_t file = 0) const;

  /// Event index for a (run, subrun, event) triple from run_info, or nothing. The first call
  /// reads run_info for every file. When a triple repeats, the first event wins.
  std::optional<std::uint64_t> find(std::int64_t run, std::int64_t subrun,
                                    std::int64_t event) const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace spine
