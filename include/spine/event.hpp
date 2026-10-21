#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

#include <spine/descriptor.hpp>
#include <spine/model.hpp>

namespace spine {

namespace detail {
struct EventData;
}

/// A reco-truth match: the matched object and SPINE's overlap score (IoU).
template <class T>
struct Match {
  const T* object;
  float overlap;
};

/// One event's SPINE objects. Cheap to copy; immutable; safe to share across threads.
///
/// Relations (particles of an interaction, matches) are resolved by SPINE id within the
/// event. Ids that point at nothing, such as a truth match in a file without truth
/// products, are skipped.
class Event {
 public:
  std::uint64_t index() const;     // position across all files of the Reader
  std::size_t file_index() const;  // which of the Reader's files
  std::uint64_t entry() const;     // event number within that file

  /// True when the file has this product and the Reader was asked to read it.
  bool has(std::string_view product) const;

  const std::vector<RecoParticle>& reco_particles() const {
    return objects<RecoParticle>("reco_particles");
  }
  const std::vector<RecoInteraction>& reco_interactions() const {
    return objects<RecoInteraction>("reco_interactions");
  }
  const std::vector<TruthParticle>& truth_particles() const {
    return objects<TruthParticle>("truth_particles");
  }
  const std::vector<TruthInteraction>& truth_interactions() const {
    return objects<TruthInteraction>("truth_interactions");
  }
  /// nullptr when the file has no run_info.
  const RunInfo* run_info() const;
  /// nullptr when the file has no meta.
  const ImageMeta3D* meta() const;

  /// Objects of any product, by key. Empty when the product is absent. Throws spine::Error
  /// if the product holds a class other than T.
  template <class T>
  const std::vector<T>& objects(std::string_view product) const {
    static const std::vector<T> none;
    const void* found = find_objects(product, T::descriptor());
    return found != nullptr ? *static_cast<const std::vector<T>*>(found) : none;
  }

  std::vector<const RecoParticle*> particles_of(const RecoInteraction& interaction) const;
  std::vector<const TruthParticle*> particles_of(const TruthInteraction& interaction) const;
  const RecoInteraction* interaction_of(const RecoParticle& particle) const;
  const TruthInteraction* interaction_of(const TruthParticle& particle) const;

  /// Matches in SPINE's order: best overlap first.
  std::vector<Match<TruthParticle>> matches(const RecoParticle& particle) const;
  std::vector<Match<RecoParticle>> matches(const TruthParticle& particle) const;
  std::vector<Match<TruthInteraction>> matches(const RecoInteraction& interaction) const;
  std::vector<Match<RecoInteraction>> matches(const TruthInteraction& interaction) const;

 private:
  friend class Reader;
  explicit Event(std::shared_ptr<const detail::EventData> data);
  const void* find_objects(std::string_view product, const ClassDescriptor& cls) const;

  std::shared_ptr<const detail::EventData> data_;
};

}  // namespace spine
