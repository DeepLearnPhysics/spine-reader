#include <spine/event.hpp>

#include <cstdint>
#include <limits>
#include <string>

#include <spine/error.hpp>

#include "event_data.hpp"

namespace spine {
namespace {

// SPINE ids equal list positions today, so try the position first and fall back to a scan.
template <class T, class Id>
const T* by_id(const std::vector<T>& objects, Id id) {
  const auto wanted = static_cast<std::int64_t>(id);
  const auto size = static_cast<std::int64_t>(objects.size());
  if (wanted >= 0 && wanted < size &&
      static_cast<std::int64_t>(objects[static_cast<std::size_t>(wanted)].id) == wanted) {
    return &objects[static_cast<std::size_t>(wanted)];
  }
  for (const T& object : objects) {
    if (static_cast<std::int64_t>(object.id) == wanted) return &object;
  }
  return nullptr;
}

template <class T, class Ids>
std::vector<const T*> all_by_id(const std::vector<T>& objects, const Ids& ids) {
  std::vector<const T*> found;
  found.reserve(ids.size());
  for (const auto id : ids) {
    if (const T* object = by_id(objects, id)) found.push_back(object);
  }
  return found;
}

template <class T, class Source>
std::vector<Match<T>> matches_in(const std::vector<T>& targets, const Source& source) {
  std::vector<Match<T>> found;
  for (std::size_t k = 0; k < source.match_ids.size(); ++k) {
    const T* target = by_id(targets, source.match_ids[k]);
    if (target == nullptr) continue;
    const float overlap = k < source.match_overlaps.size()
                              ? static_cast<float>(source.match_overlaps[k])
                              : std::numeric_limits<float>::quiet_NaN();
    found.push_back({target, overlap});
  }
  return found;
}

}  // namespace

Event::Event(std::shared_ptr<const detail::EventData> data) : data_(std::move(data)) {}

std::uint64_t Event::index() const { return data_->index; }
std::size_t Event::file_index() const { return data_->file; }
std::uint64_t Event::entry() const { return data_->entry; }

bool Event::has(std::string_view product) const {
  return data_->products.find(product) != data_->products.end();
}

const void* Event::find_objects(std::string_view product, const ClassDescriptor& cls) const {
  const auto it = data_->products.find(product);
  if (it == data_->products.end()) return nullptr;
  if (it->second.cls != &cls) {
    throw Error("spine-reader: product '" + std::string(product) + "' holds " +
                std::string(it->second.cls->name) + ", not " + std::string(cls.name));
  }
  return it->second.objects.get();
}

const RunInfo* Event::run_info() const {
  const std::vector<RunInfo>& all = objects<RunInfo>("run_info");
  return all.empty() ? nullptr : &all.front();
}

const ImageMeta3D* Event::meta() const {
  const std::vector<ImageMeta3D>& all = objects<ImageMeta3D>("meta");
  return all.empty() ? nullptr : &all.front();
}

std::vector<const RecoParticle*> Event::particles_of(const RecoInteraction& interaction) const {
  return all_by_id(reco_particles(), interaction.particle_ids);
}

std::vector<const TruthParticle*> Event::particles_of(const TruthInteraction& interaction) const {
  return all_by_id(truth_particles(), interaction.particle_ids);
}

const RecoInteraction* Event::interaction_of(const RecoParticle& particle) const {
  return by_id(reco_interactions(), particle.interaction_id);
}

const TruthInteraction* Event::interaction_of(const TruthParticle& particle) const {
  return by_id(truth_interactions(), particle.interaction_id);
}

std::vector<Match<TruthParticle>> Event::matches(const RecoParticle& particle) const {
  return matches_in(truth_particles(), particle);
}

std::vector<Match<RecoParticle>> Event::matches(const TruthParticle& particle) const {
  return matches_in(reco_particles(), particle);
}

std::vector<Match<TruthInteraction>> Event::matches(const RecoInteraction& interaction) const {
  return matches_in(truth_interactions(), interaction);
}

std::vector<Match<RecoInteraction>> Event::matches(const TruthInteraction& interaction) const {
  return matches_in(reco_interactions(), interaction);
}

}  // namespace spine
