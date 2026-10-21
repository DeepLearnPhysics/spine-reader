// Golden tests: every fixture in tests/data/fixtures was written by SPINE's own writer and
// decoded independently with h5py (tools/fixtures/decode_raw.py). The core must reproduce the
// decoded file, product by product, column by column, row by row.

#include <doctest.h>
#include <json.hpp>

#include <array>
#include <cmath>
#include <fstream>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <type_traits>
#include <vector>

#include <spine/detail/assign.hpp>
#include <spine/error.hpp>
#include <spine/model.hpp>
#include <spine/reader.hpp>

#include "core/source.hpp"

namespace {

using nlohmann::json;
using spine::ScalarType;
using spine::detail::ColumnBuffer;

const std::vector<std::string> kCases = {
    "v1.4.0_v1",      "v1.4.0_v2", "v1.4.0_v1_lite", "v1.4.0_v2_lite",
    "v1.4.0_v2_fixed_only", "v2root", "v0.11.1_v1",
};

std::string fixture(const std::string& name, const std::string& extension) {
  return std::string(SPINE_READER_TEST_DATA) + "/fixtures/" + name + extension;
}

json expected(const std::string& name) {
  std::ifstream in(fixture(name, ".expected.json"));
  REQUIRE(in.good());
  return json::parse(in);
}

bool same_number(const json& want, double got) {
  if (want.is_string()) {
    const std::string s = want.get<std::string>();
    if (s == "nan") return std::isnan(got);
    if (s == "inf") return std::isinf(got) && got > 0;
    if (s == "-inf") return std::isinf(got) && got < 0;
    return false;
  }
  if (want.is_boolean()) return got == (want.get<bool>() ? 1.0 : 0.0);
  return want.get<double>() == got;
}

std::string describe(const json& want, const ColumnBuffer& column, std::size_t row) {
  std::string got;
  if (column.type == ScalarType::String) return want.dump() + " vs \"" + column.strings[row] + "\"";
  std::size_t begin = row * column.count, end = begin + column.count;
  if (column.variable) {
    begin = column.offsets[row];
    end = column.offsets[row + 1];
  }
  for (std::size_t i = begin; i < end; ++i) {
    got += (i > begin ? "," : "") + std::to_string(spine::detail::load<double>(column, i));
  }
  return want.dump() + " vs [" + got + "]";
}

// One row of one column against the decoded value.
bool same_value(const json& want, const ColumnBuffer& column, std::size_t row) {
  if (column.type == ScalarType::String) {
    return want.is_string() && want.get<std::string>() == column.strings[row];
  }
  if (column.variable) {
    const std::size_t begin = column.offsets[row];
    const std::size_t end = column.offsets[row + 1];
    if (!want.is_array() || want.size() != end - begin) return false;
    for (std::size_t i = begin; i < end; ++i) {
      if (!same_number(want[i - begin], spine::detail::load<double>(column, i))) return false;
    }
    return true;
  }
  if (column.count == 1 && !want.is_array()) {
    return same_number(want, spine::detail::load<double>(column, row));
  }
  if (!want.is_array() || want.size() != column.count) return false;
  for (std::size_t k = 0; k < column.count; ++k) {
    if (!same_number(want[k], spine::detail::load<double>(column, row * column.count + k))) {
      return false;
    }
  }
  return true;
}

void check_columns(const spine::core::ProductInfo& info, const json& want) {
  std::set<std::string> got_names, want_names;
  for (const auto& c : info.columns) got_names.insert(c.name);
  for (const auto& [name, unused] : want["columns"].items()) want_names.insert(name);
  CHECK(got_names == want_names);

  for (const auto& c : info.columns) {
    if (!want["columns"].contains(c.name)) continue;
    const json& w = want["columns"][c.name];
    INFO("column " << c.name);
    CHECK(std::string(spine::to_string(c.type)) == w["type"].get<std::string>());
    if (c.type != ScalarType::String) {
      CHECK(c.variable == w["variable"].get<bool>());
      CHECK(c.count == w["count"].get<std::size_t>());
    }
    // The decoder reports SPINE's {FALSE, TRUE} bool enum as a plain bool; the core also
    // lists its two labels.
    std::map<std::string, std::int64_t> labels;
    if (c.type != ScalarType::Bool) {
      for (const auto& m : c.enum_members) labels[m.name] = m.value;
    }
    std::map<std::string, std::int64_t> want_labels;
    if (!w["enum"].is_null()) want_labels = w["enum"].get<std::map<std::string, std::int64_t>>();
    CHECK(labels == want_labels);
  }
}

void check_values(spine::core::ProductSource& product, const json& want, std::uint64_t events) {
  const spine::core::ProductInfo& info = product.info();
  std::vector<spine::core::ColumnRequest> requests;
  for (const auto& c : info.columns) requests.push_back({&c, c.type});

  std::vector<ColumnBuffer> buffers;
  for (std::uint64_t e = 0; e < events; ++e) {
    INFO("event " << e);
    const json& rows = want["events"][e];
    const spine::core::RowRange range = product.rows(e);
    REQUIRE(range.size() == rows.size());
    product.read(range, requests, buffers);
    REQUIRE(buffers.size() == requests.size());
    for (std::size_t r = 0; r < rows.size(); ++r) {
      for (std::size_t k = 0; k < requests.size(); ++k) {
        const std::string& name = info.columns[k].name;
        const json& value =
            info.kind == spine::core::ProductKind::Objects ? rows[r][name] : rows[r];
        INFO("row " << r << ", column '" << name << "': " << describe(value, buffers[k], r));
        CHECK(same_value(value, buffers[k], r));
      }
    }
  }
}

}  // namespace

