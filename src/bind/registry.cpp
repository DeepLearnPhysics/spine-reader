#include "bind/registry.hpp"

#include <vector>

#include <spine/model.hpp>

namespace spine::bind {
namespace {

template <class T>
std::shared_ptr<const void> read_objects(const ProductBinding& binding, core::RowRange rows) {
  auto objects = std::make_shared<std::vector<T>>();
  binding.read(rows, *objects);
  return objects;
}

#define SPINE_READER_CLASS_ENTRY(Class) ClassEntry{&Class::descriptor, &read_objects<Class>},
const ClassEntry kClasses[] = {SPINE_READER_FOR_EACH_CLASS(SPINE_READER_CLASS_ENTRY)};
#undef SPINE_READER_CLASS_ENTRY

}  // namespace

const ClassEntry* find_class(std::string_view class_name) {
  // Older SPINE releases record ImageMeta3D under its alias Meta.
  if (class_name == "Meta") class_name = "ImageMeta3D";
  for (const ClassEntry& entry : kClasses) {
    if (entry.descriptor().name == class_name) return &entry;
  }
  return nullptr;
}

}  // namespace spine::bind
