#include <spine/reader.hpp>

#include <algorithm>
#include <map>
#include <mutex>
#include <tuple>

#include <spine/error.hpp>

#include "bind/binder.hpp"
#include "bind/registry.hpp"
#include "core/source.hpp"
#include "event_data.hpp"

namespace spine {
namespace {

struct BoundProduct {
  std::string key;
  const bind::ClassEntry* entry;
  std::unique_ptr<bind::ProductBinding> binding;
};

struct OpenFile {
  std::unique_ptr<core::FileSource> source;
  std::vector<BoundProduct> products;
  SchemaReport report;
  std::uint64_t first_event = 0;  // index of its first event across all files
};

bool wanted(const ReaderOptions& options, const std::string& key) {
  return options.products.empty() ||
         std::find(options.products.begin(), options.products.end(), key) !=
             options.products.end();
}

OpenFile open_file(const std::string& path, const ReaderOptions& options) {
  OpenFile file;
  core::OpenOptions open_options;
  open_options.allow_incomplete = options.allow_incomplete;
  file.source = core::FileSource::open(path, open_options);

  const core::FileInfo& info = file.source->info();
  file.report.path = path;
  file.report.spine_version = info.spine_version;
  file.report.format_version = info.format_version;
  file.report.layout = std::string(core::to_string(info.layout));
  file.report.model_version = std::string(RecoParticle::descriptor().schema_version);

  for (const std::string& key : file.source->product_names()) {
    if (!wanted(options, key)) continue;
    core::ProductSource& product = file.source->product(key);
    const core::ProductInfo& product_info = product.info();
    if (product_info.kind != core::ProductKind::Objects) {
      file.report.skipped.push_back(key + ": " + std::string(core::to_string(product_info.kind)) +
                                    " product");
      continue;
    }
    const bind::ClassEntry* entry = bind::find_class(product_info.class_name);
    if (entry == nullptr) {
      file.report.skipped.push_back(key + ": no model class " + product_info.class_name);
      continue;
    }
    auto binding = std::make_unique<bind::ProductBinding>(entry->descriptor(), product);
    file.report.products.push_back(binding->report());
    if (options.strict && !binding->report().complete()) {
      throw Error("spine-reader: " + path + ": product " + key +
                  " lacks model fields\n" + file.report.summary());
    }
    file.products.push_back({key, entry, std::move(binding)});
  }
  return file;
}

using RunKey = std::tuple<std::int64_t, std::int64_t, std::int64_t>;

}  // namespace

struct Reader::Impl {
  ReaderOptions options;
  std::vector<OpenFile> files;
  std::uint64_t num_events = 0;
  std::optional<std::map<RunKey, std::uint64_t>> run_index;  // built by find()

  void build_run_index() {
    std::map<RunKey, std::uint64_t> index;
    for (OpenFile& file : files) {
      if (!file.source->has_product("run_info")) {
        throw Error("spine-reader: " + file.report.path + " has no run_info product");
      }
      core::ProductSource& product = file.source->product("run_info");
      bind::ProductBinding binding(RunInfo::descriptor(), product);
      std::vector<RunInfo> runs;
      binding.read(core::RowRange{0, product.num_rows()}, runs);
      for (std::size_t entry = 0; entry < runs.size(); ++entry) {
        const RunInfo& run = runs[entry];
        index.emplace(RunKey{run.run, run.subrun, run.event}, file.first_event + entry);
      }
    }
    run_index = std::move(index);
  }
};

Reader::Reader(const std::string& path, ReaderOptions options)
    : Reader(std::vector<std::string>{path}, std::move(options)) {}

Reader::Reader(const std::vector<std::string>& paths, ReaderOptions options)
    : impl_(std::make_unique<Impl>()) {
  std::lock_guard<std::recursive_mutex> lock(core::hdf5_mutex());
  impl_->options = std::move(options);
  for (const std::string& path : paths) {
    OpenFile file = open_file(path, impl_->options);
    file.first_event = impl_->num_events;
    impl_->num_events += file.source->info().num_events;
    impl_->files.push_back(std::move(file));
  }
}

Reader::~Reader() {
  if (!impl_) return;
  std::lock_guard<std::recursive_mutex> lock(core::hdf5_mutex());
  impl_.reset();
}

Reader::Reader(Reader&&) noexcept = default;
Reader& Reader::operator=(Reader&&) noexcept = default;

std::uint64_t Reader::num_events() const { return impl_->num_events; }

std::size_t Reader::num_files() const { return impl_->files.size(); }

const SchemaReport& Reader::schema_report(std::size_t file) const {
  return impl_->files.at(file).report;
}

Event Reader::event(std::uint64_t index) const {
  if (index >= impl_->num_events) {
    throw Error("spine-reader: event " + std::to_string(index) + " is out of range (" +
                std::to_string(impl_->num_events) + " events)");
  }
  std::lock_guard<std::recursive_mutex> lock(core::hdf5_mutex());
  const auto after = std::upper_bound(
      impl_->files.begin(), impl_->files.end(), index,
      [](std::uint64_t i, const OpenFile& file) { return i < file.first_event; });
  const std::size_t file_index = static_cast<std::size_t>(after - impl_->files.begin()) - 1;
  OpenFile& file = impl_->files[file_index];

  auto data = std::make_shared<detail::EventData>();
  data->index = index;
  data->file = file_index;
  data->entry = index - file.first_event;
  for (const BoundProduct& product : file.products) {
    const core::RowRange rows = product.binding->source().rows(data->entry);
    data->products.emplace(product.key, detail::EventData::Product{
                                            &product.entry->descriptor(),
                                            product.entry->read(*product.binding, rows)});
  }
  return Event(std::move(data));
}

std::optional<std::uint64_t> Reader::find(std::int64_t run, std::int64_t subrun,
                                          std::int64_t event) const {
  std::lock_guard<std::recursive_mutex> lock(core::hdf5_mutex());
  if (!impl_->run_index) impl_->build_run_index();
  const auto it = impl_->run_index->find(RunKey{run, subrun, event});
  if (it == impl_->run_index->end()) return std::nullopt;
  return it->second;
}

}  // namespace spine