TEST_CASE("the core reproduces every golden fixture") {
  std::lock_guard<std::recursive_mutex> lock(spine::core::hdf5_mutex());
  for (const std::string& name : kCases) {
    INFO("fixture " << name);
    const json want = expected(name);
    auto file = spine::core::FileSource::open(fixture(name, ".h5"));
    const spine::core::FileInfo& info = file->info();
    CHECK(std::string(spine::core::to_string(info.layout)) == want["layout"].get<std::string>());
    CHECK(info.num_events == want["num_events"].get<std::uint64_t>());
    // what the writer says about itself; a file without the attribute reports the default
    const json& writer = want["writer"];
    CHECK(info.spine_version == writer["spine_version"].get<std::string>());
    CHECK(info.format_version == writer["format_version"].get<int>());
    CHECK(info.complete == (writer["complete"].is_null() || writer["complete"].get<bool>()));

    std::set<std::string> got_products(file->product_names().begin(),
                                       file->product_names().end());
    std::set<std::string> want_products;
    for (const auto& [key, unused] : want["products"].items()) want_products.insert(key);
    CHECK(got_products == want_products);

    for (const auto& [key, product_want] : want["products"].items()) {
      INFO("product " << key);
      REQUIRE(file->has_product(key));
      spine::core::ProductSource& product = file->product(key);
      const spine::core::ProductInfo& product_info = product.info();
      CHECK(std::string(spine::core::to_string(product_info.kind)) ==
            product_want["kind"].get<std::string>());
      const std::string want_class = product_want["class_name"].is_null()
                                         ? std::string()
                                         : product_want["class_name"].get<std::string>();
      CHECK(product_info.class_name == want_class);
      CHECK(product_info.scalar == product_want["scalar"].get<bool>());
      check_columns(product_info, product_want);
      check_values(product, product_want, info.num_events);
    }
  }
}

TEST_CASE("incomplete files are refused unless allowed") {
  std::lock_guard<std::recursive_mutex> lock(spine::core::hdf5_mutex());
  const std::string path = fixture("incomplete", ".h5");
  CHECK_THROWS_AS(spine::core::FileSource::open(path), spine::Error);
  spine::core::OpenOptions options;
  options.allow_incomplete = true;
  auto file = spine::core::FileSource::open(path, options);
  CHECK_FALSE(file->info().complete);
}

