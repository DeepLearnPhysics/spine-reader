#include "h5/util.hpp"

#include <algorithm>
#include <cstring>

#include "h5/error.hpp"

namespace spine::h5 {

File open_file(const std::string& path) {
  return File(check(H5Fopen(path.c_str(), H5F_ACC_RDONLY, H5P_DEFAULT), "H5Fopen", path));
}

Object open_object(hid_t parent, const std::string& name) {
  return Object(check(H5Oopen(parent, name.c_str(), H5P_DEFAULT), "H5Oopen", name));
}

Group open_group(hid_t parent, const std::string& name) {
  return Group(check(H5Gopen2(parent, name.c_str(), H5P_DEFAULT), "H5Gopen2", name));
}

Dataset open_dataset(hid_t parent, const std::string& name) {
  return Dataset(check(H5Dopen2(parent, name.c_str(), H5P_DEFAULT), "H5Dopen2", name));
}

H5I_type_t object_kind(hid_t parent, const std::string& name) {
  Object object = open_object(parent, name);
  return H5Iget_type(object.get());
}

bool has_link(hid_t parent, const std::string& name) {
  return check(H5Lexists(parent, name.c_str(), H5P_DEFAULT), "H5Lexists", name) > 0;
}

Datatype dataset_type(hid_t dataset) {
  return Datatype(check(H5Dget_type(dataset), "H5Dget_type"));
}

Dataspace dataset_space(hid_t dataset) {
  return Dataspace(check(H5Dget_space(dataset), "H5Dget_space"));
}

Datatype copy_type(hid_t type) { return Datatype(check(H5Tcopy(type), "H5Tcopy")); }

Datatype member_type(hid_t compound, unsigned index) {
  return Datatype(check(H5Tget_member_type(compound, index), "H5Tget_member_type"));
}

std::string member_name(hid_t compound, unsigned index) {
  char* raw = H5Tget_member_name(compound, index);
  if (raw == nullptr) fail("H5Tget_member_name");
  std::string name(raw);
  H5free_memory(raw);
  return name;
}

std::vector<hsize_t> extent(hid_t space) {
  const int rank = check(H5Sget_simple_extent_ndims(space), "H5Sget_simple_extent_ndims");
  std::vector<hsize_t> dims(static_cast<std::size_t>(rank));
  if (rank > 0) {
    check(H5Sget_simple_extent_dims(space, dims.data(), nullptr), "H5Sget_simple_extent_dims");
  }
  return dims;
}

namespace {

herr_t collect_name(hid_t, const char* name, const H5L_info2_t*, void* data) {
  try {
    static_cast<std::vector<std::string>*>(data)->push_back(name);
  } catch (...) {
    return -1;  // must not let an exception cross the C library
  }
  return 0;
}

}  // namespace

std::vector<std::string> child_names(hid_t group, bool* by_creation_order) {
  unsigned tracking = 0;
  {
    const hid_t plist = check(H5Gget_create_plist(group), "H5Gget_create_plist");
    const herr_t status = H5Pget_link_creation_order(plist, &tracking);
    H5Pclose(plist);
    check(status, "H5Pget_link_creation_order");
  }
  const bool creation = (tracking & H5P_CRT_ORDER_TRACKED) != 0;
  if (by_creation_order != nullptr) *by_creation_order = creation;

  std::vector<std::string> names;
  hsize_t position = 0;
  check(H5Literate2(group, creation ? H5_INDEX_CRT_ORDER : H5_INDEX_NAME, H5_ITER_INC, &position,
                    collect_name, &names),
        "H5Literate2");
  return names;
}

bool has_attribute(hid_t object, const std::string& name) {
  return check(H5Aexists(object, name.c_str()), "H5Aexists", name) > 0;
}

namespace {

Attribute open_attribute(hid_t object, const std::string& name) {
  return Attribute(check(H5Aopen(object, name.c_str(), H5P_DEFAULT), "H5Aopen", name));
}

/// An attribute holds one value if its dataspace is scalar or has a single element.
void require_one_element(hid_t attribute, const std::string& name) {
  Dataspace space(check(H5Aget_space(attribute), "H5Aget_space", name));
  if (check(H5Sget_simple_extent_npoints(space.get()), "H5Sget_simple_extent_npoints", name) != 1) {
    throw Error("attribute '" + name + "' does not hold exactly one value");
  }
}

}  // namespace

std::int64_t read_int_attribute(hid_t object, const std::string& name) {
  Attribute attribute = open_attribute(object, name);
  Datatype type(check(H5Aget_type(attribute.get()), "H5Aget_type", name));
  const H5T_class_t type_class = H5Tget_class(type.get());
  if (type_class != H5T_INTEGER && type_class != H5T_ENUM) {
    throw Error("attribute '" + name + "' is not an integer");
  }
  require_one_element(attribute.get(), name);
  std::int64_t value = 0;
  check(H5Aread(attribute.get(), H5T_NATIVE_INT64, &value), "H5Aread", name);
  return value;
}

std::string read_string_attribute(hid_t object, const std::string& name) {
  Attribute attribute = open_attribute(object, name);
  Datatype type(check(H5Aget_type(attribute.get()), "H5Aget_type", name));
  if (H5Tget_class(type.get()) != H5T_STRING) {
    throw Error("attribute '" + name + "' is not a string");
  }
  require_one_element(attribute.get(), name);

  // The file's own type is the memory type. H5T_C_S1 is ASCII, and HDF5 finds no conversion
  // path from the UTF-8 strings SPINE writes to it.
  Datatype memory = copy_type(type.get());
  if (check(H5Tis_variable_str(type.get()), "H5Tis_variable_str", name) > 0) {
    Dataspace one(check(H5Screate(H5S_SCALAR), "H5Screate"));
    char* text = nullptr;
    Reclaim reclaim(memory.get(), one.get(), &text);
    check(H5Aread(attribute.get(), memory.get(), &text), "H5Aread", name);
    return text != nullptr ? std::string(text) : std::string();
  }

  std::string text(H5Tget_size(type.get()), '\0');
  check(H5Aread(attribute.get(), memory.get(), text.data()), "H5Aread", name);
  if (H5Tget_strpad(type.get()) == H5T_STR_SPACEPAD) {
    text.erase(text.find_last_not_of(' ') + 1);
  } else {
    text.erase(std::find(text.begin(), text.end(), '\0'), text.end());
  }
  return text;
}

}  // namespace spine::h5
