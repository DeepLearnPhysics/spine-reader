// The RAII layer over the HDF5 C API (src/h5) and the small parsers in src/core.

#include <doctest.h>

#include <string>

#include "core/yaml_list.hpp"
#include "core_test_util.hpp"
#include "h5/error.hpp"
#include "h5/handle.hpp"
#include "h5/util.hpp"

using namespace spine;
using namespace core_test;

namespace {

bool contains(const std::string& text, const std::string& part) {
  return text.find(part) != std::string::npos;
}

}  // namespace

TEST_CASE("h5 handles close what they own, and only once") {
  Hdf5Lock lock;
  hid_t raw = H5I_INVALID_HID;
  {
    h5::File file = h5::open_file(data_path("spine_v2_events.h5"));
    raw = file.get();
    CHECK(H5Iis_valid(raw) > 0);
    h5::File moved = std::move(file);
    CHECK_FALSE(file);  // NOLINT: moved-from handles are specified to be empty
    CHECK(moved.get() == raw);
    h5::File assigned;
    assigned = std::move(moved);
    CHECK(assigned.get() == raw);
    CHECK(H5Iis_valid(raw) > 0);
  }
  CHECK(H5Iis_valid(raw) == 0);
}

TEST_CASE("h5 errors name the call, the object and the innermost HDF5 message") {
  Hdf5Lock lock;
  h5::ErrorSilencer silence;

  const std::string missing_file = data_path("no_such_file.h5");
  try {
    h5::open_file(missing_file);
    FAIL("opening a missing file must throw");
  } catch (const Error& error) {
    const std::string text = error.what();
    CHECK(contains(text, "H5Fopen"));
    CHECK(contains(text, missing_file));
    CHECK(contains(text, "unable to open file"));  // the innermost message of the HDF5 stack
    CHECK(contains(text, "No such file or directory"));
  }

  h5::File file = h5::open_file(data_path("spine_v2_events.h5"));
  try {
    h5::open_dataset(file.get(), "no_such_dataset");
    FAIL("opening a missing dataset must throw");
  } catch (const Error& error) {
    CHECK(contains(error.what(), "H5Dopen2"));
    CHECK(contains(error.what(), "no_such_dataset"));
  }
}

namespace {
int printed = 0;
herr_t counting_handler(hid_t, void*) {
  ++printed;
  return 0;
}
int marker = 0;
}  // namespace

TEST_CASE("the error silencer restores the host's automatic error printing") {
  Hdf5Lock lock;
  H5E_auto2_t before_function = nullptr;
  void* before_data = nullptr;
  H5Eget_auto2(H5E_DEFAULT, &before_function, &before_data);
  H5Eset_auto2(H5E_DEFAULT, counting_handler, &marker);  // the host's own setting

  {
    h5::ErrorSilencer outer;
    H5E_auto2_t function = counting_handler;
    void* data = &marker;
    H5Eget_auto2(H5E_DEFAULT, &function, &data);
    CHECK(function == nullptr);  // printing is off inside

    {
      h5::ErrorSilencer inner;  // nesting is fine
      CHECK_THROWS_AS(h5::open_file(data_path("no_such_file.h5")), Error);
    }
    H5Eget_auto2(H5E_DEFAULT, &function, &data);
    CHECK(function == nullptr);
    CHECK_THROWS_AS(h5::open_file(data_path("no_such_file.h5")), Error);
  }
  CHECK(printed == 0);  // nothing was printed while it was off

  H5E_auto2_t after_function = nullptr;
  void* after_data = nullptr;
  H5Eget_auto2(H5E_DEFAULT, &after_function, &after_data);
  CHECK(after_function == counting_handler);
  CHECK(after_data == &marker);

  H5Eset_auto2(H5E_DEFAULT, before_function, before_data);
}

TEST_CASE("h5 attributes: integers, enums and strings of every kind") {
  Hdf5Lock lock;
  h5::ErrorSilencer silence;
  h5::File file = h5::open_file(data_path("edge_attributes.h5"));
  const hid_t root = file.get();

  CHECK(h5::has_attribute(root, "int64"));
  CHECK_FALSE(h5::has_attribute(root, "absent"));

  CHECK(h5::read_int_attribute(root, "int64") == -5);
  CHECK(h5::read_int_attribute(root, "int8") == 7);
  CHECK(h5::read_int_attribute(root, "uint8") == 200);
  CHECK(h5::read_int_attribute(root, "bool_true") == 1);  // SPINE's {FALSE, TRUE} enum
  CHECK(h5::read_int_attribute(root, "bool_false") == 0);
  CHECK_THROWS_AS(h5::read_int_attribute(root, "float"), Error);
  CHECK_THROWS_AS(h5::read_int_attribute(root, "vlen_utf8"), Error);
  CHECK_THROWS_AS(h5::read_int_attribute(root, "two_ints"), Error);
  CHECK_THROWS_AS(h5::read_int_attribute(root, "absent"), Error);

  CHECK(h5::read_string_attribute(root, "vlen_utf8") == "h\xc3\xa9llo \xc2\xb5");  // "héllo µ"
  CHECK(h5::read_string_attribute(root, "vlen_ascii") == "plain");
  CHECK(h5::read_string_attribute(root, "fixed_null") == "abc");   // null-padded to 8
  CHECK(h5::read_string_attribute(root, "fixed_exact") == "abcd");  // no terminator at all
  CHECK(h5::read_string_attribute(root, "empty_text").empty());
  CHECK_THROWS_AS(h5::read_string_attribute(root, "int64"), Error);
  CHECK_THROWS_AS(h5::read_string_attribute(root, "absent"), Error);

  // Attributes of a dataset read the same way.
  h5::Dataset data = h5::open_dataset(root, "data");
  CHECK(h5::read_int_attribute(data.get(), "on_dataset") == 42);
}

