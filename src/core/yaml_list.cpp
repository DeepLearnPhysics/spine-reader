#include "core/yaml_list.hpp"

#include <spine/error.hpp>

namespace spine::core {
namespace {

std::string trim(const std::string& text) {
  const auto first = text.find_first_not_of(" \t\r");
  if (first == std::string::npos) return {};
  return text.substr(first, text.find_last_not_of(" \t\r") + 1 - first);
}

}  // namespace

std::vector<std::string> parse_yaml_list(const std::string& text) {
  std::vector<std::string> items;
  std::size_t start = 0;
  while (start <= text.size()) {
    std::size_t end = text.find('\n', start);
    if (end == std::string::npos) end = text.size();
    const std::string line = trim(text.substr(start, end - start));
    start = end + 1;

    if (line.empty() || line == "---" || line == "[]") continue;
    if (line[0] != '-' || (line.size() > 1 && line[1] != ' ')) {
      throw Error("cannot parse the YAML list '" + text + "'");
    }
    std::string item = trim(line.substr(1));
    if (item.size() >= 2 && (item.front() == '\'' || item.front() == '"') &&
        item.back() == item.front()) {
      item = item.substr(1, item.size() - 2);
    }
    if (item.empty()) throw Error("empty item in the YAML list '" + text + "'");
    items.push_back(std::move(item));
  }
  return items;
}

}  // namespace spine::core
