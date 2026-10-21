#include "core_test_util.hpp"

#include <fstream>

namespace core_test {

const json& expected() {
  static const json parsed = [] {
    std::ifstream in(data_path("expected.json"));
    return json::parse(in);
  }();
  return parsed;
}

}  // namespace core_test