TEST_CASE("h5 listings and object kinds") {
  Hdf5Lock lock;
  h5::ErrorSilencer silence;
  h5::File file = h5::open_file(data_path("spine_v2_events.h5"));
  CHECK(h5::has_link(file.get(), "events"));
  CHECK_FALSE(h5::has_link(file.get(), "reco_particles"));
  CHECK(h5::object_kind(file.get(), "products") == H5I_GROUP);
  CHECK(h5::object_kind(file.get(), "events") == H5I_DATASET);

  h5::Group products = h5::open_group(file.get(), "products");
  bool by_creation = true;
  const std::vector<std::string> names = h5::child_names(products.get(), &by_creation);
  CHECK_FALSE(by_creation);  // h5py does not track creation order: name order it is
  CHECK(names.size() == 17);
  CHECK(std::is_sorted(names.begin(), names.end()));
  CHECK(names.front() == "crthits");
  CHECK(names.back() == "truth_particles");
}

TEST_CASE("yaml lists as SPINE writes them") {
  using spine::core::parse_yaml_list;
  CHECK(parse_yaml_list("- units\n- topology\n") == std::vector<std::string>{"units", "topology"});
  CHECK(parse_yaml_list("- units") == std::vector<std::string>{"units"});
  CHECK(parse_yaml_list("[]\n").empty());
  CHECK(parse_yaml_list("").empty());
  CHECK(parse_yaml_list("- 'on'\n- \"yes\"\n- plain\n") ==
        std::vector<std::string>{"on", "yes", "plain"});
  CHECK(parse_yaml_list("  - a  \r\n- b\n\n") == std::vector<std::string>{"a", "b"});
  CHECK_THROWS_AS(parse_yaml_list("units"), Error);
  CHECK_THROWS_AS(parse_yaml_list("a: b\n"), Error);
  CHECK_THROWS_AS(parse_yaml_list("-a\n"), Error);
  CHECK_THROWS_AS(parse_yaml_list("- \n"), Error);
}

TEST_CASE("fail() puts the call, the object and the innermost message together") {
  Hdf5Lock lock;
  h5::ErrorSilencer silence;

  // nothing on the HDF5 error stack: the text says so, and still names the call and the object
  H5Eclear2(H5E_DEFAULT);
  try {
    h5::fail("H5Xcall", "the object");
    FAIL("fail() throws");
  } catch (const Error& error) {
    CHECK(std::string(error.what()) ==
          "H5Xcall on 'the object' failed (no message on the HDF5 error stack)");
  }
  H5Eclear2(H5E_DEFAULT);
  try {
    h5::fail("H5Ycall");
    FAIL("fail() throws");
  } catch (const Error& error) {
    CHECK(std::string(error.what()) == "H5Ycall failed (no message on the HDF5 error stack)");
  }

  // after a real failure the stack holds HDF5's messages, outermost first; the innermost is kept
  const std::string missing = data_path("no_such_file.h5");
  H5Fopen(missing.c_str(), H5F_ACC_RDONLY, H5P_DEFAULT);
  try {
    h5::fail("H5Fopen", "somewhere else");
    FAIL("fail() throws");
  } catch (const Error& error) {
    const std::string text = error.what();
    CHECK(text.rfind("H5Fopen on 'somewhere else' failed: ", 0) == 0);
    CHECK(contains(text, "No such file or directory"));
    CHECK_FALSE(contains(text, "no message"));
  }

  // check(): negative is failure, zero and positive are results
  CHECK(h5::check(0, "H5X") == 0);
  CHECK(h5::check(5, "H5X") == 5);
  CHECK(h5::check(static_cast<hid_t>(3), "H5X") == 3);
  CHECK_THROWS_AS(h5::check(-1, "H5X"), Error);
  CHECK_THROWS_AS(h5::check(static_cast<hssize_t>(-1), "H5X", "thing"), Error);
}

TEST_CASE("fixed-length string attributes: space padding is trimmed, text inside is kept") {
  Hdf5Lock lock;
  h5::ErrorSilencer silence;
  h5::File file = h5::open_file(data_path("edge_attributes_padded.h5"));
  CHECK(h5::read_string_attribute(file.get(), "space_padded") == "ab c");  // 8 bytes: "ab c    "
  CHECK(h5::read_string_attribute(file.get(), "space_padded_blank").empty());
}

TEST_CASE("children are listed in creation order when the group records it, else by name") {
  Hdf5Lock lock;
  h5::ErrorSilencer silence;
  h5::File file = h5::open_file(data_path("edge_creation_order.h5"));
  h5::Group products = h5::open_group(file.get(), "products");
  bool by_creation = false;
  const std::vector<std::string> names = h5::child_names(products.get(), &by_creation);
  CHECK(by_creation);
  CHECK(names == std::vector<std::string>{"zeta", "alpha", "mid"});  // not alphabetical

  // the product list of the file follows
  auto source = open("edge_creation_order.h5");
  CHECK(source->product_names() == std::vector<std::string>{"zeta", "alpha", "mid"});

  // the root of this file does not track order: name order
  by_creation = true;
  h5::Group root_group = h5::open_group(file.get(), "/");
  const std::vector<std::string> root = h5::child_names(root_group.get(), &by_creation);
  CHECK_FALSE(by_creation);
  CHECK(root == std::vector<std::string>{"events", "info", "products"});
}