namespace {

// ---------------------------------------------------------------------------------------------
// Model objects against the decoded fixture, member by member (golden_members.inc lists them)
// ---------------------------------------------------------------------------------------------

template <class T>
struct is_std_array : std::false_type {};
template <class T, std::size_t N>
struct is_std_array<std::array<T, N>> : std::true_type {};
template <class T>
struct is_std_vector : std::false_type {};
template <class T, class A>
struct is_std_vector<std::vector<T, A>> : std::true_type {};

/// True if `value` is what the fixture holds. Floats compare as the member's own type: the
/// fixture stores the exact double of a float32, and a float64 column read into a float member
/// is rounded to float.
template <class M>
bool equal_member(const M& value, const json& want) {
  if constexpr (std::is_same_v<M, std::string>) {
    return want.is_string() && want.get<std::string>() == value;
  } else if constexpr (std::is_same_v<M, bool>) {
    return want.is_boolean() ? want.get<bool>() == value : (want.get<double>() != 0) == value;
  } else if constexpr (std::is_enum_v<M>) {
    return want.is_number_integer() && want.get<std::int64_t>() == static_cast<std::int64_t>(value);
  } else if constexpr (std::is_floating_point_v<M>) {
    if (want.is_string()) {
      const std::string text = want.get<std::string>();
      if (text == "nan") return std::isnan(value);
      if (text == "inf") return std::isinf(value) && value > 0;
      if (text == "-inf") return std::isinf(value) && value < 0;
      return false;
    }
    return want.is_number() && static_cast<M>(want.get<double>()) == value;
  } else if constexpr (std::is_arithmetic_v<M>) {
    return want.is_number_integer() && static_cast<M>(want.get<std::int64_t>()) == value;
  } else {
    static_assert(is_std_array<M>::value || is_std_vector<M>::value, "unsupported member");
    if (!want.is_array() || want.size() != value.size()) return false;
    for (std::size_t i = 0; i < value.size(); ++i) {
      if (!equal_member(value[i], want[i])) return false;
    }
    return true;
  }
}

#include "golden_members.inc"

/// One member: present exactly when the fixture has the column, and equal to it.
#define GOLDEN_CHECK_MEMBER(name)                                                             \
  {                                                                                           \
    const bool in_file = row.contains(#name);                                                 \
    CHECK_MESSAGE(object.has(Cls::Field::name) == in_file, #name << ": has() vs the file");   \
    if (in_file) {                                                                            \
      CHECK_MESSAGE(equal_member(object.name, row[#name]),                                    \
                    #name << ": the file has " << row[#name].dump());                         \
    }                                                                                         \
  }

/// check_objects(const std::vector<Class>&, rows) for every model class.
#define GOLDEN_CHECK_OBJECTS(Class)                                      \
  void check_objects(const std::vector<spine::Class>& objects, const json& rows) {           \
    using Cls = spine::Class;                                                                 \
    REQUIRE(objects.size() == rows.size());                                                   \
    for (std::size_t r = 0; r < rows.size(); ++r) {                                           \
      INFO(#Class << " row " << r);                                                           \
      const Cls& object = objects[r];                                                         \
      const json& row = rows[r];                                                              \
      GOLDEN_MEMBERS_##Class(GOLDEN_CHECK_MEMBER)                                             \
    }                                                                                         \
  }
SPINE_READER_FOR_EACH_CLASS(GOLDEN_CHECK_OBJECTS)

struct ClassTools {
  const spine::ClassDescriptor* descriptor;
  std::function<void(const spine::Event&, const std::string&, const json&)> check;
};

const std::map<std::string, ClassTools>& class_tools() {
  static const std::map<std::string, ClassTools> tools = {
#define GOLDEN_TOOLS(Class)                                                                   \
  {#Class,                                                                                    \
   {&spine::Class::descriptor(),                                                              \
    [](const spine::Event& event, const std::string& key, const json& rows) {                 \
      check_objects(event.objects<spine::Class>(key), rows);                                  \
    }}},
      SPINE_READER_FOR_EACH_CLASS(GOLDEN_TOOLS)
#undef GOLDEN_TOOLS
  };
  return tools;
}

/// Row of `rows` (a list of decoded objects) whose "id" is `id`, or null.
const json* by_id(const json& rows, std::int64_t id) {
  for (const json& row : rows) {
    if (row["id"].get<std::int64_t>() == id) return &row;
  }
  return nullptr;
}

}  // namespace

TEST_CASE("the reader fills every member of every model object as the fixtures say") {
  std::size_t objects_checked = 0;
  for (const std::string& name : kCases) {
    INFO("fixture " << name);
    const json want = expected(name);
    spine::Reader reader(fixture(name, ".h5"));
    REQUIRE(reader.num_events() == want["num_events"].get<std::uint64_t>());
    REQUIRE(reader.num_files() == 1);

    // The report agrees with the file: a model field is missing exactly when the file lacks the
    // column, a file column is extra exactly when the model lacks the field, nothing is
    // mismatched. (The 'index' product and friends are not objects and are not read.)
    const spine::SchemaReport& report = reader.schema_report();
    CHECK(report.layout == want["layout"].get<std::string>());
    CHECK(report.format_version == want["writer"]["format_version"].get<int>());
    CHECK(report.spine_version == want["writer"]["spine_version"].get<std::string>());
    std::size_t reported = 0;
    for (const auto& [key, product] : want["products"].items()) {
      if (product["kind"].get<std::string>() != "objects") continue;
      INFO("product " << key);
      const auto tools = class_tools().find(product["class_name"].get<std::string>());
      REQUIRE(tools != class_tools().end());
      const spine::ClassDescriptor& cls = *tools->second.descriptor;

      std::set<std::string> want_missing, want_extra;
      for (std::size_t i = 0; i < cls.num_fields; ++i) {
        if (!product["columns"].contains(std::string(cls.fields[i].name))) {
          want_missing.insert(std::string(cls.fields[i].name));
        }
      }
      for (const auto& [column, unused] : product["columns"].items()) {
        if (cls.find(column) == nullptr) want_extra.insert(column);
      }
      const auto found =
          std::find_if(report.products.begin(), report.products.end(),
                       [&](const spine::ProductReport& p) { return p.product == key; });
      REQUIRE(found != report.products.end());
      ++reported;
      CHECK(found->class_name == product["class_name"].get<std::string>());
      CHECK(std::set<std::string>(found->missing.begin(), found->missing.end()) == want_missing);
      CHECK(std::set<std::string>(found->extra.begin(), found->extra.end()) == want_extra);
      CHECK(found->mismatched.empty());
      CHECK(found->complete() == want_missing.empty());  // extra columns do not count
    }
    CHECK(report.products.size() == reported);

    for (std::uint64_t e = 0; e < reader.num_events(); ++e) {
      INFO("event " << e);
      const spine::Event event = reader.event(e);
      CHECK(event.index() == e);
      CHECK(event.entry() == e);
      CHECK(event.file_index() == 0);
      for (const auto& [key, product] : want["products"].items()) {
        if (product["kind"].get<std::string>() != "objects") continue;
        INFO("product " << key);
        const json& rows = product["events"][e];
        CHECK(event.has(key));
        class_tools().at(product["class_name"].get<std::string>()).check(event, key, rows);
        objects_checked += rows.size();
      }
    }
  }
  CHECK(objects_checked > 300);  // the loops above are not empty (about 50 objects per fixture)
}

namespace {

/// A list of numbers of a decoded row, or nothing when the file lacks the column.
template <class T>
std::vector<T> list_of(const json& row, const char* column) {
  std::vector<T> out;
  if (row.contains(column)) {
    for (const json& item : row[column]) out.push_back(item.get<T>());
  }
  return out;
}

}  // namespace

TEST_CASE("event relations agree with the ids the fixtures hold") {
  std::size_t resolved = 0, matched = 0, members = 0;
  for (const std::string& name : kCases) {
    INFO("fixture " << name);
    const json want = expected(name);
    spine::Reader reader(fixture(name, ".h5"));
    for (std::uint64_t e = 0; e < reader.num_events(); ++e) {
      INFO("event " << e);
      const spine::Event event = reader.event(e);
      const json& products = want["products"];
      const json& reco_particles = products["reco_particles"]["events"][e];
      const json& reco_interactions = products["reco_interactions"]["events"][e];
      const json& truth_particles = products["truth_particles"]["events"][e];
      const json& truth_interactions = products["truth_interactions"]["events"][e];
      REQUIRE(event.reco_particles().size() == reco_particles.size());
      REQUIRE(event.truth_particles().size() == truth_particles.size());

      // particle -> interaction
      for (std::size_t i = 0; i < reco_particles.size(); ++i) {
        const json* target =
            by_id(reco_interactions, reco_particles[i]["interaction_id"].get<std::int64_t>());
        const spine::RecoInteraction* got = event.interaction_of(event.reco_particles()[i]);
        if (target == nullptr) {
          CHECK(got == nullptr);
        } else {
          REQUIRE(got != nullptr);
          CHECK(got->id == (*target)["id"].get<std::int64_t>());
          ++resolved;
        }
      }
      for (std::size_t i = 0; i < truth_particles.size(); ++i) {
        const json* target =
            by_id(truth_interactions, truth_particles[i]["interaction_id"].get<std::int64_t>());
        const spine::TruthInteraction* got = event.interaction_of(event.truth_particles()[i]);
        if (target == nullptr) {
          CHECK(got == nullptr);
        } else {
          REQUIRE(got != nullptr);
          CHECK(got->id == (*target)["id"].get<std::int64_t>());
          ++resolved;
        }
      }

      // interaction -> particles, in the order of particle_ids, without the ids that point nowhere
      for (std::size_t i = 0; i < reco_interactions.size(); ++i) {
        std::vector<std::int64_t> want_ids;
        for (std::int64_t id : list_of<std::int64_t>(reco_interactions[i], "particle_ids")) {
          if (by_id(reco_particles, id) != nullptr) want_ids.push_back(id);
        }
        const auto got = event.particles_of(event.reco_interactions()[i]);
        REQUIRE(got.size() == want_ids.size());
        for (std::size_t k = 0; k < got.size(); ++k) CHECK(got[k]->id == want_ids[k]);
        members += got.size();
      }
      for (std::size_t i = 0; i < truth_interactions.size(); ++i) {
        std::vector<std::int64_t> want_ids;
        for (std::int64_t id : list_of<std::int64_t>(truth_interactions[i], "particle_ids")) {
          if (by_id(truth_particles, id) != nullptr) want_ids.push_back(id);
        }
        const auto got = event.particles_of(event.truth_interactions()[i]);
        REQUIRE(got.size() == want_ids.size());
        for (std::size_t k = 0; k < got.size(); ++k) CHECK(got[k]->id == want_ids[k]);
        members += got.size();
      }

      // matches: the ids of match_ids that exist on the other side, in file order, with their
      // overlaps
      const auto check_matches = [&](const json& rows, const json& others, auto&& matches_of,
                                     const auto& objects) {
        for (std::size_t i = 0; i < rows.size(); ++i) {
          const std::vector<std::int64_t> ids = list_of<std::int64_t>(rows[i], "match_ids");
          const std::vector<float> overlaps = list_of<float>(rows[i], "match_overlaps");
          std::vector<std::pair<std::int64_t, float>> expected_matches;
          for (std::size_t k = 0; k < ids.size(); ++k) {
            if (by_id(others, ids[k]) != nullptr) {
              expected_matches.emplace_back(ids[k], overlaps.at(k));
            }
          }
          const auto got = matches_of(objects[i]);
          REQUIRE(got.size() == expected_matches.size());
          for (std::size_t k = 0; k < got.size(); ++k) {
            CHECK(got[k].object->id == expected_matches[k].first);
            CHECK(got[k].overlap == expected_matches[k].second);
            ++matched;
          }
        }
      };
      check_matches(reco_particles, truth_particles,
                    [&](const spine::RecoParticle& p) { return event.matches(p); },
                    event.reco_particles());
      check_matches(truth_particles, reco_particles,
                    [&](const spine::TruthParticle& p) { return event.matches(p); },
                    event.truth_particles());
      check_matches(reco_interactions, truth_interactions,
                    [&](const spine::RecoInteraction& p) { return event.matches(p); },
                    event.reco_interactions());
      check_matches(truth_interactions, reco_interactions,
                    [&](const spine::TruthInteraction& p) { return event.matches(p); },
                    event.truth_interactions());
    }

    // every event is found again by its (run, subrun, event)
    for (std::uint64_t e = 0; e < reader.num_events(); ++e) {
      const json& run = want["products"]["run_info"]["events"][e][0];
      const auto found =
          reader.find(run["run"].get<std::int64_t>(), run["subrun"].get<std::int64_t>(),
                      run["event"].get<std::int64_t>());
      REQUIRE(found.has_value());
      CHECK(*found == e);
    }
  }
  // the fixtures resolve links in both directions and have matches: the loops above are not empty
  CHECK(resolved > 30);
  CHECK(members > 30);
  CHECK(matched > 10);
}
